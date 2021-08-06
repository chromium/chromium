// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"

// values.h is a widely included header and its size has significant impact on
// build time. Try not to raise this limit unless absolutely necessary. See
// https://chromium.googlesource.com/chromium/src/+/HEAD/docs/wmax_tokens.md
#ifndef NACL_TC_REV
#pragma clang max_tokens_here 400000
#endif

#include <algorithm>
#include <cmath>
#include <ostream>
#include <tuple>
#include <utility>

#include "base/as_const.h"
#include "base/bit_cast.h"
#include "base/check_op.h"
#include "base/containers/checked_iterators.h"
#include "base/containers/contains.h"
#include "base/cxx17_backports.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/base_tracing.h"
#include "base/tracing_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

#if BUILDFLAG(ENABLE_BASE_TRACING)
#include "base/trace_event/memory_usage_estimator.h"  // no-presubmit-check
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

namespace base {

namespace {

const char* const kTypeNames[] = {"null",   "boolean", "integer",    "double",
                                  "string", "binary",  "dictionary", "list"};
static_assert(base::size(kTypeNames) ==
                  static_cast<size_t>(Value::Type::LIST) + 1,
              "kTypeNames Has Wrong Size");

std::unique_ptr<Value> CopyWithoutEmptyChildren(const Value& node);

// Make a deep copy of |node|, but don't include empty lists or dictionaries
// in the copy. It's possible for this function to return NULL and it
// expects |node| to always be non-NULL.
std::unique_ptr<Value> CopyListWithoutEmptyChildren(const Value& list) {
  Value copy(Value::Type::LIST);
  for (const auto& entry : list.GetList()) {
    std::unique_ptr<Value> child_copy = CopyWithoutEmptyChildren(entry);
    if (child_copy)
      copy.Append(std::move(*child_copy));
  }
  return copy.GetList().empty() ? nullptr
                                : std::make_unique<Value>(std::move(copy));
}

std::unique_ptr<DictionaryValue> CopyDictionaryWithoutEmptyChildren(
    const DictionaryValue& dict) {
  std::unique_ptr<DictionaryValue> copy;
  for (auto it : dict.DictItems()) {
    std::unique_ptr<Value> child_copy = CopyWithoutEmptyChildren(it.second);
    if (child_copy) {
      if (!copy)
        copy = std::make_unique<DictionaryValue>();
      copy->SetKey(it.first, std::move(*child_copy));
    }
  }
  return copy;
}

std::unique_ptr<Value> CopyWithoutEmptyChildren(const Value& node) {
  switch (node.type()) {
    case Value::Type::LIST:
      return CopyListWithoutEmptyChildren(static_cast<const ListValue&>(node));

    case Value::Type::DICTIONARY:
      return CopyDictionaryWithoutEmptyChildren(
          static_cast<const DictionaryValue&>(node));

    default:
      return std::make_unique<Value>(node.Clone());
  }
}

// Helper class to enumerate the path components from a StringPiece
// without performing heap allocations. Components are simply separated
// by single dots (e.g. "foo.bar.baz"  -> ["foo", "bar", "baz"]).
//
// Usage example:
//    PathSplitter splitter(some_path);
//    while (splitter.HasNext()) {
//       StringPiece component = splitter.Next();
//       ...
//    }
//
class PathSplitter {
 public:
  explicit PathSplitter(StringPiece path) : path_(path) {}

  bool HasNext() const { return pos_ < path_.size(); }

  StringPiece Next() {
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
  StringPiece path_;
  size_t pos_ = 0;
};

}  // namespace

// static
Value Value::FromUniquePtrValue(std::unique_ptr<Value> val) {
  return std::move(*val);
}

// static
std::unique_ptr<Value> Value::ToUniquePtrValue(Value val) {
  return std::make_unique<Value>(std::move(val));
}

// static
const DictionaryValue& Value::AsDictionaryValue(const Value& val) {
  CHECK(val.is_dict());
  return static_cast<const DictionaryValue&>(val);
}

// static
const ListValue& Value::AsListValue(const Value& val) {
  CHECK(val.is_list());
  return static_cast<const ListValue&>(val);
}

Value::Value() noexcept = default;

Value::Value(Value&& that) noexcept = default;

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
      data_.emplace<DoubleStorage>(bit_cast<DoubleStorage>(0.0));
      return;
    case Type::STRING:
      data_.emplace<std::string>();
      return;
    case Type::BINARY:
      data_.emplace<BlobStorage>();
      return;
    case Type::DICTIONARY:
      data_.emplace<LegacyDictStorage>();
      return;
    case Type::LIST:
      data_.emplace<ListStorage>();
      return;
  }

  NOTREACHED();
}

Value::Value(bool in_bool) : data_(in_bool) {}

Value::Value(int in_int) : data_(in_int) {}

Value::Value(double in_double) : data_(bit_cast<DoubleStorage>(in_double)) {
  if (!std::isfinite(in_double)) {
    NOTREACHED() << "Non-finite (i.e. NaN or positive/negative infinity) "
                 << "values cannot be represented in JSON";
    data_ = bit_cast<DoubleStorage>(0.0);
  }
}

Value::Value(const char* in_string) : Value(std::string(in_string)) {}

Value::Value(StringPiece in_string) : Value(std::string(in_string)) {}

Value::Value(std::string&& in_string) noexcept : data_(std::move(in_string)) {
  DCHECK(IsStringUTF8AllowingNoncharacters(GetString()));
}

Value::Value(const char16_t* in_string16) : Value(StringPiece16(in_string16)) {}

Value::Value(StringPiece16 in_string16) : Value(UTF16ToUTF8(in_string16)) {}

