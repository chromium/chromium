// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_VALUES_H_
#define BASE_VALUES_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <initializer_list>
#include <iosfwd>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base_export.h"
#include "base/bit_cast.h"
#include "base/compiler_specific.h"
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

// The `Value` class is a variant type can hold one of the following types:
// - null
// - bool
// - int
// - double
// - string (internally UTF8-encoded)
// - binary data (i.e. a blob)
// - dictionary of string keys to `Value`s
// - list of `Value`s
//
// With the exception of binary blobs, `Value` is intended to be the C++ version
// of data types that can be represented in JSON.
//
// Warning: blob support may be removed in the future.
//
// ## Usage
//
// Do not use `Value` if a more specific type would be more appropriate.  For
// example, a function that only accepts dictionary values should have a
// `base::Value::Dict` parameter, not a `base::Value` parameter.
//
// Construction:
//
// `Value` is directly constructible from `bool`, `int`, `double`, binary blobs
// (`std::vector<uint8_t>`), `base::StringPiece`, `base::StringPiece16`,
// `Value::Dict`, and `Value::List`.
//
// Copying:
//
// `Value` does not support C++ copy semantics to make it harder to accidentally
// copy large values. Instead, use `Clone()` to manually create a deep copy.
//
// Reading:
//
// `GetBool()`, GetInt()`, et cetera `CHECK()` that the `Value` has the correct
// subtype before returning the contained value. `bool`, `int`, `double` are
// returned by value. Binary blobs, `std::string`, `Value::Dict`, `Value::List`
// are returned by reference.
//
// `GetIfBool()`, `GetIfInt()`, et cetera return `absl::nullopt`/`nullptr` if
// the `Value` does not have the correct subtype; otherwise, returns the value
// wrapped in an `absl::optional` (for `bool`, `int`, `double`) or by pointer
// (for binary blobs, `std::string`, `Value::Dict`, `Value::List`).
//
// Note: both `GetDouble()` and `GetIfDouble()` still return a non-null result
// when the subtype is `Value::Type::INT`. In that case, the stored value is
// coerced to a double before being returned.
//
// Assignment:
//
// It is not possible to directly assign `bool`, `int`, et cetera to a `Value`.
// Instead, wrap the underlying type in `Value` before assigning.
//
// ## Dictionaries and Lists
//
// `Value` provides the `Value::Dict` and `Value::List` container types for
// working with dictionaries and lists of values respectively, rather than
// exposing the underlying container types directly. This allows the types to
// provide convenient helpers for dictionaries and lists, as well as giving
// greater flexibility for changing implementation details in the future.
//
// Both container types support enough STL-isms to be usable in range-based for
// loops and generic operations such as those from <algorithm>.
//
// Dictionaries support:
// - `empty()`, `size()`, `begin()`, `end()`, `cbegin()`, `cend()`,
//       `contains()`, `clear()`, `erase()`: Identical to the STL container
//       equivalents, with additional safety checks, e.g. iterators will
//       `CHECK()` if `end()` is dereferenced.
//
// - `Clone()`: Create a deep copy.
// - `Merge()`: Merge another dictionary into this dictionary.
// - `Find()`: Find a value by `StringPiece` key, returning nullptr if the key
//       is not present.
// - `FindBool()`, `FindInt()`, ...: Similar to `Find()`, but ensures that the
//       `Value` also has the correct subtype. Same return semantics as
//       `GetIfBool()`, `GetIfInt()`, et cetera, returning `absl::nullopt` or
//       `nullptr` if the key is not present or the value has the wrong subtype.
// - `Set()`: Associate a value with a `StringPiece` key. Accepts `Value` or any
//       of the subtypes that `Value` can hold.
// - `Remove()`: Remove the key from this dictionary, if present.
// - `Extract()`: If the key is present in the dictionary, removes the key from
//       the dictionary and transfers ownership of `Value` to the caller.
//       Otherwise, returns `absl::nullopt`.
//
// Dictionaries also support an additional set of helper methods that operate on
// "paths": `FindByDottedPath()`, `SetByDottedPath()`, `RemoveByDottedPath()`,
// and `ExtractByDottedPath()`. Dotted paths are a convenience method of naming
// intermediate nested dictionaries, separating the components of the path using
// '.' characters. For example, finding a string path on a `Value::Dict` using
// the dotted path:
//
//   "aaa.bbb.ccc"
//
// Will first look for a `Value::Type::DICT` associated with the key "aaa", then
// another `Value::Type::DICT` under the "aaa" dict associated with the
// key "bbb", and then a `Value::Type::STRING` under the "bbb" dict associated
// with the key "ccc".
//
// If a path only has one component (i.e. has no dots), please use the regular,
// non-path APIs.
//
// Lists support:
// - `empty()`, `size()`, `begin()`, `end()`, `cbegin()`, `cend()`,
//       `front()`, `back()`, `reserve()`, `operator[]`, `clear()`, `erase()`:
//       Identical to the STL container equivalents, with additional safety
//       checks, e.g. `operator[]` will `CHECK()` if the index is out of range.
// - `Clone()`: Create a deep copy.
// - `Append()`: Append a value to the end of the list. Accepts `Value` or any
//       of the subtypes that `Value` can hold.
// - `Insert()`: Insert a `Value` at a specified point in the list.
// - `EraseValue()`: Erases all matching `Value`s from the list.
// - `EraseIf()`: Erase all `Value`s matching an arbitrary predicate from the
//       list.
//
// ## Refactoring Notes
//
// `Value` was originally implemented as a class hierarchy, with a `Value` base
// class, and a leaf class for each of the different types of `Value` subtypes.
// https://docs.google.com/document/d/1uDLu5uTRlCWePxQUEHc8yNQdEoE1BDISYdpggWEABnw
// proposed an overhaul of the `Value` API that has now largely been
// implemented, though there remains a significant amount of legacy code that is
// still being migrated as part of the code health migration.
//
// OLD WAY:
//
//   std::unique_ptr<base::Value> GetFoo() {
//     std::unique_ptr<DictionaryValue> dict;
//     dict->SetString("mykey", "foo");
//     return dict;
//   }
//
// NEW WAY:
//
//   base::Value GetFoo() {
//     base::Value::Dict dict;
//     dict.Set("mykey", "abc");
//     return base::Value(std::move(dict));
//   }
//
// To avoid losing type information with the new variant-based design, migration
// off the deprecated types should use more specific subtypes where possible:
//
// OLD WAY:
//
//   void AlwaysTakesList(std::unique_ptr<base::ListValue> list);
//   void AlwaysTakesDict(std::unique_ptr<base::DictionaryValue> dict);
//
// DEPRECATED (PREVIOUS) WAY:
//
//   void AlwaysTakesList(std::vector<base::Value> list);
//   void AlwaysTakesListAlternative1(base::Value::ConstListView list);
//   void AlwaysTakesListAlternative2(base::Value::ListView& list);
//   void AlwaysTakesListAlterantive3(base::Value::ListStorage);
//   void AlwaysTakesDict(base::flat_map<std::string, base::Value> dict);
//   void AlwaysTakesDictAlternative(base::Value::DictStorage);
//
// NEW WAY:
//
//   void AlwaysTakesList(base::Value::List list);
//   void AlwaysTakesDict(base::Value::Dict dict);
//
// Migrating code may require conversions on API boundaries. If something seems
// awkward/inefficient, please reach out to #code-health-rotation on Slack for
// consultation: it is entirely possible that certain classes of APIs may be
// missing due to an unrealized need.
class BASE_EXPORT GSL_OWNER Value {
 public:
  using BlobStorage = std::vector<uint8_t>;

