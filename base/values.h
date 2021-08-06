// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file specifies a recursive data storage class called Value intended for
// storing settings and other persistable data.
//
// A Value represents something that can be stored in JSON or passed to/from
// JavaScript. As such, it is NOT a generalized variant type, since only the
// types supported by JavaScript/JSON are supported.
//
// IN PARTICULAR this means that there is no support for int64_t or unsigned
// numbers. Writing JSON with such types would violate the spec. If you need
// something like this, either use a double or make a string value containing
// the number you want.
//
// NOTE: A Value parameter that is always a Value::STRING should just be passed
// as a std::string. Similarly for Values that are always Value::DICTIONARY
// (should be flat_map), Value::LIST (should be std::vector), et cetera.

#ifndef BASE_VALUES_H_
#define BASE_VALUES_H_

#include <stddef.h>
#include <stdint.h>

#include <initializer_list>
#include <iosfwd>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/base_export.h"
#include "base/containers/checked_iterators.h"
#include "base/containers/checked_range.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "base/trace_event/base_tracing_forward.h"
#include "base/value_iterators.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace base {

class DictionaryValue;
class ListValue;

// The Value class is the base class for Values. A Value can be instantiated
// via passing the appropriate type or backing storage to the constructor.
//
// See the file-level comment above for more information.
//
// base::Value is currently in the process of being refactored. Design doc:
// https://docs.google.com/document/d/1uDLu5uTRlCWePxQUEHc8yNQdEoE1BDISYdpggWEABnw
//
// Previously (which is how most code that currently exists is written), Value
// used derived types to implement the individual data types, and base::Value
// was just a base class to refer to them. This required everything be heap
// allocated.
//
// OLD WAY:
//
//   std::unique_ptr<base::Value> GetFoo() {
//     std::unique_ptr<DictionaryValue> dict;
//     dict->SetString("mykey", foo);
//     return dict;
//   }
//
// The new design makes base::Value a variant type that holds everything in
// a union. It is now recommended to pass by value with std::move rather than
// use heap allocated values. The DictionaryValue and ListValue subclasses
// exist only as a compatibility shim that we're in the process of removing.
//
// NEW WAY:
//
//   base::Value GetFoo() {
//     base::Value dict(base::Value::Type::DICTIONARY);
//     dict.SetKey("mykey", base::Value(foo));
//     return dict;
//   }
//
// The new design tries to avoid losing type information. Thus when migrating
// off deprecated types, existing usages of base::ListValue should be replaced
// by std::vector<base::Value>, and existing usages of base::DictionaryValue
// should be replaced with base::flat_map<std::string, base::Value>.
//
// OLD WAY:
//
//   void AlwaysTakesList(std::unique_ptr<base::ListValue> list);
//   void AlwaysTakesDict(std::unique_ptr<base::DictionaryValue> dict);
//
// NEW WAY:
//
//   void AlwaysTakesList(std::vector<base::Value> list);
//   void AlwaysTakesDict(base::flat_map<std::string, base::Value> dict);
//
// Migrating code will require conversions on API boundaries. This can be done
// cheaply by making use of overloaded base::Value constructors and the
// `Value::TakeList()` and `Value::TakeDict()` APIs.
class BASE_EXPORT Value {
 public:
  using BlobStorage = std::vector<uint8_t>;
  using ListStorage = std::vector<Value>;
  using DictStorage = flat_map<std::string, Value>;

  // Like `DictStorage`, but with std::unique_ptr in the mapped type. This is
  // due to legacy reasons, and should be removed once no caller relies on
  // stability of pointers anymore.
  using LegacyDictStorage = flat_map<std::string, std::unique_ptr<Value>>;

  using ListView = CheckedContiguousRange<ListStorage>;
  using ConstListView = CheckedContiguousConstRange<ListStorage>;

  enum class Type : unsigned char {
    NONE = 0,
    BOOLEAN,
    INTEGER,
    DOUBLE,
    STRING,
    BINARY,
    DICTIONARY,
    LIST,
    // Note: Do not add more types. See the file-level comment above for why.
  };

  // Adaptors for converting from the old way to the new way and vice versa.
  static Value FromUniquePtrValue(std::unique_ptr<Value> val);
  static std::unique_ptr<Value> ToUniquePtrValue(Value val);
  static const DictionaryValue& AsDictionaryValue(const Value& val);
  static const ListValue& AsListValue(const Value& val);

  Value() noexcept;
  Value(Value&& that) noexcept;

  // Value's copy constructor and copy assignment operator are deleted. Use this
  // to obtain a deep copy explicitly.
  Value Clone() const;

  explicit Value(Type type);
  explicit Value(bool in_bool);
  explicit Value(int in_int);
  explicit Value(double in_double);

  // Value(const char*) and Value(const char16_t*) are required despite
  // Value(StringPiece) and Value(StringPiece16) because otherwise the
  // compiler will choose the Value(bool) constructor for these arguments.
  // Value(std::string&&) allow for efficient move construction.
  explicit Value(const char* in_string);
  explicit Value(StringPiece in_string);
  explicit Value(std::string&& in_string) noexcept;
  explicit Value(const char16_t* in_string16);
  explicit Value(StringPiece16 in_string16);

