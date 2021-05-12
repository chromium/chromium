// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BIND_H_
#define BASE_BIND_H_

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include "base/bind_internal.h"
#include "base/compiler_specific.h"
#include "base/template_util.h"
#include "build/build_config.h"

#if defined(OS_APPLE) && !HAS_FEATURE(objc_arc)
#include "base/mac/scoped_block.h"
#endif

// -----------------------------------------------------------------------------
// Usage documentation
// -----------------------------------------------------------------------------
//
// Overview:
// base::BindOnce() and base::BindRepeating() are helpers for creating
// base::OnceCallback and base::RepeatingCallback objects respectively.
//
// For a runnable object of n-arity, the base::Bind*() family allows partial
// application of the first m arguments. The remaining n - m arguments must be
// passed when invoking the callback with Run().
//
//   // The first argument is bound at callback creation; the remaining
//   // two must be passed when calling Run() on the callback object.
//   base::OnceCallback<long(int, long)> cb = base::BindOnce(
//       [](short x, int y, long z) { return x * y * z; }, 42);
//
// When binding to a method, the receiver object must also be specified at
// callback creation time. When Run() is invoked, the method will be invoked on
// the specified receiver object.
//
//   class C : public base::RefCounted<C> { void F(); };
//   auto instance = base::MakeRefCounted<C>();
//   auto cb = base::BindOnce(&C::F, instance);
//   std::move(cb).Run();  // Identical to instance->F()
//
// See //docs/callback.md for the full documentation.
//
// -----------------------------------------------------------------------------
// Implementation notes
// -----------------------------------------------------------------------------
//
// If you're reading the implementation, before proceeding further, you should
// read the top comment of base/bind_internal.h for a definition of common
// terms and concepts.