  using DeprecatedListStorage = std::vector<Value>;
  // TODO(https://crbug.com/1291666): Make this private.
  using ListStorage = DeprecatedListStorage;

  // Like `DictStorage`, but with std::unique_ptr in the mapped type. This is
  // due to legacy reasons, and should be replaced with
  // flat_map<std::string, Value> once no caller relies on stability of pointers
  // anymore.
  using LegacyDictStorage = flat_map<std::string, std::unique_ptr<Value>>;

  using DeprecatedListView = CheckedContiguousRange<ListStorage>;
  using DeprecatedConstListView = CheckedContiguousConstRange<ListStorage>;
  // TODO(https://crbug.com/1291666): Make these private.
  using ListView = DeprecatedListView;
  using ConstListView = DeprecatedConstListView;

  class Dict;
  class List;

  enum class Type : unsigned char {
    NONE = 0,
    BOOLEAN,
    INTEGER,
    DOUBLE,
    STRING,
    BINARY,
    DICT,
    // TODO(https://crbug.com/1291670): Deprecated and will be removed.
    DICTIONARY = DICT,
    LIST,
    // Note: Do not add more types. See the file-level comment above for why.
  };

  // Adaptors for converting from the old way to the new way and vice versa.
  static Value FromUniquePtrValue(std::unique_ptr<Value> val);
  static std::unique_ptr<Value> ToUniquePtrValue(Value val);
  static const DictionaryValue& AsDictionaryValue(const Value& val);
  static const ListValue& AsListValue(const Value& val);

  Value() noexcept;

  Value(Value&&) noexcept;
  Value& operator=(Value&&) noexcept;

  // Deleted to prevent accidental copying.
  Value(const Value&) = delete;
  Value& operator=(const Value&) = delete;

  // Creates a deep copy of this value.
  Value Clone() const;

  // Creates a `Value` of `type`. The data of the corresponding type will be
  // default constructed.
  explicit Value(Type type);

  // Constructor for `Value::Type::BOOLEAN`.
  explicit Value(bool value);

  // Prevent pointers from implicitly converting to bool. Another way to write
  // this would be to template the bool constructor and use SFINAE to only allow
  // use if `std::is_same_v<T, bool>` is true, but this has surprising behavior
  // with range-based for loops over a `std::vector<bool>` (which will
  // unintuitively match the int overload instead).
  //
  // The `const` is load-bearing; otherwise, a `char*` argument would prefer the
  // deleted overload due to requiring a qualification conversion.
  template <typename T>
  explicit Value(const T*) = delete;

  // Constructor for `Value::Type::INT`.
  explicit Value(int value);

  // Constructor for `Value::Type::DOUBLE`.
  explicit Value(double value);

  // Constructors for `Value::Type::STRING`.
  explicit Value(StringPiece value);
  explicit Value(StringPiece16 value);
  // `char*` and `char16_t*` are needed to provide a more specific overload than
  // the deleted `const T*` overload above.
  explicit Value(const char* value);
  explicit Value(const char16_t* value);
  // `std::string&&` allows for efficient move construction.
  explicit Value(std::string&& value) noexcept;

  // Constructors for `Value::Type::BINARY`.
  explicit Value(const std::vector<char>& value);
  explicit Value(base::span<const uint8_t> value);
  explicit Value(BlobStorage&& value) noexcept;

  // Constructor for `Value::Type::DICT`.
  explicit Value(Dict&& value) noexcept;

  // Constructor for `Value::Type::LIST`.
  explicit Value(List&& value) noexcept;

  // DEPRECATED: prefer `Value(List&&)`.
  explicit Value(span<const Value> value);
  explicit Value(ListStorage&& value) noexcept;

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
  bool is_dict() const { return type() == Type::DICT; }
  bool is_list() const { return type() == Type::LIST; }

  // Returns the stored data if the type matches, or `absl::nullopt`/`nullptr`
  // otherwise. `bool`, `int`, and `double` are returned in a wrapped
  // `absl::optional`; blobs, `Value::Dict`, and `Value::List` are returned by
  // pointer.
  absl::optional<bool> GetIfBool() const;
  absl::optional<int> GetIfInt() const;
  // Returns a non-null value for both `Value::Type::DOUBLE` and
  // `Value::Type::INT`, converting the latter to a double.
  absl::optional<double> GetIfDouble() const;
  const std::string* GetIfString() const;
  std::string* GetIfString();
  const BlobStorage* GetIfBlob() const;
  const Dict* GetIfDict() const;
  Dict* GetIfDict();
  const List* GetIfList() const;
  List* GetIfList();

  // Similar to the `GetIf...()` variants above, but fails with a `CHECK()` on a
  // type mismatch. `bool`, `int`, and `double` are returned by value; blobs,
  // `Value::Dict`, and `Value::List` are returned by reference.
  bool GetBool() const;
  int GetInt() const;
  // Returns a value for both `Value::Type::DOUBLE` and `Value::Type::INT`,
  // converting the latter to a double.
  double GetDouble() const;
  // Callers that want to transfer ownership can use std::move() in conjunction
  // with one of the mutable variants below, e.g.:
  //   std::string taken_string = std::move(value.GetString());
  //   base::Value::Dict taken_dict = std::move(value.GetDict());
  //   base::Value::List taken_list = std::move(value.GetList());
  const std::string& GetString() const;
  std::string& GetString();
  const BlobStorage& GetBlob() const;
  const Dict& GetDict() const;
  Dict& GetDict();
  const List& GetList() const;
  List& GetList();

  // Represents a dictionary of string keys to Values.
  class BASE_EXPORT GSL_OWNER Dict {
   public:
    using iterator = detail::dict_iterator;
    using const_iterator = detail::const_dict_iterator;

    Dict();

    Dict(Dict&&) noexcept;
    Dict& operator=(Dict&&) noexcept;

    // Deleted to prevent accidental copying.
    Dict(const Dict&) = delete;
    Dict& operator=(const Dict&) = delete;

    // Takes move_iterators iterators that return std::pair<std::string, Value>,
    // and moves their values into a new Dict. Adding all entries at once
    // results in a faster initial sort operation. Takes move iterators to avoid
    // having to clone the input.
    template <class IteratorType>
    explicit Dict(std::move_iterator<IteratorType> first,
                  std::move_iterator<IteratorType> last) {
      // Need to move into a vector first, since `storage_` currently uses
      // unique_ptrs.
      std::vector<std::pair<std::string, std::unique_ptr<Value>>> values;
      for (auto current = first; current != last; ++current) {
        // With move iterators, no need to call Clone(), but do need to move
        // to a temporary first, as accessing either field individually will
        // directly from the iterator will delete the other field.
        auto value = *current;
        values.emplace_back(std::move(value.first),
                            std::make_unique<Value>(std::move(value.second)));
      }
      storage_ =
          flat_map<std::string, std::unique_ptr<Value>>(std::move(values));
    }