  // Disable constructions from other pointers, so that there is no silent
  // conversion to bool.
  template <typename T,
            typename = std::enable_if_t<
                !std::is_convertible<T*, std::string>::value &&
                !std::is_convertible<T*, std::u16string>::value>>
  explicit Value(T* ptr) = delete;

  explicit Value(const std::vector<char>& in_blob);
  explicit Value(base::span<const uint8_t> in_blob);
  explicit Value(BlobStorage&& in_blob) noexcept;

  explicit Value(const DictStorage& in_dict);
  explicit Value(DictStorage&& in_dict) noexcept;

  explicit Value(span<const Value> in_list);
  explicit Value(ListStorage&& in_list) noexcept;

  Value& operator=(Value&& that) noexcept;
  Value(const Value&) = delete;
  Value& operator=(const Value&) = delete;

  ~Value();

  // Returns the name for a given `type`.
  static const char* GetTypeName(Type type);

  // Returns the type of the value stored by the current Value object.
  Type type() const { return static_cast<Type>(data_.index()); }

  // Returns true if the current object represents a given type.
  bool is_none() const { return type() == Type::NONE; }
  bool is_bool() const { return type() == Type::BOOLEAN; }
  bool is_int() const { return type() == Type::INTEGER; }
  bool is_double() const { return type() == Type::DOUBLE; }
  bool is_string() const { return type() == Type::STRING; }
  bool is_blob() const { return type() == Type::BINARY; }
  bool is_dict() const { return type() == Type::DICTIONARY; }
  bool is_list() const { return type() == Type::LIST; }

  // These will return nullopt / nullptr if the type does not match.
  absl::optional<bool> GetIfBool() const;
  absl::optional<int> GetIfInt() const;
  // Implicitly converts from int if necessary.
  absl::optional<double> GetIfDouble() const;
  const std::string* GetIfString() const;
  const BlobStorage* GetIfBlob() const;

  // These will all CHECK that the type matches.
  bool GetBool() const;
  int GetInt() const;
  double GetDouble() const;  // Implicitly converts from int if necessary.
  const std::string& GetString() const;
  std::string& GetString();
  const BlobStorage& GetBlob() const;

  // Returns the Values in a list as a view. The mutable overload allows for
  // modification of the underlying values, but does not allow changing the
  // structure of the list. If this is desired, use `TakeList()`, perform the
  // operations, and return the list back to the Value via move assignment.
  ListView GetList();
  ConstListView GetList() const;

  // Transfers ownership of the underlying list to the caller. Subsequent
  // calls to `GetList()` will return an empty list.
  // Note: This requires that `type()` is Type::LIST.
  ListStorage TakeList() &&;

  // Appends `value` to the end of the list.
  // Note: These CHECK that `type()` is Type::LIST.
  void Append(bool value);
  void Append(int value);
  void Append(double value);

  void Append(const char* value);
  void Append(StringPiece value);
  void Append(std::string&& value);
  void Append(const char16_t* value);

  // Disable `Append()` from other pointers, so that there is no silent
  // conversion to bool.
  template <typename T,
            typename = std::enable_if_t<
                !std::is_convertible<T*, std::string>::value &&
                !std::is_convertible<T*, std::u16string>::value>>
  void Append(T* ptr) = delete;

  void Append(StringPiece16 value);
  void Append(Value&& value);

  // Inserts `value` before `pos`.
  // Note: This CHECK that `type()` is Type::LIST.
  CheckedContiguousIterator<Value> Insert(
      CheckedContiguousConstIterator<Value> pos,
      Value&& value);

  // Erases the Value pointed to by `iter`. Returns false if `iter` is out of
  // bounds.
  // Note: This requires that `type()` is Type::LIST.
  bool EraseListIter(CheckedContiguousConstIterator<Value> iter);

  // Erases all Values that compare equal to `val`. Returns the number of
  // deleted Values.
  // Note: This requires that `type()` is Type::LIST.
  size_t EraseListValue(const Value& val);

  // Erases all Values for which `pred` returns true. Returns the number of
  // deleted Values.
  // Note: This requires that `type()` is Type::LIST.
  template <typename Predicate>
  size_t EraseListValueIf(Predicate pred) {
    return base::EraseIf(list(), pred);
  }

  // Erases all Values from the list.
  // Note: This requires that `type()` is Type::LIST.
  void ClearList();

  // `FindKey` looks up `key` in the underlying dictionary. If found, it returns
  // a pointer to the element. Otherwise it returns nullptr.
  // returned. Callers are expected to perform a check against null before using
  // the pointer.
  // Note: This requires that `type()` is Type::DICTIONARY.
  //
  // Example:
  //   auto* found = FindKey("foo");
  Value* FindKey(StringPiece key);
  const Value* FindKey(StringPiece key) const;

