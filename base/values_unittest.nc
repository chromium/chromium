// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/values.h"

#include <stdint.h>

namespace base {

void G(ValueView v) {}

#if defined(NCTEST_VALUE_CTOR_PTR_DOES_NOT_CONVERT_TO_BOOL)  // [r"fatal error: call to deleted constructor"]

void F() {
  int* ptr = nullptr;
  Value v(ptr);
}

#elif defined(NCTEST_DICT_SET_PTR_DOES_NOT_CONVERT_TO_BOOL)  // [r"fatal error: call to deleted member function"]

void F() {
  int* ptr = nullptr;
  Value::Dict dict;
  dict.Set("moo", ptr);
}

#elif defined(NCTEST_DICT_SETBYDOTTEDPATH_PTR_DOES_NOT_CONVERT_TO_BOOL)  // [r"fatal error: call to deleted member function"]

void F() {
  int* ptr = nullptr;
  Value::Dict dict;
  dict.SetByDottedPath("moo.moo", ptr);
}

#elif defined(NCTEST_LIST_APPEND_PTR_DOES_NOT_CONVERT_TO_BOOL)  // [r"fatal error: call to deleted member function"]

void F() {
  int* ptr = nullptr;
  Value::List list;
  list.Append(ptr);
}

#elif defined(NCTEST_VALUE_CTOR_INT64_T)  // [r"fatal error: ambiguous conversion for functional-style cast from 'int64_t' \(aka '.+?'\) to 'Value'"]

Value F(int64_t value) {
  return Value(value);
}

#elif defined(NCTEST_SET_INT64_T)  // [r"fatal error: call to member function 'Set' is ambiguous"]

Value::Dict F(int64_t value) {
  Value::Dict dict;
  dict.Set("あいうえお", value);
  return dict;
}

#elif defined(NCTEST_SETBYDOTTEDPATH_INT64_T)  // [r"fatal error: call to member function 'SetByDottedPath' is ambiguous"]

Value::Dict F(int64_t value) {
  Value::Dict dict;
  dict.SetByDottedPath("あいうえお", value);
  return dict;
}

#elif defined(NCTEST_LIST_APPEND_INT64_T)  // [r"fatal error: call to member function 'Append' is ambiguous"]

Value::List F(int64_t value) {
  Value::List list;
  list.Append(value);
  return list;
}

#elif defined(NCTEST_VALUEVIEW_FROM_CONST_NON_CHAR_POINTER)  // [r"fatal error: conversion function from 'const int \*' to 'ValueView' invokes a deleted function"]

void F() {
  const int* ptr = nullptr;
  ValueView v = ptr;
  G(v);
}

#elif defined(NCTEST_VALUEVIEW_FROM_NON_CHAR_POINTER)  // [r"fatal error: conversion function from 'int \*' to 'ValueView' invokes a deleted function"]

void F() {
  int* ptr = nullptr;
  ValueView v = ptr;
  G(v);
}

#elif defined(NCTEST_VALUEVIEW_FROM_STRING_TEMPORARY)  // [r"fatal error: object backing the pointer will be destroyed at the end of the full-expression"]

void F() {
  ValueView v = std::string();
  G(v);
}

#elif defined(NCTEST_VALUEVIEW_FROM_BLOB_TEMPORARY)  // [r"fatal error: object backing the pointer will be destroyed at the end of the full-expression"]

void F() {
  ValueView v = Value::BlobStorage();
  G(v);
}

#elif defined(NCTEST_VALUEVIEW_FROM_DICT_TEMPORARY)  // [r"fatal error: object backing the pointer will be destroyed at the end of the full-expression"]

void F() {
  ValueView v = Value::Dict();
  G(v);
}

#elif defined(NCTEST_VALUEVIEW_FROM_LIST_TEMPORARY)  // [r"fatal error: object backing the pointer will be destroyed at the end of the full-expression"]

void F() {
  ValueView v = Value::List();
  G(v);
}

#elif defined(NCTEST_VALUEVIEW_FROM_VALUE_TEMPORARY)  // [r"fatal error: object backing the pointer will be destroyed at the end of the full-expression"]

void F() {
  ValueView v = Value();
  G(v);
}

#endif

}  // namespace base