    ~Dict();

    // TODO(dcheng): Probably need to allow construction from a pair of
    // iterators for now due to the prevalence of DictStorage.

    // Returns true if there are no entries in this dictionary and false
    // otherwise.
    bool empty() const;

    // Returns the number of entries in this dictionary.
    size_t size() const;

    // Returns an iterator to the first entry in this dictionary.
    iterator begin();
    const_iterator begin() const;
    const_iterator cbegin() const;

    // Returns an iterator following the last entry in this dictionary. May not
    // be dereferenced.
    iterator end();
    const_iterator end() const;
    const_iterator cend() const;

    // Returns true if `key` is an entry in this dictionary.
    bool contains(base::StringPiece key) const;

    // Removes all entries from this dictionary.
    void clear();

    // Removes the entry referenced by `pos` in this dictionary and returns an
    // iterator to the entry following the removed entry.
    iterator erase(iterator pos);
    iterator erase(const_iterator pos);

    // Creates a deep copy of this dictionary.
    Dict Clone() const;

    // Merges the entries from `dict` into this dictionary. If an entry with the
    // same key exists in this dictionary and `dict`:
    // - if both entries are dictionaries, they will be recursively merged
    // - otherwise, the already-existing entry in this dictionary will be
    //   overwritten with the entry from `dict`.
    void Merge(Dict dict);

    // Finds the entry corresponding to `key` in this dictionary. Returns
    // nullptr if there is no such entry.
    const Value* Find(StringPiece key) const;
    Value* Find(StringPiece key);

    // Similar to `Find()` above, but returns `absl::nullopt`/`nullptr` if the
    // type of the entry does not match. `bool`, `int`, and `double` are
    // returned in a wrapped `absl::optional`; blobs, `Value::Dict`, and
    // `Value::List` are returned by pointer.
    absl::optional<bool> FindBool(StringPiece key) const;
    absl::optional<int> FindInt(StringPiece key) const;
    // Returns a non-null value for both `Value::Type::DOUBLE` and
    // `Value::Type::INT`, converting the latter to a double.
    absl::optional<double> FindDouble(StringPiece key) const;
    const std::string* FindString(StringPiece key) const;
    std::string* FindString(StringPiece key);
    const BlobStorage* FindBlob(StringPiece key) const;
    const Dict* FindDict(StringPiece key) const;
    Dict* FindDict(StringPiece key);
    const List* FindList(StringPiece key) const;
    List* FindList(StringPiece key);

    // Sets an entry with `key` and `value` in this dictionary, overwriting any
    // existing entry with the same `key`. Returns a pointer to the set `value`.
    Value* Set(StringPiece key, Value&& value);
    Value* Set(StringPiece key, bool value);
    template <typename T>
    Value* Set(StringPiece, const T*) = delete;
    Value* Set(StringPiece key, int value);
    Value* Set(StringPiece key, double value);
    Value* Set(StringPiece key, StringPiece value);
    Value* Set(StringPiece key, StringPiece16 value);
    Value* Set(StringPiece key, const char* value);
    Value* Set(StringPiece key, const char16_t* value);
    Value* Set(StringPiece key, std::string&& value);
    Value* Set(StringPiece key, BlobStorage&& value);
    Value* Set(StringPiece key, Dict&& value);
    Value* Set(StringPiece key, List&& value);

    // Removes the entry corresponding to `key` from this dictionary. Returns
    // true if an entry was removed or false otherwise.
    bool Remove(StringPiece key);

    // Similar to `Remove()`, but returns the value corresponding to the removed
    // entry or `absl::nullopt` otherwise.
    absl::optional<Value> Extract(StringPiece key);

    // Equivalent to the above methods but operating on paths instead of keys.
    // A path is shorthand syntax for referring to a key nested inside
    // intermediate dictionaries, with components delimited by ".". Paths may
    // not be empty.
    //
    // Prefer the non-path methods above when possible. Paths that have only one
    // component (i.e. no dots in the path) should never use the path-based
    // methods.
    //
    // Originally, the path-based APIs were the only way of specifying a key, so
    // there are likely to be many legacy (and unnecessary) uses of the path
    // APIs that do not actually require traversing nested dictionaries.
    const Value* FindByDottedPath(StringPiece path) const;
    Value* FindByDottedPath(StringPiece path);

    absl::optional<bool> FindBoolByDottedPath(StringPiece path) const;
    absl::optional<int> FindIntByDottedPath(StringPiece path) const;
    // Returns a non-null value for both `Value::Type::DOUBLE` and
    // `Value::Type::INT`, converting the latter to a double.
    absl::optional<double> FindDoubleByDottedPath(StringPiece path) const;
    const std::string* FindStringByDottedPath(StringPiece path) const;
    std::string* FindStringByDottedPath(StringPiece path);
    const BlobStorage* FindBlobByDottedPath(StringPiece path) const;
    const Dict* FindDictByDottedPath(StringPiece path) const;
    Dict* FindDictByDottedPath(StringPiece path);
    const List* FindListByDottedPath(StringPiece path) const;
    List* FindListByDottedPath(StringPiece path);

    // Creates a new entry with a dictionary for any non-last component that is
    // missing an entry while performing the path traversal. Will fail if any
    // non-last component of the path refers to an already-existing entry that
    // is not a dictionary. Returns `nullptr` on failure.
    Value* SetByDottedPath(StringPiece path, Value&& value);
    Value* SetByDottedPath(StringPiece path, bool value);
    template <typename T>
    Value* SetByDottedPath(StringPiece, const T*) = delete;
    Value* SetByDottedPath(StringPiece path, int value);
    Value* SetByDottedPath(StringPiece path, double value);
    Value* SetByDottedPath(StringPiece path, StringPiece value);
    Value* SetByDottedPath(StringPiece path, StringPiece16 value);
    Value* SetByDottedPath(StringPiece path, const char* value);
    Value* SetByDottedPath(StringPiece path, const char16_t* value);
    Value* SetByDottedPath(StringPiece path, std::string&& value);
    Value* SetByDottedPath(StringPiece path, BlobStorage&& value);
    Value* SetByDottedPath(StringPiece path, Dict&& value);
    Value* SetByDottedPath(StringPiece path, List&& value);

    bool RemoveByDottedPath(StringPiece path);

    absl::optional<Value> ExtractByDottedPath(StringPiece path);

    // Serializes to a string for logging and debug purposes.
    std::string DebugString() const;

#if BUILDFLAG(ENABLE_BASE_TRACING)
    // Write this object into a trace.
    void WriteIntoTrace(perfetto::TracedValue) const;
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

   private:
    BASE_EXPORT friend bool operator==(const Dict& lhs, const Dict& rhs);
    BASE_EXPORT friend bool operator!=(const Dict& lhs, const Dict& rhs);
    BASE_EXPORT friend bool operator<(const Dict& lhs, const Dict& rhs);
    BASE_EXPORT friend bool operator>(const Dict& lhs, const Dict& rhs);
    BASE_EXPORT friend bool operator<=(const Dict& lhs, const Dict& rhs);
    BASE_EXPORT friend bool operator>=(const Dict& lhs, const Dict& rhs);

