// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUNCTIONAL_OVERLOADED_H_
#define BASE_FUNCTIONAL_OVERLOADED_H_

namespace base {

// absl::visit needs to be called with a functor object, such as
//
//  struct Visitor {
//    std::string operator()(const PackageA& source) {
//      return "PackageA";
//    }
//
//    std::string operator()(const PackageB& source) {
//      return "PackageB";
//    }
//  };
//
//  return absl::visit(Visitor(), event.first);
//
// The following file enables the above code to be written as shown below:
//
//  absl::variant<PackageA, PackageB> var = PackageA();
//  absl::visit(
//     Overloaded{
//         [](PackageA& pack) { return "PackageA"; },
//         [](PackageB& pack) { return "PackageB"; }
//     }, var);
//
// Note: Lambdas should be implemented for all the variant options. Otherwise, there
// will be compilation error.

// This struct inherits operator() method from all its base classes.
// Introduces operator() method from all its base classes into its definition.
template <typename... Callables>
struct Overloaded : Callables... {
  using Callables::operator()...;
};

// Uses template argument deduction so that the struct |Overloaded| can be used
// without specifying its template argument. This allows anonymous lambdas
// passed into |Overloaded| constructor.
template <typename... Callables>
Overloaded(Callables...) -> Overloaded<Callables...>;

}  // namespace base

#endif  // BASE_FUNCTIONAL_OVERLOADED_H_