Value::Value(const std::vector<char>& in_blob)
    : data_(absl::in_place_type_t<BlobStorage>(),
            in_blob.begin(),
            in_blob.end()) {}

Value::Value(base::span<const uint8_t> in_blob)
    : data_(absl::in_place_type_t<BlobStorage>(),
            in_blob.begin(),
            in_blob.end()) {}

Value::Value(BlobStorage&& in_blob) noexcept : data_(std::move(in_blob)) {}

Value::Value(const DictStorage& in_dict)
    : data_(absl::in_place_type_t<LegacyDictStorage>()) {
  dict().reserve(in_dict.size());
  for (const auto& it : in_dict) {
    dict().try_emplace(dict().end(), it.first,
                       std::make_unique<Value>(it.second.Clone()));
  }
}

Value::Value(DictStorage&& in_dict) noexcept
    : data_(absl::in_place_type_t<LegacyDictStorage>()) {
  dict().reserve(in_dict.size());
  for (auto& it : in_dict) {
    dict().try_emplace(dict().end(), std::move(it.first),
                       std::make_unique<Value>(std::move(it.second)));
  }
}

Value::Value(span<const Value> in_list)
    : data_(absl::in_place_type_t<ListStorage>()) {
  list().reserve(in_list.size());
  for (const auto& val : in_list)
    list().emplace_back(val.Clone());
}

Value::Value(ListStorage&& in_list) noexcept : data_(std::move(in_list)) {}

Value& Value::operator=(Value&& that) noexcept = default;

Value::Value(const LegacyDictStorage& storage)
    : data_(absl::in_place_type_t<LegacyDictStorage>()) {
  dict().reserve(storage.size());
  for (const auto& it : storage) {
    dict().try_emplace(dict().end(), it.first,
                       std::make_unique<Value>(it.second->Clone()));
  }
}

Value::Value(LegacyDictStorage&& storage) noexcept
    : data_(std::move(storage)) {}

Value::Value(absl::monostate) {}

Value::Value(DoubleStorage storage) : data_(std::move(storage)) {}

double Value::AsDoubleInternal() const {
  return bit_cast<double>(absl::get<DoubleStorage>(data_));
}

Value Value::Clone() const {
  return absl::visit([](const auto& member) { return Value(member); }, data_);
}

Value::~Value() = default;

// static
const char* Value::GetTypeName(Value::Type type) {
  DCHECK_GE(static_cast<int>(type), 0);
  DCHECK_LT(static_cast<size_t>(type), base::size(kTypeNames));
  return kTypeNames[static_cast<size_t>(type)];
}

absl::optional<bool> Value::GetIfBool() const {
  return is_bool() ? absl::make_optional(GetBool()) : absl::nullopt;
}

absl::optional<int> Value::GetIfInt() const {
  return is_int() ? absl::make_optional(GetInt()) : absl::nullopt;
}

absl::optional<double> Value::GetIfDouble() const {
  return (is_int() || is_double()) ? absl::make_optional(GetDouble())
                                   : absl::nullopt;
}

const std::string* Value::GetIfString() const {
  return absl::get_if<std::string>(&data_);
}

const Value::BlobStorage* Value::GetIfBlob() const {
  return absl::get_if<BlobStorage>(&data_);
}

bool Value::GetBool() const {
  return absl::get<bool>(data_);
}

int Value::GetInt() const {
  return absl::get<int>(data_);
}

double Value::GetDouble() const {
  if (is_double())
    return AsDoubleInternal();
  if (is_int())
    return GetInt();
  CHECK(false);
  return 0.0;
}

const std::string& Value::GetString() const {
  return absl::get<std::string>(data_);
}

std::string& Value::GetString() {
  return absl::get<std::string>(data_);
}

const Value::BlobStorage& Value::GetBlob() const {
  return absl::get<BlobStorage>(data_);
}

Value::ListView Value::GetList() {
  return list();
}

Value::ConstListView Value::GetList() const {
  return list();
}

Value::ListStorage Value::TakeList() && {
  return std::exchange(list(), {});
}

void Value::Append(bool value) {
  list().emplace_back(value);
}

void Value::Append(int value) {
  list().emplace_back(value);
}

void Value::Append(double value) {
  list().emplace_back(value);
}

void Value::Append(const char* value) {
  list().emplace_back(value);
}

void Value::Append(StringPiece value) {
  list().emplace_back(value);
}

void Value::Append(std::string&& value) {
  list().emplace_back(std::move(value));
}

void Value::Append(const char16_t* value) {
  list().emplace_back(value);
}

void Value::Append(StringPiece16 value) {
  list().emplace_back(value);
}

void Value::Append(Value&& value) {
  list().emplace_back(std::move(value));
}

CheckedContiguousIterator<Value> Value::Insert(
    CheckedContiguousConstIterator<Value> pos,
    Value&& value) {
  const auto offset = pos - make_span(list()).begin();
  list().insert(list().begin() + offset, std::move(value));
  return make_span(list()).begin() + offset;
}

bool Value::EraseListIter(CheckedContiguousConstIterator<Value> iter) {
  const auto offset = iter - ListView(list()).begin();
  auto list_iter = list().begin() + offset;
  if (list_iter == list().end())
    return false;

  list().erase(list_iter);
  return true;
}

size_t Value::EraseListValue(const Value& val) {
  return EraseListValueIf([&val](const Value& other) { return val == other; });
}

void Value::ClearList() {
  list().clear();
}

Value* Value::FindKey(StringPiece key) {
  return const_cast<Value*>(as_const(*this).FindKey(key));
}