    // For legacy access to the internal storage type.
    friend Value;

    explicit Dict(const flat_map<std::string, std::unique_ptr<Value>>& storage);

    flat_map<std::string, std::unique_ptr<Value>> storage_;
  };

  // Represents a list of Values.
  class BASE_EXPORT GSL_OWNER List {
   public:
    using iterator = CheckedContiguousIterator<Value>;
    using const_iterator = CheckedContiguousConstIterator<Value>;
    using value_type = Value;

    List();

    List(List&&) noexcept;
    List& operator=(List&&) noexcept;

    // Deleted to prevent accidental copying.
    List(const List&) = delete;
    List& operator=(const List&) = delete;

    ~List();

    // TODO(dcheng): Probably need to allow construction from a pair of
    // iterators for now due to the prevalence of ListStorage now.

    // Returns true if there are no values in this list and false otherwise.
    bool empty() const;

    // Returns the number of values in this list.
    size_t size() const;

    // Returns an iterator to the first value in this list.
    iterator begin();
    const_iterator begin() const;
    const_iterator cbegin() const;

    // Returns an iterator following the last value in this list. May not be
    // dereferenced.
    iterator end();
    const_iterator end() const;
    const_iterator cend() const;

    // Returns a reference to the first value in the container. Fails with
    // `CHECK()` if the list is empty.
    const Value& front() const;
    Value& front();

    // Returns a reference to the last value in the container. Fails with
    // `CHECK()` if the list is empty.
    const Value& back() const;
    Value& back();

    // Increase the capacity of the backing container, but does not change
    // the size. Assume all existing iterators will be invalidated.
    void reserve(size_t capacity);

    // Returns a reference to the value at `index` in this list. Fails with a
    // `CHECK()` if `index >= size()`.
    const Value& operator[](size_t index) const;
    Value& operator[](size_t index);

    // Removes all value from this list.
    void clear();

    // Removes the value referenced by `pos` in this list and returns an
    // iterator to the value following the removed value.
    iterator erase(iterator pos);
    const_iterator erase(const_iterator pos);

    // Remove the values in the range [`first`, `last`). Returns iterator to the
    // first value following the removed range, which is `last`. If `first` ==
    // `last`, removes nothing and returns `last`.
    iterator erase(iterator first, iterator last);
    const_iterator erase(const_iterator first, const_iterator last);

    // Creates a deep copy of this dictionary.
    List Clone() const;

    // Appends `value` to the end of this list.
    void Append(Value&& value);
    void Append(bool value);
    template <typename T>
    void Append(const T*) = delete;
    void Append(int value);
    void Append(double value);
    void Append(StringPiece value);
    void Append(StringPiece16 value);
    void Append(const char* value);
    void Append(const char16_t* value);
    void Append(std::string&& value);
    void Append(BlobStorage&& value);
    void Append(Dict&& value);
    void Append(List&& value);

    // Inserts `value` before `pos` in this list. Returns an iterator to the
    // inserted value.
    // TODO(dcheng): Should this provide the same set of overloads that Append()
    // does?
    iterator Insert(const_iterator pos, Value&& value);

    // Erases all values equal to `value` from this list.
    size_t EraseValue(const Value& value);

    // Erases all values for which `predicate` evaluates to true from this list.
    template <typename Predicate>
    size_t EraseIf(Predicate predicate) {
      return base::EraseIf(storage_, predicate);
    }

    // Serializes to a string for logging and debug purposes.
    std::string DebugString() const;

#if BUILDFLAG(ENABLE_BASE_TRACING)
    // Write this object into a trace.
    void WriteIntoTrace(perfetto::TracedValue) const;
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

   private:
    BASE_EXPORT friend bool operator==(const List& lhs, const List& rhs);
    BASE_EXPORT friend bool operator!=(const List& lhs, const List& rhs);
    BASE_EXPORT friend bool operator<(const List& lhs, const List& rhs);
    BASE_EXPORT friend bool operator>(const List& lhs, const List& rhs);
    BASE_EXPORT friend bool operator<=(const List& lhs, const List& rhs);
    BASE_EXPORT friend bool operator>=(const List& lhs, const List& rhs);

    // For legacy access to the internal storage type.
    friend Value;

    explicit List(const std::vector<Value>& storage);

    std::vector<Value> storage_;
  };

  // ===== DEPRECATED methods that require `type() == Type::LIST` =====

  // Returns the Values in a list as a view. The mutable overload allows for
  // modification of the underlying values, but does not allow changing the
  // structure of the list.
  //
  // DEPRECATED: prefer direct use `base::Value::List` where possible, or
  // `GetList()` otherwise.
  DeprecatedListView GetListDeprecated();
  DeprecatedConstListView GetListDeprecated() const;

  // Appends `value` to the end of the list.
  //
  // DEPRECATED: prefer `Value::List::Append()`.
  void Append(Value&& value);
  // DEPRECATED: prefer `Value::List::Append()`.
  void Append(bool value);
  template <typename T>
  void Append(const T* ptr) = delete;
  // DEPRECATED: prefer `Value::List::Append()`.
  void Append(int value);
  // DEPRECATED: prefer `Value::List::Append()`.
  void Append(double value);
  // DEPRECATED: prefer `Value::List::Append()`.
  void Append(StringPiece value);
  // DEPRECATED: prefer `Value::List::Append()`.
  void Append(StringPiece16 value);
  // DEPRECATED: prefer `Value::List::Append()`.
  void Append(const char* value);
  // DEPRECATED: prefer `Value::List::Append()`.
  void Append(std::string&& value);

  // Inserts `value` before `pos`.
  //
  // DEPRECATED: prefer `Value::List::Insert()`.
  CheckedContiguousIterator<Value> Insert(
      CheckedContiguousConstIterator<Value> pos,
      Value&& value);

  // Erases the Value pointed to by `iter`. Returns false if `iter` is out of
  // bounds.
  //
  // DEPRECATED: prefer `Value::List::erase(iter)`.
  bool EraseListIter(CheckedContiguousConstIterator<Value> iter);

  // Erases all Values that compare equal to `val`. Returns the number of
  // deleted Values.
  //
  // DEPRECATED: prefer `Value::List::EraseValue(val)`.
  size_t EraseListValue(const Value& val);

  // Erases all Values for which `pred` returns true. Returns the number of
  // deleted Values.
  //
  // DEPRECATED: prefer `Value::List::EraseIf(pred)`.
  template <typename Predicate>
  size_t EraseListValueIf(Predicate pred) {
    return base::EraseIf(list(), pred);
  }

  // Erases all Values from the list.
  //
  // DEPRECATED: prefer `Value::List::clear()`.
  void ClearList();

  // ===== DEPRECATED methods that require `type() == Type::DICT` =====

  // `FindKey` looks up `key` in the underlying dictionary. If found, it returns
  // a pointer to the element. Otherwise it returns nullptr.
  //
  // DEPRECATED: prefer `Value::Dict::Find()`.
  Value* FindKey(StringPiece key);
  const Value* FindKey(StringPiece key) const;