  // `FindKeyOfType` is similar to `FindKey`, but it also requires the found
  // value to have type `type`. If no type is found, or the found value is of a
  // different type nullptr is returned.
  // Callers are expected to perform a check against null before using the
  // pointer.
  // Note: This requires that `type()` is Type::DICTIONARY.
  //
  // Example:
  //   auto* found = FindKey("foo", Type::DOUBLE);
  Value* FindKeyOfType(StringPiece key, Type type);
  const Value* FindKeyOfType(StringPiece key, Type type) const;

  // These are convenience forms of `FindKey`. They return absl::nullopt if
  // the value is not found or doesn't have the type specified in the
  // function's name.
  absl::optional<bool> FindBoolKey(StringPiece key) const;
  absl::optional<int> FindIntKey(StringPiece key) const;
  // Note `FindDoubleKey()` will auto-convert INTEGER keys to their double
  // value, for consistency with `GetDouble()`.
  absl::optional<double> FindDoubleKey(StringPiece key) const;

  // `FindStringKey` returns `nullptr` if value is not found or not a string.
  const std::string* FindStringKey(StringPiece key) const;
  std::string* FindStringKey(StringPiece key);

  // Returns nullptr is value is not found or not a binary.
  const BlobStorage* FindBlobKey(StringPiece key) const;

  // Returns nullptr if value is not found or not a dictionary.
  const Value* FindDictKey(StringPiece key) const;
  Value* FindDictKey(StringPiece key);

  // Returns nullptr if value is not found or not a list.
  const Value* FindListKey(StringPiece key) const;
  Value* FindListKey(StringPiece key);

  // `SetKey` looks up `key` in the underlying dictionary and sets the mapped
  // value to `value`. If `key` could not be found, a new element is inserted.
  // A pointer to the modified item is returned.
  // Note: This requires that `type()` is Type::DICTIONARY.
  // Note: Prefer `Set<Type>Key()` for simple values.
  //
  // Example:
  //   SetKey("foo", std::move(myvalue));
  Value* SetKey(StringPiece key, Value&& value);
  // This overload results in a performance improvement for std::string&&.
  Value* SetKey(std::string&& key, Value&& value);
  // This overload is necessary to avoid ambiguity for const char* arguments.
  Value* SetKey(const char* key, Value&& value);

  // `Set`Type>Key` looks up `key` in the underlying dictionary and associates a
  // corresponding Value() constructed from the second parameter. Compared to
  // `SetKey()`, this avoids un-necessary temporary `Value()` creation, as well
  // ambiguities in the value type.
  Value* SetBoolKey(StringPiece key, bool val);
  Value* SetIntKey(StringPiece key, int val);
  Value* SetDoubleKey(StringPiece key, double val);
  Value* SetStringKey(StringPiece key, StringPiece val);
  Value* SetStringKey(StringPiece key, StringPiece16 val);
  // NOTE: The following two overloads are provided as performance / code
  // generation optimizations.
  Value* SetStringKey(StringPiece key, const char* val);
  Value* SetStringKey(StringPiece key, std::string&& val);

  // This attempts to remove the value associated with `key`. In case of
  // failure, e.g. the key does not exist, false is returned and the underlying
  // dictionary is not changed. In case of success, `key` is deleted from the
  // dictionary and the method returns true.
  // Note: This requires that `type()` is Type::DICTIONARY.
  //
  // Example:
  //   bool success = dict.RemoveKey("foo");
  bool RemoveKey(StringPiece key);

  // This attempts to extract the value associated with `key`. In case of
  // failure, e.g. the key does not exist, nullopt is returned and the
  // underlying dictionary is not changed. In case of success, `key` is deleted
  // from the dictionary and the method returns the extracted Value.
  // Note: This requires that `type()` is Type::DICTIONARY.
  //
  // Example:
  //   absl::optional<Value> maybe_value = dict.ExtractKey("foo");
  absl::optional<Value> ExtractKey(StringPiece key);

  // Searches a hierarchy of dictionary values for a given value. If a path
  // of dictionaries exist, returns the item at that path. If any of the path
  // components do not exist or if any but the last path components are not
  // dictionaries, returns nullptr.
  //
  // The type of the leaf Value is not checked.
  //
  // Implementation note: This can't return an iterator because the iterator
  // will actually be into another Value, so it can't be compared to iterators
  // from this one (in particular, the DictItems().end() iterator).
  //
  // This version takes a StringPiece for the path, using dots as separators.
  // Example:
  //    auto* found = FindPath("foo.bar");
  Value* FindPath(StringPiece path);
  const Value* FindPath(StringPiece path) const;

  // There are also deprecated versions that take the path parameter
  // as either a std::initializer_list<StringPiece> or a
  // span<const StringPiece>. The latter is useful to use a
  // std::vector<std::string> as a parameter but creates huge dynamic
  // allocations and should be avoided!
  // Note: If there is only one component in the path, use `FindKey()` instead.
  //
  // Example:
  //   std::vector<StringPiece> components = ...
  //   auto* found = FindPath(components);
  Value* FindPath(std::initializer_list<StringPiece> path);
  Value* FindPath(span<const StringPiece> path);
  const Value* FindPath(std::initializer_list<StringPiece> path) const;
  const Value* FindPath(span<const StringPiece> path) const;

