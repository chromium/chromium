// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#define FORCE_UNRETAINED_COMPLETENESS_CHECKS_FOR_TESTS 1

#include <stdint.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/disallow_unretained.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"

namespace base {

void NonConstFunctionWithConstObject() {
  struct S : RefCounted<S> {
    void Method() {}
  } s;
  const S* const const_s_ptr = &s;
  // Non-`const` methods may not be bound with a `const` receiver.
  BindRepeating(&S::Method, const_s_ptr);  // expected-error@*:* {{Type mismatch between bound argument and bound functor's parameter.}}
  // `const` pointer cannot be bound to non-`const` parameter.
  BindRepeating([] (S*) {}, const_s_ptr);  // expected-error@*:* {{Type mismatch between bound argument and bound functor's parameter.}}
}

void WrongReceiverTypeForNonRefcounted() {
  // 1. Non-refcounted objects must use `Unretained()` for the `this` argument.
  // 2. Reference-like objects may not be used as the receiver.
  struct A {
    void Method() {}
    void ConstMethod() const {}
  } a;
  // Using distinct types causes distinct template instantiations, so we get
  // assertion failures below where we expect. These types facilitate that.
  struct B : A {} b;
  struct C : A {} c;
  struct D : A {} d;
  struct E : A {};
  A* ptr_a = &a;
  A& ref_a = a;
  raw_ptr<A> rawptr_a(&a);
  raw_ref<A> rawref_a(a);
  const B const_b;
  B* ptr_b = &b;
  const B* const_ptr_b = &const_b;
  B& ref_b = b;
  const B& const_ref_b = const_b;
  raw_ptr<B> rawptr_b(&b);
  raw_ptr<const B> const_rawptr_b(&const_b);
  raw_ref<B> rawref_b(b);
  raw_ref<const B> const_rawref_b(const_b);
  C& ref_c = c;
  D& ref_d = d;
  const E const_e;
  const E& const_ref_e = const_e;
  BindRepeating(&A::Method, &a);                   // expected-error@*:* {{Receivers may not be raw pointers.}}
  BindRepeating(&A::Method, ptr_a);                // expected-error@*:* {{Receivers may not be raw pointers.}}
  BindRepeating(&A::Method, a);                    // expected-error@*:* {{Cannot convert `this` argument to address.}}
  BindRepeating(&C::Method, ref_c);                // expected-error@*:* {{Cannot convert `this` argument to address.}}
  BindRepeating(&A::Method, std::ref(a));          // expected-error@*:* {{Cannot convert `this` argument to address.}}
  BindRepeating(&A::Method, std::cref(a));         // expected-error@*:* {{Cannot convert `this` argument to address.}}
  BindRepeating(&A::Method, rawptr_a);             // expected-error@*:* {{Receivers may not be raw pointers.}}
  BindRepeating(&A::Method, rawref_a);             // expected-error@*:* {{Receivers may not be raw_ref<T>.}}
  BindRepeating(&B::ConstMethod, &b);              // expected-error@*:* {{Receivers may not be raw pointers.}}
  BindRepeating(&B::ConstMethod, &const_b);        // expected-error@*:* {{Receivers may not be raw pointers.}}
  BindRepeating(&B::ConstMethod, ptr_b);           // expected-error@*:* {{Receivers may not be raw pointers.}}
  BindRepeating(&B::ConstMethod, const_ptr_b);     // expected-error@*:* {{Receivers may not be raw pointers.}}
  BindRepeating(&B::ConstMethod, b);               // expected-error@*:* {{Cannot convert `this` argument to address.}}
  BindRepeating(&D::ConstMethod, ref_d);           // expected-error@*:* {{Cannot convert `this` argument to address.}}
  BindRepeating(&E::ConstMethod, const_ref_e);     // expected-error@*:* {{Cannot convert `this` argument to address.}}
  BindRepeating(&B::ConstMethod, std::ref(b));     // expected-error@*:* {{Cannot convert `this` argument to address.}}
  BindRepeating(&B::ConstMethod, std::cref(b));    // expected-error@*:* {{Cannot convert `this` argument to address.}}
  BindRepeating(&B::ConstMethod, rawptr_b);        // expected-error@*:* {{Receivers may not be raw pointers.}}
  BindRepeating(&B::ConstMethod, const_rawptr_b);  // expected-error@*:* {{Receivers may not be raw pointers.}}
  BindRepeating(&B::ConstMethod, rawref_b);        // expected-error@*:* {{Receivers may not be raw_ref<T>.}}
  BindRepeating(&B::ConstMethod, const_rawref_b);  // expected-error@*:* {{Receivers may not be raw_ref<T>.}}
}

void WrongReceiverTypeForRefcounted() {
  // Refcounted objects must pass a pointer-like `this` argument.
  struct A : RefCounted<A> {
    void Method() const {}
  } a;
  // Using distinct types causes distinct template instantiations, so we get
  // assertion failures below where we expect. These types facilitate that.
  struct B : A {} b;
  struct C : A {};
  const A const_a;
  B& ref_b = b;
  const C const_c;
  const C& const_ref_c = const_c;
  raw_ref<A> rawref_a(a);
  raw_ref<const A> const_rawref_a(const_a);
  BindRepeating(&A::Method, a);               // expected-error@*:* {{Cannot convert `this` argument to address.}}
  BindRepeating(&B::Method, ref_b);           // expected-error@*:* {{Cannot convert `this` argument to address.}}
  BindRepeating(&C::Method, const_ref_c);     // expected-error@*:* {{Cannot convert `this` argument to address.}}
  BindRepeating(&A::Method, std::ref(a));     // expected-error@*:* {{Cannot convert `this` argument to address.}}
  BindRepeating(&A::Method, std::cref(a));    // expected-error@*:* {{Cannot convert `this` argument to address.}}
  BindRepeating(&A::Method, rawref_a);        // expected-error@*:* {{Receivers may not be raw_ref<T>.}}
  BindRepeating(&A::Method, const_rawref_a);  // expected-error@*:* {{Receivers may not be raw_ref<T>.}}
}

void RemovesConst() {
  // Callbacks that expect non-const refs/ptrs should not be callable with const
  // ones.
  const int i = 0;
  const int* p = &i;
  BindRepeating([] (int&) {}).Run(i);  // expected-error {{no matching member function for call to 'Run'}}
  BindRepeating([] (int*) {}, p);      // expected-error@*:* {{Type mismatch between bound argument and bound functor's parameter.}}
  BindRepeating([] (int*) {}).Run(p);  // expected-error {{no matching member function for call to 'Run'}}
}

void PassingIncorrectRef() {
  // Functions that take non-const reference arguments require the parameters to
  // be bound as matching `std::ref()`s or `OwnedRef()`s.
  int i = 1;
  float f = 1.0f;
  // No wrapper.
  BindOnce([] (int&) {}, i);       // expected-error@*:* {{Bound argument for non-const reference parameter must be wrapped in std::ref() or base::OwnedRef().}}
  BindRepeating([] (int&) {}, i);  // expected-error@*:* {{Bound argument for non-const reference parameter must be wrapped in std::ref() or base::OwnedRef().}}
  // Wrapper, but with mismatched type.
  BindOnce([] (int&) {}, f);            // expected-error@*:* {{Type mismatch between bound argument and bound functor's parameter.}}
  BindOnce([] (int&) {}, std::ref(f));  // expected-error@*:* {{Type mismatch between bound argument and bound functor's parameter.}}
  BindOnce([] (int&) {}, OwnedRef(f));  // expected-error@*:* {{Type mismatch between bound argument and bound functor's parameter.}}
}

void ArrayAsReceiver() {
  // A method should not be bindable with an array of objects. Users could
  // unintentionally attempt to do this via array->pointer decay.
  struct S : RefCounted<S> {
    void Method() const {}
  };
  S s[2];
  BindRepeating(&S::Method, s);  // expected-error@*:* {{First bound argument to a method cannot be an array.}}
}

void RefCountedArgs() {
  // Refcounted types should not be bound as a raw pointers.
  struct S : RefCounted<S> {};
  S s;
  const S const_s;
  S* ptr_s = &s;
  const S* const_ptr_s = &const_s;
  raw_ptr<S> rawptr(&s);
  raw_ptr<const S> const_rawptr(&const_s);
  raw_ref<S> rawref(s);
  raw_ref<const S> const_rawref(const_s);
  BindRepeating([] (S*) {}, &s);                          // expected-error@*:* {{A parameter is a refcounted type and needs scoped_refptr.}}
  BindRepeating([] (const S*) {}, &const_s);              // expected-error@*:* {{A parameter is a refcounted type and needs scoped_refptr.}}
  BindRepeating([] (S*) {}, ptr_s);                       // expected-error@*:* {{A parameter is a refcounted type and needs scoped_refptr.}}
  BindRepeating([] (const S*) {}, const_ptr_s);           // expected-error@*:* {{A parameter is a refcounted type and needs scoped_refptr.}}
  BindRepeating([] (S*) {}, rawptr);                      // expected-error@*:* {{A parameter is a refcounted type and needs scoped_refptr.}}
  BindRepeating([] (const S*) {}, const_rawptr);          // expected-error@*:* {{A parameter is a refcounted type and needs scoped_refptr.}}
  BindRepeating([] (raw_ref<S>) {}, rawref);              // expected-error@*:* {{A parameter is a refcounted type and needs scoped_refptr.}}
  BindRepeating([] (raw_ref<const S>) {}, const_rawref);  // expected-error@*:* {{A parameter is a refcounted type and needs scoped_refptr.}}
}

void WeakPtrWithReturnType() {
  // WeakPtrs cannot be bound to methods with return types, since if the WeakPtr
  // is null when the callback runs, it's not clear what the framework should
  // return.
  struct S {
    int ReturnsInt() const { return 1; }
  } s;
  WeakPtrFactory<S> weak_factory(&s);
  BindRepeating(&S::ReturnsInt, weak_factory.GetWeakPtr());  // expected-error@*:* {{WeakPtrs can only bind to methods without return values.}}
}

void CallbackConversion() {
  // Callbacks should not be constructible from other callbacks in ways that
  // would drop ref or pointer constness or change arity.
  RepeatingCallback<int(int&)> wrong_ref_constness = BindRepeating([] (const int&) {});  // expected-error {{no viable conversion from 'RepeatingCallback<UnboundRunType>' to 'RepeatingCallback<int (int &)>'}}
  RepeatingCallback<int(int*)> wrong_ptr_constness = BindRepeating([] (const int*) {});  // expected-error {{no viable conversion from 'RepeatingCallback<UnboundRunType>' to 'RepeatingCallback<int (int *)>'}}
  RepeatingClosure arg_count_too_low = BindRepeating([] (int) {});                       // expected-error {{no viable conversion from 'RepeatingCallback<UnboundRunType>' to 'RepeatingCallback<void ()>'}}
  RepeatingCallback<int(int)> arg_count_too_high = BindRepeating([] { return 0; });      // expected-error {{no viable conversion from 'RepeatingCallback<UnboundRunType>' to 'RepeatingCallback<int (int)>'}}
  RepeatingClosure discarding_return = BindRepeating([] { return 0; });                  // expected-error {{no viable conversion from 'RepeatingCallback<UnboundRunType>' to 'RepeatingCallback<void ()>'}}
}

void CapturingLambdaOrFunctor() {
  // Bind disallows capturing lambdas and stateful functors.
  int i = 0, j = 0;
  struct S {
    void operator()() const {}
    int x;
  };
  BindOnce([&] { j = i; });        // expected-error@*:* {{Capturing lambdas and stateful functors are intentionally not supported.}}
  BindRepeating([&] { j = i; });   // expected-error@*:* {{Capturing lambdas and stateful functors are intentionally not supported.}}
  BindRepeating(S());                // expected-error@*:* {{Capturing lambdas and stateful functors are intentionally not supported.}}
}

void OnceCallbackRequiresNonConstRvalue() {
  // `OnceCallback::Run()` can only be invoked on a non-const rvalue.
  // Using distinct types causes distinct template instantiations, so we get
  // assertion failures below where we expect. These types facilitate that.
  enum class A {};
  enum class B {};
  enum class C {};
  OnceCallback<void(A)> cb_a = BindOnce([] (A) {});
  const OnceCallback<void(B)> const_cb_b = BindOnce([] (B) {});
  const OnceCallback<void(C)> const_cb_c = BindOnce([] (C) {});
  cb_a.Run(A{});                   // expected-error@*:* {{OnceCallback::Run() may only be invoked on a non-const rvalue, i.e. std::move(callback).Run().}}
  const_cb_b.Run(B{});             // expected-error@*:* {{OnceCallback::Run() may only be invoked on a non-const rvalue, i.e. std::move(callback).Run().}}
  std::move(const_cb_c).Run(C{});  // expected-error@*:* {{OnceCallback::Run() may only be invoked on a non-const rvalue, i.e. std::move(callback).Run().}}
}

void OnceCallbackAsArgMustBeNonConstRvalue() {
  // A `OnceCallback` passed to another callback must be a non-const rvalue.
  auto cb = BindOnce([] (int) {});
  const auto const_cb = BindOnce([] (int) {});
  BindOnce(cb, 0);                   // expected-error@*:* {{BindOnce() requires non-const rvalue for OnceCallback binding, i.e. base::BindOnce(std::move(callback)).}}
  BindOnce(std::move(const_cb), 0);  // expected-error@*:* {{BindOnce() requires non-const rvalue for OnceCallback binding, i.e. base::BindOnce(std::move(callback)).}}
}

void OnceCallbackBoundByRepeatingCallback() {
  // `BindRepeating()` does not accept `OnceCallback`s.
  BindRepeating(BindOnce([] (int) {}), 0);  // expected-error@*:* {{BindRepeating() cannot bind OnceCallback. Use BindOnce() with std::move().}}
}

void MoveOnlyArg() {
  // Move-only types require `std::move()` for `BindOnce()` and `base::Passed()` for `BindRepeating()`.
  struct S {
    S() = default;
    S(S&&) = default;
    S& operator=(S&&) = default;
  } s1, s2;
  BindOnce([] (S) {}, s1);                  // expected-error@*:* {{Attempting to bind a move-only type. Use std::move() to transfer ownership to the created callback.}}
  BindOnce([] (S) {}, Passed(&s1));         // expected-error@*:* {{Use std::move() instead of base::Passed() with base::BindOnce().}}
  BindRepeating([] (S) {}, s2);             // expected-error@*:* {{base::BindRepeating() argument is a move-only type. Use base::Passed() instead of std::move() to transfer ownership from the callback to the bound functor.}}
  BindRepeating([] (S) {}, std::move(s2));  // expected-error@*:* {{base::BindRepeating() argument is a move-only type. Use base::Passed() instead of std::move() to transfer ownership from the callback to the bound functor.}}
}

void NonCopyableNonMovable() {
  // Arguments must be either copyable or movable to be captured.
  struct S {
    S() = default;
    S(const S&) = delete;
    S& operator=(const S&) = delete;
  } s;
  BindOnce([](const S&) {}, s);  // expected-error@*:* {{Cannot capture argument: is the argument copyable or movable?}}
}

void OverloadedFunction() {
  // Overloaded function types cannot be disambiguated. (It might be nice to fix
  // this.)
  void F(int);
  void F(float);
  BindOnce(&F, 1);          // expected-error {{reference to overloaded function could not be resolved; did you mean to call it?}}
  BindRepeating(&F, 1.0f);  // expected-error {{reference to overloaded function could not be resolved; did you mean to call it?}}
}

void OverloadedOperator() {
  // It's not possible to bind to a functor with an overloaded `operator()()`
  // unless the caller supplies arguments that can invoke a unique overload.
  struct A {
    int64_t operator()(int, int64_t x) { return x; }
    uint64_t operator()(int, uint64_t x) { return x; }
    A operator()(double, A a) { return a; }
  } a;
  // Using distinct types causes distinct template instantiations, so we get
  // assertion failures below where we expect. These types facilitate that.
  struct B : A {} b;
  struct C : A {} c;
  struct D : A {} d;

  // Partial function application isn't supported, even if it's sufficient to
  // "narrow the field" to a single candidate that _could_ eventually match.
  BindOnce(a);              // expected-error@*:* {{Could not determine how to invoke functor.}}
  BindOnce(b, 1.0);         // expected-error@*:* {{Could not determine how to invoke functor.}}

  // The supplied args don't match any candidates.
  BindOnce(c, 1, nullptr);  // expected-error@*:* {{Could not determine how to invoke functor.}}

  // The supplied args inexactly match multiple candidates.
  BindOnce(d, 1, 1);        // expected-error@*:* {{Could not determine how to invoke functor.}}
}

void RefQualifiedOverloadedOperator() {
  // Invocations with lvalues should attempt to use lvalue-ref-qualified
  // methods.
  struct A {
    void operator()() const& = delete;
    void operator()() && {}
  } a;
  // Using distinct types causes distinct template instantiations, so we get
  // assertion failures below where we expect. This type facilitates that.
  struct B : A {};
  BindRepeating(a);    // expected-error@*:* {{Could not determine how to invoke functor.}}
  BindRepeating(B());  // expected-error@*:* {{Could not determine how to invoke functor.}}

  // Invocations with rvalues should attempt to use rvalue-ref-qualified
  // methods.
  struct C {
    void operator()() const& {}
    void operator()() && = delete;
  };
  BindRepeating(Passed(C()));  // expected-error@*:* {{Could not determine how to invoke functor.}}
  BindOnce(C());               // expected-error@*:* {{Could not determine how to invoke functor.}}
}

// Define a type that disallows `Unretained()` via the internal customization
// point, so the next test can use it.
struct BlockViaCustomizationPoint {};
namespace internal {
template <>
constexpr bool kCustomizeSupportsUnretained<BlockViaCustomizationPoint> = false;
}  // namespace internal

void CanDetectTypesThatDisallowUnretained() {
  // It shouldn't be possible to directly bind any type that doesn't support
  // `Unretained()`, whether because it's incomplete, or is marked with
  // `DISALLOW_RETAINED()`, or has `kCustomizeSupportsUnretained` specialized to
  // be `false`.
  struct BlockPublicly {
    DISALLOW_UNRETAINED();
  } publicly;
  class BlockPrivately {
    DISALLOW_UNRETAINED();
  } privately;
  struct BlockViaInheritance : BlockPublicly {} inheritance;
  BlockViaCustomizationPoint customization;
  struct BlockDueToBeingIncomplete;
  BlockDueToBeingIncomplete* ptr_incomplete;
  BindOnce([](BlockPublicly*) {}, &publicly);                    // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  BindOnce([](BlockPrivately*) {}, &privately);                  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  BindOnce([](BlockViaInheritance*) {}, &inheritance);           // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  BindOnce([](BlockViaCustomizationPoint*) {}, &customization);  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  BindOnce([](BlockDueToBeingIncomplete*) {}, ptr_incomplete);   // expected-error@*:* {{Argument requires unretained storage, but type is not fully defined.}}
}

void OtherWaysOfPassingDisallowedTypes() {
  // In addition to the direct passing tested above, arguments passed as
  // `Unretained()` pointers or as refs must support `Unretained()`.
  struct A {
    void Method() {}
    DISALLOW_UNRETAINED();
  } a;
  // Using distinct types causes distinct template instantiations, so we get
  // assertion failures below where we expect. This type facilitates that.
  struct B : A {} b;
  BindOnce(&A::Method, Unretained(&a));      // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  BindOnce([] (const A&) {}, std::cref(a));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  BindOnce([] (B&) {}, std::ref(b));         // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
}

void UnsafeDangling() {
  // Pointers marked as `UnsafeDangling` may only be be received by
  // `MayBeDangling` args with matching traits.
  int i;
  BindOnce([] (int*) {}, UnsafeDangling(&i));                      // expected-error@*:* {{base::UnsafeDangling() pointers should only be passed to parameters marked MayBeDangling<T>.}}
  BindOnce([] (MayBeDangling<int>) {},
           UnsafeDangling<int, RawPtrTraits::kDummyForTest>(&i));  // expected-error@*:* {{Pointers passed to MayBeDangling<T> parameters must be created by base::UnsafeDangling() with the same RawPtrTraits.}}
  BindOnce([] (raw_ptr<int>) {}, UnsafeDanglingUntriaged(&i));     // expected-error@*:* {{Use T* or T& instead of raw_ptr<T> for function parameters, unless you must mark the parameter as MayBeDangling<T>.}}
}

}  // namespace base