  // `FindKeyOfType` is similar to `FindKey`, but it also requires the found
  // value to have type `type`. If no type is found, or the found value is of a
  // different type nullptr is returned.
  //
  // DEPRECATED: prefer `Value::Dict::FindBool()`, `Value::Dict::FindInt()`, et
  // cetera.
  Value* FindKeyOfType(StringPiece key, Type type);
  const Value* FindKeyOfType(StringPiece key, Type type) const;

  // These are convenience forms of `FindKey`. They return `absl::nullopt` or
  // `nullptr` if the value is not found or doesn't have the type specified in
  // the function's name.
  //
  // DEPRECATED: prefer `Value::Dict::FindBool()`.
  absl::optional<bool> FindBoolKey(StringPiece key) const;
  // DEPRECATED: prefer `Value::Dict::FindInt()`.
  absl::optional<int> FindIntKey(StringPiece key) const;
  // Returns a non-null value for both `Value::Type::DOUBLE` and
  // `Value::Type::INT`, converting the latter to a double.
  //
  // DEPRECATED: prefer `Value::Dict::FindDouble()`.
  absl::optional<double> FindDoubleKey(StringPiece key) const;
  // DEPRECATED: prefer `Value::Dict::FindString()`.
  const std::string* FindStringKey(StringPiece key) const;
  std::string* FindStringKey(StringPiece key);
  // DEPRECATED: prefer `Value::Dict::FindBlob()`.
  const BlobStorage* FindBlobKey(StringPiece key) const;
  // DEPRECATED: prefer `Value::Dict::FindDict()`.
  const Value* FindDictKey(StringPiece key) const;
  Value* FindDictKey(StringPiece key);
  // DEPRECATED: prefer `Value::Dict::FindList()`.
  const Value* FindListKey(StringPiece key) const;
  Value* FindListKey(StringPiece key);

  // `SetKey` looks up `key` in the underlying dictionary and sets the mapped
  // value to `value`. If `key` could not be found, a new element is inserted.
  // A pointer to the modified item is returned.
  //
  // Note: Prefer `Set<Type>Key()` if the input is not already a `Value`.
  //
  // DEPRECATED: Prefer `Value::Dict::Set()`.
  Value* SetKey(StringPiece key, Value&& value);

  // `Set`Type>Key` looks up `key` in the underlying dictionary and associates a
  // corresponding Value() constructed from the second parameter. Compared to
  // `SetKey()`, this avoids un-necessary temporary `Value()` creation, as well
  // ambiguities in the value type.
  //
  // DEPRECATED: Prefer `Value::Dict::Set()`.
  Value* SetBoolKey(StringPiece key, bool val);
  // DEPRECATED: Prefer `Value::Dict::Set()`.
  Value* SetIntKey(StringPiece key, int val);
  // DEPRECATED: Prefer `Value::Dict::Set()`.
  Value* SetDoubleKey(StringPiece key, double val);
  // DEPRECATED: Prefer `Value::Dict::Set()`.
  Value* SetStringKey(StringPiece key, StringPiece val);
  // DEPRECATED: Prefer `Value::Dict::Set()`.
  Value* SetStringKey(StringPiece key, StringPiece16 val);
  // DEPRECATED: Prefer `Value::Dict::Set()`.
  Value* SetStringKey(StringPiece key, const char* val);
  // DEPRECATED: Prefer `Value::Dict::Set()`.
  Value* SetStringKey(StringPiece key, std::string&& val);

  // This attempts to remove the value associated with `key`. In case of
  // failure, e.g. the key does not exist, false is returned and the underlying
  // dictionary is not changed. In case of success, `key` is deleted from the
  // dictionary and the method returns true.
  //
  // Deprecated: Prefer `Value::Dict::Remove()`.
  bool RemoveKey(StringPiece key);

  // This attempts to extract the value associated with `key`. In case of
  // failure, e.g. the key does not exist, nullopt is returned and the
  // underlying dictionary is not changed. In case of success, `key` is deleted
  // from the dictionary and the method returns the extracted Value.
  //
  // DEPRECATED: Prefer `Value::Dict::Extract()`.
  absl::optional<Value> ExtractKey(StringPiece key);

  // Searches a hierarchy of dictionary values for a given value. If a path
  // of dictionaries exist, returns the item at that path. If any of the path
  // components do not exist or if any but the last path components are not
  // dictionaries, returns nullptr. The type of the leaf Value is not checked.
  //
  // This version takes a StringPiece for the path, using dots as separators.
  //
  // DEPRECATED: Prefer `Value::Dict::FindByDottedPath()`.
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
  //
  // DEPRECATED: These are not common, and there is no currently planned
  // replacement.
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
  //
  // DEPRECATED: Use `Value::Dict::FindBoolByDottedPath()`,
  // `Value::Dict::FindIntByDottedPath()`, et cetera.
  Value* FindPathOfType(StringPiece path, Type type);
  const Value* FindPathOfType(StringPiece path, Type type) const;

  // Convenience accessors used when the expected type of a value is known.
  // Similar to Find<Type>Key() but accepts paths instead of keys.
  //
  // DEPRECATED: Use `Value::Dict::FindBoolByDottedPath()`, or
  // `Value::Dict::FindBool()` if the path only has one component, i.e. has no
  // dots.
  absl::optional<bool> FindBoolPath(StringPiece path) const;
  // DEPRECATED: Use `Value::Dict::FindIntByDottedPath()`, or
  // `Value::Dict::FindInt()` if the path only has one component, i.e. has no
  // dots.
  absl::optional<int> FindIntPath(StringPiece path) const;
  // DEPRECATED: Use `Value::Dict::FindDoubleByDottedPath()`, or
  // `Value::Dict::FindDouble()` if the path only has one component, i.e. has no
  // dots.
  absl::optional<double> FindDoublePath(StringPiece path) const;
  // DEPRECATED: Use `Value::Dict::FindStringByDottedPath()`, or
  // `Value::Dict::FindString()` if the path only has one component, i.e. has no
  // dots.
  const std::string* FindStringPath(StringPiece path) const;
  std::string* FindStringPath(StringPiece path);
  // DEPRECATED: Use `Value::Dict::FindDictByDottedPath()`, or
  // `Value::Dict::FindDict()` if the path only has one component, i.e. has no
  // dots.
  Value* FindDictPath(StringPiece path);
  const Value* FindDictPath(StringPiece path) const;
  // DEPRECATED: Use `Value::Dict::FindListByDottedPath()`, or
  // `Value::Dict::FindList()` if the path only has one component, i.e. has no
  // dots.
  Value* FindListPath(StringPiece path);
  const Value* FindListPath(StringPiece path) const;

  // The following forms are deprecated too, use the ones that take the path
  // as a single StringPiece instead.
  //
  // DEPRECATED: These are not common, and there is no currently planned
  // replacement.
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
  // Note: If there is only one component in the path, use `SetKey()` instead.
  // Note: Using `Set<Type>Path()` might be more convenient and efficient.
  //
  // DEPRECATED: Use `Value::Dict::SetByDottedPath()`.
  Value* SetPath(StringPiece path, Value&& value);