const Value* Value::FindKey(StringPiece key) const {
  auto found = dict().find(key);
  if (found == dict().end())
    return nullptr;
  return found->second.get();
}

Value* Value::FindKeyOfType(StringPiece key, Type type) {
  return const_cast<Value*>(as_const(*this).FindKeyOfType(key, type));
}

const Value* Value::FindKeyOfType(StringPiece key, Type type) const {
  const Value* result = FindKey(key);
  if (!result || result->type() != type)
    return nullptr;
  return result;
}

absl::optional<bool> Value::FindBoolKey(StringPiece key) const {
  const Value* result = FindKeyOfType(key, Type::BOOLEAN);
  return result ? absl::make_optional(result->GetBool()) : absl::nullopt;
}

absl::optional<int> Value::FindIntKey(StringPiece key) const {
  const Value* result = FindKeyOfType(key, Type::INTEGER);
  return result ? absl::make_optional(result->GetInt()) : absl::nullopt;
}

absl::optional<double> Value::FindDoubleKey(StringPiece key) const {
  if (const Value* cur = FindKey(key)) {
    if (cur->is_int() || cur->is_double())
      return cur->GetDouble();
  }

  return absl::nullopt;
}

const std::string* Value::FindStringKey(StringPiece key) const {
  const Value* result = FindKey(key);
  return result ? absl::get_if<std::string>(&result->data_) : nullptr;
}

std::string* Value::FindStringKey(StringPiece key) {
  return const_cast<std::string*>(as_const(*this).FindStringKey(key));
}

const Value::BlobStorage* Value::FindBlobKey(StringPiece key) const {
  const Value* result = FindKey(key);
  return result ? absl::get_if<BlobStorage>(&result->data_) : nullptr;
}

const Value* Value::FindDictKey(StringPiece key) const {
  return FindKeyOfType(key, Type::DICTIONARY);
}

Value* Value::FindDictKey(StringPiece key) {
  return FindKeyOfType(key, Type::DICTIONARY);
}

const Value* Value::FindListKey(StringPiece key) const {
  return FindKeyOfType(key, Type::LIST);
}

Value* Value::FindListKey(StringPiece key) {
  return FindKeyOfType(key, Type::LIST);
}

Value* Value::SetKey(StringPiece key, Value&& value) {
  return SetKeyInternal(key, std::make_unique<Value>(std::move(value)));
}

Value* Value::SetKey(std::string&& key, Value&& value) {
  return dict()
      .insert_or_assign(std::move(key),
                        std::make_unique<Value>(std::move(value)))
      .first->second.get();
}

Value* Value::SetKey(const char* key, Value&& value) {
  return SetKeyInternal(key, std::make_unique<Value>(std::move(value)));
}

Value* Value::SetBoolKey(StringPiece key, bool value) {
  return SetKeyInternal(key, std::make_unique<Value>(value));
}

Value* Value::SetIntKey(StringPiece key, int value) {
  return SetKeyInternal(key, std::make_unique<Value>(value));
}

Value* Value::SetDoubleKey(StringPiece key, double value) {
  return SetKeyInternal(key, std::make_unique<Value>(value));
}

Value* Value::SetStringKey(StringPiece key, StringPiece value) {
  return SetKeyInternal(key, std::make_unique<Value>(value));
}

Value* Value::SetStringKey(StringPiece key, StringPiece16 value) {
  return SetKeyInternal(key, std::make_unique<Value>(value));
}

Value* Value::SetStringKey(StringPiece key, const char* value) {
  return SetKeyInternal(key, std::make_unique<Value>(value));
}

Value* Value::SetStringKey(StringPiece key, std::string&& value) {
  return SetKeyInternal(key, std::make_unique<Value>(std::move(value)));
}

bool Value::RemoveKey(StringPiece key) {
  return dict().erase(key) != 0;
}

absl::optional<Value> Value::ExtractKey(StringPiece key) {
  auto found = dict().find(key);
  if (found == dict().end())
    return absl::nullopt;

  Value value = std::move(*found->second);
  dict().erase(found);
  return std::move(value);
}

Value* Value::FindPath(StringPiece path) {
  return const_cast<Value*>(as_const(*this).FindPath(path));
}

const Value* Value::FindPath(StringPiece path) const {
  CHECK(is_dict());
  const Value* cur = this;
  PathSplitter splitter(path);
  while (splitter.HasNext()) {
    if (!cur->is_dict() || (cur = cur->FindKey(splitter.Next())) == nullptr)
      return nullptr;
  }
  return cur;
}

Value* Value::FindPathOfType(StringPiece path, Type type) {
  return const_cast<Value*>(as_const(*this).FindPathOfType(path, type));
}

const Value* Value::FindPathOfType(StringPiece path, Type type) const {
  const Value* cur = FindPath(path);
  if (!cur || cur->type() != type)
    return nullptr;
  return cur;
}

absl::optional<bool> Value::FindBoolPath(StringPiece path) const {
  const Value* cur = FindPath(path);
  if (!cur || !cur->is_bool())
    return absl::nullopt;
  return cur->GetBool();
}

absl::optional<int> Value::FindIntPath(StringPiece path) const {
  const Value* cur = FindPath(path);
  if (!cur || !cur->is_int())
    return absl::nullopt;
  return cur->GetInt();
}

absl::optional<double> Value::FindDoublePath(StringPiece path) const {
  if (const Value* cur = FindPath(path)) {
    if (cur->is_int() || cur->is_double())
      return cur->GetDouble();
  }

  return absl::nullopt;
}

