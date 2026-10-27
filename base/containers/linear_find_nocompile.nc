// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/containers/linear_find.h"

#include <functional>
#include <map>
#include <ranges>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/span.h"

namespace base {

struct E {
  int id = 0;
};

std::vector<int> MakeVector();
const std::vector<int> MakeConstVector();
std::map<int, int> MakeMap();
std::vector<E*> MakeRawPtrVector();

void RvalueContainersDisallowed() {
  std::ignore = LinearFind(MakeVector(), 2);  // expected-error@*:* {{no matching function for call to 'LinearFind'}}
  std::ignore = LinearFind(MakeConstVector(), 2);  // expected-error@*:* {{no matching function for call to 'LinearFind'}}
  std::ignore = LinearFind(MakeMap(), 1, &std::pair<const int, int>::first);  // expected-error@*:* {{no matching function for call to 'LinearFind'}}
  std::ignore = LinearFindIf(MakeVector(), [](int v) { return v == 2; });  // expected-error@*:* {{no matching function for call to 'LinearFindIf'}}
  // The result points into the container, so even plain pointer elements
  // don't make a temporary OK.
  std::ignore = LinearFindIf(MakeRawPtrVector(), [](E* e) { return e->id == 1; });  // expected-error@*:* {{no matching function for call to 'LinearFindIf'}}
}

void NonLvalueElementsDisallowed() {
  std::vector<int> vec = {1, 2, 3};
  std::ignore = LinearFind(std::views::iota(0, 5), 2);  // expected-error@*:* {{no matching function for call to 'LinearFind'}}
  std::ignore = LinearFindIf(std::views::iota(0, 5), [](int v) { return v == 2; });  // expected-error@*:* {{no matching function for call to 'LinearFindIf'}}
  std::ignore = LinearFind(std::views::transform(vec, [](int v) { return v * 2; }), 4);  // expected-error@*:* {{no matching function for call to 'LinearFind'}}
  std::ignore = LinearFind(std::views::as_rvalue(vec), 2);  // expected-error@*:* {{no matching function for call to 'LinearFind'}}
  // `std::vector<bool>` elements are proxies.
  std::vector<bool> bools = {true, false};
  std::ignore = LinearFind(bools, false);  // expected-error@*:* {{no matching function for call to 'LinearFind'}}
  std::ignore = LinearFindIf(bools, [](bool b) { return !b; });  // expected-error@*:* {{no matching function for call to 'LinearFindIf'}}
}

void ExplicitTemplateArgumentsDisallowed() {
  std::vector<int> vec = {1, 2, 3};
  std::ignore = LinearFind<std::vector<int>&, std::identity, long>(vec, 2);  // expected-error@*:* {{no matching function for call to 'LinearFind'}}
  std::ignore = LinearFindIf<std::vector<int>&>(vec, [](int v) { return v == 2; });  // expected-error@*:* {{no matching function for call to 'LinearFindIf'}}
}

int* ReturnLocalVectorDisallowed() {
  std::vector<int> local_vec = {1, 2, 3};
  return LinearFind(local_vec, 1);  // expected-error {{address of stack memory associated with local variable 'local_vec' returned}}
}

int* ReturnLocalVectorFindIfDisallowed() {
  std::vector<int> local_vec = {1, 2, 3};
  return LinearFindIf(local_vec, [](int v) { return v == 1; });  // expected-error {{address of stack memory associated with local variable 'local_vec' returned}}
}

void SpanOfTemporaryDisallowed() {
  [[maybe_unused]] const int* p = LinearFind(span<const int>(MakeVector()), 2);  // expected-error {{temporary whose address is used as value of local variable 'p' will be destroyed at the end of the full-expression}}
  [[maybe_unused]] const int* q = LinearFindIf(span<const int>(MakeVector()), [](int v) { return v == 2; });  // expected-error {{temporary whose address is used as value of local variable 'q' will be destroyed at the end of the full-expression}}
}

int* ReturnSpanOfLocalDisallowed() {
  std::vector<int> local_vec = {1, 2, 3};
  return LinearFind(span(local_vec), 2);  // expected-error {{address of stack memory associated with local variable 'local_vec' returned}}
}

int* ReturnSpanOfLocalFindIfDisallowed() {
  std::vector<int> local_vec = {1, 2, 3};
  return LinearFindIf(span(local_vec), [](int v) { return v == 2; });  // expected-error {{address of stack memory associated with local variable 'local_vec' returned}}
}

}  // namespace base