  // These setters are more convenient and efficient than the corresponding
  // SetPath(...) call.
  //
  // DEPRECATED: Use `Value::Dict::SetByDottedPath()`.
  Value* SetBoolPath(StringPiece path, bool value);
  // DEPRECATED: Use `Value::Dict::SetByDottedPath()`.
  Value* SetIntPath(StringPiece path, int value);
  // DEPRECATED: Use `Value::Dict::SetByDottedPath()`.
  Value* SetDoublePath(StringPiece path, double value);
  // DEPRECATED: Use `Value::Dict::SetByDottedPath()`.
  Value* SetStringPath(StringPiece path, StringPiece value);
  // DEPRECATED: Use `Value::Dict::SetByDottedPath()`.
  Value* SetStringPath(StringPiece path, const char* value);
  // DEPRECATED: Use `Value::Dict::SetByDottedPath()`.
  Value* SetStringPath(StringPiece path, std::string&& value);
  // DEPRECATED: Use `Value::Dict::SetByDottedPath()`.
  Value* SetStringPath(StringPiece path, StringPiece16 value);

  // DEPRECATED: Use `Value::Dict::SetByDottedPath()`.
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
  // DEPRECATED: Use `Value::Dict::RemoveByDottedPath()`.
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
  // DEPRECATED: Use `Value::Dict::ExtractByDottedPath()`.
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
  // DEPRECATED: Use a range-based for loop over `base::Value::Dict` directly
  // instead.
  dict_iterator_proxy DictItems();
  const_dict_iterator_proxy DictItems() const;

  // DEPRECATED: prefer `Value::Dict::size()`.
  size_t DictSize() const;

  // DEPRECATED: prefer `Value::Dict::empty()`.
  bool DictEmpty() const;

  // DEPRECATED: prefer `Value::Dict::clear()`.
  void DictClear();

  // Merge `dictionary` into this value. This is done recursively, i.e. any
  // sub-dictionaries will be merged as well. In case of key collisions, the
  // passed in dictionary takes precedence and data already present will be
  // replaced. Values within `dictionary` are deep-copied, so `dictionary` may
  // be freed any time after this call.
  // Note: This requires that `type()` and `dictionary->type()` is
  // Type::DICT.
  //
  // DEPRECATED: prefer `Value::Dict::Merge()`.
  void MergeDictionary(const Value* dictionary);

  // These methods allow the convenient retrieval of the contents of the Value.
  // If the current object can be converted into the given type, the value is
  // returned through the `out_value` parameter and true is returned;
  // otherwise, false is returned and `out_value` is unchanged.
  // DictionaryValue::From is the equivalent for std::unique_ptr conversions.
  //
  // DEPRECATED: prefer direct use `base::Value::Dict` where possible, or
  // `GetIfDict()` otherwise.
  bool GetAsDictionary(DictionaryValue** out_value);
  bool GetAsDictionary(const DictionaryValue** out_value) const;
  // Note: Do not add more types. See the file-level comment above for why.

  // This creates a deep copy of the entire Value tree, and returns a pointer
  // to the copy. The caller gets ownership of the copy, of course.
  // Subclasses return their own type directly in their overrides;
  // this works because C++ supports covariant return types.
  // TODO(crbug.com/646113): Delete this and migrate callsites.
  //
  // DEPRECATED: prefer `Value::Clone()`.
  std::unique_ptr<Value> CreateDeepCopy() const;

  // Comparison operators so that Values can easily be used with standard
  // library algorithms and associative containers.
  BASE_EXPORT friend bool operator==(const Value& lhs, const Value& rhs);
  BASE_EXPORT friend bool operator!=(const Value& lhs, const Value& rhs);
  BASE_EXPORT friend bool operator<(const Value& lhs, const Value& rhs);
  BASE_EXPORT friend bool operator>(const Value& lhs, const Value& rhs);
  BASE_EXPORT friend bool operator<=(const Value& lhs, const Value& rhs);
  BASE_EXPORT friend bool operator>=(const Value& lhs, const Value& rhs);

  BASE_EXPORT friend bool operator==(const Value& lhs, bool rhs);
  friend bool operator==(bool lhs, const Value& rhs) { return rhs == lhs; }
  friend bool operator!=(const Value& lhs, bool rhs) { return !(lhs == rhs); }
  friend bool operator!=(bool lhs, const Value& rhs) { return !(lhs == rhs); }
  template <typename T>
  friend bool operator==(const Value& lhs, const T* rhs) = delete;
  template <typename T>
  friend bool operator==(const T* lhs, const Value& rhs) = delete;
  template <typename T>
  friend bool operator!=(const Value& lhs, const T* rhs) = delete;
  template <typename T>
  friend bool operator!=(const T* lhs, const Value& rhs) = delete;
  BASE_EXPORT friend bool operator==(const Value& lhs, int rhs);
  friend bool operator==(int lhs, const Value& rhs) { return rhs == lhs; }
  friend bool operator!=(const Value& lhs, int rhs) { return !(lhs == rhs); }
  friend bool operator!=(int lhs, const Value& rhs) { return !(lhs == rhs); }
  BASE_EXPORT friend bool operator==(const Value& lhs, double rhs);
  friend bool operator==(double lhs, const Value& rhs) { return rhs == lhs; }
  friend bool operator!=(const Value& lhs, double rhs) { return !(lhs == rhs); }
  friend bool operator!=(double lhs, const Value& rhs) { return !(lhs == rhs); }
  // Note: StringPiece16 overload intentionally omitted: Value internally stores
  // strings as UTF-8. While it is possible to implement a comparison operator
  // that would not require first creating a new UTF-8 string from the UTF-16
  // string argument, it is simpler to just not implement it at all for a rare
  // use case.
  BASE_EXPORT friend bool operator==(const Value& lhs, StringPiece rhs);
  friend bool operator==(StringPiece lhs, const Value& rhs) {
    return rhs == lhs;
  }
  friend bool operator!=(const Value& lhs, StringPiece rhs) {
    return !(lhs == rhs);
  }
  friend bool operator!=(StringPiece lhs, const Value& rhs) {
    return !(lhs == rhs);
  }
  friend bool operator==(const Value& lhs, const char* rhs) {
    return lhs == StringPiece(rhs);
  }
  friend bool operator==(const char* lhs, const Value& rhs) {
    return rhs == lhs;
  }
  friend bool operator!=(const Value& lhs, const char* rhs) {
    return !(lhs == rhs);
  }
  friend bool operator!=(const char* lhs, const Value& rhs) {
    return !(lhs == rhs);
  }
  friend bool operator==(const Value& lhs, const std::string& rhs) {
    return lhs == StringPiece(rhs);
  }
  friend bool operator==(const std::string& lhs, const Value& rhs) {
    return rhs == lhs;
  }
  friend bool operator!=(const Value& lhs, const std::string& rhs) {
    return !(lhs == rhs);
  }
  friend bool operator!=(const std::string& lhs, const Value& rhs) {
    return !(lhs == rhs);
  }
  // Note: Blob support intentionally omitted as an experiment for potentially
  // wholly removing Blob support from Value itself in the future.
  BASE_EXPORT friend bool operator==(const Value& lhs, const Value::Dict& rhs);
  friend bool operator==(const Value::Dict& lhs, const Value& rhs) {
    return rhs == lhs;
  }
  friend bool operator!=(const Value& lhs, const Value::Dict& rhs) {
    return !(lhs == rhs);
  }
  friend bool operator!=(const Value::Dict& lhs, const Value& rhs) {
    return !(lhs == rhs);
  }
  BASE_EXPORT friend bool operator==(const Value& lhs, const Value::List& rhs);
  friend bool operator==(const Value::List& lhs, const Value& rhs) {
    return rhs == lhs;
  }
  friend bool operator!=(const Value& lhs, const Value::List& rhs) {
    return !(lhs == rhs);
  }
  friend bool operator!=(const Value::List& lhs, const Value& rhs) {
    return !(lhs == rhs);
  }

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