const std::string* Value::FindStringPath(StringPiece path) const {
  const Value* result = FindPath(path);
  return result ? absl::get_if<std::string>(&result->data_) : nullptr;
}

std::string* Value::FindStringPath(StringPiece path) {
  return const_cast<std::string*>(as_const(*this).FindStringPath(path));
}

const Value::BlobStorage* Value::FindBlobPath(StringPiece path) const {
  const Value* result = FindPath(path);
  return result ? absl::get_if<BlobStorage>(&result->data_) : nullptr;
}

const Value* Value::FindDictPath(StringPiece path) const {
  return FindPathOfType(path, Type::DICTIONARY);
}

Value* Value::FindDictPath(StringPiece path) {
  return FindPathOfType(path, Type::DICTIONARY);
}

const Value* Value::FindListPath(StringPiece path) const {
  return FindPathOfType(path, Type::LIST);
}

Value* Value::FindListPath(StringPiece path) {
  return FindPathOfType(path, Type::LIST);
}

Value* Value::SetPath(StringPiece path, Value&& value) {
  return SetPathInternal(path, std::make_unique<Value>(std::move(value)));
}

Value* Value::SetBoolPath(StringPiece path, bool value) {
  return SetPathInternal(path, std::make_unique<Value>(value));
}

Value* Value::SetIntPath(StringPiece path, int value) {
  return SetPathInternal(path, std::make_unique<Value>(value));
}

Value* Value::SetDoublePath(StringPiece path, double value) {
  return SetPathInternal(path, std::make_unique<Value>(value));
}

Value* Value::SetStringPath(StringPiece path, StringPiece value) {
  return SetPathInternal(path, std::make_unique<Value>(value));
}

Value* Value::SetStringPath(StringPiece path, std::string&& value) {
  return SetPathInternal(path, std::make_unique<Value>(std::move(value)));
}

Value* Value::SetStringPath(StringPiece path, const char* value) {
  return SetPathInternal(path, std::make_unique<Value>(value));
}

Value* Value::SetStringPath(StringPiece path, StringPiece16 value) {
  return SetPathInternal(path, std::make_unique<Value>(value));
}

bool Value::RemovePath(StringPiece path) {
  return ExtractPath(path).has_value();
}

absl::optional<Value> Value::ExtractPath(StringPiece path) {
  if (!is_dict() || path.empty())
    return absl::nullopt;

  // NOTE: PathSplitter is not being used here because recursion is used to
  // ensure that dictionaries that become empty due to this operation are
  // removed automatically.
  size_t pos = path.find('.');
  if (pos == path.npos)
    return ExtractKey(path);

  auto found = dict().find(path.substr(0, pos));
  if (found == dict().end() || !found->second->is_dict())
    return absl::nullopt;

  absl::optional<Value> extracted =
      found->second->ExtractPath(path.substr(pos + 1));
  if (extracted && found->second->dict().empty())
    dict().erase(found);

  return extracted;
}

// DEPRECATED METHODS
Value* Value::FindPath(std::initializer_list<StringPiece> path) {
  return const_cast<Value*>(as_const(*this).FindPath(path));
}

Value* Value::FindPath(span<const StringPiece> path) {
  return const_cast<Value*>(as_const(*this).FindPath(path));
}

const Value* Value::FindPath(std::initializer_list<StringPiece> path) const {
  DCHECK_GE(path.size(), 2u) << "Use FindKey() for a path of length 1.";
  return FindPath(make_span(path.begin(), path.size()));
}

const Value* Value::FindPath(span<const StringPiece> path) const {
  const Value* cur = this;
  for (const StringPiece& component : path) {
    if (!cur->is_dict() || (cur = cur->FindKey(component)) == nullptr)
      return nullptr;
  }
  return cur;
}

Value* Value::FindPathOfType(std::initializer_list<StringPiece> path,
                             Type type) {
  return const_cast<Value*>(as_const(*this).FindPathOfType(path, type));
}

Value* Value::FindPathOfType(span<const StringPiece> path, Type type) {
  return const_cast<Value*>(as_const(*this).FindPathOfType(path, type));
}

const Value* Value::FindPathOfType(std::initializer_list<StringPiece> path,
                                   Type type) const {
  DCHECK_GE(path.size(), 2u) << "Use FindKeyOfType() for a path of length 1.";
  return FindPathOfType(make_span(path.begin(), path.size()), type);
}

const Value* Value::FindPathOfType(span<const StringPiece> path,
                                   Type type) const {
  const Value* result = FindPath(path);
  if (!result || result->type() != type)
    return nullptr;
  return result;
}

Value* Value::SetPath(std::initializer_list<StringPiece> path, Value&& value) {
  DCHECK_GE(path.size(), 2u) << "Use SetKey() for a path of length 1.";
  return SetPath(make_span(path.begin(), path.size()), std::move(value));
}

Value* Value::SetPath(span<const StringPiece> path, Value&& value) {
  DCHECK(path.begin() != path.end());  // Can't be empty path.

  // Walk/construct intermediate dictionaries. The last element requires
  // special handling so skip it in this loop.
  Value* cur = this;
  auto cur_path = path.begin();
  for (; (cur_path + 1) < path.end(); ++cur_path) {
    if (!cur->is_dict())
      return nullptr;

    // Use lower_bound to avoid doing the search twice for missing keys.
    const StringPiece path_component = *cur_path;
    auto found = cur->dict().lower_bound(path_component);
    if (found == cur->dict().end() || found->first != path_component) {
      // No key found, insert one.
      auto inserted = cur->dict().try_emplace(
          found, path_component, std::make_unique<Value>(Type::DICTIONARY));
      cur = inserted->second.get();
    } else {
      cur = found->second.get();
    }
  }

  // "cur" will now contain the last dictionary to insert or replace into.
  if (!cur->is_dict())
    return nullptr;
  return cur->SetKey(*cur_path, std::move(value));
}