  // Like FindPath() but will only return the value if the leaf Value type
  // matches the given type. Will return nullptr otherwise.
  // Note: Prefer `Find<Type>Path()` for simple values.
  //
  // Note: If there is only one component in the path, use `FindKeyOfType()`
  // instead for slightly better performance.
  Value* FindPathOfType(StringPiece path, Type type);
  const Value* FindPathOfType(StringPiece path, Type type) const;

  // Convenience accessors used when the expected type of a value is known.
  // Similar to Find<Type>Key() but accepts paths instead of keys.
  absl::optional<bool> FindBoolPath(StringPiece path) const;
  absl::optional<int> FindIntPath(StringPiece path) const;
  absl::optional<double> FindDoublePath(StringPiece path) const;
  const std::string* FindStringPath(StringPiece path) const;
  std::string* FindStringPath(StringPiece path);
  const BlobStorage* FindBlobPath(StringPiece path) const;
  Value* FindDictPath(StringPiece path);
  const Value* FindDictPath(StringPiece path) const;
  Value* FindListPath(StringPiece path);
  const Value* FindListPath(StringPiece path) const;

  // The following forms are deprecated too, use the ones that take the path
  // as a single StringPiece instead.
  Value* FindPathOfType(std::initializer_list<StringPiece> path, Type type);
  Value* FindPathOfType(span<const StringPiece> path, Type type);
  const Value* FindPathOfType(std::initializer_list<StringPiece> path,
                              Type type) const;
  const Value* FindPathOfType(span<const StringPiece> path, Type type) const;

  // Sets the given path, expanding and creating dictionary keys as necessary.
  //
  // If the current value is not a dictionary, the function returns nullptr. If
  // path components do not exist, they will be created. If any but the last
  // components matches a value that is not a dictionary, the function will fail
  // (it will not overwrite the value) and return nullptr. The last path
  // component will be unconditionally overwritten if it exists, and created if
  // it doesn't.
  //
  // Example:
  //   value.SetPath("foo.bar", std::move(myvalue));
  //
  // Note: If there is only one component in the path, use `SetKey()` instead.
  // Note: Using `Set<Type>Path()` might be more convenient and efficient.
  Value* SetPath(StringPiece path, Value&& value);

  // These setters are more convenient and efficient than the corresponding
  // SetPath(...) call.
  Value* SetBoolPath(StringPiece path, bool value);
  Value* SetIntPath(StringPiece path, int value);
  Value* SetDoublePath(StringPiece path, double value);
  Value* SetStringPath(StringPiece path, StringPiece value);
  Value* SetStringPath(StringPiece path, const char* value);
  Value* SetStringPath(StringPiece path, std::string&& value);
  Value* SetStringPath(StringPiece path, StringPiece16 value);

  // Deprecated: use the `SetPath(StringPiece, ...)` methods above instead.
  Value* SetPath(std::initializer_list<StringPiece> path, Value&& value);
  Value* SetPath(span<const StringPiece> path, Value&& value);

  // Tries to remove a Value at the given path.
  //
  // If the current value is not a dictionary or any path component does not
  // exist, this operation fails, leaves underlying Values untouched and returns
  // `false`. In case intermediate dictionaries become empty as a result of this
  // path removal, they will be removed as well.
  // Note: If there is only one component in the path, use `RemoveKey()`
  // instead.
  //
  // Example:
  //   bool success = value.RemovePath("foo.bar");
  bool RemovePath(StringPiece path);

  // Tries to extract a Value at the given path.
  //
  // If the current value is not a dictionary or any path component does not
  // exist, this operation fails, leaves underlying Values untouched and returns
  // nullopt. In case intermediate dictionaries become empty as a result of this
  // path removal, they will be removed as well. Returns the extracted value on
  // success.
  // Note: If there is only one component in the path, use `ExtractKey()`
  // instead.
  //
  // Example:
  //   absl::optional<Value> maybe_value = value.ExtractPath("foo.bar");
  absl::optional<Value> ExtractPath(StringPiece path);

  using dict_iterator_proxy = detail::dict_iterator_proxy;
  using const_dict_iterator_proxy = detail::const_dict_iterator_proxy;

  // `DictItems` returns a proxy object that exposes iterators to the underlying
  // dictionary. These are intended for iteration over all items in the
  // dictionary and are compatible with for-each loops and standard library
  // algorithms.
  //
  // Unlike with std::map, a range-for over the non-const version of
  // `DictItems()` will range over items of type
  // `pair<const std::string&, Value&>`, so code of the form
  //   for (auto kv : my_value.DictItems())
  //     Mutate(kv.second);
  // will actually alter `my_value` in place (if it isn't const).
  //
  // Note: These CHECK that `type()` is Type::DICTIONARY.
  dict_iterator_proxy DictItems();
  const_dict_iterator_proxy DictItems() const;