  template <typename Visitor>
  auto Visit(Visitor&& visitor) const {
    return absl::visit(std::forward<Visitor>(visitor), data_);
  }

 protected:
  // Checked convenience accessors for dict and list.
  const LegacyDictStorage& dict() const { return GetDict().storage_; }
  LegacyDictStorage& dict() { return GetDict().storage_; }
  const ListStorage& list() const { return GetList().storage_; }
  ListStorage& list() { return GetList().storage_; }

  // Internal constructors, allowing the simplify the implementation of Clone().
  explicit Value(const LegacyDictStorage& storage);
  explicit Value(LegacyDictStorage&& storage) noexcept;

 private:
  // For access to DoubleStorage.
  friend class ValueView;

  // Special case for doubles, which are aligned to 8 bytes on some
  // 32-bit architectures. In this case, a simple declaration as a
  // double member would make the whole union 8 byte-aligned, which
  // would also force 4 bytes of wasted padding space before it in
  // the Value layout.
  //
  // To override this, store the value as an array of 32-bit integers, and
  // perform the appropriate bit casts when reading / writing to it.
  class BASE_EXPORT DoubleStorage {
   public:
    explicit DoubleStorage(double v);
    DoubleStorage(const DoubleStorage&) = default;
    DoubleStorage& operator=(const DoubleStorage&) = default;

    // Provide an implicit conversion to double to simplify the use of visitors
    // with `Value::Visit()`. Otherwise, visitors would need a branch for
    // handling `DoubleStorage` like:
    //
    //   value.Visit([] (const auto& member) {
    //     using T = std::decay_t<decltype(member)>;
    //     if constexpr (std::is_same_v<T, Value::DoubleStorage>) {
    //       SomeFunction(double{member});
    //     } else {
    //       SomeFunction(member);
    //     }
    //   });
    operator double() const { return base::bit_cast<double>(v_); }

   private:
    friend bool operator==(const DoubleStorage& lhs, const DoubleStorage& rhs) {
      return double{lhs} == double{rhs};
    }

    friend bool operator!=(const DoubleStorage& lhs, const DoubleStorage& rhs) {
      return !(lhs == rhs);
    }

    friend bool operator<(const DoubleStorage& lhs, const DoubleStorage& rhs) {
      return double{lhs} < double{rhs};
    }

    friend bool operator>(const DoubleStorage& lhs, const DoubleStorage& rhs) {
      return rhs < lhs;
    }

    friend bool operator<=(const DoubleStorage& lhs, const DoubleStorage& rhs) {
      return !(rhs < lhs);
    }

    friend bool operator>=(const DoubleStorage& lhs, const DoubleStorage& rhs) {
      return !(lhs < rhs);
    }

    alignas(4) std::array<char, sizeof(double)> v_;
  };

  // Internal constructors, allowing the simplify the implementation of Clone().
  explicit Value(absl::monostate);
  explicit Value(DoubleStorage storage);

  absl::variant<absl::monostate,
                bool,
                int,
                DoubleStorage,
                std::string,
                BlobStorage,
                Dict,
                List>
      data_;
};

// DictionaryValue provides a key-value dictionary with (optional) "path"
// parsing for recursive access; see the comment at the top of the file. Keys
// are std::string's and should be UTF-8 encoded.
//
// DEPRECATED: prefer `Value::Dict`.
class BASE_EXPORT DictionaryValue : public Value {
 public:
  // Returns `value` if it is a dictionary, nullptr otherwise.
  static std::unique_ptr<DictionaryValue> From(std::unique_ptr<Value> value);

  DictionaryValue();
  explicit DictionaryValue(const LegacyDictStorage& in_dict);
  explicit DictionaryValue(LegacyDictStorage&& in_dict) noexcept;

  // Sets the Value associated with the given path starting from this object.
  // A path has the form "<key>" or "<key>.<key>.[...]", where "." indexes
  // into the next DictionaryValue down.  Obviously, "." can't be used
  // within a key, but there are no other restrictions on keys.
  // If the key at any step of the way doesn't exist, or exists but isn't
  // a DictionaryValue, a new DictionaryValue will be created and attached
  // to the path in that location. `in_value` must be non-null.
  // Returns a pointer to the inserted value.
  //
  // DEPRECATED: prefer `Value::Dict::Set()` (if the path only has one
  // component, i.e. has no dots), or `Value::Dict::SetByDottedPath()`
  // otherwise.
  Value* Set(StringPiece path, std::unique_ptr<Value> in_value);

  // Convenience forms of Set().  These methods will replace any existing
  // value at that path, even if it has a different type.
  //
  // DEPRECATED: prefer `Value::Dict::Set()` (if the path only has one
  // component, i.e. has no dots), or `Value::Dict::SetByDottedPath()`
  // otherwise.
  Value* SetBoolean(StringPiece path, bool in_value);
  // DEPRECATED: prefer `Value::Dict::Set()` (if the path only has one
  // component, i.e. has no dots), or `Value::Dict::SetByDottedPath()`
  // otherwise.
  Value* SetInteger(StringPiece path, int in_value);
  // DEPRECATED: prefer `Value::Dict::Set()` (if the path only has one
  // component, i.e. has no dots), or `Value::Dict::SetByDottedPath()`
  // otherwise.
  Value* SetDouble(StringPiece path, double in_value);
  // DEPRECATED: prefer `Value::Dict::Set()` (if the path only has one
  // component, i.e. has no dots), or `Value::Dict::SetByDottedPath()`
  // otherwise.
  Value* SetString(StringPiece path, StringPiece in_value);
  // DEPRECATED: prefer `Value::Dict::Set()` (if the path only has one
  // component, i.e. has no dots), or `Value::Dict::SetByDottedPath()`
  // otherwise.
  Value* SetString(StringPiece path, const std::u16string& in_value);
  // DEPRECATED: prefer `Value::Dict::Set()` (if the path only has one
  // component, i.e. has no dots), or `Value::Dict::SetByDottedPath()`
  // otherwise.
  ListValue* SetList(StringPiece path, std::unique_ptr<ListValue> in_value);

