// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/values.h"

#include <stdint.h>

namespace base {

// Trying to construct a Value from a pointer should not work or implicitly
// convert to bool.
void DisallowValueConstructionFromPointers() {
  int* ptr = nullptr;

  {
    Value v(ptr);  // expected-error {{call to deleted constructor of 'Value'}}
  }

  {
    Value::Dict dict;
    dict.Set("moo", ptr);  // expected-error {{call to deleted member function 'Set'}}
    dict.SetByDottedPath("moo.moo", ptr);  // expected-error {{call to deleted member function 'SetByDottedPath'}}

    Value::Dict().Set("moo", ptr);  // expected-error {{call to deleted member function 'Set'}}
    Value::Dict().SetByDottedPath("moo", ptr);  // expected-error {{call to deleted member function 'SetByDottedPath'}}
  }

  {
    Value::List list;
    list.Append(ptr);  // expected-error {{call to deleted member function 'Append'}}

    Value::List().Append(ptr);  // expected-error {{call to deleted member function 'Append'}}
  }
}

// Value (largely) follows the semantics of JSON, which does not support 64-bit
// integers. Constructing a Value from a 64-bit integer should not work.

void DisallowValueConstructionFromInt64() {
  int64_t big_int = 0;

  {
    Value v(big_int);  // expected-error {{call to constructor of 'Value' is ambiguous}}
  }

  {
    Value::Dict dict;
    dict.Set("あいうえお", big_int);  // expected-error {{call to member function 'Set' is ambiguous}}
    dict.SetByDottedPath("あいうえお", big_int);  // expected-error {{call to member function 'SetByDottedPath' is ambiguous}}

    Value::Dict().Set("あいうえお", big_int);  // expected-error {{call to member function 'Set' is ambiguous}}
    Value::Dict().SetByDottedPath("あいうえお", big_int);  // expected-error {{call to member function 'SetByDottedPath' is ambiguous}}
  }

  {
    Value::List list;
    list.Append(big_int);  // expected-error {{call to member function 'Append' is ambiguous}}

    Value::List().Append(big_int);  // expected-error {{call to member function 'Append' is ambiguous}}
  }
}

void TakesValueView(ValueView v) {}

// Trying to construct a ValueView from a pointer should not work or implicitly
// convert to bool.
void DisallowValueViewConstructionFromPointers() {
  int* ptr = nullptr;

  ValueView v(ptr);  // expected-error {{call to deleted constructor of 'ValueView'}}
  TakesValueView(ptr);  // expected-error {{conversion function from 'int *' to 'ValueView' invokes a deleted function}}
}

// Verify that obvious ways of unsafely constructing a ValueView from a
// temporary are disallowed.
void DisallowValueViewConstructionFromTemporaryString() {
  [[maybe_unused]] ValueView v = std::string();  // expected-error {{object backing the pointer will be destroyed at the end of the full-expression}}
  // Not an error here since the lifetime of the temporary lasts until the end
  // of the full expression, i.e. until TakesValueView() returns.
  TakesValueView(std::string());
}

void DisallowValueViewConstructionFromTemporaryBlob() {
  [[maybe_unused]] ValueView v = Value::BlobStorage();  // expected-error {{object backing the pointer will be destroyed at the end of the full-expression}}
  // Not an error here since the lifetime of the temporary lasts until the end
  // of the full expression, i.e. until TakesValueView() returns.
  TakesValueView(Value::BlobStorage());
}

void DisallowValueViewConstructionFromTemporaryDict() {
  [[maybe_unused]] ValueView v = Value::Dict();  // expected-error {{object backing the pointer will be destroyed at the end of the full-expression}}
  // Not an error here since the lifetime of the temporary lasts until the end
  // of the full expression, i.e. until TakesValueView() returns.
  TakesValueView(Value::Dict());
}

void DisallowValueViewConstructionFromTemporaryList() {
  [[maybe_unused]] ValueView v = Value::List();  // expected-error {{object backing the pointer will be destroyed at the end of the full-expression}}
  // Not an error here since the lifetime of the temporary lasts until the end
  // of the full expression, i.e. until TakesValueView() returns.
  TakesValueView(Value::List());
}

void DisallowValueViewConstructionFromTemporaryValue() {
  [[maybe_unused]] ValueView v = Value();  // expected-error {{object backing the pointer will be destroyed at the end of the full-expression}}
  // Not an error here since the lifetime of the temporary lasts until the end
  // of the full expression, i.e. until TakesValueView() returns.
  TakesValueView(Value());
}

}  // namespace base