Value::dict_iterator_proxy Value::DictItems() {
  return dict_iterator_proxy(&dict());
}

Value::const_dict_iterator_proxy Value::DictItems() const {
  return const_dict_iterator_proxy(&dict());
}

Value::DictStorage Value::TakeDict() && {
  DictStorage storage;
  storage.reserve(dict().size());
  for (auto& pair : dict()) {
    storage.try_emplace(storage.end(), std::move(pair.first),
                        std::move(*pair.second));
  }

  dict().clear();
  return storage;
}

size_t Value::DictSize() const {
  return dict().size();
}

bool Value::DictEmpty() const {
  return dict().empty();
}

void Value::DictClear() {
  dict().clear();
}

void Value::MergeDictionary(const Value* dictionary) {
  for (const auto& pair : dictionary->dict()) {
    const auto& key = pair.first;
    const auto& val = pair.second;
    // Check whether we have to merge dictionaries.
    if (val->is_dict()) {
      auto found = dict().find(key);
      if (found != dict().end() && found->second->is_dict()) {
        found->second->MergeDictionary(val.get());
        continue;
      }
    }

    // All other cases: Make a copy and hook it up.
    SetKey(key, val->Clone());
  }
}

bool Value::GetAsBoolean(bool* out_value) const {
  if (out_value && is_bool()) {
    *out_value = GetBool();
    return true;
  }
  return is_bool();
}

bool Value::GetAsString(std::string* out_value) const {
  if (out_value && is_string()) {
    *out_value = GetString();
    return true;
  }
  return is_string();
}

bool Value::GetAsString(std::u16string* out_value) const {
  if (out_value && is_string()) {
    *out_value = UTF8ToUTF16(GetString());
    return true;
  }
  return is_string();
}

bool Value::GetAsString(const Value** out_value) const {
  if (out_value && is_string()) {
    *out_value = this;
    return true;
  }
  return is_string();
}

bool Value::GetAsString(StringPiece* out_value) const {
  if (out_value && is_string()) {
    *out_value = GetString();
    return true;
  }
  return is_string();
}

bool Value::GetAsList(ListValue** out_value) {
  if (out_value && is_list()) {
    *out_value = static_cast<ListValue*>(this);
    return true;
  }
  return is_list();
}

bool Value::GetAsList(const ListValue** out_value) const {
  if (out_value && is_list()) {
    *out_value = static_cast<const ListValue*>(this);
    return true;
  }
  return is_list();
}

bool Value::GetAsDictionary(DictionaryValue** out_value) {
  if (out_value && is_dict()) {
    *out_value = static_cast<DictionaryValue*>(this);
    return true;
  }
  return is_dict();
}

bool Value::GetAsDictionary(const DictionaryValue** out_value) const {
  if (out_value && is_dict()) {
    *out_value = static_cast<const DictionaryValue*>(this);
    return true;
  }
  return is_dict();
}

Value* Value::DeepCopy() const {
  return new Value(Clone());
}

std::unique_ptr<Value> Value::CreateDeepCopy() const {
  return std::make_unique<Value>(Clone());
}

bool operator==(const Value& lhs, const Value& rhs) {
  if (lhs.type() != rhs.type())
    return false;

  switch (lhs.type()) {
    case Value::Type::NONE:
      return true;
    case Value::Type::BOOLEAN:
      return lhs.GetBool() == rhs.GetBool();
    case Value::Type::INTEGER:
      return lhs.GetInt() == rhs.GetInt();
    case Value::Type::DOUBLE:
      return lhs.AsDoubleInternal() == rhs.AsDoubleInternal();
    case Value::Type::STRING:
      return lhs.GetString() == rhs.GetString();
    case Value::Type::BINARY:
      return lhs.GetBlob() == rhs.GetBlob();
    // TODO(crbug.com/646113): Clean this up when DictionaryValue and ListValue
    // are completely inlined.
    case Value::Type::DICTIONARY:
      if (lhs.dict().size() != rhs.dict().size())
        return false;
      return std::equal(
          std::begin(lhs.dict()), std::end(lhs.dict()), std::begin(rhs.dict()),
          [](const auto& u, const auto& v) {
            return std::tie(u.first, *u.second) == std::tie(v.first, *v.second);
          });
    case Value::Type::LIST:
      return lhs.list() == rhs.list();
  }

  NOTREACHED();
  return false;
}

bool operator!=(const Value& lhs, const Value& rhs) {
  return !(lhs == rhs);
}

