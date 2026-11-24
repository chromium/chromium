// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/types/expected_macros.h"

#include <optional>
#include <string>

#include "base/types/expected.h"

namespace base {

bool WrongHandler(std::string);

struct WrongHandlerClass {
  bool Handle(std::string);
};

// A handler that can't be invoked with the error fails to compile, instead of
// silently never running.
void HandlerNotInvocableWithErrorInVoidFunction() {
  base::expected<int, int> e = base::ok(1);
  RETURN_IF_ERROR(e, [](std::string) {});  // expected-error {{no matching function for call to 'ErrorReturn'}}
  RETURN_IF_ERROR(e, [](int, std::string) {}, 1);  // expected-error {{no matching function for call to 'ErrorReturn'}}
}

// That also holds where the handler itself converts to the function's return
// type, here to `true`.
bool HandlerNotInvocableWithError() {
  base::expected<int, int> e = base::ok(1);
  RETURN_IF_ERROR(e, [](std::string) { return false; });  // expected-error {{no matching function for call to 'ErrorReturn'}}
  RETURN_IF_ERROR(e, &WrongHandler);  // expected-error {{no matching function for call to 'ErrorReturn'}}
  RETURN_IF_ERROR(e, &WrongHandlerClass::Handle);  // expected-error {{no matching function for call to 'ErrorReturn'}}
  std::optional<int> o = 1;
  RETURN_IF_ERROR(o, [](std::string) { return false; });  // expected-error {{no matching function for call to 'ErrorReturn'}}
  RETURN_IF_ERROR(o, &WrongHandler);  // expected-error {{no matching function for call to 'ErrorReturn'}}
  return false;
}

// A value is returned like in a return statement, so it is not wrapped in
// `base::unexpected`.
base::expected<void, int> ValueIsNotWrappedInUnexpected() {
  base::expected<int, int> e = base::ok(1);
  RETURN_IF_ERROR(e, 2);  // expected-error {{no viable conversion from returned value}}
  return base::ok();
}

// A value is not copied, so a temporary can't be returned from a function that
// returns a reference.
const std::string& TemporaryValueInFunctionReturningReference(
    const std::string& s) {
  base::expected<int, int> e = base::ok(1);
  std::optional<int> o = 1;
  RETURN_IF_ERROR(e, std::string());  // expected-error {{returning reference}}
  RETURN_IF_ERROR(o, std::string());  // expected-error {{returning reference}}
  return s;
}

// Only a single argument is treated as a value to return.
int MultipleValues() {
  base::expected<int, int> e = base::ok(1);
  std::optional<int> o = 1;
  RETURN_IF_ERROR(e, 1, 2);  // expected-error {{no matching function for call to 'ErrorReturn'}}
  RETURN_IF_ERROR(o, 1, 2);  // expected-error {{no matching function for call to 'ErrorReturn'}}
  return 0;
}

base::expected<void, int> TernaryInAssignOrReturn() {
  base::expected<int, int> e = base::ok(1);
  int a, b;
  ASSIGN_OR_RETURN((false ? a : b), e);  // expected-error {{Identified possible ternary in `lhs`}}
}

}  // namespace base