  // Transfers ownership of the underlying dict to the caller. Subsequent
  // calls to DictItems() will return an empty dict.
  // Note: This requires that `type()` is Type::DICTIONARY.
  DictStorage TakeDict() &&;

  // Returns the size of the dictionary, if the dictionary is empty, and clears
  // the dictionary. Note: These CHECK that `type()` is Type::DICTIONARY.
  size_t DictSize() const;
  bool DictEmpty() const;
  void DictClear();

  // Merge `dictionary` into this value. This is done recursively, i.e. any
  // sub-dictionaries will be merged as well. In case of key collisions, the
  // passed in dictionary takes precedence and data already present will be
  // replaced. Values within `dictionary` are deep-copied, so `dictionary` may
  // be freed any time after this call.
  // Note: This requires that `type()` and `dictionary->type()` is
  // Type::DICTIONARY.
  void MergeDictionary(const Value* dictionary);

  // These methods allow the convenient retrieval of the contents of the Value.
  // If the current object can be converted into the given type, the value is
  // returned through the `out_value` parameter and true is returned;
  // otherwise, false is returned and `out_value` is unchanged.
  // DEPRECATED, use `GetIfBool()` instead.
  bool GetAsBoolean(bool* out_value) const;
  // DEPRECATED, use `GetIfString()` instead.
  bool GetAsString(std::string* out_value) const;
  bool GetAsString(std::u16string* out_value) const;
  bool GetAsString(const Value** out_value) const;
  bool GetAsString(StringPiece* out_value) const;
  // ListValue::From is the equivalent for std::unique_ptr conversions.
  // DEPRECATED, use `is_list()` instead.
  bool GetAsList(ListValue** out_value);
  bool GetAsList(const ListValue** out_value) const;
  // DictionaryValue::From is the equivalent for std::unique_ptr conversions.
  bool GetAsDictionary(DictionaryValue** out_value);
  bool GetAsDictionary(const DictionaryValue** out_value) const;
  // Note: Do not add more types. See the file-level comment above for why.

  // This creates a deep copy of the entire Value tree, and returns a pointer
  // to the copy. The caller gets ownership of the copy, of course.
  // Subclasses return their own type directly in their overrides;
  // this works because C++ supports covariant return types.
  // DEPRECATED, use `Value::Clone()` instead.
  // TODO(crbug.com/646113): Delete this and migrate callsites.
  Value* DeepCopy() const;
  // DEPRECATED, use `Value::Clone()` instead.
  // TODO(crbug.com/646113): Delete this and migrate callsites.
  std::unique_ptr<Value> CreateDeepCopy() const;

  // Comparison operators so that Values can easily be used with standard
  // library algorithms and associative containers.
  BASE_EXPORT friend bool operator==(const Value& lhs, const Value& rhs);
  BASE_EXPORT friend bool operator!=(const Value& lhs, const Value& rhs);
  BASE_EXPORT friend bool operator<(const Value& lhs, const Value& rhs);
  BASE_EXPORT friend bool operator>(const Value& lhs, const Value& rhs);
  BASE_EXPORT friend bool operator<=(const Value& lhs, const Value& rhs);
  BASE_EXPORT friend bool operator>=(const Value& lhs, const Value& rhs);

  // Compares if two Value objects have equal contents.
  // DEPRECATED, use `operator==(const Value& lhs, const Value& rhs)` instead.
  // TODO(crbug.com/646113): Delete this and migrate callsites.
  bool Equals(const Value* other) const;

  // Estimates dynamic memory usage. Requires tracing support
  // (enable_base_tracing gn flag), otherwise always returns 0. See
  // base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  // Serializes to a string for logging and debug purposes.
  std::string DebugString() const;

#if BUILDFLAG(ENABLE_BASE_TRACING)
  // Write this object into a trace.
  void WriteIntoTrace(perfetto::TracedValue) const;
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

 protected:
  // Checked convenience accessors for dict and list.
  const LegacyDictStorage& dict() const {
    return absl::get<LegacyDictStorage>(data_);
  }
  LegacyDictStorage& dict() { return absl::get<LegacyDictStorage>(data_); }
  const ListStorage& list() const { return absl::get<ListStorage>(data_); }
  ListStorage& list() { return absl::get<ListStorage>(data_); }

  // Internal constructors, allowing the simplify the implementation of Clone().
  explicit Value(const LegacyDictStorage& storage);
  explicit Value(LegacyDictStorage&& storage) noexcept;

 private:
  // Special case for doubles, which are aligned to 8 bytes on some
  // 32-bit architectures. In this case, a simple declaration as a
  // double member would make the whole union 8 byte-aligned, which
  // would also force 4 bytes of wasted padding space before it in
  // the Value layout.
  //
  // To override this, store the value as an array of 32-bit integers, and
  // perform the appropriate bit casts when reading / writing to it.
  using DoubleStorage = struct { alignas(4) char v[sizeof(double)]; };

