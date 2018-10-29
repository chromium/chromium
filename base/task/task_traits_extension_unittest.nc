// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/task/task_traits.h"

#include "base/task/test_task_traits_extension.h"

namespace base {

#if defined(NCTEST_TASK_TRAITS_EXTENSION_MULTIPLE_BASE_TRAITS)  // [r"Multiple arguments of the same type were provided to the constructor of TaskTraits."]
constexpr TaskTraits traits = {MayBlock(), MayBlock()};
#elif defined(NCTEST_TASK_TRAITS_EXTENSION_MULTIPLE_EXTENSION_TRAITS)  // [r"Multiple arguments of the same type were provided to the constructor of TaskTraits."]
constexpr TaskTraits traits = {TestExtensionEnumTrait::kB, TestExtensionEnumTrait::kC};
#elif defined(NCTEST_TASK_TRAITS_EXTENSION_INVALID_TYPE)  // [r"no matching constructor for initialization of 'const base::TaskTraits'"]
constexpr TaskTraits traits = {TestExtensionEnumTrait::kB, 123};
#elif defined(NCTEST_TASK_TRAITS_EXTENSION_TOO_MUCH_DATA_FOR_STORAGE)  // [r"no matching constructor for initialization of 'base::TaskTraitsExtensionStorage'"]
constexpr TaskTraitsExtensionStorage TestSerializeTaskTraitsWithTooMuchData() {
  constexpr std::array<uint8_t, TaskTraitsExtensionStorage::kStorageSize + 1>
      data = {};
  return {TaskTraitsExtensionStorage::kFirstEmbedderExtensionId, data};
}
#endif

}  // namespace base