  // Like Set(), but without special treatment of '.'.  This allows e.g. URLs to
  // be used as paths.
  //
  // DEPRECATED: prefer `Value::Dict::Set()`.
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
  //
  // DEPRECATED: prefer `Value::Dict::Find()` (if the path only has one
  // component, i.e. has no dots), or `Value::Dict::FindByDottedPath()`
  // otherwise.
  bool Get(StringPiece path, const Value** out_value) const;
  bool Get(StringPiece path, Value** out_value);

  // These are convenience forms of `Get()`.  The value will be retrieved
  // and the return value will be true if the path is valid and the value at
  // the end of the path can be returned in the form specified.
  // `out_value` is optional and will only be set if non-NULL.
  //
  // DEPRECATED: prefer `Value::Dict::FindInt()` (if the path only has one
  // component, i.e. has no dots), or `Value::Dict::FindIntByDottedPath()`
  // otherwise.
  bool GetInteger(StringPiece path, int* out_value) const;
  // DEPRECATED: prefer `Value::Dict::FindString()` (if the path only has one
  // component, i.e. has no dots), or `Value::Dict::FindStringByDottedPath()`
  // otherwise.
  bool GetString(StringPiece path, std::string* out_value) const;
  bool GetString(StringPiece path, std::u16string* out_value) const;
  // DEPRECATED: prefer `Value::Dict::FindDict()` (if the path only has one
  // component, i.e. has no dots), or `Value::Dict::FindDictByDottedPath()`
  // otherwise.
  bool GetDictionary(StringPiece path, const DictionaryValue** out_value) const;
  bool GetDictionary(StringPiece path, DictionaryValue** out_value);
  // DEPRECATED: prefer `Value::Dict::FindList()` (if the path only has one
  // component, i.e. has no dots), or `Value::Dict::FindListByDottedPath()`
  // otherwise.
  bool GetList(StringPiece path, const ListValue** out_value) const;
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
  //
  // DEPRECATED: Use a range-based for loop over `base::Value::Dict` directly
  // instead.
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

  // DEPRECATED, use `Value::Dict::Clone()` instead.
  // TODO(crbug.com/646113): Delete this and migrate callsites.
  DictionaryValue* DeepCopy() const;
  // DEPRECATED, use `Value::Dict::Clone()` instead.
  // TODO(crbug.com/646113): Delete this and migrate callsites.
  std::unique_ptr<DictionaryValue> CreateDeepCopy() const;
};

// This type of Value represents a list of other Value values.
//
// DEPRECATED: prefer `base::Value::List`.
class BASE_EXPORT ListValue : public Value {
 public:
  using const_iterator = ListView::const_iterator;
  using iterator = ListView::iterator;

  // Returns `value` if it is a list, nullptr otherwise.
  static std::unique_ptr<ListValue> From(std::unique_ptr<Value> value);

  ListValue();

  // Convenience forms of `Get()`.  Modifies `out_value` (and returns true)
  // only if the index is valid and the Value at that index can be returned
  // in the specified form.
  // `out_value` is optional and will only be set if non-NULL.
  //
  // DEPRECATED: prefer `Value::List::operator[]` + `GetIfDict()`.
  bool GetDictionary(size_t index, const DictionaryValue** out_value) const;
  bool GetDictionary(size_t index, DictionaryValue** out_value);

  // Appends a Value to the end of the list.
  // DEPRECATED: prefer `Value::List::Append()`.
  using Value::Append;
  // DEPRECATED: prefer `Value::List::Append()`. Provided to simplify
  // incremental migration and intentionally only defined on ListValue and not
  // Value.
  void Append(base::Value::Dict in_dict);
  void Append(base::Value::List in_list);

  // Swaps contents with the `other` list.
  //
  // DEPRECATED: prefer `base::Value::List` + `std::swap()`.
  void Swap(ListValue* other);

  // Iteration: Use a range-based for loop over `base::Value::List` directly
  // instead.
};

// Adapter so `Value::Dict` or `Value::List` can be directly passed to JSON
// serialization methods without having to clone the contents and transfer
// ownership of the clone to a `Value` wrapper object.
//
// Like `StringPiece` and `span<T>`, this adapter does NOT retain ownership. Any
// underlying object that is passed by reference (i.e. `std::string`,
// `Value::BlobStorage`, `Value::Dict`, `Value::List`, or `Value`) MUST remain
// live as long as there is a `ValueView` referencing it.
//
// While it might be nice to just use the `absl::variant` type directly, the
// need to use `std::reference_wrapper` makes it clunky. `absl::variant` and
// `std::reference_wrapper` both support implicit construction, but C++ only
// allows at most one user-defined conversion in an implicit conversion
// sequence. If this adapter and its implicit constructors did not exist,
// callers would need to use `std::ref` or `std::cref` to pass `Value::Dict` or
// `Value::List` to a function with a `ValueView` parameter.
class BASE_EXPORT GSL_POINTER ValueView {
 public:
  ValueView() = default;
  ValueView(bool value) : data_view_(value) {}
  template <typename T>
  ValueView(const T*) = delete;
  ValueView(int value) : data_view_(value) {}
  ValueView(double value)
      : data_view_(absl::in_place_type_t<Value::DoubleStorage>(), value) {}
  ValueView(StringPiece value) : data_view_(value) {}
  ValueView(const char* value) : ValueView(StringPiece(value)) {}
  ValueView(const std::string& value) : ValueView(StringPiece(value)) {}
  // Note: UTF-16 is intentionally not supported. ValueView is intended to be a
  // low-cost view abstraction, but Value internally represents strings as
  // UTF-8, so it would not be possible to implement this without allocating an
  // entirely new UTF-8 string.
  ValueView(const Value::BlobStorage& value) : data_view_(value) {}
  ValueView(const Value::Dict& value) : data_view_(value) {}
  ValueView(const Value::List& value) : data_view_(value) {}
  ValueView(const Value& value);

  // This is the only 'getter' method provided as `ValueView` is not intended
  // to be a general replacement of `Value`.
  template <typename Visitor>
  auto Visit(Visitor&& visitor) const {
    return absl::visit(std::forward<Visitor>(visitor), data_view_);
  }

 private:
  using ViewType =
      absl::variant<absl::monostate,
                    bool,
                    int,
                    Value::DoubleStorage,
                    StringPiece,
                    std::reference_wrapper<const Value::BlobStorage>,
                    std::reference_wrapper<const Value::Dict>,
                    std::reference_wrapper<const Value::List>>;

 public:
  using DoubleStorageForTest = Value::DoubleStorage;
  const ViewType& data_view_for_test() const { return data_view_; }

 private:
  ViewType data_view_;
};

// This interface is implemented by classes that know how to serialize
// Value objects.
class BASE_EXPORT ValueSerializer {
 public:
  virtual ~ValueSerializer();

  virtual bool Serialize(ValueView root) = 0;
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
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
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

// Stream operator so Values can be pretty printed by gtest.
BASE_EXPORT std::ostream& operator<<(std::ostream& out, const Value& value);
BASE_EXPORT std::ostream& operator<<(std::ostream& out,
                                     const Value::Dict& dict);
BASE_EXPORT std::ostream& operator<<(std::ostream& out,
                                     const Value::List& list);

// Hints for DictionaryValue and ListValue; otherwise, gtest tends to prefer the
// default template implementation over an upcast to Value.
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