namespace base {

// Bind as OnceCallback.
template <typename Functor, typename... Args>
inline OnceCallback<internal::MakeUnboundRunType<Functor, Args...>> BindOnce(
    Functor&& functor,
    Args&&... args) {
  static_assert(!internal::IsOnceCallback<std::decay_t<Functor>>() ||
                    (std::is_rvalue_reference<Functor&&>() &&
                     !std::is_const<std::remove_reference_t<Functor>>()),
                "BindOnce requires non-const rvalue for OnceCallback binding."
                " I.e.: base::BindOnce(std::move(callback)).");
  static_assert(
      conjunction<
          internal::AssertBindArgIsNotBasePassed<std::decay_t<Args>>...>::value,
      "Use std::move() instead of base::Passed() with base::BindOnce()");

  return internal::BindImpl<OnceCallback>(std::forward<Functor>(functor),
                                          std::forward<Args>(args)...);
}

// Bind as RepeatingCallback.
template <typename Functor, typename... Args>
inline RepeatingCallback<internal::MakeUnboundRunType<Functor, Args...>>
BindRepeating(Functor&& functor, Args&&... args) {
  static_assert(
      !internal::IsOnceCallback<std::decay_t<Functor>>(),
      "BindRepeating cannot bind OnceCallback. Use BindOnce with std::move().");

  return internal::BindImpl<RepeatingCallback>(std::forward<Functor>(functor),
                                               std::forward<Args>(args)...);
}

// Special cases for binding to a base::{Once, Repeating}Callback without extra
// bound arguments. We CHECK() the validity of callback to guard against null
// pointers accidentally ending up in posted tasks, causing hard-to-debug
// crashes.
template <typename Signature>
OnceCallback<Signature> BindOnce(OnceCallback<Signature> callback) {
  CHECK(callback);
  return callback;
}

template <typename Signature>
OnceCallback<Signature> BindOnce(RepeatingCallback<Signature> callback) {
  CHECK(callback);
  return callback;
}

template <typename Signature>
RepeatingCallback<Signature> BindRepeating(
    RepeatingCallback<Signature> callback) {
  CHECK(callback);
  return callback;
}

// Unretained() allows binding a non-refcounted class, and to disable
// refcounting on arguments that are refcounted objects.
//
// EXAMPLE OF Unretained():
//
//   class Foo {
//    public:
//     void func() { cout << "Foo:f" << endl; }
//   };
//
//   // In some function somewhere.
//   Foo foo;
//   OnceClosure foo_callback =
//       BindOnce(&Foo::func, Unretained(&foo));
//   std::move(foo_callback).Run();  // Prints "Foo:f".
//
// Without the Unretained() wrapper on |&foo|, the above call would fail
// to compile because Foo does not support the AddRef() and Release() methods.
template <typename T>
inline internal::UnretainedWrapper<T> Unretained(T* o) {
  return internal::UnretainedWrapper<T>(o);
}

// RetainedRef() accepts a ref counted object and retains a reference to it.
// When the callback is called, the object is passed as a raw pointer.
//
// EXAMPLE OF RetainedRef():
//
//    void foo(RefCountedBytes* bytes) {}
//
//    scoped_refptr<RefCountedBytes> bytes = ...;
//    OnceClosure callback = BindOnce(&foo, base::RetainedRef(bytes));
//    std::move(callback).Run();
//
// Without RetainedRef, the scoped_refptr would try to implicitly convert to
// a raw pointer and fail compilation:
//
//    OnceClosure callback = BindOnce(&foo, bytes); // ERROR!
template <typename T>
inline internal::RetainedRefWrapper<T> RetainedRef(T* o) {
  return internal::RetainedRefWrapper<T>(o);
}
template <typename T>
inline internal::RetainedRefWrapper<T> RetainedRef(scoped_refptr<T> o) {
  return internal::RetainedRefWrapper<T>(std::move(o));
}

// Owned() transfers ownership of an object to the callback resulting from
// bind; the object will be deleted when the callback is deleted.
//
// EXAMPLE OF Owned():
//
//   void foo(int* arg) { cout << *arg << endl }
//
//   int* pn = new int(1);
//   RepeatingClosure foo_callback = BindRepeating(&foo, Owned(pn));
//
//   foo_callback.Run();  // Prints "1"
//   foo_callback.Run();  // Prints "1"
//   *pn = 2;
//   foo_callback.Run();  // Prints "2"
//
//   foo_callback.Reset();  // |pn| is deleted.  Also will happen when
//                          // |foo_callback| goes out of scope.
//
// Without Owned(), someone would have to know to delete |pn| when the last
// reference to the callback is deleted.
template <typename T>
inline internal::OwnedWrapper<T> Owned(T* o) {
  return internal::OwnedWrapper<T>(o);
}

template <typename T, typename Deleter>
inline internal::OwnedWrapper<T, Deleter> Owned(
    std::unique_ptr<T, Deleter>&& ptr) {
  return internal::OwnedWrapper<T, Deleter>(std::move(ptr));
}

// OwnedRef() stores an object in the callback resulting from
// bind and passes a reference to the object to the bound function.
//
// EXAMPLE OF OwnedRef():
//
//   void foo(int& arg) { cout << ++arg << endl }
//
//   int counter = 0;
//   RepeatingClosure foo_callback = BindRepeating(&foo, OwnedRef(counter));
//
//   foo_callback.Run();  // Prints "1"
//   foo_callback.Run();  // Prints "2"
//   foo_callback.Run();  // Prints "3"
//
//   cout << counter;     // Prints "0", OwnedRef creates a copy of counter.
//
//  Supports OnceCallbacks as well, useful to pass placeholder arguments:
//
//   void bar(int& ignore, const std::string& s) { cout << s << endl }
//
//   OnceClosure bar_callback = BindOnce(&bar, OwnedRef(0), "Hello");
//
//   std::move(bar_callback).Run(); // Prints "Hello"
//
// Without OwnedRef() it would not be possible to pass a mutable reference to an
// object owned by the callback.
template <typename T>
internal::OwnedRefWrapper<std::decay_t<T>> OwnedRef(T&& t) {
  return internal::OwnedRefWrapper<std::decay_t<T>>(std::forward<T>(t));
}

// Passed() is for transferring movable-but-not-copyable types (eg. unique_ptr)
// through a RepeatingCallback. Logically, this signifies a destructive transfer
// of the state of the argument into the target function. Invoking
// RepeatingCallback::Run() twice on a callback that was created with a Passed()
// argument will CHECK() because the first invocation would have already
// transferred ownership to the target function.
//
// Note that Passed() is not necessary with BindOnce(), as std::move() does the
// same thing. Avoid Passed() in favor of std::move() with BindOnce().
//
// EXAMPLE OF Passed():
//
//   void TakesOwnership(std::unique_ptr<Foo> arg) { }
//   std::unique_ptr<Foo> CreateFoo() { return std::make_unique<Foo>();
//   }
//
//   auto f = std::make_unique<Foo>();
//
//   // |cb| is given ownership of Foo(). |f| is now NULL.
//   // You can use std::move(f) in place of &f, but it's more verbose.
//   RepeatingClosure cb = BindRepeating(&TakesOwnership, Passed(&f));
//
//   // Run was never called so |cb| still owns Foo() and deletes
//   // it on Reset().
//   cb.Reset();
//
//   // |cb| is given a new Foo created by CreateFoo().
//   cb = BindRepeating(&TakesOwnership, Passed(CreateFoo()));
//
//   // |arg| in TakesOwnership() is given ownership of Foo(). |cb|
//   // no longer owns Foo() and, if reset, would not delete Foo().
//   cb.Run();  // Foo() is now transferred to |arg| and deleted.
//   cb.Run();  // This CHECK()s since Foo() already been used once.
//
// We offer 2 syntaxes for calling Passed(). The first takes an rvalue and is
// best suited for use with the return value of a function or other temporary
// rvalues. The second takes a pointer to the scoper and is just syntactic sugar
// to avoid having to write Passed(std::move(scoper)).
//
// Both versions of Passed() prevent T from being an lvalue reference. The first
// via use of enable_if, and the second takes a T* which will not bind to T&.
template <typename T,
          std::enable_if_t<!std::is_lvalue_reference<T>::value>* = nullptr>
inline internal::PassedWrapper<T> Passed(T&& scoper) {
  return internal::PassedWrapper<T>(std::move(scoper));
}
template <typename T>
inline internal::PassedWrapper<T> Passed(T* scoper) {
  return internal::PassedWrapper<T>(std::move(*scoper));
}

// IgnoreResult() is used to adapt a function or callback with a return type to
// one with a void return. This is most useful if you have a function with,
// say, a pesky ignorable bool return that you want to use with PostTask or
// something else that expect a callback with a void return.
//
// EXAMPLE OF IgnoreResult():
//
//   int DoSomething(int arg) { cout << arg << endl; }
//
//   // Assign to a callback with a void return type.
//   OnceCallback<void(int)> cb = BindOnce(IgnoreResult(&DoSomething));
//   std::move(cb).Run(1);  // Prints "1".
//
//   // Prints "2" on |ml|.
//   ml->PostTask(FROM_HERE, BindOnce(IgnoreResult(&DoSomething), 2);
template <typename T>
inline internal::IgnoreResultHelper<T> IgnoreResult(T data) {
  return internal::IgnoreResultHelper<T>(std::move(data));
}

#if defined(OS_APPLE) && !HAS_FEATURE(objc_arc)

// RetainBlock() is used to adapt an Objective-C block when Automated Reference
// Counting (ARC) is disabled. This is unnecessary when ARC is enabled, as the
// BindOnce and BindRepeating already support blocks then.
//
// EXAMPLE OF RetainBlock():
//
//   // Wrap the block and bind it to a callback.
//   OnceCallback<void(int)> cb =
//       BindOnce(RetainBlock(^(int n) { NSLog(@"%d", n); }));
//   std::move(cb).Run(1);  // Logs "1".
template <typename R, typename... Args>
base::mac::ScopedBlock<R (^)(Args...)> RetainBlock(R (^block)(Args...)) {
  return base::mac::ScopedBlock<R (^)(Args...)>(block,
                                                base::scoped_policy::RETAIN);
}

#endif  // defined(OS_APPLE) && !HAS_FEATURE(objc_arc)

}  // namespace base

#endif  // BASE_BIND_H_
