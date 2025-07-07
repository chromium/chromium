// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_VARIANT_VISITOR_H_
#define CHROME_BROWSER_ACTOR_VARIANT_VISITOR_H_

namespace actor {

// Generic visitor functor for use with std::visit.
//
// Example usage:
//   using VariantType = std::variant<T1, T2>;
//   constexpr Visitor DoSomethingFn {
//     [](const T1& t) { return "T1"; },
//     [](const T2& t) { return "T2"; },
//   };
//
//   VariantType var;
//   std::string s = std::visit(DoSomethingFn, var);
template <typename... Base>
struct Visitor : Base... {
  using Base::operator()...;
};

template <typename... T>
Visitor(T...) -> Visitor<T...>;

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_VARIANT_VISITOR_H_
