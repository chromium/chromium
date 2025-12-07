// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_LAZY_STRING_BUILDER_H_
#define BASE_STRINGS_LAZY_STRING_BUILDER_H_

#include <stddef.h>

#include <initializer_list>
#include <list>
#include <string>
#include <string_view>
#include <type_traits>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/memory/stack_allocated.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

// Predeclare classes that will have access to LazyStringBuilder.
namespace url {
class Origin;
class SchemeHostPort;
}  // namespace url

namespace net {
class HttpCache;
class NetworkIsolationKey;
class SchemefulSite;
}  // namespace net

namespace base {

// A string builder that avoids copying the string contents in the normal case.
// This means it is unsafe to use after the underlying string has been deleted.
// There is an AppendByValue() method for when a runtime-allocated string needs
// to be included in the output, but this class should not be used when that is
// the normal case.
//
// In most cases, base::StrCat() should be used directly in preference to this
// class. LazyStringBuilder should only be used when multiple modules need
// to coordinate to assemble a string, and it is critical to reduce the number
// of allocations. For this reason, its use is restricted to the set of classes
// explicitly listed in LazyStringBuilder::AccessKey.
class BASE_EXPORT LazyStringBuilder {
  // Prevent objects being allocated on the heap to reduce the risk of
  // use-after-free bugs.
  STACK_ALLOCATED();

 public:
  class AccessKey {
   public:
    ~AccessKey() = default;

   private:
    // Tests should not be added to this list, but use the CreateForTesting()
    // static method instead.
    friend LazyStringBuilder;
    friend url::Origin;
    friend url::SchemeHostPort;
    friend net::SchemefulSite;
    friend net::NetworkIsolationKey;
    friend net::HttpCache;

    AccessKey() = default;
    AccessKey(const AccessKey&) = default;
    AccessKey& operator=(const AccessKey&) = default;
  };

  // Tests should acquire a LazyStringBuilder like
  //   auto builder = LazyStringBuilder::CreateForTesting();
  // This should never be called in production code.
  static LazyStringBuilder CreateForTesting();

  // Construct an empty object.
  explicit LazyStringBuilder(AccessKey);

  ~LazyStringBuilder();

  // Since `views_` can point into `scratch_`, it is not trivial to copy this
  // type.
  LazyStringBuilder(const LazyStringBuilder&) = delete;
  LazyStringBuilder& operator=(const LazyStringBuilder&) = delete;

  // LazyStringBuilder needs to be movable for CreateForTesting() to work.
  LazyStringBuilder(LazyStringBuilder&&);
  LazyStringBuilder& operator=(LazyStringBuilder&&);

  // Appends a single std::string_view to the output string. The contents of
  // `view` are referenced, not copied. Can be used with std::strings as long as
  // they outlive this object.
  void AppendByReference(std::string_view view LIFETIME_CAPTURE_BY(this));

  // Prevent AppendByReference() from being called for rvalue strings, as it
  // would result in a dangling reference. Use AppendByValue() instead.
  void AppendByReference(std::string&&) = delete;

  // This overload is needed to disambiguate calls to AppendByReference() with
  // string constants.
  void AppendByReference(const char* str LIFETIME_CAPTURE_BY(this)) {
    AppendByReference(std::string_view(str));
  }

  // Moves `string` into this object. Memory-safe but inefficient, so usage
  // should be rare.
  void AppendByValue(std::string string);

  // Appends multiple string_views to the output string, eg.
  //   string_view_joiner.AppendByReference(view1, " ", view2);
  //
  // This is more efficient than appending them one at a time. This cannot be
  // used for runtime-allocated strings; they must use AppendByValue().
  template <typename... T>
    requires(sizeof...(T) > 1 &&
             (std::convertible_to<T, std::string_view> && ...) &&
             (!std::same_as<T, std::string> && ...))
  void AppendByReference(T&&... views LIFETIME_CAPTURE_BY(this)) {
    AppendInternal({std::string_view(views)...});
  }

  // Copies and returns the output string.
  std::string Build() const;

 private:
  // This is private because the public API makes it easier to check that no-one
  // is passing in rvalue strings, but we want to make the actual implementation
  // out-of-line to avoid exploding binary size.
  void AppendInternal(std::initializer_list<std::string_view> views);

  // The views that have been appended to the object. Heap memory need not be
  // allocated unless there are more than 32.
  absl::InlinedVector<std::string_view, 32u> views_;

  // Any strings that have been passed to AppendByValue(). These are referenced
  // by `views_`. std::list rather than vector because pointer stability is
  // required.
  std::list<std::string> scratch_;
};

}  // namespace base

#endif  // BASE_STRINGS_LAZY_STRING_BUILDER_H_