  // Internal constructors, allowing the simplify the implementation of Clone().
  explicit Value(absl::monostate);
  explicit Value(DoubleStorage storage);

  friend class ValuesTest_SizeOfValue_Test;
  double AsDoubleInternal() const;

  // NOTE: Using a movable reference here is done for performance (it avoids
  // creating + moving + destroying a temporary unique ptr).
  Value* SetKeyInternal(StringPiece key, std::unique_ptr<Value>&& val_ptr);
  Value* SetPathInternal(StringPiece path, std::unique_ptr<Value>&& value_ptr);

  absl::variant<absl::monostate,
                bool,
                int,
                DoubleStorage,
                std::string,
                BlobStorage,
                LegacyDictStorage,
                ListStorage>
      data_;
};

// DictionaryValue provides a key-value dictionary with (optional) "path"
// parsing for recursive access; see the comment at the top of the file. Keys
// are std::string's and should be UTF-8 encoded.
class BASE_EXPORT DictionaryValue : public Value {
 public:
  // Returns `value` if it is a dictionary, nullptr otherwise.
  static std::unique_ptr<DictionaryValue> From(std::unique_ptr<Value> value);

  DictionaryValue();
  explicit DictionaryValue(const LegacyDictStorage& in_dict);
  explicit DictionaryValue(LegacyDictStorage&& in_dict) noexcept;

  // Returns true if the current dictionary has a value for the given key.
  // DEPRECATED, use `Value::FindKey(key)` instead.
  bool HasKey(StringPiece key) const;

  // Clears any current contents of this dictionary.
  // DEPRECATED, use `Value::DictClear()` instead.
  void Clear();

  // Sets the Value associated with the given path starting from this object.
  // A path has the form "<key>" or "<key>.<key>.[...]", where "." indexes
  // into the next DictionaryValue down.  Obviously, "." can't be used
  // within a key, but there are no other restrictions on keys.
  // If the key at any step of the way doesn't exist, or exists but isn't
  // a DictionaryValue, a new DictionaryValue will be created and attached
  // to the path in that location. `in_value` must be non-null.
  // Returns a pointer to the inserted value.
  // DEPRECATED, use `Value::SetPath(path, value)` instead.
  Value* Set(StringPiece path, std::unique_ptr<Value> in_value);

  // Convenience forms of Set().  These methods will replace any existing
  // value at that path, even if it has a different type.
  // DEPRECATED, use `Value::SetBoolKey()` or `Value::SetBoolPath()`.
  Value* SetBoolean(StringPiece path, bool in_value);
  // DEPRECATED, use `Value::SetIntPath()`.
  Value* SetInteger(StringPiece path, int in_value);
  // DEPRECATED, use `Value::SetDoublePath()`.
  Value* SetDouble(StringPiece path, double in_value);
  // DEPRECATED, use `Value::SetStringPath()`.
  Value* SetString(StringPiece path, StringPiece in_value);
  // DEPRECATED, use `Value::SetStringPath()`.
  Value* SetString(StringPiece path, const std::u16string& in_value);
  // DEPRECATED, use `Value::SetPath()`.
  DictionaryValue* SetDictionary(StringPiece path,
                                 std::unique_ptr<DictionaryValue> in_value);
  // DEPRECATED, use `Value::SetPath()`.
  ListValue* SetList(StringPiece path, std::unique_ptr<ListValue> in_value);

  // Like Set(), but without special treatment of '.'.  This allows e.g. URLs to
  // be used as paths.
  // DEPRECATED, use `Value::SetKey(key, value)` instead.
  Value* SetWithoutPathExpansion(StringPiece key,
                                 std::unique_ptr<Value> in_value);

  // Gets the Value associated with the given path starting from this object.
  // A path has the form "<key>" or "<key>.<key>.[...]", where "." indexes
  // into the next DictionaryValue down.  If the path can be resolved
  // successfully, the value for the last key in the path will be returned
  // through the `out_value` parameter, and the function will return true.
  // Otherwise, it will return false and `out_value` will be untouched.
  // Note that the dictionary always owns the value that's returned.
  // `out_value` is optional and will only be set if non-NULL.
  // DEPRECATED, use `Value::FindPath(path)` instead.
  bool Get(StringPiece path, const Value** out_value) const;
  // DEPRECATED, use `Value::FindPath(path)` instead.
  bool Get(StringPiece path, Value** out_value);

