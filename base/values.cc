// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <ostream>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/bit_cast.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/checked_iterators.h"
#include "base/containers/map_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/base_tracing.h"
#include "base/tracing_buildflags.h"
#include "base/types/to_address.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

#if BUILDFLAG(ENABLE_BASE_TRACING)
#include "base/trace_event/memory_usage_estimator.h"  // no-presubmit-check
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

namespace base {

namespace {

constexpr auto kTypeNames =
    std::to_array<const char*>({"null", "boolean", "integer", "double",
                                "string", "binary", "dictionary", "list"});
static_assert(kTypeNames.size() == static_cast<size_t>(Value::Type::LIST) + 1,
              "kTypeNames Has Wrong Size");

// Helper class to enumerate the path components from a std::string_view
// without performing heap allocations. Components are simply separated
// by single dots (e.g. "foo.bar.baz"  -> ["foo", "bar", "baz"]).
//
// Usage example:
//    PathSplitter splitter(some_path);
//    while (splitter.HasNext()) {
//       std::string_view component = splitter.Next();
//       ...
//    }
//
class PathSplitter {
 public:
  explicit PathSplitter(std::string_view path) : path_(path) {}

  bool HasNext() const { return pos_ < path_.size(); }

  std::string_view Next() {
    DCHECK(HasNext());
    size_t start = pos_;
    size_t pos = path_.find('.', start);
    size_t end;
    if (pos == path_.npos) {
      end = path_.size();
      pos_ = end;
    } else {
      end = pos;
      pos_ = pos + 1;
    }
    return path_.substr(start, end - start);
  }

 private:
  std::string_view path_;
  size_t pos_ = 0;
};

std::string DebugStringImpl(ValueView value) {
  std::string json;
  JSONWriter::WriteWithOptions(value, JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return json;
}

}  // namespace

// A helper used to provide templated functions for cloning to Value, and
// ValueView. This private class is used so the cloning method may have access
// to the special private constructors in Value, created specifically for
// cloning.
class Value::CloningHelper {
 public:
  // This set of overloads are used to unwrap the reference wrappers, which are
  // presented when cloning a ValueView.
  template <typename T>
  static const T& UnwrapReference(std::reference_wrapper<const T> value) {
    return value.get();
  }

  template <typename T>
  static const T& UnwrapReference(const T& value) {
    return value;
  }

