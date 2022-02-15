// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/values.h"

namespace base {

#if defined(NCTEST_VALUE_CTOR_PTR_DOES_NOT_CONVERT_TO_BOOL)  // [r"fatal error: call to deleted constructor"]

void F() {
  int* ptr = nullptr;
  base::Value v(ptr);
}

#elif defined(NCTEST_DICT_SET_PTR_DOES_NOT_CONVERT_TO_BOOL)  // [r"fatal error: call to deleted member function"]

void F() {
  int* ptr = nullptr;
  base::Value::Dict dict;
  dict.Set("moo", ptr);
}

#elif defined(NCTEST_DICT_SETBYDOTTEDPATH_PTR_DOES_NOT_CONVERT_TO_BOOL)  // [r"fatal error: call to deleted member function"]

void F() {
  int* ptr = nullptr;
  base::Value::Dict dict;
  dict.SetByDottedPath("moo.moo", ptr);
}

#elif defined(NCTEST_LIST_APPEND_PTR_DOES_NOT_CONVERT_TO_BOOL)  // [r"fatal error: call to deleted member function"]

void F() {
  int* ptr = nullptr;
  base::Value::List list;
  list.Append(ptr);
}

#endif

}  // namespace base