  // These are convenience forms of `Get()`.  The value will be retrieved
  // and the return value will be true if the path is valid and the value at
  // the end of the path can be returned in the form specified.
  // `out_value` is optional and will only be set if non-NULL.
  // DEPRECATED, use `Value::FindBoolPath(path)` instead.
  bool GetBoolean(StringPiece path, bool* out_value) const;
  // DEPRECATED, use `Value::FindIntPath(path)` instead.
  bool GetInteger(StringPiece path, int* out_value) const;
  // Values of both type Type::INTEGER and Type::DOUBLE can be obtained as
  // doubles.
  // DEPRECATED, use `Value::FindDoublePath(path)`.
  bool GetDouble(StringPiece path, double* out_value) const;
  // DEPRECATED, use `Value::FindStringPath(path)` instead.
  bool GetString(StringPiece path, std::string* out_value) const;
  // DEPRECATED, use `Value::FindStringPath(path)` instead.
  bool GetString(StringPiece path, std::u16string* out_value) const;
  // DEPRECATED, use `Value::FindString(path)` and `IsAsciiString()` instead.
  bool GetStringASCII(StringPiece path, std::string* out_value) const;
  // DEPRECATED, use `Value::FindBlobPath(path)` instead.
  bool GetBinary(StringPiece path, const Value** out_value) const;
  // DEPRECATED, use `Value::FindBlobPath(path)` instead.
  bool GetBinary(StringPiece path, Value** out_value);
  // DEPRECATED, use `Value::FindPath(path)` and Value's Dictionary API
  // instead.
  bool GetDictionary(StringPiece path, const DictionaryValue** out_value) const;
  // DEPRECATED, use `Value::FindPath(path)` and Value's Dictionary API
  // instead.
  bool GetDictionary(StringPiece path, DictionaryValue** out_value);
  // DEPRECATED, use `Value::FindPath(path)` and `Value::GetList()` instead.
  bool GetList(StringPiece path, const ListValue** out_value) const;
  // DEPRECATED, use `Value::FindPath(path)` and `Value::GetList()` instead.
  bool GetList(StringPiece path, ListValue** out_value);

  // Like `Get()`, but without special treatment of '.'.  This allows e.g. URLs
  // to be used as paths.
  // DEPRECATED, use `Value::FindDictKey(key)` instead.
  bool GetDictionaryWithoutPathExpansion(
      StringPiece key,
      const DictionaryValue** out_value) const;
  // DEPRECATED, use `Value::FindDictKey(key)` instead.
  bool GetDictionaryWithoutPathExpansion(StringPiece key,
                                         DictionaryValue** out_value);
  // DEPRECATED, use `Value::FindListKey(key)` instead.
  bool GetListWithoutPathExpansion(StringPiece key,
                                   const ListValue** out_value) const;
  // DEPRECATED, use `Value::FindListKey(key)` instead.
  bool GetListWithoutPathExpansion(StringPiece key, ListValue** out_value);

  // Makes a copy of `this` but doesn't include empty dictionaries and lists in
  // the copy.  This never returns NULL, even if `this` itself is empty.
  std::unique_ptr<DictionaryValue> DeepCopyWithoutEmptyChildren() const;

  // Swaps contents with the `other` dictionary.
  void Swap(DictionaryValue* other);

  // This class provides an iterator over both keys and values in the
  // dictionary.  It can't be used to modify the dictionary.
  // DEPRECATED, use `Value::DictItems()` instead.
  class BASE_EXPORT Iterator {
   public:
    explicit Iterator(const DictionaryValue& target);
    Iterator(const Iterator& other);
    ~Iterator();

    bool IsAtEnd() const { return it_ == target_.DictItems().end(); }
    void Advance() { ++it_; }

    const std::string& key() const { return it_->first; }
    const Value& value() const { return it_->second; }

   private:
    const DictionaryValue& target_;
    detail::const_dict_iterator it_;
  };

  // DEPRECATED, use `Value::Clone()` instead.
  // TODO(crbug.com/646113): Delete this and migrate callsites.
  DictionaryValue* DeepCopy() const;
  // DEPRECATED, use `Value::Clone()` instead.
  // TODO(crbug.com/646113): Delete this and migrate callsites.
  std::unique_ptr<DictionaryValue> CreateDeepCopy() const;
};

// This type of Value represents a list of other Value values.
// DEPRECATED: Use std::vector<base::Value> instead.
class BASE_EXPORT ListValue : public Value {
 public:
  using const_iterator = ListView::const_iterator;
  using iterator = ListView::iterator;

  // Returns `value` if it is a list, nullptr otherwise.
  static std::unique_ptr<ListValue> From(std::unique_ptr<Value> value);

  ListValue();
  explicit ListValue(span<const Value> in_list);
  explicit ListValue(ListStorage&& in_list) noexcept;

  // Returns the number of Values in this list.
  // DEPRECATED, use `GetList()::size()` instead.
  size_t GetSize() const { return list().size(); }

  // Sets the list item at the given index to be the Value specified by
  // the value given.  If the index beyond the current end of the list, null
  // Values will be used to pad out the list.
  // Returns true if successful, or false if the index was negative or
  // the value is a null pointer.
  // DEPRECATED, use `GetList()::operator[] instead.
  bool Set(size_t index, std::unique_ptr<Value> in_value);

  // Gets the Value at the given index.  Modifies `out_value` (and returns true)
  // only if the index falls within the current list range.
  // Note that the list always owns the Value passed out via `out_value`.
  // `out_value` is optional and will only be set if non-NULL.
  // DEPRECATED, use `GetList()::operator[] instead.
  bool Get(size_t index, const Value** out_value) const;
  bool Get(size_t index, Value** out_value);

