// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/task_runner.h"
#include "base/task/promise/promise.h"
#include "base/task/promise/promise_result.h"

namespace base {

#if defined(NCTEST_METHOD_CANT_CATCH_NOREJECT_PROMISE) // [r"fatal error: static_assert failed .*\"Can't catch a NoReject promise\."]
void WontCompile() {
  Promise<int> p;
  p.CatchHere(FROM_HERE, BindOnce([]() {}));
}
#elif defined(NCTEST_METHOD_CANT_CATCH_NOREJECT_PROMISE_TYPE_TWO) // [r"fatal error: static_assert failed .*\"Can't catch a NoReject promise\."]
void WontCompile() {
  Promise<int> p;
  p.ThenHere(FROM_HERE, BindOnce([](int) {}), BindOnce([]() {}));
}
#elif defined(NCTEST_METHOD_RESOLVE_CALLBACK_TYPE_MISSMATCH) // [r"fatal error: static_assert failed .*\"|on_resolve| callback must accept Promise::ResolveType or void\."]
void WontCompile() {
  Promise<int, void> p;
  p.ThenHere(FROM_HERE, BindOnce([](std::string) { }));
}
#elif defined(NCTEST_METHOD_REJECT_CALLBACK_TYPE_MISSMATCH) // [r"fatal error: static_assert failed .*\"|on_reject| callback must accept Promise::ResolveType or void\."]
void WontCompile() {
  Promise<int, void> p;
  p.CatchHere(FROM_HERE, BindOnce([](bool) { }));
}
#elif defined(NCTEST_METHOD_REJECT_CALLBACK_TYPE_MISSMATCH_TYPE_TWO) // [r"fatal error: static_assert failed .*\"|on_reject| callback must accept Promise::ResolveType or void\."]
void WontCompile() {
  Promise<int, void> p;
  p.ThenHere(FROM_HERE, BindOnce([](int) { }), BindOnce([](bool) { }));
}
#elif defined(NCTEST_METHOD_INCOMPATIBLE_RETURN_TYPES) // [r"fatal error: static_assert failed .*\"|on_resolve| callback and |on_resolve| callback must return compatible types\."]
void WontCompile() {
  Promise<void> p;
  p.ThenHere(
      FROM_HERE,
      BindOnce([]() -> PromiseResult<int, std::string> { return 123; }),
      BindOnce([](int err) -> Rejected<bool> { return "123"; }));
}
#elif defined(NCTEST_METHOD_INCOMPATIBLE_RETURN_TYPES2) // [r"fatal error: static_assert failed .*\"|on_resolve| callback and |on_resolve| callback must return compatible types\."]
void WontCompile() {
  Promise<void> p;
  p.ThenHere(
      FROM_HERE,
      BindOnce([]() -> PromiseResult<int, std::string> { return 123; }),
      BindOnce([](int err) -> Resolved<std::string> { return "123"; }));
}
#elif defined(NCTEST_METHOD_INCOMPATIBLE_RETURN_TYPES3) // [r"fatal error: static_assert failed .*\"|on_resolve| callback and |on_resolve| callback must return compatible types\."]
void WontCompile() {
  Promise<int, void> p;
  p.ThenHere(FROM_HERE, BindOnce([](int) { return true; }),
                             BindOnce([](int) { return 123.0; }));
}
#elif defined(NCTEST_METHOD_AMBIGUOUS_CONSTRUCTOR) // [r"fatal error: static_assert failed .*\"Ambiguous because ResolveType and RejectType are the same"]
void WontCompile() {
  PromiseResult<void, void> pr;
}
#elif defined(NCTEST_METHOD_AMBIGUOUS_CONSTRUCTOR2) // [r"fatal error: static_assert failed .*\"Ambiguous because ResolveType and RejectType are the same"]
void WontCompile() {
  PromiseResult<int, int> pr(123);
}
#elif defined(NCTEST_METHOD_REJECTED_NOREJECT) // [r"fatal error: static_assert failed .*\"Can't have Rejected<NoReject>"]
void WontCompile() {
  Rejected<NoReject>();
}
#elif defined(NCTEST_METHOD_ARGUMENT_DOESNT_MATCH) // [r"fatal error: static_assert failed .*\"Argument matches neither resolve nor reject type\."]
void WontCompile() {
  PromiseResult<int, float> pr("invalid");
}
#elif defined(NCTEST_METHOD_ARGUMENT_DOESNT_MATCH2) // [r"fatal error: static_assert failed .*\"Promise resolve types don't match"]
void WontCompile() {
  Promise<void> p;
  PromiseResult<int, float> pr(p);
}
#elif defined(NCTEST_METHOD_UNRELATED_RESOLVE) // [r"fatal error: static_assert failed .*\"T in Resolved<T> is not ResolveType"]
void WontCompile() {
  struct Unrelated{};
  PromiseResult<int, void> pr(Resolved<Unrelated>{});
}
#elif defined(NCTEST_METHOD_UNRELATED_REJECT) // [r"fatal error: static_assert failed .*\"T in Rejected<T> is not RejectType"]
void WontCompile() {
  struct Unrelated{};
  PromiseResult<int, void> pr(Rejected<Unrelated>{});
}
#elif defined(NCTEST_METHOD_AMBIGUOUS_REJECT_TYPE) // [r"fatal error: static_assert failed .*\"Ambiguous promise reject type"]
void WontCompile() {
  Promise<int, void> p1;
  // If supported |p2| would have type Promise<NoReject, variant<void, bool>>.
  auto p2 = p1.ThenHere(FROM_HERE, BindOnce([]() { return Rejected<bool>(true); }));
}
#elif defined(NCTEST_METHOD_AMBIGUOUS_RESOLVE_TYPE) // [r"fatal error: static_assert failed .*\"Ambiguous promise resolve type"]
void WontCompile() {
  Promise<int, void> p1;
  // If supported |p2| would have type Promise<variant<int, bool>, NoReject>.
  auto p2 = p1.CatchHere(FROM_HERE, BindOnce([](int) { return Resolved<bool>(true); }));
}
#elif defined(NCTEST_METHOD_NON_CONST_REFERENCE) // [r"fatal error: static_assert failed .*\"Google C.. Style: References in function parameters must be const\."]
void WontCompile() {
  Promise<std::unique_ptr<int>> p;
  p.ThenHere(FROM_HERE, BindOnce([](std::unique_ptr<int>& result) {}));
}
#elif defined(NCTEST_METHOD_NON_CONST_REFERENCE2) // [r"fatal error: static_assert failed .*\"Google C.. Style: References in function parameters must be const\."]
void WontCompile() {
  Promise<int&> p;
}
#elif defined(NCTEST_METHOD_NON_CONST_REFERENCE3) // [r"fatal error: static_assert failed .*\"Google C.. Style: References in function parameters must be const\."]
void WontCompile() {
  Promise<void, int&> p;
}
#endif

}  // namespace base