bool operator<(const Value& lhs, const Value& rhs) {
  if (lhs.type() != rhs.type())
    return lhs.type() < rhs.type();

  switch (lhs.type()) {
    case Value::Type::NONE:
      return false;
    case Value::Type::BOOLEAN:
      return lhs.GetBool() < rhs.GetBool();
    case Value::Type::INTEGER:
      return lhs.GetInt() < rhs.GetInt();
    case Value::Type::DOUBLE:
      return lhs.AsDoubleInternal() < rhs.AsDoubleInternal();
    case Value::Type::STRING:
      return lhs.GetString() < rhs.GetString();
    case Value::Type::BINARY:
      return lhs.GetBlob() < rhs.GetBlob();
    // TODO(crbug.com/646113): Clean this up when DictionaryValue and ListValue
    // are completely inlined.
    case Value::Type::DICTIONARY:
      return std::lexicographical_compare(
          std::begin(lhs.dict()), std::end(lhs.dict()), std::begin(rhs.dict()),
          std::end(rhs.dict()),
          [](const Value::LegacyDictStorage::value_type& u,
             const Value::LegacyDictStorage::value_type& v) {
            return std::tie(u.first, *u.second) < std::tie(v.first, *v.second);
          });
    case Value::Type::LIST:
      return lhs.list() < rhs.list();
  }

  NOTREACHED();
  return false;
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

bool Value::Equals(const Value* other) const {
  DCHECK(other);
  return *this == *other;
}

size_t Value::EstimateMemoryUsage() const {
  switch (type()) {
#if BUILDFLAG(ENABLE_BASE_TRACING)
    case Type::STRING:
      return base::trace_event::EstimateMemoryUsage(GetString());
    case Type::BINARY:
      return base::trace_event::EstimateMemoryUsage(GetBlob());
    case Type::DICTIONARY:
      return base::trace_event::EstimateMemoryUsage(dict());
    case Type::LIST:
      return base::trace_event::EstimateMemoryUsage(list());
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
    default:
      return 0;
  }
}

std::string Value::DebugString() const {
  std::string json;
  JSONWriter::WriteWithOptions(*this, JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return json;
}

#if BUILDFLAG(ENABLE_BASE_TRACING)
void Value::WriteIntoTrace(perfetto::TracedValue context) const {
  switch (type()) {
    case Type::BOOLEAN:
      std::move(context).WriteBoolean(GetBool());
      return;
    case Type::INTEGER:
      std::move(context).WriteInt64(GetInt());
      return;
    case Type::DOUBLE:
      std::move(context).WriteDouble(GetDouble());
      return;
    case Type::STRING:
      std::move(context).WriteString(GetString());
      return;
    case Type::BINARY:
      std::move(context).WriteString("<binary data not supported>");
      return;
    case Type::DICTIONARY: {
      perfetto::TracedDictionary dict = std::move(context).WriteDictionary();
      for (auto kv : DictItems())
        dict.Add(perfetto::DynamicString{kv.first}, kv.second);
      return;
    }
    case Type::LIST: {
      perfetto::TracedArray array = std::move(context).WriteArray();
      for (const auto& item : GetList())
        array.Append(item);
      return;
    }
    case Type::NONE:
      std::move(context).WriteString("<none>");
      return;
  }
}
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

Value* Value::SetKeyInternal(StringPiece key,
                             std::unique_ptr<Value>&& val_ptr) {
  CHECK(is_dict());
  // NOTE: We can't use |insert_or_assign| here, as only |try_emplace| does
  // an explicit conversion from StringPiece to std::string if necessary.
  auto result = dict().try_emplace(key, std::move(val_ptr));
  if (!result.second) {
    // val_ptr is guaranteed to be still intact at this point.
    result.first->second = std::move(val_ptr);
  }
  return result.first->second.get();
}

Value* Value::SetPathInternal(StringPiece path,
                              std::unique_ptr<Value>&& value_ptr) {
  PathSplitter splitter(path);
  DCHECK(splitter.HasNext()) << "Cannot call SetPath() with empty path";
  // Walk/construct intermediate dictionaries. The last element requires
  // special handling so skip it in this loop.
  Value* cur = this;
  StringPiece path_component = splitter.Next();
  while (splitter.HasNext()) {
    if (!cur->is_dict())
      return nullptr;

    // Use lower_bound to avoid doing the search twice for missing keys.
    auto found = cur->dict().lower_bound(path_component);
    if (found == cur->dict().end() || found->first != path_component) {
      // No key found, insert one.
      auto inserted = cur->dict().try_emplace(
          found, path_component, std::make_unique<Value>(Type::DICTIONARY));
      cur = inserted->second.get();
    } else {
      cur = found->second.get();
    }
    path_component = splitter.Next();
  }

  // "cur" will now contain the last dictionary to insert or replace into.
  if (!cur->is_dict())
    return nullptr;
  return cur->SetKeyInternal(path_component, std::move(value_ptr));
}

///////////////////// DictionaryValue ////////////////////

// static
std::unique_ptr<DictionaryValue> DictionaryValue::From(
    std::unique_ptr<Value> value) {
  DictionaryValue* out;
  if (value && value->GetAsDictionary(&out)) {
    ignore_result(value.release());
    return WrapUnique(out);
  }
  return nullptr;
}

DictionaryValue::DictionaryValue() : Value(Type::DICTIONARY) {}

DictionaryValue::DictionaryValue(const LegacyDictStorage& storage)
    : Value(storage) {}

DictionaryValue::DictionaryValue(LegacyDictStorage&& storage) noexcept
    : Value(std::move(storage)) {}

bool DictionaryValue::HasKey(StringPiece key) const {
  DCHECK(IsStringUTF8AllowingNoncharacters(key));
  auto current_entry = dict().find(key);
  DCHECK((current_entry == dict().end()) || current_entry->second);
  return current_entry != dict().end();
}

void DictionaryValue::Clear() {
  DictClear();
}

Value* DictionaryValue::Set(StringPiece path, std::unique_ptr<Value> in_value) {
  DCHECK(IsStringUTF8AllowingNoncharacters(path));
  DCHECK(in_value);

  // IMPORTANT NOTE: Do not replace with SetPathInternal() yet, because the
  // latter fails when over-writing a non-dict intermediate node, while this
  // method just replaces it with one. This difference makes some tests actually
  // fail (http://crbug.com/949461).
  StringPiece current_path(path);
  Value* current_dictionary = this;
  for (size_t delimiter_position = current_path.find('.');
       delimiter_position != StringPiece::npos;
       delimiter_position = current_path.find('.')) {
    // Assume that we're indexing into a dictionary.
    StringPiece key = current_path.substr(0, delimiter_position);
    Value* child_dictionary =
        current_dictionary->FindKeyOfType(key, Type::DICTIONARY);
    if (!child_dictionary) {
      child_dictionary =
          current_dictionary->SetKey(key, Value(Type::DICTIONARY));
    }

    current_dictionary = child_dictionary;
    current_path = current_path.substr(delimiter_position + 1);
  }

  return static_cast<DictionaryValue*>(current_dictionary)
      ->SetWithoutPathExpansion(current_path, std::move(in_value));
}

Value* DictionaryValue::SetBoolean(StringPiece path, bool in_value) {
  return Set(path, std::make_unique<Value>(in_value));
}

Value* DictionaryValue::SetInteger(StringPiece path, int in_value) {
  return Set(path, std::make_unique<Value>(in_value));
}

Value* DictionaryValue::SetDouble(StringPiece path, double in_value) {
  return Set(path, std::make_unique<Value>(in_value));
}

Value* DictionaryValue::SetString(StringPiece path, StringPiece in_value) {
  return Set(path, std::make_unique<Value>(in_value));
}

Value* DictionaryValue::SetString(StringPiece path,
                                  const std::u16string& in_value) {
  return Set(path, std::make_unique<Value>(in_value));
}

DictionaryValue* DictionaryValue::SetDictionary(
    StringPiece path,
    std::unique_ptr<DictionaryValue> in_value) {
  return static_cast<DictionaryValue*>(Set(path, std::move(in_value)));
}

ListValue* DictionaryValue::SetList(StringPiece path,
                                    std::unique_ptr<ListValue> in_value) {
  return static_cast<ListValue*>(Set(path, std::move(in_value)));
}

Value* DictionaryValue::SetWithoutPathExpansion(
    StringPiece key,
    std::unique_ptr<Value> in_value) {
  // NOTE: We can't use |insert_or_assign| here, as only |try_emplace| does
  // an explicit conversion from StringPiece to std::string if necessary.
  auto result = dict().try_emplace(key, std::move(in_value));
  if (!result.second) {
    // in_value is guaranteed to be still intact at this point.
    result.first->second = std::move(in_value);
  }
  return result.first->second.get();
}

bool DictionaryValue::Get(StringPiece path, const Value** out_value) const {
  DCHECK(IsStringUTF8AllowingNoncharacters(path));
  const Value* value = FindPath(path);
  if (!value)
    return false;
  if (out_value)
    *out_value = value;
  return true;
}

bool DictionaryValue::Get(StringPiece path, Value** out_value) {
  return as_const(*this).Get(path, const_cast<const Value**>(out_value));
}

bool DictionaryValue::GetBoolean(StringPiece path, bool* bool_value) const {
  const Value* value;
  if (!Get(path, &value))
    return false;

  return value->GetAsBoolean(bool_value);
}

bool DictionaryValue::GetInteger(StringPiece path, int* out_value) const {
  const Value* value;
  if (!Get(path, &value))
    return false;

  bool is_int = value->is_int();
  if (is_int && out_value)
    *out_value = value->GetInt();
  return is_int;
}

bool DictionaryValue::GetDouble(StringPiece path, double* out_value) const {
  const Value* value;
  if (!Get(path, &value))
    return false;

  const bool is_convertible_to_double = value->is_double() || value->is_int();
  if (out_value && is_convertible_to_double) {
    *out_value = value->GetDouble();
  }

  return is_convertible_to_double;
}

bool DictionaryValue::GetString(StringPiece path,
                                std::string* out_value) const {
  const Value* value;
  if (!Get(path, &value))
    return false;

  return value->GetAsString(out_value);
}

bool DictionaryValue::GetString(StringPiece path,
                                std::u16string* out_value) const {
  const Value* value;
  if (!Get(path, &value))
    return false;

  return value->GetAsString(out_value);
}

bool DictionaryValue::GetStringASCII(StringPiece path,
                                     std::string* out_value) const {
  std::string out;
  if (!GetString(path, &out))
    return false;

  if (!IsStringASCII(out)) {
    NOTREACHED();
    return false;
  }

  out_value->assign(out);
  return true;
}

bool DictionaryValue::GetBinary(StringPiece path,
                                const Value** out_value) const {
  const Value* value;
  bool result = Get(path, &value);
  if (!result || !value->is_blob())
    return false;

  if (out_value)
    *out_value = value;

  return true;
}

bool DictionaryValue::GetBinary(StringPiece path, Value** out_value) {
  return as_const(*this).GetBinary(path, const_cast<const Value**>(out_value));
}

bool DictionaryValue::GetDictionary(StringPiece path,
                                    const DictionaryValue** out_value) const {
  const Value* value;
  bool result = Get(path, &value);
  if (!result || !value->is_dict())
    return false;

  if (out_value)
    *out_value = static_cast<const DictionaryValue*>(value);

  return true;
}

bool DictionaryValue::GetDictionary(StringPiece path,
                                    DictionaryValue** out_value) {
  return as_const(*this).GetDictionary(
      path, const_cast<const DictionaryValue**>(out_value));
}

bool DictionaryValue::GetList(StringPiece path,
                              const ListValue** out_value) const {
  const Value* value;
  bool result = Get(path, &value);
  if (!result || !value->is_list())
    return false;

  if (out_value)
    *out_value = static_cast<const ListValue*>(value);

  return true;
}

bool DictionaryValue::GetList(StringPiece path, ListValue** out_value) {
  return as_const(*this).GetList(path,
                                 const_cast<const ListValue**>(out_value));
}

bool DictionaryValue::GetDictionaryWithoutPathExpansion(
    StringPiece key,
    const DictionaryValue** out_value) const {
  const Value* value = FindKey(key);
  if (!value || !value->is_dict())
    return false;

  if (out_value)
    *out_value = static_cast<const DictionaryValue*>(value);

  return true;
}

bool DictionaryValue::GetDictionaryWithoutPathExpansion(
    StringPiece key,
    DictionaryValue** out_value) {
  return as_const(*this).GetDictionaryWithoutPathExpansion(
      key, const_cast<const DictionaryValue**>(out_value));
}

bool DictionaryValue::GetListWithoutPathExpansion(
    StringPiece key,
    const ListValue** out_value) const {
  const Value* value = FindKey(key);
  if (!value || !value->is_list())
    return false;

  if (out_value)
    *out_value = static_cast<const ListValue*>(value);

  return true;
}

bool DictionaryValue::GetListWithoutPathExpansion(StringPiece key,
                                                  ListValue** out_value) {
  return as_const(*this).GetListWithoutPathExpansion(
      key, const_cast<const ListValue**>(out_value));
}

std::unique_ptr<DictionaryValue> DictionaryValue::DeepCopyWithoutEmptyChildren()
    const {
  std::unique_ptr<DictionaryValue> copy =
      CopyDictionaryWithoutEmptyChildren(*this);
  if (!copy)
    copy = std::make_unique<DictionaryValue>();
  return copy;
}

void DictionaryValue::Swap(DictionaryValue* other) {
  CHECK(other->is_dict());
  dict().swap(other->dict());
}

DictionaryValue::Iterator::Iterator(const DictionaryValue& target)
    : target_(target), it_(target.DictItems().begin()) {}

DictionaryValue::Iterator::Iterator(const Iterator& other) = default;

DictionaryValue::Iterator::~Iterator() = default;

DictionaryValue* DictionaryValue::DeepCopy() const {
  return new DictionaryValue(dict());
}

std::unique_ptr<DictionaryValue> DictionaryValue::CreateDeepCopy() const {
  return std::make_unique<DictionaryValue>(dict());
}

///////////////////// ListValue ////////////////////

// static
std::unique_ptr<ListValue> ListValue::From(std::unique_ptr<Value> value) {
  ListValue* out;
  if (value && value->GetAsList(&out)) {
    ignore_result(value.release());
    return WrapUnique(out);
  }
  return nullptr;
}

ListValue::ListValue() : Value(Type::LIST) {}
ListValue::ListValue(span<const Value> in_list) : Value(in_list) {}
ListValue::ListValue(ListStorage&& in_list) noexcept
    : Value(std::move(in_list)) {}

bool ListValue::Set(size_t index, std::unique_ptr<Value> in_value) {
  if (!in_value)
    return false;

  if (index >= list().size())
    list().resize(index + 1);

  list()[index] = std::move(*in_value);
  return true;
}

bool ListValue::Get(size_t index, const Value** out_value) const {
  if (index >= list().size())
    return false;

  if (out_value)
    *out_value = &list()[index];

  return true;
}

bool ListValue::Get(size_t index, Value** out_value) {
  return as_const(*this).Get(index, const_cast<const Value**>(out_value));
}

bool ListValue::GetBoolean(size_t index, bool* bool_value) const {
  const Value* value;
  if (!Get(index, &value))
    return false;

  return value->GetAsBoolean(bool_value);
}

bool ListValue::GetString(size_t index, std::string* out_value) const {
  const Value* value;
  if (!Get(index, &value))
    return false;

  return value->GetAsString(out_value);
}

bool ListValue::GetString(size_t index, std::u16string* out_value) const {
  const Value* value;
  if (!Get(index, &value))
    return false;

  return value->GetAsString(out_value);
}

bool ListValue::GetDictionary(size_t index,
                              const DictionaryValue** out_value) const {
  const Value* value;
  bool result = Get(index, &value);
  if (!result || !value->is_dict())
    return false;

  if (out_value)
    *out_value = static_cast<const DictionaryValue*>(value);

  return true;
}

bool ListValue::GetDictionary(size_t index, DictionaryValue** out_value) {
  return as_const(*this).GetDictionary(
      index, const_cast<const DictionaryValue**>(out_value));
}

void ListValue::Append(std::unique_ptr<Value> in_value) {
  list().push_back(std::move(*in_value));
}

void ListValue::AppendBoolean(bool in_value) {
  list().emplace_back(in_value);
}

void ListValue::AppendInteger(int in_value) {
  list().emplace_back(in_value);
}

void ListValue::AppendString(StringPiece in_value) {
  list().emplace_back(in_value);
}

void ListValue::AppendString(const std::u16string& in_value) {
  list().emplace_back(in_value);
}

void ListValue::Swap(ListValue* other) {
  CHECK(other->is_list());
  list().swap(other->list());
}

std::unique_ptr<ListValue> ListValue::CreateDeepCopy() const {
  return std::make_unique<ListValue>(list());
}

ValueSerializer::~ValueSerializer() = default;

ValueDeserializer::~ValueDeserializer() = default;

std::ostream& operator<<(std::ostream& out, const Value& value) {
  return out << value.DebugString();
}

std::ostream& operator<<(std::ostream& out, const Value::Type& type) {
  if (static_cast<int>(type) < 0 ||
      static_cast<size_t>(type) >= base::size(kTypeNames))
    return out << "Invalid Type (index = " << static_cast<int>(type) << ")";
  return out << Value::GetTypeName(type);
}

}  // namespace base