  // Convenience forms of `Get()`.  Modifies `out_value` (and returns true)
  // only if the index is valid and the Value at that index can be returned
  // in the specified form.
  // `out_value` is optional and will only be set if non-NULL.
  // DEPRECATED, use `GetList()::operator[]::GetBool()` instead.
  bool GetBoolean(size_t index, bool* out_value) const;
  // DEPRECATED, use `GetList()::operator[]::GetString()` instead.
  bool GetString(size_t index, std::string* out_value) const;
  bool GetString(size_t index, std::u16string* out_value) const;

  bool GetDictionary(size_t index, const DictionaryValue** out_value) const;
  bool GetDictionary(size_t index, DictionaryValue** out_value);

  using Value::Append;
  // Appends a Value to the end of the list.
  // DEPRECATED, use `Value::Append()` instead.
  void Append(std::unique_ptr<Value> in_value);

  // Convenience forms of Append.
  // DEPRECATED, use `Value::Append()` instead.
  void AppendBoolean(bool in_value);
  void AppendInteger(int in_value);
  void AppendString(StringPiece in_value);
  void AppendString(const std::u16string& in_value);

  // Swaps contents with the `other` list.
  // DEPRECATED, use `GetList()::swap()` instead.
  void Swap(ListValue* other);

  // Iteration.
  //
  // ListValue no longer supports iteration. Instead, use GetList() to get the
  // underlying list:
  //
  // for (const auto& entry : list_value.GetList()) {
  //   ...
  //
  // for (auto it = list_value.GetList().begin();
  //      it != list_value.GetList().end(); ++it) {
  //   ...

  // DEPRECATED, use `Value::Clone()` instead.
  // TODO(crbug.com/646113): Delete this and migrate callsites.
  std::unique_ptr<ListValue> CreateDeepCopy() const;
};

// This interface is implemented by classes that know how to serialize
// Value objects.
class BASE_EXPORT ValueSerializer {
 public:
  virtual ~ValueSerializer();

  virtual bool Serialize(const Value& root) = 0;
};

// This interface is implemented by classes that know how to deserialize Value
// objects.
class BASE_EXPORT ValueDeserializer {
 public:
  virtual ~ValueDeserializer();

  // This method deserializes the subclass-specific format into a Value object.
  // If the return value is non-NULL, the caller takes ownership of returned
  // Value.
  //
  // If the return value is nullptr, and if `error_code` is non-nullptr,
  // `*error_code` will be set to an integer value representing the underlying
  // error. See "enum ErrorCode" below for more detail about the integer value.
  //
  // If `error_message` is non-nullptr, it will be filled in with a formatted
  // error message including the location of the error if appropriate.
  virtual std::unique_ptr<Value> Deserialize(int* error_code,
                                             std::string* error_message) = 0;

  // The integer-valued error codes form four groups:
  //  - The value 0 means no error.
  //  - Values between 1 and 999 inclusive mean an error in the data (i.e.
  //    content). The bytes being deserialized are not in the right format.
  //  - Values 1000 and above mean an error in the metadata (i.e. context). The
  //    file could not be read, the network is down, etc.
  //  - Negative values are reserved.
  enum ErrorCode {
    kErrorCodeNoError = 0,
    // kErrorCodeInvalidFormat is a generic error code for "the data is not in
    // the right format". Subclasses of ValueDeserializer may return other
    // values for more specific errors.
    kErrorCodeInvalidFormat = 1,
    // kErrorCodeFirstMetadataError is the minimum value (inclusive) of the
    // range of metadata errors.
    kErrorCodeFirstMetadataError = 1000,
  };

  // The `error_code` argument can be one of the ErrorCode values, but it is
  // not restricted to only being 0, 1 or 1000. Subclasses of ValueDeserializer
  // can define their own error code values.
  static inline bool ErrorCodeIsDataError(int error_code) {
    return (kErrorCodeInvalidFormat <= error_code) &&
           (error_code < kErrorCodeFirstMetadataError);
  }
};

// Stream operator so Values can be used in assertion statements.  In order that
// gtest uses this operator to print readable output on test failures, we must
// override each specific type. Otherwise, the default template implementation
// is preferred over an upcast.
BASE_EXPORT std::ostream& operator<<(std::ostream& out, const Value& value);

BASE_EXPORT inline std::ostream& operator<<(std::ostream& out,
                                            const DictionaryValue& value) {
  return out << static_cast<const Value&>(value);
}

BASE_EXPORT inline std::ostream& operator<<(std::ostream& out,
                                            const ListValue& value) {
  return out << static_cast<const Value&>(value);
}

// Stream operator so that enum class Types can be used in log statements.
BASE_EXPORT std::ostream& operator<<(std::ostream& out,
                                     const Value::Type& type);

}  // namespace base

#endif  // BASE_VALUES_H_