  // Returns a new Value object using the contents of the |storage| variant.
  template <typename Storage>
  static Value Clone(const Storage& storage) {
    return absl::visit(
        [](const auto& member) {
          const auto& value = UnwrapReference(member);
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, DictValue> ||
                        std::is_same_v<T, ListValue>) {
            return Value(value.Clone());
          } else {
            return Value(value);
          }
        },
        storage);
  }
};

// static
Value Value::FromUniquePtrValue(std::unique_ptr<Value> val) {
  return std::move(*val);
}

// static
std::unique_ptr<Value> Value::ToUniquePtrValue(Value val) {
  return std::make_unique<Value>(std::move(val));
}

Value::Value() noexcept = default;

Value::Value(Value&&) noexcept = default;

Value& Value::operator=(Value&&) noexcept = default;

Value::Value(Type type) {
  // Initialize with the default value.
  switch (type) {
    case Type::NONE:
      return;

    case Type::BOOLEAN:
      data_.emplace<bool>(false);
      return;
    case Type::INTEGER:
      data_.emplace<int>(0);
      return;
    case Type::DOUBLE:
      data_.emplace<DoubleStorage>(0.0);
      return;
    case Type::STRING:
      data_.emplace<std::string>();
      return;
    case Type::BINARY:
      data_.emplace<BlobStorage>();
      return;
    case Type::DICT:
      data_.emplace<Dict>();
      return;
    case Type::LIST:
      data_.emplace<List>();
      return;
  }

  NOTREACHED();
}

Value::Value(bool value) : data_(value) {}

Value::Value(int value) : data_(value) {}

Value::Value(double value)
    : data_(absl::in_place_type_t<DoubleStorage>(), value) {}

Value::Value(std::string_view value) : Value(std::string(value)) {}

Value::Value(std::u16string_view value) : Value(UTF16ToUTF8(value)) {}

Value::Value(const char* value) : Value(std::string(value)) {}

Value::Value(const char16_t* value) : Value(UTF16ToUTF8(value)) {}

Value::Value(std::string&& value) noexcept : data_(std::move(value)) {
  DCHECK(IsStringUTF8AllowingNoncharacters(GetString()));
}

Value::Value(const std::vector<char>& value)
    : data_(absl::in_place_type_t<BlobStorage>(), value.begin(), value.end()) {}

Value::Value(base::span<const uint8_t> value)
    : data_(absl::in_place_type_t<BlobStorage>(), value.size()) {
  // This is 100x faster than using the "range" constructor for a 512k blob:
  // crbug.com/1343636
  std::ranges::copy(value, absl::get<BlobStorage>(data_).data());
}

Value::Value(BlobStorage&& value) noexcept : data_(std::move(value)) {}

Value::Value(Dict&& value) noexcept : data_(std::move(value)) {}

Value::Value(List&& value) noexcept : data_(std::move(value)) {}

Value::Value(absl::monostate) {}

Value::Value(DoubleStorage storage) : data_(std::move(storage)) {}

Value::DoubleStorage::DoubleStorage(double v) : v_(bit_cast<decltype(v_)>(v)) {
  if (!std::isfinite(v)) {
    DUMP_WILL_BE_NOTREACHED()
        << "Non-finite (i.e. NaN or positive/negative infinity) "
        << "values cannot be represented in JSON";
    v_ = bit_cast<decltype(v_)>(0.0);
  }
}

Value Value::Clone() const {
  return CloningHelper::Clone(data_);
}

Value::~Value() = default;

// static
const char* Value::GetTypeName(Value::Type type) {
  DCHECK_GE(static_cast<int>(type), 0);
  DCHECK_LT(static_cast<size_t>(type), std::size(kTypeNames));
  return kTypeNames[static_cast<size_t>(type)];
}

std::optional<bool> Value::GetIfBool() const {
  return is_bool() ? std::make_optional(GetBool()) : std::nullopt;
}

std::optional<int> Value::GetIfInt() const {
  return is_int() ? std::make_optional(GetInt()) : std::nullopt;
}

std::optional<double> Value::GetIfDouble() const {
  return (is_int() || is_double()) ? std::make_optional(GetDouble())
                                   : std::nullopt;
}

const std::string* Value::GetIfString() const {
  return absl::get_if<std::string>(&data_);
}

std::string* Value::GetIfString() {
  return absl::get_if<std::string>(&data_);
}

const Value::BlobStorage* Value::GetIfBlob() const {
  return absl::get_if<BlobStorage>(&data_);
}

Value::BlobStorage* Value::GetIfBlob() {
  return absl::get_if<BlobStorage>(&data_);
}

const DictValue* Value::GetIfDict() const {
  return absl::get_if<Dict>(&data_);
}

DictValue* Value::GetIfDict() {
  return absl::get_if<Dict>(&data_);
}

const ListValue* Value::GetIfList() const {
  return absl::get_if<List>(&data_);
}

ListValue* Value::GetIfList() {
  return absl::get_if<List>(&data_);
}

bool Value::GetBool() const {
  DCHECK(is_bool());
  return absl::get<bool>(data_);
}

int Value::GetInt() const {
  DCHECK(is_int());
  return absl::get<int>(data_);
}

double Value::GetDouble() const {
  if (is_double()) {
    return absl::get<DoubleStorage>(data_);
  }
  CHECK(is_int());
  return GetInt();
}

const std::string& Value::GetString() const {
  DCHECK(is_string());
  return absl::get<std::string>(data_);
}

std::string& Value::GetString() {
  DCHECK(is_string());
  return absl::get<std::string>(data_);
}

const Value::BlobStorage& Value::GetBlob() const {
  DCHECK(is_blob());
  return absl::get<BlobStorage>(data_);
}

Value::BlobStorage& Value::GetBlob() {
  DCHECK(is_blob());
  return absl::get<BlobStorage>(data_);
}

const DictValue& Value::GetDict() const {
  DCHECK(is_dict());
  return absl::get<Dict>(data_);
}

DictValue& Value::GetDict() {
  DCHECK(is_dict());
  return absl::get<Dict>(data_);
}

const ListValue& Value::GetList() const {
  DCHECK(is_list());
  return absl::get<List>(data_);
}

ListValue& Value::GetList() {
  DCHECK(is_list());
  return absl::get<List>(data_);
}

std::string Value::TakeString() && {
  return std::move(GetString());
}

Value::BlobStorage Value::TakeBlob() && {
  return std::move(GetBlob());
}

DictValue Value::TakeDict() && {
  return std::move(GetDict());
}

ListValue Value::TakeList() && {
  return std::move(GetList());
}

DictValue::DictValue() = default;

DictValue::DictValue(DictValue&&) noexcept = default;

DictValue& DictValue::operator=(DictValue&&) noexcept = default;

DictValue::~DictValue() = default;

bool DictValue::empty() const {
  return storage_.empty();
}

size_t DictValue::size() const {
  return storage_.size();
}

DictValue::iterator DictValue::begin() {
  return iterator(storage_.begin());
}

DictValue::const_iterator DictValue::begin() const {
  return const_iterator(storage_.begin());
}

DictValue::const_iterator DictValue::cbegin() const {
  return const_iterator(storage_.cbegin());
}

DictValue::iterator DictValue::end() {
  return iterator(storage_.end());
}

DictValue::const_iterator DictValue::end() const {
  return const_iterator(storage_.end());
}

DictValue::const_iterator DictValue::cend() const {
  return const_iterator(storage_.cend());
}

bool DictValue::contains(std::string_view key) const {
  DCHECK(IsStringUTF8AllowingNoncharacters(key));

  return storage_.contains(key);
}

void DictValue::clear() {
  return storage_.clear();
}

DictValue::iterator DictValue::erase(iterator pos) {
  return iterator(storage_.erase(pos.GetUnderlyingIteratorDoNotUse()));
}

DictValue::iterator DictValue::erase(const_iterator pos) {
  return iterator(storage_.erase(pos.GetUnderlyingIteratorDoNotUse()));
}

DictValue DictValue::Clone() const {
  std::vector<std::pair<std::string, std::unique_ptr<Value>>> storage;
  storage.reserve(storage_.size());

  for (const auto& [key, value] : storage_) {
    storage.emplace_back(key, std::make_unique<Value>(value->Clone()));
  }

  DictValue result;
  // `storage` is already sorted and unique by construction, which allows us to
  // avoid an additional O(n log n) step.
  result.storage_ = flat_map<std::string, std::unique_ptr<Value>>(
      sorted_unique, std::move(storage));
  return result;
}

void DictValue::Merge(DictValue dict) {
  for (const auto [key, value] : dict) {
    if (DictValue* nested_dict = value.GetIfDict()) {
      if (DictValue* current_dict = FindDict(key)) {
        // If `key` is a nested dictionary in this dictionary and the dictionary
        // being merged, recursively merge the two dictionaries.
        current_dict->Merge(std::move(*nested_dict));
        continue;
      }
    }

    // Otherwise, unconditionally set the value, overwriting any value that may
    // already be associated with the key.
    Set(key, std::move(value));
  }
}

const Value* DictValue::Find(std::string_view key) const {
  DCHECK(IsStringUTF8AllowingNoncharacters(key));
  return FindPtrOrNull(storage_, key);
}

Value* DictValue::Find(std::string_view key) {
  return FindPtrOrNull(storage_, key);
}

std::optional<bool> DictValue::FindBool(std::string_view key) const {
  const Value* v = Find(key);
  return v ? v->GetIfBool() : std::nullopt;
}

std::optional<int> DictValue::FindInt(std::string_view key) const {
  const Value* v = Find(key);
  return v ? v->GetIfInt() : std::nullopt;
}

std::optional<double> DictValue::FindDouble(std::string_view key) const {
  const Value* v = Find(key);
  return v ? v->GetIfDouble() : std::nullopt;
}

const std::string* DictValue::FindString(std::string_view key) const {
  const Value* v = Find(key);
  return v ? v->GetIfString() : nullptr;
}

std::string* DictValue::FindString(std::string_view key) {
  Value* v = Find(key);
  return v ? v->GetIfString() : nullptr;
}

const Value::BlobStorage* DictValue::FindBlob(std::string_view key) const {
  const Value* v = Find(key);
  return v ? v->GetIfBlob() : nullptr;
}

Value::BlobStorage* DictValue::FindBlob(std::string_view key) {
  Value* v = Find(key);
  return v ? v->GetIfBlob() : nullptr;
}

const DictValue* DictValue::FindDict(std::string_view key) const {
  const Value* v = Find(key);
  return v ? v->GetIfDict() : nullptr;
}

DictValue* DictValue::FindDict(std::string_view key) {
  Value* v = Find(key);
  return v ? v->GetIfDict() : nullptr;
}

const ListValue* DictValue::FindList(std::string_view key) const {
  const Value* v = Find(key);
  return v ? v->GetIfList() : nullptr;
}

ListValue* DictValue::FindList(std::string_view key) {
  Value* v = Find(key);
  return v ? v->GetIfList() : nullptr;
}

DictValue* DictValue::EnsureDict(std::string_view key) {
  DictValue* dict = FindDict(key);
  if (dict) {
    return dict;
  }
  return &Set(key, DictValue())->GetDict();
}

ListValue* DictValue::EnsureList(std::string_view key) {
  ListValue* list = FindList(key);
  if (list) {
    return list;
  }
  return &Set(key, ListValue())->GetList();
}

Value* DictValue::Set(std::string_view key, Value&& value) & {
  DCHECK(IsStringUTF8AllowingNoncharacters(key));

  auto wrapped_value = std::make_unique<Value>(std::move(value));
  auto* raw_value = wrapped_value.get();
  storage_.insert_or_assign(key, std::move(wrapped_value));
  return raw_value;
}

Value* DictValue::Set(std::string_view key, bool value) & {
  return Set(key, Value(value));
}

Value* DictValue::Set(std::string_view key, int value) & {
  return Set(key, Value(value));
}

Value* DictValue::Set(std::string_view key, double value) & {
  return Set(key, Value(value));
}

Value* DictValue::Set(std::string_view key, std::string_view value) & {
  return Set(key, Value(value));
}

Value* DictValue::Set(std::string_view key, std::u16string_view value) & {
  return Set(key, Value(value));
}

Value* DictValue::Set(std::string_view key, const char* value) & {
  return Set(key, Value(value));
}

Value* DictValue::Set(std::string_view key, const char16_t* value) & {
  return Set(key, Value(value));
}

Value* DictValue::Set(std::string_view key, std::string&& value) & {
  return Set(key, Value(std::move(value)));
}

Value* DictValue::Set(std::string_view key, BlobStorage&& value) & {
  return Set(key, Value(std::move(value)));
}

Value* DictValue::Set(std::string_view key, DictValue&& value) & {
  return Set(key, Value(std::move(value)));
}

Value* DictValue::Set(std::string_view key, ListValue&& value) & {
  return Set(key, Value(std::move(value)));
}

DictValue&& DictValue::Set(std::string_view key, Value&& value) && {
  DCHECK(IsStringUTF8AllowingNoncharacters(key));
  storage_.insert_or_assign(key, std::make_unique<Value>(std::move(value)));
  return std::move(*this);
}

DictValue&& DictValue::Set(std::string_view key, bool value) && {
  return std::move(*this).Set(key, Value(value));
}

DictValue&& DictValue::Set(std::string_view key, int value) && {
  return std::move(*this).Set(key, Value(value));
}

DictValue&& DictValue::Set(std::string_view key, double value) && {
  return std::move(*this).Set(key, Value(value));
}

DictValue&& DictValue::Set(std::string_view key, std::string_view value) && {
  return std::move(*this).Set(key, Value(value));
}

DictValue&& DictValue::Set(std::string_view key, std::u16string_view value) && {
  return std::move(*this).Set(key, Value(value));
}

DictValue&& DictValue::Set(std::string_view key, const char* value) && {
  return std::move(*this).Set(key, Value(value));
}

DictValue&& DictValue::Set(std::string_view key, const char16_t* value) && {
  return std::move(*this).Set(key, Value(value));
}

DictValue&& DictValue::Set(std::string_view key, std::string&& value) && {
  return std::move(*this).Set(key, Value(std::move(value)));
}

DictValue&& DictValue::Set(std::string_view key, BlobStorage&& value) && {
  return std::move(*this).Set(key, Value(std::move(value)));
}

DictValue&& DictValue::Set(std::string_view key, DictValue&& value) && {
  return std::move(*this).Set(key, Value(std::move(value)));
}

DictValue&& DictValue::Set(std::string_view key, ListValue&& value) && {
  return std::move(*this).Set(key, Value(std::move(value)));
}

bool DictValue::Remove(std::string_view key) {
  DCHECK(IsStringUTF8AllowingNoncharacters(key));

  return storage_.erase(key) > 0;
}

std::optional<Value> DictValue::Extract(std::string_view key) {
  DCHECK(IsStringUTF8AllowingNoncharacters(key));

  auto it = storage_.find(key);
  if (it == storage_.end()) {
    return std::nullopt;
  }
  Value v = std::move(*it->second);
  storage_.erase(it);
  return v;
}

const Value* DictValue::FindByDottedPath(std::string_view path) const {
  DCHECK(!path.empty());
  DCHECK(IsStringUTF8AllowingNoncharacters(path));

  const DictValue* current_dict = this;
  const Value* current_value = nullptr;
  PathSplitter splitter(path);
  while (true) {
    current_value = current_dict->Find(splitter.Next());
    if (!splitter.HasNext()) {
      return current_value;
    }
    if (!current_value) {
      return nullptr;
    }
    current_dict = current_value->GetIfDict();
    if (!current_dict) {
      return nullptr;
    }
  }
}

Value* DictValue::FindByDottedPath(std::string_view path) {
  return const_cast<Value*>(std::as_const(*this).FindByDottedPath(path));
}

std::optional<bool> DictValue::FindBoolByDottedPath(
    std::string_view path) const {
  const Value* v = FindByDottedPath(path);
  return v ? v->GetIfBool() : std::nullopt;
}

std::optional<int> DictValue::FindIntByDottedPath(std::string_view path) const {
  const Value* v = FindByDottedPath(path);
  return v ? v->GetIfInt() : std::nullopt;
}

std::optional<double> DictValue::FindDoubleByDottedPath(
    std::string_view path) const {
  const Value* v = FindByDottedPath(path);
  return v ? v->GetIfDouble() : std::nullopt;
}

const std::string* DictValue::FindStringByDottedPath(
    std::string_view path) const {
  const Value* v = FindByDottedPath(path);
  return v ? v->GetIfString() : nullptr;
}

std::string* DictValue::FindStringByDottedPath(std::string_view path) {
  Value* v = FindByDottedPath(path);
  return v ? v->GetIfString() : nullptr;
}

const Value::BlobStorage* DictValue::FindBlobByDottedPath(
    std::string_view path) const {
  const Value* v = FindByDottedPath(path);
  return v ? v->GetIfBlob() : nullptr;
}

Value::BlobStorage* DictValue::FindBlobByDottedPath(std::string_view path) {
  Value* v = FindByDottedPath(path);
  return v ? v->GetIfBlob() : nullptr;
}

const DictValue* DictValue::FindDictByDottedPath(std::string_view path) const {
  const Value* v = FindByDottedPath(path);
  return v ? v->GetIfDict() : nullptr;
}

DictValue* DictValue::FindDictByDottedPath(std::string_view path) {
  Value* v = FindByDottedPath(path);
  return v ? v->GetIfDict() : nullptr;
}

const ListValue* DictValue::FindListByDottedPath(std::string_view path) const {
  const Value* v = FindByDottedPath(path);
  return v ? v->GetIfList() : nullptr;
}

ListValue* DictValue::FindListByDottedPath(std::string_view path) {
  Value* v = FindByDottedPath(path);
  return v ? v->GetIfList() : nullptr;
}

Value* DictValue::SetByDottedPath(std::string_view path, Value&& value) & {
  DCHECK(!path.empty());
  DCHECK(IsStringUTF8AllowingNoncharacters(path));

  DictValue* current_dict = this;
  Value* current_value = nullptr;
  PathSplitter splitter(path);
  while (true) {
    std::string_view next_key = splitter.Next();
    if (!splitter.HasNext()) {
      return current_dict->Set(next_key, std::move(value));
    }
    // This could be clever to avoid a double-lookup via use of lower_bound(),
    // but for now, just implement it the most straightforward way.
    current_value = current_dict->Find(next_key);
    if (current_value) {
      // Unlike the legacy DictionaryValue API, encountering an intermediate
      // node that is not a `Value::Type::DICT` is an error.
      current_dict = current_value->GetIfDict();
      if (!current_dict) {
        return nullptr;
      }
    } else {
      current_dict = &current_dict->Set(next_key, DictValue())->GetDict();
    }
  }
}

Value* DictValue::SetByDottedPath(std::string_view path, bool value) & {
  return SetByDottedPath(path, Value(value));
}

Value* DictValue::SetByDottedPath(std::string_view path, int value) & {
  return SetByDottedPath(path, Value(value));
}

Value* DictValue::SetByDottedPath(std::string_view path, double value) & {
  return SetByDottedPath(path, Value(value));
}

Value* DictValue::SetByDottedPath(std::string_view path,
                                  std::string_view value) & {
  return SetByDottedPath(path, Value(value));
}

Value* DictValue::SetByDottedPath(std::string_view path,
                                  std::u16string_view value) & {
  return SetByDottedPath(path, Value(value));
}

Value* DictValue::SetByDottedPath(std::string_view path, const char* value) & {
  return SetByDottedPath(path, Value(value));
}

Value* DictValue::SetByDottedPath(std::string_view path,
                                  const char16_t* value) & {
  return SetByDottedPath(path, Value(value));
}

Value* DictValue::SetByDottedPath(std::string_view path,
                                  std::string&& value) & {
  return SetByDottedPath(path, Value(std::move(value)));
}

Value* DictValue::SetByDottedPath(std::string_view path,
                                  BlobStorage&& value) & {
  return SetByDottedPath(path, Value(std::move(value)));
}

Value* DictValue::SetByDottedPath(std::string_view path, DictValue&& value) & {
  return SetByDottedPath(path, Value(std::move(value)));
}

Value* DictValue::SetByDottedPath(std::string_view path, ListValue&& value) & {
  return SetByDottedPath(path, Value(std::move(value)));
}

bool DictValue::RemoveByDottedPath(std::string_view path) {
  return ExtractByDottedPath(path).has_value();
}

DictValue&& DictValue::SetByDottedPath(std::string_view path,
                                       Value&& value) && {
  SetByDottedPath(path, std::move(value));
  return std::move(*this);
}

DictValue&& DictValue::SetByDottedPath(std::string_view path, bool value) && {
  SetByDottedPath(path, Value(value));
  return std::move(*this);
}

DictValue&& DictValue::SetByDottedPath(std::string_view path, int value) && {
  SetByDottedPath(path, Value(value));
  return std::move(*this);
}

DictValue&& DictValue::SetByDottedPath(std::string_view path, double value) && {
  SetByDottedPath(path, Value(value));
  return std::move(*this);
}

DictValue&& DictValue::SetByDottedPath(std::string_view path,
                                       std::string_view value) && {
  SetByDottedPath(path, Value(value));
  return std::move(*this);
}

DictValue&& DictValue::SetByDottedPath(std::string_view path,
                                       std::u16string_view value) && {
  SetByDottedPath(path, Value(value));
  return std::move(*this);
}

DictValue&& DictValue::SetByDottedPath(std::string_view path,
                                       const char* value) && {
  SetByDottedPath(path, Value(value));
  return std::move(*this);
}

DictValue&& DictValue::SetByDottedPath(std::string_view path,
                                       const char16_t* value) && {
  SetByDottedPath(path, Value(value));
  return std::move(*this);
}

DictValue&& DictValue::SetByDottedPath(std::string_view path,
                                       std::string&& value) && {
  SetByDottedPath(path, Value(std::move(value)));
  return std::move(*this);
}

DictValue&& DictValue::SetByDottedPath(std::string_view path,
                                       BlobStorage&& value) && {
  SetByDottedPath(path, Value(std::move(value)));
  return std::move(*this);
}

DictValue&& DictValue::SetByDottedPath(std::string_view path,
                                       DictValue&& value) && {
  SetByDottedPath(path, Value(std::move(value)));
  return std::move(*this);
}

DictValue&& DictValue::SetByDottedPath(std::string_view path,
                                       ListValue&& value) && {
  SetByDottedPath(path, Value(std::move(value)));
  return std::move(*this);
}

std::optional<Value> DictValue::ExtractByDottedPath(std::string_view path) {
  DCHECK(!path.empty());
  DCHECK(IsStringUTF8AllowingNoncharacters(path));

  // Use recursion instead of PathSplitter here, as it simplifies code for
  // removing dictionaries that become empty if a value matching `path` is
  // extracted.
  size_t dot_index = path.find('.');
  if (dot_index == std::string_view::npos) {
    return Extract(path);
  }
  // This could be clever to avoid a double-lookup by using storage_ directly,
  // but for now, just implement it in the most straightforward way.
  std::string_view next_key = path.substr(0, dot_index);
  auto* next_dict = FindDict(next_key);
  if (!next_dict) {
    return std::nullopt;
  }
  std::optional<Value> extracted =
      next_dict->ExtractByDottedPath(path.substr(dot_index + 1));
  if (extracted && next_dict->empty()) {
    Remove(next_key);
  }
  return extracted;
}

size_t DictValue::EstimateMemoryUsage() const {
#if BUILDFLAG(ENABLE_BASE_TRACING)
  return base::trace_event::EstimateMemoryUsage(storage_);
#else   // BUILDFLAG(ENABLE_BASE_TRACING)
  return 0;
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
}

std::string DictValue::DebugString() const {
  return DebugStringImpl(*this);
}

#if BUILDFLAG(ENABLE_BASE_TRACING)
void DictValue::WriteIntoTrace(perfetto::TracedValue context) const {
  perfetto::TracedDictionary dict = std::move(context).WriteDictionary();
  for (auto kv : *this) {
    dict.Add(perfetto::DynamicString(kv.first), kv.second);
  }
}
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

bool operator==(const DictValue& lhs, const DictValue& rhs) {
  auto deref_2nd = [](const auto& p) { return std::tie(p.first, *p.second); };
  return std::ranges::equal(lhs.storage_, rhs.storage_, {}, deref_2nd,
                            deref_2nd);
}

bool operator!=(const DictValue& lhs, const DictValue& rhs) {
  return !(lhs == rhs);
}

bool operator<(const DictValue& lhs, const DictValue& rhs) {
  auto deref_2nd = [](const auto& p) { return std::tie(p.first, *p.second); };
  return std::ranges::lexicographical_compare(lhs.storage_, rhs.storage_, {},
                                              deref_2nd, deref_2nd);
}

bool operator>(const DictValue& lhs, const DictValue& rhs) {
  return rhs < lhs;
}

bool operator<=(const DictValue& lhs, const DictValue& rhs) {
  return !(rhs < lhs);
}

bool operator>=(const DictValue& lhs, const DictValue& rhs) {
  return !(lhs < rhs);
}

// static
ListValue ListValue::with_capacity(size_t capacity) {
  ListValue result;
  result.reserve(capacity);
  return result;
}

ListValue::ListValue() = default;

ListValue::ListValue(ListValue&&) noexcept = default;

ListValue& ListValue::operator=(ListValue&&) noexcept = default;

ListValue::~ListValue() = default;

bool ListValue::empty() const {
  return storage_.empty();
}

size_t ListValue::size() const {
  return storage_.size();
}

ListValue::iterator ListValue::begin() {
  // SAFETY: Both iterators point to a single allocation.
  return UNSAFE_BUFFERS(iterator(base::to_address(storage_.begin()),
                                 base::to_address(storage_.end())));
}

ListValue::const_iterator ListValue::begin() const {
  // SAFETY: Both iterators point to a single allocation.
  return UNSAFE_BUFFERS(const_iterator(base::to_address(storage_.begin()),
                                       base::to_address(storage_.end())));
}

ListValue::const_iterator ListValue::cbegin() const {
  // SAFETY: Both iterators point to a single allocation.
  return UNSAFE_BUFFERS(const_iterator(base::to_address(storage_.cbegin()),
                                       base::to_address(storage_.cend())));
}

ListValue::iterator ListValue::end() {
  // SAFETY: All iterators point to a single allocation.
  return UNSAFE_BUFFERS(iterator(base::to_address(storage_.begin()),
                                 base::to_address(storage_.end()),
                                 base::to_address(storage_.end())));
}

ListValue::const_iterator ListValue::end() const {
  // SAFETY: All iterators point to a single allocation.
  return UNSAFE_BUFFERS(const_iterator(base::to_address(storage_.begin()),
                                       base::to_address(storage_.end()),
                                       base::to_address(storage_.end())));
}

ListValue::const_iterator ListValue::cend() const {
  // SAFETY: All iterators point to a single allocation.
  return UNSAFE_BUFFERS(const_iterator(base::to_address(storage_.cbegin()),
                                       base::to_address(storage_.cend()),
                                       base::to_address(storage_.cend())));
}

ListValue::reverse_iterator ListValue::rend() {
  return reverse_iterator(begin());
}

ListValue::const_reverse_iterator ListValue::rend() const {
  return const_reverse_iterator(begin());
}

ListValue::reverse_iterator ListValue::rbegin() {
  return reverse_iterator(end());
}

ListValue::const_reverse_iterator ListValue::rbegin() const {
  return const_reverse_iterator(end());
}

const Value& ListValue::front() const {
  CHECK(!storage_.empty());
  return storage_.front();
}

Value& ListValue::front() {
  CHECK(!storage_.empty());
  return storage_.front();
}

const Value& ListValue::back() const {
  CHECK(!storage_.empty());
  return storage_.back();
}

Value& ListValue::back() {
  CHECK(!storage_.empty());
  return storage_.back();
}

void ListValue::reserve(size_t capacity) {
  storage_.reserve(capacity);
}

void ListValue::resize(size_t new_size) {
  storage_.resize(new_size);
}

const Value& ListValue::operator[](size_t index) const {
  CHECK_LT(index, storage_.size());
  return storage_[index];
}

Value& ListValue::operator[](size_t index) {
  CHECK_LT(index, storage_.size());
  return storage_[index];
}

bool ListValue::contains(bool val) const {
  return contains(val, &Value::is_bool, &Value::GetBool);
}

bool ListValue::contains(int val) const {
  return contains(val, &Value::is_int, &Value::GetInt);
}

bool ListValue::contains(double val) const {
  return contains(val, &Value::is_double, &Value::GetDouble);
}

bool ListValue::contains(std::string_view val) const {
  return contains(val, &Value::is_string, &Value::GetString);
}

bool ListValue::contains(const char* val) const {
  return contains(std::string_view(val), &Value::is_string, &Value::GetString);
}

bool ListValue::contains(const BlobStorage& val) const {
  return contains(val, &Value::is_blob, &Value::GetBlob);
}

bool ListValue::contains(const DictValue& val) const {
  return contains(val, &Value::is_dict, &Value::GetDict);
}

bool ListValue::contains(const ListValue& val) const {
  return contains(val, &Value::is_list, &Value::GetList);
}

void ListValue::clear() {
  storage_.clear();
}

ListValue::iterator ListValue::erase(iterator pos) {
  auto next_it = storage_.erase(storage_.begin() + (pos - begin()));
  // SAFETY: All iterators point to a single allocation.
  return UNSAFE_BUFFERS(iterator(base::to_address(storage_.begin()),
                                 base::to_address(next_it),
                                 base::to_address(storage_.end())));
}

ListValue::const_iterator ListValue::erase(const_iterator pos) {
  auto next_it = storage_.erase(storage_.begin() + (pos - begin()));
  // SAFETY: All iterators point to a single allocation.
  return UNSAFE_BUFFERS(const_iterator(base::to_address(storage_.begin()),
                                       base::to_address(next_it),
                                       base::to_address(storage_.end())));
}

ListValue::iterator ListValue::erase(iterator first, iterator last) {
  auto next_it = storage_.erase(storage_.begin() + (first - begin()),
                                storage_.begin() + (last - begin()));
  // SAFETY: All iterators point to a single allocation.
  return UNSAFE_BUFFERS(iterator(base::to_address(storage_.begin()),
                                 base::to_address(next_it),
                                 base::to_address(storage_.end())));
}

ListValue::const_iterator ListValue::erase(const_iterator first,
                                           const_iterator last) {
  auto next_it = storage_.erase(storage_.begin() + (first - begin()),
                                storage_.begin() + (last - begin()));
  // SAFETY: All iterators point to a single allocation.
  return UNSAFE_BUFFERS(const_iterator(base::to_address(storage_.begin()),
                                       base::to_address(next_it),
                                       base::to_address(storage_.end())));
}

ListValue ListValue::Clone() const {
  return ListValue(storage_);
}

void ListValue::Append(Value&& value) & {
  storage_.emplace_back(std::move(value));
}

void ListValue::Append(bool value) & {
  storage_.emplace_back(value);
}

void ListValue::Append(int value) & {
  storage_.emplace_back(value);
}

void ListValue::Append(double value) & {
  storage_.emplace_back(value);
}

void ListValue::Append(std::string_view value) & {
  Append(Value(value));
}

void ListValue::Append(std::u16string_view value) & {
  storage_.emplace_back(value);
}

void ListValue::Append(const char* value) & {
  storage_.emplace_back(value);
}

void ListValue::Append(const char16_t* value) & {
  storage_.emplace_back(value);
}

void ListValue::Append(std::string&& value) & {
  storage_.emplace_back(std::move(value));
}

void ListValue::Append(BlobStorage&& value) & {
  storage_.emplace_back(std::move(value));
}

void ListValue::Append(DictValue&& value) & {
  storage_.emplace_back(std::move(value));
}

void ListValue::Append(ListValue&& value) & {
  storage_.emplace_back(std::move(value));
}

ListValue&& ListValue::Append(Value&& value) && {
  storage_.emplace_back(std::move(value));
  return std::move(*this);
}

ListValue&& ListValue::Append(bool value) && {
  storage_.emplace_back(value);
  return std::move(*this);
}

ListValue&& ListValue::Append(int value) && {
  storage_.emplace_back(value);
  return std::move(*this);
}

ListValue&& ListValue::Append(double value) && {
  storage_.emplace_back(value);
  return std::move(*this);
}

ListValue&& ListValue::Append(std::string_view value) && {
  Append(Value(value));
  return std::move(*this);
}

ListValue&& ListValue::Append(std::u16string_view value) && {
  storage_.emplace_back(value);
  return std::move(*this);
}

ListValue&& ListValue::Append(const char* value) && {
  storage_.emplace_back(value);
  return std::move(*this);
}

ListValue&& ListValue::Append(const char16_t* value) && {
  storage_.emplace_back(value);
  return std::move(*this);
}

ListValue&& ListValue::Append(std::string&& value) && {
  storage_.emplace_back(std::move(value));
  return std::move(*this);
}

ListValue&& ListValue::Append(BlobStorage&& value) && {
  storage_.emplace_back(std::move(value));
  return std::move(*this);
}

ListValue&& ListValue::Append(DictValue&& value) && {
  storage_.emplace_back(std::move(value));
  return std::move(*this);
}

ListValue&& ListValue::Append(ListValue&& value) && {
  storage_.emplace_back(std::move(value));
  return std::move(*this);
}

ListValue::iterator ListValue::Insert(const_iterator pos, Value&& value) {
  auto inserted_it =
      storage_.insert(storage_.begin() + (pos - begin()), std::move(value));
  // SAFETY: All pointers point to a single allocation.
  return UNSAFE_BUFFERS(iterator(base::to_address(storage_.begin()),
                                 base::to_address(inserted_it),
                                 base::to_address(storage_.end())));
}

size_t ListValue::EraseValue(const Value& value) {
  return std::erase(storage_, value);
}

size_t ListValue::EstimateMemoryUsage() const {
#if BUILDFLAG(ENABLE_BASE_TRACING)
  return base::trace_event::EstimateMemoryUsage(storage_);
#else   // BUILDFLAG(ENABLE_BASE_TRACING)
  return 0;
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
}

std::string ListValue::DebugString() const {
  return DebugStringImpl(*this);
}

#if BUILDFLAG(ENABLE_BASE_TRACING)
void ListValue::WriteIntoTrace(perfetto::TracedValue context) const {
  perfetto::TracedArray array = std::move(context).WriteArray();
  for (const auto& item : *this) {
    array.Append(item);
  }
}
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

ListValue::ListValue(const std::vector<Value>& storage) {
  storage_.reserve(storage.size());
  for (const auto& value : storage) {
    storage_.push_back(value.Clone());
  }
}

bool operator==(const ListValue& lhs, const ListValue& rhs) {
  return lhs.storage_ == rhs.storage_;
}

bool operator!=(const ListValue& lhs, const ListValue& rhs) {
  return !(lhs == rhs);
}

bool operator<(const ListValue& lhs, const ListValue& rhs) {
  return lhs.storage_ < rhs.storage_;
}

bool operator>(const ListValue& lhs, const ListValue& rhs) {
  return rhs < lhs;
}

bool operator<=(const ListValue& lhs, const ListValue& rhs) {
  return !(rhs < lhs);
}

bool operator>=(const ListValue& lhs, const ListValue& rhs) {
  return !(lhs < rhs);
}

bool operator==(const Value& lhs, const Value& rhs) {
  return lhs.data_ == rhs.data_;
}

bool operator!=(const Value& lhs, const Value& rhs) {
  return !(lhs == rhs);
}

bool operator<(const Value& lhs, const Value& rhs) {
  return lhs.data_ < rhs.data_;
}

bool operator>(const Value& lhs, const Value& rhs) {
  return rhs < lhs;
}

bool operator<=(const Value& lhs, const Value& rhs) {
  return !(rhs < lhs);
}

bool operator>=(const Value& lhs, const Value& rhs) {
  return !(lhs < rhs);
}

bool operator==(const Value& lhs, bool rhs) {
  return lhs.is_bool() && lhs.GetBool() == rhs;
}

bool operator==(const Value& lhs, int rhs) {
  return lhs.is_int() && lhs.GetInt() == rhs;
}

bool operator==(const Value& lhs, double rhs) {
  return lhs.is_double() && lhs.GetDouble() == rhs;
}

bool operator==(const Value& lhs, std::string_view rhs) {
  return lhs.is_string() && lhs.GetString() == rhs;
}

bool operator==(const Value& lhs, const DictValue& rhs) {
  return lhs.is_dict() && lhs.GetDict() == rhs;
}

bool operator==(const Value& lhs, const ListValue& rhs) {
  return lhs.is_list() && lhs.GetList() == rhs;
}

size_t Value::EstimateMemoryUsage() const {
  switch (type()) {
#if BUILDFLAG(ENABLE_BASE_TRACING)
    case Type::STRING:
      return base::trace_event::EstimateMemoryUsage(GetString());
    case Type::BINARY:
      return base::trace_event::EstimateMemoryUsage(GetBlob());
    case Type::DICT:
      return GetDict().EstimateMemoryUsage();
    case Type::LIST:
      return GetList().EstimateMemoryUsage();
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
    default:
      return 0;
  }
}

std::string Value::DebugString() const {
  return DebugStringImpl(*this);
}

#if BUILDFLAG(ENABLE_BASE_TRACING)
void Value::WriteIntoTrace(perfetto::TracedValue context) const {
  Visit([&](const auto& member) {
    using T = std::decay_t<decltype(member)>;
    if constexpr (std::is_same_v<T, absl::monostate>) {
      std::move(context).WriteString("<none>");
    } else if constexpr (std::is_same_v<T, bool>) {
      std::move(context).WriteBoolean(member);
    } else if constexpr (std::is_same_v<T, int>) {
      std::move(context).WriteInt64(member);
    } else if constexpr (std::is_same_v<T, DoubleStorage>) {
      std::move(context).WriteDouble(member);
    } else if constexpr (std::is_same_v<T, std::string>) {
      std::move(context).WriteString(member);
    } else if constexpr (std::is_same_v<T, BlobStorage>) {
      std::move(context).WriteString("<binary data not supported>");
    } else if constexpr (std::is_same_v<T, Dict>) {
      member.WriteIntoTrace(std::move(context));
    } else if constexpr (std::is_same_v<T, List>) {
      member.WriteIntoTrace(std::move(context));
    }
  });
}
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

ValueView::ValueView(const Value& value)
    : data_view_(
          value.Visit([](const auto& member) { return ViewType(member); })) {}

Value ValueView::ToValue() const {
  return Value::CloningHelper::Clone(data_view_);
}

ValueSerializer::~ValueSerializer() = default;

ValueDeserializer::~ValueDeserializer() = default;

std::ostream& operator<<(std::ostream& out, const Value& value) {
  return out << value.DebugString();
}

std::ostream& operator<<(std::ostream& out, const DictValue& dict) {
  return out << dict.DebugString();
}

std::ostream& operator<<(std::ostream& out, const ListValue& list) {
  return out << list.DebugString();
}

std::ostream& operator<<(std::ostream& out, const Value::Type& type) {
  if (static_cast<int>(type) < 0 ||
      static_cast<size_t>(type) >= std::size(kTypeNames)) {
    return out << "Invalid Type (index = " << static_cast<int>(type) << ")";
  }
  return out << Value::GetTypeName(type);
}

}  // namespace base
