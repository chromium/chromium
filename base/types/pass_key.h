// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TYPES_PASS_KEY_H_
#define BASE_TYPES_PASS_KEY_H_

#include <concepts>
#include <type_traits>

namespace base {

namespace pass_key_internal {
template <typename T, typename... Args>
concept OneOf = (std::same_as<T, Args> || ...);

// Calculates how many instances of <T> there are among <Args...>.
template <typename T, typename... Args>
constexpr int type_frequency =
    (static_cast<int>(std::is_same_v<T, Args>) + ... + 0);

// All types in <Args...> are unique if each type occurs there exactly once!
template <typename... Args>
concept PairwiseUnique = ((type_frequency<Args, Args...> == 1) && ...);
}  // namespace pass_key_internal

// base::PassKey can be used to restrict access to functions to an authorized
// caller. The primary use case is restricting the construction of an object in
// situations where the constructor needs to be public, which may be the case
// if the object must be constructed through a helper function, such as
// blink::MakeGarbageCollected.
//
// Basic usage:
// To limit the creation of 'Foo' to the 'Manager' class:
//
//  class Foo {
//   public:
//    Foo(base::PassKey<Manager>);
//  };
//
//  class Manager {
//   public:
//    using PassKey = base::PassKey<Manager>;
//    Manager() : foo_(blink::MakeGarbageCollected<Foo>(PassKey())) {}
//    ...
//  };
//
// In the above example, the 'Foo' constructor requires an instance of
// base::PassKey<Manager>. Only Manager is allowed to create such instances,
// making the constructor unusable elsewhere.
//
// Advanced usage with multiple authorized types:
// A PassKey can be authorized for multiple types. A multi-type PassKey can be
// constructed from any other PassKey whose authorized types are a subset of the
// target's. This is useful for granting access to a group of related classes.
//
// For example, to allow a 'Service' to be created by either 'A' or
// 'B':
//
//  class Service {
//   public:
//    Service(base::PassKey<A, B>);
//  };
//
//  class A {
//   public:
//    void DoSomething() {
//      auto service = std::make_unique<Service>(base::PassKey<A>()); // OK
//    }
//  };
//
//  class B {
//   public:
//    void DoSomethingElse() {
//      auto service = std::make_unique<Service>(base::PassKey<B>()); // OK
//    }
//  };
//
//  class Unauthorized {
//   public:
//    using PassKey = base::PassKey<Unauthorized>;
//    void DoIllegal() {
//      // Fails to compile: Cannot convert PassKey<Unauthorized> to
//      // PassKey<A, B>.
//      auto service = std::make_unique<Service>(PassKey());
//    }
//  };
//
// It is also possible to convert from one multi-type PassKey to another:
//
//  class SuperService {
//   public:
//    SuperService(base::PassKey<A, B, C>);
//  };
//
//  class SomeOtherService {
//   public:
//    SomeOtherService(base::PassKey<A, C, D>);
//  };
//
//  void MakeServices(base::PassKey<A, B> key) {
//    // Converts PassKey<A, B> to PassKey<A, B, C>
//    auto super_s = std::make_unique<SuperService>(key); // OK
//
//    // Doesn't compile: PassKey<A, B> isn't convertible to PassKey<A, C, D>
//    auto other_s = std::make_unique<SomeOtherService>(key); // Not OK
//  }
template <typename... Args>
class PassKey;

template <typename T>
class PassKey<T> {
  friend T;
  PassKey() = default;
};

// Specialization for multi-argument PassKey. This allows a PassKey to be
// constructed from another PassKey if all its arguments are present in
// the target PassKey's argument list. This enables flexible access control
// where a function might accept a PassKey that covers a broader set of
// authorized callers, and callers can provide more specific PassKey-s.
template <typename... Args>
  requires(sizeof...(Args) > 1)
class PassKey<Args...> {
  static_assert(pass_key_internal::PairwiseUnique<Args...>,
                "PassKey<> arguments must be pairwise unique.");

 public:
  // Allows constructing a PassKey<A, B> from PassKey<A> or PassKey<B>.
  // Implicit to allow feeding PassKey<A> into a function that expects
  // PassKey<A, B>.
  template <typename T>
    requires pass_key_internal::OneOf<T, Args...>
  // NOLINTNEXTLINE(google-explicit-constructor)
  PassKey(PassKey<T>) {}

  // Allows constructing a PassKey<A, B, ...> from a PassKey<A, B>.
  // The exact order of arguments doesn't matter.
  // Implicit to allow feeding PassKey<A, B> into a function that expects
  // PassKey<A, B, ...>.
  template <typename... SourceArgs>
    requires(sizeof...(SourceArgs) > 1 &&
             (pass_key_internal::OneOf<SourceArgs, Args...> && ...))
  // NOLINTNEXTLINE(google-explicit-constructor)
  PassKey(PassKey<SourceArgs...>) {}
};

// NonCopyablePassKey is a version of PassKey that also disallows copy/move
// construction/assignment. This way functions called with a passkey cannot use
// that key to invoke other passkey-protected functions.
template <typename T>
class NonCopyablePassKey {
  friend T;
  NonCopyablePassKey() = default;
  NonCopyablePassKey(const NonCopyablePassKey&) = delete;
  NonCopyablePassKey& operator=(const NonCopyablePassKey&) = delete;
};

}  // namespace base

#endif  // BASE_TYPES_PASS_KEY_H_
