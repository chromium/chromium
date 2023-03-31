// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUNCTIONAL_BIND_INTERNAL_H_
#define BASE_FUNCTIONAL_BIND_INTERNAL_H_

#include <stddef.h>

#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/functional/callback_internal.h"
#include "base/functional/disallow_unretained.h"
#include "base/functional/unretained_traits.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_asan_bound_arg_tracker.h"
#include "base/memory/raw_ptr_asan_service.h"
#include "base/memory/raw_ref.h"
#include "base/memory/raw_scoped_refptr_mismatch_checker.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/types/always_false.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/functional/function_ref.h"

#if BUILDFLAG(IS_APPLE) && !HAS_FEATURE(objc_arc)
#include "base/mac/scoped_block.h"
#endif

// See base/functional/callback.h for user documentation.
//
//
// CONCEPTS:
//  Functor -- A movable type representing something that should be called.
//             All function pointers and Callback<> are functors even if the
//             invocation syntax differs.
//  RunType -- A function type (as opposed to function _pointer_ type) for
//             a Callback<>::Run().  Usually just a convenience typedef.
//  (Bound)Args -- A set of types that stores the arguments.
//
// Types:
//  ForceVoidReturn<> -- Helper class for translating function signatures to
//                       equivalent forms with a "void" return type.
//  FunctorTraits<> -- Type traits used to determine the correct RunType and
//                     invocation manner for a Functor.  This is where function
//                     signature adapters are applied.
//  StorageTraits<> -- Type traits that determine how a bound argument is
//                     stored in BindState.
//  InvokeHelper<> -- Take a Functor + arguments and actually invokes it.
//                    Handle the differing syntaxes needed for WeakPtr<>
//                    support.  This is separate from Invoker to avoid creating
//                    multiple version of Invoker<>.
//  Invoker<> -- Unwraps the curried parameters and executes the Functor.
//  BindState<> -- Stores the curried parameters, and is the main entry point
//                 into the Bind() system.

#if BUILDFLAG(IS_WIN)
namespace Microsoft {
namespace WRL {
template <typename>
class ComPtr;
}  // namespace WRL
}  // namespace Microsoft
#endif

namespace base {

template <typename T>
struct IsWeakReceiver;

template <typename>
struct BindUnwrapTraits;

template <typename Functor, typename BoundArgsTuple, typename SFINAE = void>
struct CallbackCancellationTraits;

template <typename Signature>
class FunctionRef;

namespace unretained_traits {

// UnretainedWrapper will check and report if pointer is dangling upon
// invocation.
struct MayNotDangle {};
// UnretainedWrapper won't check if pointer is dangling upon invocation. For
// extra safety, the receiver must be of type MayBeDangling<>.
struct MayDangle {};
// UnretainedWrapper won't check if pointer is dangling upon invocation. The
// receiver doesn't have to be a raw_ptr<>. This is just a temporary state, to
// allow dangling pointers that would otherwise crash if MayNotDangle was used.
// It should be replaced ASAP with MayNotDangle (after fixing the dangling
// pointers) or with MayDangle if there is really no other way (after making
// receivers MayBeDangling<>).
struct MayDangleUntriaged {};

}  // namespace unretained_traits

namespace internal {

template <typename Functor, typename SFINAE = void>
struct FunctorTraits;

template <typename T,
          typename UnretainedTrait,
          RawPtrTraits PtrTraits = RawPtrTraits::kEmpty>
class UnretainedWrapper {
  // Note that if PtrTraits already includes MayDangle, DanglingRawPtrType
  // will be identical to `raw_ptr<T, PtrTraits>`.
  using DanglingRawPtrType = MayBeDangling<T, PtrTraits>;

 public:
  // We want the getter type to match the receiver parameter that it is passed
  // into, to minimize `raw_ptr<T>` <-> `T*` conversions. We also would like to
  // match `StorageType`, but sometimes we can't have both, as shown in
  // https://docs.google.com/document/d/1dLM34aKqbNBfRdOYxxV_T-zQU4J5wjmXwIBJZr7JvZM/edit
  // When we can't have both, prefer the former, mostly because
  // `GetPtrType`=`raw_ptr<T>` would break if e.g. UnretainedWrapper() is
  // constructed using `char*`, but the receiver is of type `std::string&`.
  // This is enforced by static_asserts in base::internal::AssertConstructible.
  using GetPtrType = std::conditional_t<
      raw_ptr_traits::IsSupportedType<T>::value &&
          std::is_same_v<UnretainedTrait, unretained_traits::MayDangle>,
      DanglingRawPtrType,
      T*>;

  static_assert(TypeSupportsUnretainedV<T>,
                "Callback cannot capture an unprotected C++ pointer since this "
                "Type is annotated with DISALLOW_UNRETAINED(). Please see "
                "base/functional/disallow_unretained.h for alternatives.");

  // Raw pointer makes sense only if there are no PtrTraits. If there are,
  // it means that a `raw_ptr` is being passed, so use the ctors below instead.
  template <RawPtrTraits PTraits = PtrTraits,
            typename = std::enable_if_t<PTraits == RawPtrTraits::kEmpty>>
  explicit UnretainedWrapper(T* o) : ptr_(o) {}

  // Trick to only instantiate these constructors if they are used. Otherwise,
  // instantiating UnretainedWrapper with a T that is not supported by
  // raw_ptr would trigger raw_ptr<T>'s static_assert.
  template <typename U = T>
  explicit UnretainedWrapper(const raw_ptr<U, PtrTraits>& o) : ptr_(o) {}
  template <typename U = T>
  explicit UnretainedWrapper(raw_ptr<U, PtrTraits>&& o) : ptr_(std::move(o)) {}

  GetPtrType get() const { return GetInternal(ptr_); }

 private:
  // `ptr_` is either a `raw_ptr` or a regular C++ pointer.
  template <typename U>
  static GetPtrType GetInternal(U* ptr) {
    static_assert(std::is_same_v<T, U>);
    return ptr;
  }
  template <typename U, RawPtrTraits Traits>
  static GetPtrType GetInternal(const raw_ptr<U, Traits>& ptr) {
    static_assert(std::is_same_v<T, U>);
    if constexpr (std::is_same_v<UnretainedTrait,
                                 unretained_traits::MayNotDangle>) {
      ptr.ReportIfDangling();
    }
    return ptr;
  }

  // `Unretained()` arguments often dangle by design (a common design pattern
  // is to manage an object's lifetime inside the callback itself, using
  // stateful information), so disable direct dangling pointer detection
  // of `ptr_`.
  //
  // If the callback is invoked, dangling pointer detection will be triggered
  // before invoking the bound functor (unless stated otherwise, see
  // `UnsafeDangling()` and `UnsafeDanglingUntriaged()`), when retrieving the
  // pointer value via `get()` above.
  using StorageType =
      std::conditional_t<raw_ptr_traits::IsSupportedType<T>::value,
                         DanglingRawPtrType,
                         T*>;
  // Avoid converting between different `raw_ptr` types when calling `get()`.
  // It is allowable to convert `raw_ptr<T>` -> `T*`, but not in the other
  // direction. See the comment by `GetPtrType` describing for more details.
  static_assert(std::is_pointer_v<GetPtrType> ||
                std::is_same_v<GetPtrType, StorageType>);
  StorageType ptr_;
};

// Storage type for std::reference_wrapper so `BindState` can internally store
// unprotected references using raw_ref.
//
// std::reference_wrapper<T> and T& do not work, since the reference lifetime is
// not safely protected by MiraclePtr.
//
// UnretainedWrapper<T> and raw_ptr<T> do not work, since BindUnwrapTraits would
// try to pass by T* rather than T&.
template <typename T,
          typename UnretainedTrait,
          RawPtrTraits PtrTraits = RawPtrTraits::kEmpty>
class UnretainedRefWrapper {
 public:
  static_assert(
      TypeSupportsUnretainedV<T>,
      "Callback cannot capture an unprotected C++ reference since this "
      "type is annotated with DISALLOW_UNRETAINED(). Please see "
      "base/functional/disallow_unretained.h for alternatives.");

  // Raw reference makes sense only if there are no PtrTraits. If there are,
  // it means that a `raw_ref` is being passed, so use the ctors below instead.
  template <RawPtrTraits PTraits = PtrTraits,
            typename = std::enable_if_t<PTraits == RawPtrTraits::kEmpty>>
  explicit UnretainedRefWrapper(T& o) : ref_(o) {}

  // Trick to only instantiate these constructors if they are used. Otherwise,
  // instantiating UnretainedWrapper with a T that is not supported by
  // raw_ref would trigger raw_ref<T>'s static_assert.
  template <typename U = T>
  explicit UnretainedRefWrapper(const raw_ref<U, PtrTraits>& o) : ref_(o) {}
  template <typename U = T>
  explicit UnretainedRefWrapper(raw_ref<U, PtrTraits>&& o)
      : ref_(std::move(o)) {}

  T& get() const { return GetInternal(ref_); }

 private:
  // `ref_` is either a `raw_ref` or a regular C++ reference.
  template <typename U>
  static T& GetInternal(U& ref) {
    static_assert(std::is_same_v<T, U>);
    return ref;
  }
  template <typename U, RawPtrTraits Traits>
  static T& GetInternal(const raw_ref<U, Traits>& ref) {
    static_assert(std::is_same_v<T, U>);
    // The ultimate goal is to crash when a callback is invoked with a
    // dangling pointer. This is checked here. For now, it is configured to
    // either crash, DumpWithoutCrashing or be ignored. This depends on the
    // PartitionAllocUnretainedDanglingPtr feature.
    if constexpr (std::is_same_v<UnretainedTrait,
                                 unretained_traits::MayNotDangle>) {
      ref.ReportIfDangling();
    }
    // We can't use operator* here, we need to use raw_ptr's GetForExtraction
    // instead of GetForDereference. If we did use GetForDereference then we'd
    // crash in ASAN builds on calling a bound callback with a dangling
    // reference parameter even if that parameter is not used. This could hide
    // a later unprotected issue that would be reached in release builds.
    return ref.get();
  }

  // `Unretained()` arguments often dangle by design (a common design pattern
  // is to manage an object's lifetime inside the callback itself, using
  // stateful information), so disable direct dangling pointer detection
  // of `ref_`.
  //
  // If the callback is invoked, dangling pointer detection will be triggered
  // before invoking the bound functor (unless stated otherwise, see
  // `UnsafeDangling()` and `UnsafeDanglingUntriaged()`), when retrieving the
  // pointer value via `get()` above.
  using StorageType =
      std::conditional_t<raw_ptr_traits::IsSupportedType<T>::value,
                         raw_ref<T, DisableDanglingPtrDetection>,
                         T&>;

  StorageType ref_;
};

// The class is used to wrap `UnretainedRefWrapper` when the latter is used as
// a method receiver (a reference on `this` argument). This is needed because
// the internal callback mechanism expects the receiver to have the type
// `MyClass*` and to have `operator*`.
// This is used as storage.
template <typename T, typename UnretainedTrait, RawPtrTraits PtrTraits>
class UnretainedRefWrapperReceiver {
 public:
  // NOLINTNEXTLINE(google-explicit-constructor)
  UnretainedRefWrapperReceiver(
      UnretainedRefWrapper<T, UnretainedTrait, PtrTraits>&& o)
      : obj_(std::move(o)) {}
  // NOLINTNEXTLINE(google-explicit-constructor)
  T& operator*() const { return obj_.get(); }

 private:
  UnretainedRefWrapper<T, UnretainedTrait, PtrTraits> obj_;
};

// MethodReceiverStorageType converts the current receiver type to its stored
// type. For instance, it converts pointers to `scoped_refptr`, and wraps
// `UnretainedRefWrapper` to make it compliant with the internal callback
// invocation mechanism.
template <typename T>
struct MethodReceiverStorageType {
  using Type =
      std::conditional_t<IsPointerV<T>, scoped_refptr<RemovePointerT<T>>, T>;
};

template <typename T, typename UnretainedTrait, RawPtrTraits PtrTraits>
struct MethodReceiverStorageType<
    UnretainedRefWrapper<T, UnretainedTrait, PtrTraits>> {
  // We can't use UnretainedRefWrapper as a receiver directly (see
  // UnretainedRefWrapperReceiver for why).
  using Type = UnretainedRefWrapperReceiver<T, UnretainedTrait, PtrTraits>;
};

template <typename T>
class RetainedRefWrapper {
 public:
  explicit RetainedRefWrapper(T* o) : ptr_(o) {}
  explicit RetainedRefWrapper(scoped_refptr<T> o) : ptr_(std::move(o)) {}
  T* get() const { return ptr_.get(); }

 private:
  scoped_refptr<T> ptr_;
};

template <typename T>
struct IgnoreResultHelper {
  explicit IgnoreResultHelper(T functor) : functor_(std::move(functor)) {}
  explicit operator bool() const { return !!functor_; }

  T functor_;
};

template <typename T, typename Deleter = std::default_delete<T>>
class OwnedWrapper {
 public:
  explicit OwnedWrapper(T* o) : ptr_(o) {}
  explicit OwnedWrapper(std::unique_ptr<T, Deleter>&& ptr)
      : ptr_(std::move(ptr)) {}
  T* get() const { return ptr_.get(); }

 private:
  std::unique_ptr<T, Deleter> ptr_;
};

template <typename T>
class OwnedRefWrapper {
 public:
  explicit OwnedRefWrapper(const T& t) : t_(t) {}
  explicit OwnedRefWrapper(T&& t) : t_(std::move(t)) {}
  T& get() const { return t_; }

 private:
  mutable T t_;
};

// PassedWrapper is a copyable adapter for a scoper that ignores const.
//
// It is needed to get around the fact that Bind() takes a const reference to
// all its arguments.  Because Bind() takes a const reference to avoid
// unnecessary copies, it is incompatible with movable-but-not-copyable
// types; doing a destructive "move" of the type into Bind() would violate
// the const correctness.
//
// This conundrum cannot be solved without either C++11 rvalue references or
// a O(2^n) blowup of Bind() templates to handle each combination of regular
// types and movable-but-not-copyable types.  Thus we introduce a wrapper type
// that is copyable to transmit the correct type information down into
// BindState<>. Ignoring const in this type makes sense because it is only
// created when we are explicitly trying to do a destructive move.
//
// Two notes:
//  1) PassedWrapper supports any type that has a move constructor, however
//     the type will need to be specifically allowed in order for it to be
//     bound to a Callback. We guard this explicitly at the call of Passed()
//     to make for clear errors. Things not given to Passed() will be forwarded
//     and stored by value which will not work for general move-only types.
//  2) is_valid_ is distinct from NULL because it is valid to bind a "NULL"
//     scoper to a Callback and allow the Callback to execute once.
template <typename T>
class PassedWrapper {
 public:
  explicit PassedWrapper(T&& scoper) : scoper_(std::move(scoper)) {}
  PassedWrapper(PassedWrapper&& other)
      : is_valid_(other.is_valid_), scoper_(std::move(other.scoper_)) {}
  T Take() const {
    CHECK(is_valid_);
    is_valid_ = false;
    return std::move(scoper_);
  }

 private:
  mutable bool is_valid_ = true;
  mutable T scoper_;
};

template <typename T>
using Unwrapper = BindUnwrapTraits<std::decay_t<T>>;

template <typename T>
decltype(auto) Unwrap(T&& o) {
  return Unwrapper<T>::Unwrap(std::forward<T>(o));
}

// IsWeakMethod is a helper that determine if we are binding a WeakPtr<> to a
// method.  It is used internally by Bind() to select the correct
// InvokeHelper that will no-op itself in the event the WeakPtr<> for
// the target object is invalidated.
//
// The first argument should be the type of the object that will be received by
// the method.
template <bool is_method, typename... Args>
struct IsWeakMethod : std::false_type {};

template <typename T, typename... Args>
struct IsWeakMethod<true, T, Args...> : IsWeakReceiver<T> {};

// Packs a list of types to hold them in a single type.
template <typename... Types>
struct TypeList {};

// Used for DropTypeListItem implementation.
template <size_t n, typename List>
struct DropTypeListItemImpl;

// Do not use enable_if and SFINAE here to avoid MSVC2013 compile failure.
template <size_t n, typename T, typename... List>
struct DropTypeListItemImpl<n, TypeList<T, List...>>
    : DropTypeListItemImpl<n - 1, TypeList<List...>> {};

template <typename T, typename... List>
struct DropTypeListItemImpl<0, TypeList<T, List...>> {
  using Type = TypeList<T, List...>;
};

template <>
struct DropTypeListItemImpl<0, TypeList<>> {
  using Type = TypeList<>;
};

// A type-level function that drops |n| list item from given TypeList.
template <size_t n, typename List>
using DropTypeListItem = typename DropTypeListItemImpl<n, List>::Type;

// Used for TakeTypeListItem implementation.
template <size_t n, typename List, typename... Accum>
struct TakeTypeListItemImpl;

// Do not use enable_if and SFINAE here to avoid MSVC2013 compile failure.
template <size_t n, typename T, typename... List, typename... Accum>
struct TakeTypeListItemImpl<n, TypeList<T, List...>, Accum...>
    : TakeTypeListItemImpl<n - 1, TypeList<List...>, Accum..., T> {};

template <typename T, typename... List, typename... Accum>
struct TakeTypeListItemImpl<0, TypeList<T, List...>, Accum...> {
  using Type = TypeList<Accum...>;
};

template <typename... Accum>
struct TakeTypeListItemImpl<0, TypeList<>, Accum...> {
  using Type = TypeList<Accum...>;
};

// A type-level function that takes first |n| list item from given TypeList.
// E.g. TakeTypeListItem<3, TypeList<A, B, C, D>> is evaluated to
// TypeList<A, B, C>.
template <size_t n, typename List>
using TakeTypeListItem = typename TakeTypeListItemImpl<n, List>::Type;

// Used for ConcatTypeLists implementation.
template <typename List1, typename List2>
struct ConcatTypeListsImpl;

template <typename... Types1, typename... Types2>
struct ConcatTypeListsImpl<TypeList<Types1...>, TypeList<Types2...>> {
  using Type = TypeList<Types1..., Types2...>;
};

// A type-level function that concats two TypeLists.
template <typename List1, typename List2>
using ConcatTypeLists = typename ConcatTypeListsImpl<List1, List2>::Type;

// Used for MakeFunctionType implementation.
template <typename R, typename ArgList>
struct MakeFunctionTypeImpl;

template <typename R, typename... Args>
struct MakeFunctionTypeImpl<R, TypeList<Args...>> {
  // MSVC 2013 doesn't support Type Alias of function types.
  // Revisit this after we update it to newer version.
  typedef R Type(Args...);
};

// A type-level function that constructs a function type that has |R| as its
// return type and has TypeLists items as its arguments.
template <typename R, typename ArgList>
using MakeFunctionType = typename MakeFunctionTypeImpl<R, ArgList>::Type;

// Used for ExtractArgs and ExtractReturnType.
template <typename Signature>
struct ExtractArgsImpl;

template <typename R, typename... Args>
struct ExtractArgsImpl<R(Args...)> {
  using ReturnType = R;
  using ArgsList = TypeList<Args...>;
};

// A type-level function that extracts function arguments into a TypeList.
// E.g. ExtractArgs<R(A, B, C)> is evaluated to TypeList<A, B, C>.
template <typename Signature>
using ExtractArgs = typename ExtractArgsImpl<Signature>::ArgsList;

// A type-level function that extracts the return type of a function.
// E.g. ExtractReturnType<R(A, B, C)> is evaluated to R.
template <typename Signature>
using ExtractReturnType = typename ExtractArgsImpl<Signature>::ReturnType;

template <typename Callable,
          typename Signature = decltype(&Callable::operator())>
struct ExtractCallableRunTypeImpl;

template <typename Callable, typename R, typename... Args>
struct ExtractCallableRunTypeImpl<Callable, R (Callable::*)(Args...)> {
  using Type = R(Args...);
};

template <typename Callable, typename R, typename... Args>
struct ExtractCallableRunTypeImpl<Callable, R (Callable::*)(Args...) const> {
  using Type = R(Args...);
};

template <typename Callable, typename R, typename... Args>
struct ExtractCallableRunTypeImpl<Callable, R (Callable::*)(Args...) noexcept> {
  using Type = R(Args...);
};

template <typename Callable, typename R, typename... Args>
struct ExtractCallableRunTypeImpl<Callable,
                                  R (Callable::*)(Args...) const noexcept> {
  using Type = R(Args...);
};

// Evaluated to RunType of the given callable type.
// Example:
//   auto f = [](int, char*) { return 0.1; };
//   ExtractCallableRunType<decltype(f)>
//   is evaluated to
//   double(int, char*);
template <typename Callable>
using ExtractCallableRunType =
    typename ExtractCallableRunTypeImpl<Callable>::Type;

// IsCallableObject<Functor> is std::true_type if |Functor| has operator().
// Otherwise, it's std::false_type.
// Example:
//   IsCallableObject<void(*)()>::value is false.
//
//   struct Foo {};
//   IsCallableObject<void(Foo::*)()>::value is false.
//
//   int i = 0;
//   auto f = [i]() {};
//   IsCallableObject<decltype(f)>::value is false.
template <typename Functor, typename SFINAE = void>
struct IsCallableObject : std::false_type {};

template <typename Callable>
struct IsCallableObject<Callable, std::void_t<decltype(&Callable::operator())>>
    : std::true_type {};

// HasRefCountedTypeAsRawPtr inherits from true_type when any of the |Args| is a
// raw pointer to a RefCounted type.
template <typename... Ts>
struct HasRefCountedTypeAsRawPtr
    : std::disjunction<NeedsScopedRefptrButGetsRawPtr<Ts>...> {};

// ForceVoidReturn<>
//
// Set of templates that support forcing the function return type to void.
template <typename Sig>
struct ForceVoidReturn;

template <typename R, typename... Args>
struct ForceVoidReturn<R(Args...)> {
  using RunType = void(Args...);
};

// FunctorTraits<>
//
// See description at top of file.
template <typename Functor, typename SFINAE>
struct FunctorTraits;

// For callable types.
// This specialization handles lambdas (captureless and capturing) and functors
// with a call operator. Capturing lambdas and stateful functors are explicitly
// disallowed by BindImpl().
//
// Example:
//
//   // Captureless lambdas are allowed.
//   []() {return 42;};
//
//   // Capturing lambdas are *not* allowed.
//   int x;
//   [x]() {return x;};
//
//   // Any empty class with operator() is allowed.
//   struct Foo {
//     void operator()() const {}
//     // No non-static member variable and no virtual functions.
//   };
template <typename Functor>
struct FunctorTraits<Functor,
                     std::enable_if_t<IsCallableObject<Functor>::value>> {
  using RunType = ExtractCallableRunType<Functor>;
  static constexpr bool is_method = false;
  static constexpr bool is_nullable = false;
  static constexpr bool is_callback = false;
  static constexpr bool is_stateless = std::is_empty_v<Functor>;

  template <typename RunFunctor, typename... RunArgs>
  static ExtractReturnType<RunType> Invoke(RunFunctor&& functor,
                                           RunArgs&&... args) {
    return std::forward<RunFunctor>(functor)(std::forward<RunArgs>(args)...);
  }
};

// For functions.
template <typename R, typename... Args>
struct FunctorTraits<R (*)(Args...)> {
  using RunType = R(Args...);
  static constexpr bool is_method = false;
  static constexpr bool is_nullable = true;
  static constexpr bool is_callback = false;
  static constexpr bool is_stateless = true;

  template <typename Function, typename... RunArgs>
  static R Invoke(Function&& function, RunArgs&&... args) {
    return std::forward<Function>(function)(std::forward<RunArgs>(args)...);
  }
};

#if BUILDFLAG(IS_WIN) && !defined(ARCH_CPU_64_BITS)

// For functions.
template <typename R, typename... Args>
struct FunctorTraits<R(__stdcall*)(Args...)> {
  using RunType = R(Args...);
  static constexpr bool is_method = false;
  static constexpr bool is_nullable = true;
  static constexpr bool is_callback = false;
  static constexpr bool is_stateless = true;

  template <typename... RunArgs>
  static R Invoke(R(__stdcall* function)(Args...), RunArgs&&... args) {
    return function(std::forward<RunArgs>(args)...);
  }
};

// For functions.
template <typename R, typename... Args>
struct FunctorTraits<R(__fastcall*)(Args...)> {
  using RunType = R(Args...);
  static constexpr bool is_method = false;
  static constexpr bool is_nullable = true;
  static constexpr bool is_callback = false;
  static constexpr bool is_stateless = true;

  template <typename... RunArgs>
  static R Invoke(R(__fastcall* function)(Args...), RunArgs&&... args) {
    return function(std::forward<RunArgs>(args)...);
  }
};

#endif  // BUILDFLAG(IS_WIN) && !defined(ARCH_CPU_64_BITS)

#if BUILDFLAG(IS_APPLE)

// Support for Objective-C blocks. There are two implementation depending
// on whether Automated Reference Counting (ARC) is enabled. When ARC is
// enabled, then the block itself can be bound as the compiler will ensure
// its lifetime will be correctly managed. Otherwise, require the block to
// be wrapped in a base::mac::ScopedBlock (via base::RetainBlock) that will
// correctly manage the block lifetime.
//
// The two implementation ensure that the One Definition Rule (ODR) is not
// broken (it is not possible to write a template base::RetainBlock that would
// work correctly both with ARC enabled and disabled).

#if HAS_FEATURE(objc_arc)

template <typename R, typename... Args>
struct FunctorTraits<R (^)(Args...)> {
  using RunType = R(Args...);
  static constexpr bool is_method = false;
  static constexpr bool is_nullable = true;
  static constexpr bool is_callback = false;
  static constexpr bool is_stateless = true;

  template <typename BlockType, typename... RunArgs>
  static R Invoke(BlockType&& block, RunArgs&&... args) {
    // According to LLVM documentation (§ 6.3), "local variables of automatic
    // storage duration do not have precise lifetime." Use objc_precise_lifetime
    // to ensure that the Objective-C block is not deallocated until it has
    // finished executing even if the Callback<> is destroyed during the block
    // execution.
    // https://clang.llvm.org/docs/AutomaticReferenceCounting.html#precise-lifetime-semantics
    __attribute__((objc_precise_lifetime)) R (^scoped_block)(Args...) = block;
    return scoped_block(std::forward<RunArgs>(args)...);
  }
};

#else  // HAS_FEATURE(objc_arc)

template <typename R, typename... Args>
struct FunctorTraits<base::mac::ScopedBlock<R (^)(Args...)>> {
  using RunType = R(Args...);
  static constexpr bool is_method = false;
  static constexpr bool is_nullable = true;
  static constexpr bool is_callback = false;
  static constexpr bool is_stateless = true;

  template <typename BlockType, typename... RunArgs>
  static R Invoke(BlockType&& block, RunArgs&&... args) {
    // Copy the block to ensure that the Objective-C block is not deallocated
    // until it has finished executing even if the Callback<> is destroyed
    // during the block execution.
    base::mac::ScopedBlock<R (^)(Args...)> scoped_block(block);
    return scoped_block.get()(std::forward<RunArgs>(args)...);
  }
};

#endif  // HAS_FEATURE(objc_arc)
#endif  // BUILDFLAG(IS_APPLE)

// For methods.
template <typename R, typename Receiver, typename... Args>
struct FunctorTraits<R (Receiver::*)(Args...)> {
  using RunType = R(Receiver*, Args...);
  static constexpr bool is_method = true;
  static constexpr bool is_nullable = true;
  static constexpr bool is_callback = false;
  static constexpr bool is_stateless = true;

  template <typename Method, typename ReceiverPtr, typename... RunArgs>
  static R Invoke(Method method,
                  ReceiverPtr&& receiver_ptr,
                  RunArgs&&... args) {
    return ((*receiver_ptr).*method)(std::forward<RunArgs>(args)...);
  }
};

// For const methods.
template <typename R, typename Receiver, typename... Args>
struct FunctorTraits<R (Receiver::*)(Args...) const> {
  using RunType = R(const Receiver*, Args...);
  static constexpr bool is_method = true;
  static constexpr bool is_nullable = true;
  static constexpr bool is_callback = false;
  static constexpr bool is_stateless = true;

  template <typename Method, typename ReceiverPtr, typename... RunArgs>
  static R Invoke(Method method,
                  ReceiverPtr&& receiver_ptr,
                  RunArgs&&... args) {
    return ((*receiver_ptr).*method)(std::forward<RunArgs>(args)...);
  }
};

#if BUILDFLAG(IS_WIN) && !defined(ARCH_CPU_64_BITS)

// For __stdcall methods.
template <typename R, typename Receiver, typename... Args>
struct FunctorTraits<R (__stdcall Receiver::*)(Args...)> {
  using RunType = R(Receiver*, Args...);
  static constexpr bool is_method = true;
  static constexpr bool is_nullable = true;
  static constexpr bool is_callback = false;
  static constexpr bool is_stateless = true;

  template <typename Method, typename ReceiverPtr, typename... RunArgs>
  static R Invoke(Method method,
                  ReceiverPtr&& receiver_ptr,
                  RunArgs&&... args) {
    return ((*receiver_ptr).*method)(std::forward<RunArgs>(args)...);
  }
};

// For __stdcall const methods.
template <typename R, typename Receiver, typename... Args>
struct FunctorTraits<R (__stdcall Receiver::*)(Args...) const> {
  using RunType = R(const Receiver*, Args...);
  static constexpr bool is_method = true;
  static constexpr bool is_nullable = true;
  static constexpr bool is_callback = false;
  static constexpr bool is_stateless = true;

  template <typename Method, typename ReceiverPtr, typename... RunArgs>
  static R Invoke(Method method,
                  ReceiverPtr&& receiver_ptr,
                  RunArgs&&... args) {
    return ((*receiver_ptr).*method)(std::forward<RunArgs>(args)...);
  }
};

#endif  // BUILDFLAG(IS_WIN) && !defined(ARCH_CPU_64_BITS)

#ifdef __cpp_noexcept_function_type
// noexcept makes a distinct function type in C++17.
// I.e. `void(*)()` and `void(*)() noexcept` are same in pre-C++17, and
// different in C++17.
template <typename R, typename... Args>
struct FunctorTraits<R (*)(Args...) noexcept> : FunctorTraits<R (*)(Args...)> {
};

template <typename R, typename Receiver, typename... Args>
struct FunctorTraits<R (Receiver::*)(Args...) noexcept>
    : FunctorTraits<R (Receiver::*)(Args...)> {};

template <typename R, typename Receiver, typename... Args>
struct FunctorTraits<R (Receiver::*)(Args...) const noexcept>
    : FunctorTraits<R (Receiver::*)(Args...) const> {};
#endif

// For IgnoreResults.
template <typename T>
struct FunctorTraits<IgnoreResultHelper<T>> : FunctorTraits<T> {
  using RunType =
      typename ForceVoidReturn<typename FunctorTraits<T>::RunType>::RunType;

  template <typename IgnoreResultType, typename... RunArgs>
  static void Invoke(IgnoreResultType&& ignore_result_helper,
                     RunArgs&&... args) {
    FunctorTraits<T>::Invoke(
        std::forward<IgnoreResultType>(ignore_result_helper).functor_,
        std::forward<RunArgs>(args)...);
  }
};

// For OnceCallbacks.
template <typename R, typename... Args>
struct FunctorTraits<OnceCallback<R(Args...)>> {
  using RunType = R(Args...);
  static constexpr bool is_method = false;
  static constexpr bool is_nullable = true;
  static constexpr bool is_callback = true;
  static constexpr bool is_stateless = true;

  template <typename CallbackType, typename... RunArgs>
  static R Invoke(CallbackType&& callback, RunArgs&&... args) {
    DCHECK(!callback.is_null());
    return std::forward<CallbackType>(callback).Run(
        std::forward<RunArgs>(args)...);
  }
};

// For RepeatingCallbacks.
template <typename R, typename... Args>
struct FunctorTraits<RepeatingCallback<R(Args...)>> {
  using RunType = R(Args...);
  static constexpr bool is_method = false;
  static constexpr bool is_nullable = true;
  static constexpr bool is_callback = true;
  static constexpr bool is_stateless = true;

  template <typename CallbackType, typename... RunArgs>
  static R Invoke(CallbackType&& callback, RunArgs&&... args) {
    DCHECK(!callback.is_null());
    return std::forward<CallbackType>(callback).Run(
        std::forward<RunArgs>(args)...);
  }
};

template <typename Functor>
using MakeFunctorTraits = FunctorTraits<std::decay_t<Functor>>;

// StorageTraits<>
//
// See description at top of file.
template <typename T>
struct StorageTraits {
  using Type = T;
};

// For T*, store as UnretainedWrapper<T> for safety, as it internally uses
// raw_ptr<T> (when possible).
template <typename T>
struct StorageTraits<T*> {
  using Type = UnretainedWrapper<T, unretained_traits::MayNotDangle>;
};

// For raw_ptr<T>, store as UnretainedWrapper<T> for safety. This may seem
// contradictory, but this ensures guaranteed protection for the pointer even
// during execution of callbacks with parameters of type raw_ptr<T>.
template <typename T, RawPtrTraits PtrTraits>
struct StorageTraits<raw_ptr<T, PtrTraits>> {
  using Type = UnretainedWrapper<T, unretained_traits::MayNotDangle, PtrTraits>;
};

// Unwrap std::reference_wrapper and store it in a custom wrapper so that
// references are also protected with raw_ptr<T>.
template <typename T>
struct StorageTraits<std::reference_wrapper<T>> {
  using Type = UnretainedRefWrapper<T, unretained_traits::MayNotDangle>;
};

template <typename T>
using MakeStorageType = typename StorageTraits<std::decay_t<T>>::Type;

// InvokeHelper<>
//
// There are 2 logical InvokeHelper<> specializations: normal, WeakCalls.
//
// The normal type just calls the underlying runnable.
//
// WeakCalls need special syntax that is applied to the first argument to check
// if they should no-op themselves.
template <bool is_weak_call, typename ReturnType, size_t... indices>
struct InvokeHelper;

template <typename ReturnType, size_t... indices>
struct InvokeHelper<false, ReturnType, indices...> {
  template <typename Functor, typename BoundArgsTuple, typename... RunArgs>
  static inline ReturnType MakeItSo(Functor&& functor,
                                    BoundArgsTuple&& bound,
                                    RunArgs&&... args) {
    using Traits = MakeFunctorTraits<Functor>;
    return Traits::Invoke(
        std::forward<Functor>(functor),
        Unwrap(std::get<indices>(std::forward<BoundArgsTuple>(bound)))...,
        std::forward<RunArgs>(args)...);
  }
};

template <typename ReturnType, size_t index_target, size_t... index_tail>
struct InvokeHelper<true, ReturnType, index_target, index_tail...> {
  // WeakCalls are only supported for functions with a void return type.
  // Otherwise, the function result would be undefined if the WeakPtr<>
  // is invalidated.
  static_assert(std::is_void_v<ReturnType>,
                "weak_ptrs can only bind to methods without return values");

  template <typename Functor, typename BoundArgsTuple, typename... RunArgs>
  static inline void MakeItSo(Functor&& functor,
                              BoundArgsTuple&& bound,
                              RunArgs&&... args) {
    static_assert(index_target == 0);
    // Note the validity of the weak pointer should be tested _after_ it is
    // unwrapped, otherwise it creates a race for weak pointer implementations
    // that allow cross-thread usage and perform `Lock()` in Unwrap() traits.
    const auto& target = Unwrap(std::get<0>(bound));
    if (!target) {
      return;
    }
    using Traits = MakeFunctorTraits<Functor>;
    Traits::Invoke(
        std::forward<Functor>(functor), target,
        Unwrap(std::get<index_tail>(std::forward<BoundArgsTuple>(bound)))...,
        std::forward<RunArgs>(args)...);
  }
};

// Invoker<>
//
// See description at the top of the file.
template <typename StorageType, typename UnboundRunType>
struct Invoker;

template <typename StorageType, typename R, typename... UnboundArgs>
struct Invoker<StorageType, R(UnboundArgs...)> {
  static R RunOnce(BindStateBase* base,
                   PassingType<UnboundArgs>... unbound_args) {
    // Local references to make debugger stepping easier. If in a debugger,
    // you really want to warp ahead and step through the
    // InvokeHelper<>::MakeItSo() call below.
    StorageType* storage = static_cast<StorageType*>(base);
    static constexpr size_t num_bound_args =
        std::tuple_size_v<decltype(storage->bound_args_)>;
    return RunImpl(std::move(storage->functor_),
                   std::move(storage->bound_args_),
                   std::make_index_sequence<num_bound_args>(),
                   std::forward<UnboundArgs>(unbound_args)...);
  }

  static R Run(BindStateBase* base, PassingType<UnboundArgs>... unbound_args) {
    // Local references to make debugger stepping easier. If in a debugger,
    // you really want to warp ahead and step through the
    // InvokeHelper<>::MakeItSo() call below.
    const StorageType* storage = static_cast<StorageType*>(base);
    static constexpr size_t num_bound_args =
        std::tuple_size_v<decltype(storage->bound_args_)>;
    return RunImpl(storage->functor_, storage->bound_args_,
                   std::make_index_sequence<num_bound_args>(),
                   std::forward<UnboundArgs>(unbound_args)...);
  }

 private:
  template <typename Functor, typename BoundArgsTuple, size_t... indices>
  static inline R RunImpl(Functor&& functor,
                          BoundArgsTuple&& bound,
                          std::index_sequence<indices...> seq,
                          UnboundArgs&&... unbound_args) {
    static constexpr bool is_method = MakeFunctorTraits<Functor>::is_method;

    using DecayedArgsTuple = std::decay_t<BoundArgsTuple>;

#if BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
    RawPtrAsanBoundArgTracker raw_ptr_asan_bound_arg_tracker;
    raw_ptr_asan_bound_arg_tracker.AddArgs(
        std::get<indices>(std::forward<BoundArgsTuple>(bound))...,
        std::forward<UnboundArgs>(unbound_args)...);
#endif  // BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)

    static constexpr bool is_weak_call =
        IsWeakMethod<is_method,
                     std::tuple_element_t<indices, DecayedArgsTuple>...>();

    // Do not `Unwrap()` here, as that immediately triggers dangling pointer
    // detection. Dangling pointer detection should only be triggered if the
    // callback is not cancelled, but cancellation status is not determined
    // until later inside the InvokeHelper::MakeItSo specialization for weak
    // calls.
    //
    // Dangling pointers when invoking a cancelled callback are not considered
    // a memory safety error because protecting raw pointers usage with weak
    // receivers (where the weak receiver usually own the pointed objects) is a
    // common and broadly used pattern in the codebase.
    return InvokeHelper<is_weak_call, R, indices...>::MakeItSo(
        std::forward<Functor>(functor), std::forward<BoundArgsTuple>(bound),
        std::forward<UnboundArgs>(unbound_args)...);
  }
};

// Extracts necessary type info from Functor and BoundArgs.
// Used to implement MakeUnboundRunType, BindOnce and BindRepeating.
template <typename Functor, typename... BoundArgs>
struct BindTypeHelper {
  static constexpr size_t num_bounds = sizeof...(BoundArgs);
  using FunctorTraits = MakeFunctorTraits<Functor>;

  // Example:
  //   When Functor is `double (Foo::*)(int, const std::string&)`, and BoundArgs
  //   is a template pack of `Foo*` and `int16_t`:
  //    - RunType is `double(Foo*, int, const std::string&)`,
  //    - ReturnType is `double`,
  //    - RunParamsList is `TypeList<Foo*, int, const std::string&>`,
  //    - BoundParamsList is `TypeList<Foo*, int>`,
  //    - UnboundParamsList is `TypeList<const std::string&>`,
  //    - BoundArgsList is `TypeList<Foo*, int16_t>`,
  //    - UnboundRunType is `double(const std::string&)`.
  using RunType = typename FunctorTraits::RunType;
  using ReturnType = ExtractReturnType<RunType>;

  using RunParamsList = ExtractArgs<RunType>;
  using BoundParamsList = TakeTypeListItem<num_bounds, RunParamsList>;
  using UnboundParamsList = DropTypeListItem<num_bounds, RunParamsList>;

  using BoundArgsList = TypeList<BoundArgs...>;

  using UnboundRunType = MakeFunctionType<ReturnType, UnboundParamsList>;
};

template <typename Functor>
std::enable_if_t<FunctorTraits<Functor>::is_nullable, bool> IsNull(
    const Functor& functor) {
  return !functor;
}

template <typename Functor>
std::enable_if_t<!FunctorTraits<Functor>::is_nullable, bool> IsNull(
    const Functor&) {
  return false;
}

// Used by QueryCancellationTraits below.
template <typename Functor, typename BoundArgsTuple, size_t... indices>
bool QueryCancellationTraitsImpl(BindStateBase::CancellationQueryMode mode,
                                 const Functor& functor,
                                 const BoundArgsTuple& bound_args,
                                 std::index_sequence<indices...>) {
  switch (mode) {
    case BindStateBase::IS_CANCELLED:
      return CallbackCancellationTraits<Functor, BoundArgsTuple>::IsCancelled(
          functor, std::get<indices>(bound_args)...);
    case BindStateBase::MAYBE_VALID:
      return CallbackCancellationTraits<Functor, BoundArgsTuple>::MaybeValid(
          functor, std::get<indices>(bound_args)...);
  }
  NOTREACHED();
  return false;
}

// Relays |base| to corresponding CallbackCancellationTraits<>::Run(). Returns
// true if the callback |base| represents is canceled.
template <typename BindStateType>
bool QueryCancellationTraits(const BindStateBase* base,
                             BindStateBase::CancellationQueryMode mode) {
  const BindStateType* storage = static_cast<const BindStateType*>(base);
  static constexpr size_t num_bound_args =
      std::tuple_size_v<decltype(storage->bound_args_)>;
  return QueryCancellationTraitsImpl(
      mode, storage->functor_, storage->bound_args_,
      std::make_index_sequence<num_bound_args>());
}

// The base case of BanUnconstructedRefCountedReceiver that checks nothing.
template <typename Functor, typename Receiver, typename... Unused>
std::enable_if_t<
    !(MakeFunctorTraits<Functor>::is_method &&
      IsPointerV<std::decay_t<Receiver>> &&
      IsRefCountedType<RemovePointerT<std::decay_t<Receiver>>>::value)>
BanUnconstructedRefCountedReceiver(const Receiver& receiver, Unused&&...) {}

template <typename Functor>
void BanUnconstructedRefCountedReceiver() {}

// Asserts that Callback is not the first owner of a ref-counted receiver.
template <typename Functor, typename Receiver, typename... Unused>
std::enable_if_t<
    MakeFunctorTraits<Functor>::is_method &&
    IsPointerV<std::decay_t<Receiver>> &&
    IsRefCountedType<RemovePointerT<std::decay_t<Receiver>>>::value>
BanUnconstructedRefCountedReceiver(const Receiver& receiver, Unused&&...) {
  DCHECK(receiver);

  // It's error prone to make the implicit first reference to ref-counted types.
  // In the example below, base::BindOnce() would make the implicit first
  // reference to the ref-counted Foo. If PostTask() failed or the posted task
  // ran fast enough, the newly created instance could be destroyed before `oo`
  // makes another reference.
  //   Foo::Foo() {
  //     base::ThreadPool::PostTask(FROM_HERE, base::BindOnce(&Foo::Bar, this));
  //   }
  //
  //   scoped_refptr<Foo> oo = new Foo();
  //
  // Hence, base::Bind{Once,Repeating}() refuses to create the first reference
  // to ref-counted objects, and DCHECK()s otherwise. As above, that typically
  // happens around PostTask() in their constructor, and such objects can be
  // destroyed before `new` returns if the task resolves fast enough.
  //
  // Instead of doing the above, please consider adding a static constructor,
  // and keep the first reference alive explicitly.
  //   // static
  //   scoped_refptr<Foo> Foo::Create() {
  //     auto foo = base::WrapRefCounted(new Foo());
  //     base::ThreadPool::PostTask(FROM_HERE, base::BindOnce(&Foo::Bar, foo));
  //     return foo;
  //   }
  //
  //   Foo::Foo() {}
  //
  //   scoped_refptr<Foo> oo = Foo::Create();
  //
  DCHECK(receiver->HasAtLeastOneRef());
}

// BindState<>
//
// This stores all the state passed into Bind().
template <typename Functor, typename... BoundArgs>
struct BindState final : BindStateBase {
  using IsCancellable = std::bool_constant<
      CallbackCancellationTraits<Functor,
                                 std::tuple<BoundArgs...>>::is_cancellable>;
  template <typename ForwardFunctor, typename... ForwardBoundArgs>
  static BindState* Create(BindStateBase::InvokeFuncStorage invoke_func,
                           ForwardFunctor&& functor,
                           ForwardBoundArgs&&... bound_args) {
    // Ban ref counted receivers that were not yet fully constructed to avoid
    // a common pattern of racy situation.
    BanUnconstructedRefCountedReceiver<ForwardFunctor>(bound_args...);

    // IsCancellable is std::false_type if
    // CallbackCancellationTraits<>::IsCancelled returns always false.
    // Otherwise, it's std::true_type.
    return new BindState(IsCancellable{}, invoke_func,
                         std::forward<ForwardFunctor>(functor),
                         std::forward<ForwardBoundArgs>(bound_args)...);
  }

  Functor functor_;
  std::tuple<BoundArgs...> bound_args_;

 private:
  static constexpr bool is_nested_callback =
      MakeFunctorTraits<Functor>::is_callback;

  template <typename ForwardFunctor, typename... ForwardBoundArgs>
  explicit BindState(std::true_type,
                     BindStateBase::InvokeFuncStorage invoke_func,
                     ForwardFunctor&& functor,
                     ForwardBoundArgs&&... bound_args)
      : BindStateBase(invoke_func,
                      &Destroy,
                      &QueryCancellationTraits<BindState>),
        functor_(std::forward<ForwardFunctor>(functor)),
        bound_args_(std::forward<ForwardBoundArgs>(bound_args)...) {
    // We check the validity of nested callbacks (e.g., Bind(callback, ...)) in
    // release builds to avoid null pointers from ending up in posted tasks,
    // causing hard-to-diagnose crashes. Ideally we'd do this for all functors
    // here, but that would have a large binary size impact.
    if (is_nested_callback) {
      CHECK(!IsNull(functor_));
    } else {
      DCHECK(!IsNull(functor_));
    }
  }

  template <typename ForwardFunctor, typename... ForwardBoundArgs>
  explicit BindState(std::false_type,
                     BindStateBase::InvokeFuncStorage invoke_func,
                     ForwardFunctor&& functor,
                     ForwardBoundArgs&&... bound_args)
      : BindStateBase(invoke_func, &Destroy),
        functor_(std::forward<ForwardFunctor>(functor)),
        bound_args_(std::forward<ForwardBoundArgs>(bound_args)...) {
    // See above for CHECK/DCHECK rationale.
    if (is_nested_callback) {
      CHECK(!IsNull(functor_));
    } else {
      DCHECK(!IsNull(functor_));
    }
  }

  ~BindState() = default;

  static void Destroy(const BindStateBase* self) {
    delete static_cast<const BindState*>(self);
  }
};

// Used to implement MakeBindStateType.
template <bool is_method, typename Functor, typename... BoundArgs>
struct MakeBindStateTypeImpl;

template <typename Functor, typename... BoundArgs>
struct MakeBindStateTypeImpl<false, Functor, BoundArgs...> {
  static_assert(!HasRefCountedTypeAsRawPtr<std::decay_t<BoundArgs>...>::value,
                "A parameter is a refcounted type and needs scoped_refptr.");
  using Type = BindState<std::decay_t<Functor>, MakeStorageType<BoundArgs>...>;
};

template <typename Functor>
struct MakeBindStateTypeImpl<true, Functor> {
  using Type = BindState<std::decay_t<Functor>>;
};

template <typename Functor, typename Receiver, typename... BoundArgs>
struct MakeBindStateTypeImpl<true, Functor, Receiver, BoundArgs...> {
 private:
  using DecayedReceiver = std::decay_t<Receiver>;
  static_assert(!std::is_array_v<std::remove_reference_t<Receiver>>,
                "First bound argument to a method cannot be an array.");
  static_assert(
      !IsRawRefV<DecayedReceiver>,
      "Receivers may not be raw_ref<T>. If using a raw_ref<T> here is safe"
      " and has no lifetime concerns, use base::Unretained() and document why"
      " it's safe.");
  static_assert(
      !IsPointerV<DecayedReceiver> ||
          IsRefCountedType<RemovePointerT<DecayedReceiver>>::value,
      "Receivers may not be raw pointers. If using a raw pointer here is safe"
      " and has no lifetime concerns, use base::Unretained() and document why"
      " it's safe.");

  static_assert(!HasRefCountedTypeAsRawPtr<std::decay_t<BoundArgs>...>::value,
                "A parameter is a refcounted type and needs scoped_refptr.");

  using ReceiverStorageType =
      typename MethodReceiverStorageType<DecayedReceiver>::Type;

 public:
  using Type = BindState<std::decay_t<Functor>,
                         ReceiverStorageType,
                         MakeStorageType<BoundArgs>...>;
};

template <typename Functor, typename... BoundArgs>
using MakeBindStateType =
    typename MakeBindStateTypeImpl<MakeFunctorTraits<Functor>::is_method,
                                   Functor,
                                   BoundArgs...>::Type;

// Returns a RunType of bound functor.
// E.g. MakeUnboundRunType<R(A, B, C), A, B> is evaluated to R(C).
template <typename Functor, typename... BoundArgs>
using MakeUnboundRunType =
    typename BindTypeHelper<Functor, BoundArgs...>::UnboundRunType;

// The implementation of TransformToUnwrappedType below.
template <bool is_once, typename T>
struct TransformToUnwrappedTypeImpl;

template <typename T>
struct TransformToUnwrappedTypeImpl<true, T> {
  using StoredType = std::decay_t<T>;
  using ForwardType = StoredType&&;
  using Unwrapped = decltype(Unwrap(std::declval<ForwardType>()));
};

template <typename T>
struct TransformToUnwrappedTypeImpl<false, T> {
  using StoredType = std::decay_t<T>;
  using ForwardType = const StoredType&;
  using Unwrapped = decltype(Unwrap(std::declval<ForwardType>()));
};

// Transform |T| into `Unwrapped` type, which is passed to the target function.
// Example:
//   In is_once == true case,
//     `int&&` -> `int&&`,
//     `const int&` -> `int&&`,
//     `OwnedWrapper<int>&` -> `int*&&`.
//   In is_once == false case,
//     `int&&` -> `const int&`,
//     `const int&` -> `const int&`,
//     `OwnedWrapper<int>&` -> `int* const &`.
template <bool is_once, typename T>
using TransformToUnwrappedType =
    typename TransformToUnwrappedTypeImpl<is_once, T>::Unwrapped;

// Transforms |Args| into `Unwrapped` types, and packs them into a TypeList.
// If |is_method| is true, tries to dereference the first argument to support
// smart pointers.
template <bool is_once, bool is_method, typename... Args>
struct MakeUnwrappedTypeListImpl {
  using Type = TypeList<TransformToUnwrappedType<is_once, Args>...>;
};

// Performs special handling for this pointers.
// Example:
//   int* -> int*,
//   std::unique_ptr<int> -> int*.
template <bool is_once, typename Receiver, typename... Args>
struct MakeUnwrappedTypeListImpl<is_once, true, Receiver, Args...> {
  using ReceiverStorageType =
      typename MethodReceiverStorageType<std::decay_t<Receiver>>::Type;
  using UnwrappedReceiver =
      TransformToUnwrappedType<is_once, ReceiverStorageType>;
  using Type = TypeList<decltype(&*std::declval<UnwrappedReceiver>()),
                        TransformToUnwrappedType<is_once, Args>...>;
};

template <bool is_once, bool is_method, typename... Args>
using MakeUnwrappedTypeList =
    typename MakeUnwrappedTypeListImpl<is_once, is_method, Args...>::Type;

// IsOnceCallback<T> is a std::true_type if |T| is a OnceCallback.
template <typename T>
struct IsOnceCallback : std::false_type {};

template <typename Signature>
struct IsOnceCallback<OnceCallback<Signature>> : std::true_type {};

// IsUnretainedMayDangle is true if StorageType is of type
// `UnretainedWrapper<T, unretained_traits::MayDangle, PtrTraits>.
// Note that it is false for unretained_traits::MayDangleUntriaged.
template <typename StorageType>
inline constexpr bool IsUnretainedMayDangle = false;
template <typename T, RawPtrTraits PtrTraits>
inline constexpr bool IsUnretainedMayDangle<
    UnretainedWrapper<T, unretained_traits::MayDangle, PtrTraits>> = true;

// UnretainedAndRawPtrHaveCompatibleTraits is true if StorageType is of type
// `UnretainedWrapper<T, unretained_traits::MayDangle, PtrTraits1>` and
// FunctionParamType is of type `raw_ptr<T, PtrTraits2>`, and the former's
// ::GetPtrType is the same type as the latter.
template <typename StorageType, typename FunctionParamType>
inline constexpr bool UnretainedAndRawPtrHaveCompatibleTraits = false;
template <typename T,
          RawPtrTraits PtrTraitsInUnretained,
          RawPtrTraits PtrTraitsInReceiver>
inline constexpr bool UnretainedAndRawPtrHaveCompatibleTraits<
    UnretainedWrapper<T, unretained_traits::MayDangle, PtrTraitsInUnretained>,
    raw_ptr<T, PtrTraitsInReceiver>> =
    std::is_same_v<
        typename UnretainedWrapper<T,
                                   unretained_traits::MayDangle,
                                   PtrTraitsInUnretained>::GetPtrType,
        raw_ptr<T, PtrTraitsInReceiver>>;

// Helpers to make error messages slightly more readable.
template <int i>
struct BindArgument {
  template <typename ForwardingType>
  struct ForwardedAs {
    template <typename FunctorParamType>
    struct ToParamWithType {
      static constexpr bool kNotARawPtr = !IsRawPtrV<FunctorParamType>;

      static constexpr bool kCanBeForwardedToBoundFunctor =
          std::is_constructible_v<FunctorParamType, ForwardingType>;

      // If the bound type can't be forwarded then test if `FunctorParamType` is
      // a non-const lvalue reference and a reference to the unwrapped type
      // *could* have been successfully forwarded.
      static constexpr bool kNonConstRefParamMustBeWrapped =
          kCanBeForwardedToBoundFunctor ||
          !(std::is_lvalue_reference_v<FunctorParamType> &&
            !std::is_const_v<std::remove_reference_t<FunctorParamType>> &&
            std::is_convertible_v<std::decay_t<ForwardingType>&,
                                  FunctorParamType>);

      // Note that this intentionally drops the const qualifier from
      // `ForwardingType`, to test if it *could* have been successfully
      // forwarded if `Passed()` had been used.
      static constexpr bool kMoveOnlyTypeMustUseBasePassed =
          kCanBeForwardedToBoundFunctor ||
          !std::is_constructible_v<FunctorParamType,
                                   std::decay_t<ForwardingType>&&>;
    };
  };

  template <typename BoundAsType>
  struct BoundAs {
    template <typename StorageType>
    struct StoredAs {
      static constexpr bool kBindArgumentCanBeCaptured =
          std::is_constructible_v<StorageType, BoundAsType>;
      // Note that this intentionally drops the const qualifier from
      // `BoundAsType`, to test if it *could* have been successfully bound if
      // `std::move()` had been used.
      static constexpr bool kMoveOnlyTypeMustUseStdMove =
          kBindArgumentCanBeCaptured ||
          !std::is_constructible_v<StorageType, std::decay_t<BoundAsType>&&>;
    };
  };

  template <typename FunctionParamType>
  struct ToParamWithType {
    template <typename StorageType>
    struct StoredAs {
      template <bool is_method>
      // true if we are handling `this` parameter.
      static constexpr bool kParamIsThisPointer = is_method && i == 0;
      // true if the current parameter is of type `raw_ptr<T>` with
      // `RawPtrTraits::kMayDangle` trait (e.g. `MayBeDangling<T>`).
      static constexpr bool kParamIsDanglingRawPtr =
          IsRawPtrMayDangleV<FunctionParamType>;
      // true if the bound parameter is of type
      // `UnretainedWrapper<T, unretained_traits::MayDangle, PtrTraits>`.
      static constexpr bool kBoundPtrMayDangle =
          IsUnretainedMayDangle<StorageType>;
      // true if bound parameter of type `UnretainedWrapper` and parameter of
      // type `raw_ptr` have compatible `RawPtrTraits`.
      static constexpr bool kMayBeDanglingTraitsCorrectness =
          UnretainedAndRawPtrHaveCompatibleTraits<StorageType,
                                                  FunctionParamType>;
      // true if the receiver argument **must** be of type `MayBeDangling<T>`.
      static constexpr bool kMayBeDanglingMustBeUsed =
          kBoundPtrMayDangle && kParamIsDanglingRawPtr;

      // true iff:
      // - bound parameter is of type
      //   `UnretainedWrapper<T, unretained_traits::MayDangle, PtrTraits>`
      // - the receiving argument is of type `MayBeDangling<T>`
      template <bool is_method>
      static constexpr bool kMayBeDanglingPtrPassedCorrectly =
          kParamIsThisPointer<is_method> ||
          kBoundPtrMayDangle == kParamIsDanglingRawPtr;

      // true if:
      // - MayBeDangling<T> must not be used as receiver parameter.
      // OR
      // - MayBeDangling<T> must be used as receiver parameter and its traits
      // are matching Unretained traits.
      static constexpr bool kUnsafeDanglingAndMayBeDanglingHaveMatchingTraits =
          !kMayBeDanglingMustBeUsed || kMayBeDanglingTraitsCorrectness;
    };
  };
};

// Helper to assert that parameter |i| of type |Arg| can be bound, which means:
// - |Arg| can be retained internally as |Storage|.
// - |Arg| can be forwarded as |Unwrapped| to |Param|.
template <int i,
          bool is_method,
          typename Arg,
          typename Storage,
          typename Unwrapped,
          typename Param>
struct AssertConstructible {
 private:
  // With `BindRepeating`, there are two decision points for how to handle a
  // move-only type:
  //
  // 1. Whether the move-only argument should be moved into the internal
  //    `BindState`. Either `std::move()` or `Passed` is sufficient to trigger
  //    move-only semantics.
  // 2. Whether or not the bound, move-only argument should be moved to the
  //    bound functor when invoked. When the argument is bound with `Passed`,
  //    invoking the callback will destructively move the bound, move-only
  //    argument to the bound functor. In contrast, if the argument is bound
  //    with `std::move()`, `RepeatingCallback` will attempt to call the bound
  //    functor with a constant reference to the bound, move-only argument. This
  //    will fail if the bound functor accepts that argument by value, since the
  //    argument cannot be copied. It is this latter case that this
  //    static_assert aims to catch.
  //
  // In contrast, `BindOnce()` only has one decision point. Once a move-only
  // type is captured by value into the internal `BindState`, the bound,
  // move-only argument will always be moved to the functor when invoked.
  // Failure to use std::move will simply fail the `kMoveOnlyTypeMustUseStdMove`
  // assert below instead.
  //
  // Note: `Passed()` is a legacy of supporting move-only types when repeating
  // callbacks were the only callback type. A `RepeatingCallback` with a
  // `Passed()` argument is really a `OnceCallback` and should eventually be
  // migrated.
  static_assert(
      BindArgument<i>::template ForwardedAs<Unwrapped>::
          template ToParamWithType<Param>::kMoveOnlyTypeMustUseBasePassed,
      "base::BindRepeating() argument is a move-only type. Use base::Passed() "
      "instead of std::move() to transfer ownership from the callback to the "
      "bound functor.");
  static_assert(
      BindArgument<i>::template ForwardedAs<Unwrapped>::
          template ToParamWithType<Param>::kNonConstRefParamMustBeWrapped,
      "Bound argument for non-const reference parameter must be wrapped in "
      "std::ref() or base::OwnedRef().");
  static_assert(
      BindArgument<i>::template ForwardedAs<Unwrapped>::
          template ToParamWithType<Param>::kCanBeForwardedToBoundFunctor,
      "Type mismatch between bound argument and bound functor's parameter.");

  static_assert(BindArgument<i>::template BoundAs<Arg>::template StoredAs<
                    Storage>::kMoveOnlyTypeMustUseStdMove,
                "Attempting to bind a move-only type. Use std::move() to "
                "transfer ownership to the created callback.");
  // In practice, this static_assert should be quite rare as the storage type
  // is deduced from the arguments passed to `BindOnce()`/`BindRepeating()`.
  static_assert(
      BindArgument<i>::template BoundAs<Arg>::template StoredAs<
          Storage>::kBindArgumentCanBeCaptured,
      "Cannot capture argument: is the argument copyable or movable?");

  // We forbid callbacks to use raw_ptr as a parameter. However, we allow
  // MayBeDangling<T> iff the callback argument was created using
  // `base::UnsafeDangling`.
  static_assert(
      BindArgument<i>::template ForwardedAs<
          Unwrapped>::template ToParamWithType<Param>::kNotARawPtr ||
          BindArgument<i>::template ToParamWithType<Param>::template StoredAs<
              Storage>::kMayBeDanglingMustBeUsed,
      "base::Bind() target functor has a parameter of type raw_ptr<T>. "
      "raw_ptr<T> should not be used for function parameters, please use T* or "
      "T& instead.");

  // A bound functor must take a dangling pointer argument (e.g. bound using the
  // UnsafeDangling helper) as a MayBeDangling<T>, to make it clear that the
  // pointee's lifetime must be externally validated before using it. For
  // methods, exempt a bound receiver (i.e. the this pointer) as it is not
  // passed as a regular function argument.
  static_assert(
      BindArgument<i>::template ToParamWithType<Param>::template StoredAs<
          Storage>::template kMayBeDanglingPtrPassedCorrectly<is_method>,
      "base::UnsafeDangling() pointers must be received by functors with "
      "MayBeDangling<T> as parameter.");

  static_assert(
      BindArgument<i>::template ToParamWithType<Param>::template StoredAs<
          Storage>::kUnsafeDanglingAndMayBeDanglingHaveMatchingTraits,
      "MayBeDangling<T> parameter must receive the same RawPtrTraits as the "
      "one passed to the corresponding base::UnsafeDangling() call.");
};

// Takes three same-length TypeLists, and applies AssertConstructible for each
// triples.
template <bool is_method,
          typename Index,
          typename Args,
          typename UnwrappedTypeList,
          typename ParamsList>
struct AssertBindArgsValidity;

template <bool is_method,
          size_t... Ns,
          typename... Args,
          typename... Unwrapped,
          typename... Params>
struct AssertBindArgsValidity<is_method,
                              std::index_sequence<Ns...>,
                              TypeList<Args...>,
                              TypeList<Unwrapped...>,
                              TypeList<Params...>>
    : AssertConstructible<Ns,
                          is_method,
                          Args,
                          std::decay_t<Args>,
                          Unwrapped,
                          Params>... {
  static constexpr bool ok = true;
};

template <typename T>
struct AssertBindArgIsNotBasePassed : public std::true_type {};

template <typename T>
struct AssertBindArgIsNotBasePassed<PassedWrapper<T>> : public std::false_type {
};

template <template <typename> class CallbackT,
          typename Functor,
          typename... Args>
decltype(auto) BindImpl(Functor&& functor, Args&&... args) {
  // This block checks if each |args| matches to the corresponding params of the
  // target function. This check does not affect the behavior of Bind, but its
  // error message should be more readable.
  static constexpr bool kIsOnce = IsOnceCallback<CallbackT<void()>>::value;
  using Helper = BindTypeHelper<Functor, Args...>;
  using FunctorTraits = typename Helper::FunctorTraits;
  using BoundArgsList = typename Helper::BoundArgsList;
  using UnwrappedArgsList =
      MakeUnwrappedTypeList<kIsOnce, FunctorTraits::is_method, Args&&...>;
  using BoundParamsList = typename Helper::BoundParamsList;
  static_assert(
      MakeFunctorTraits<Functor>::is_stateless,
      "Capturing lambdas and stateful lambdas are intentionally not supported. "
      "Please use base::Bind{Once,Repeating} directly to bind arguments.");
  static_assert(
      AssertBindArgsValidity<FunctorTraits::is_method,
                             std::make_index_sequence<Helper::num_bounds>,
                             BoundArgsList, UnwrappedArgsList,
                             BoundParamsList>::ok,
      "The bound args need to be convertible to the target params.");

  using BindState = MakeBindStateType<Functor, Args...>;
  using UnboundRunType = MakeUnboundRunType<Functor, Args...>;
  using Invoker = Invoker<BindState, UnboundRunType>;
  using CallbackType = CallbackT<UnboundRunType>;

  // Store the invoke func into PolymorphicInvoke before casting it to
  // InvokeFuncStorage, so that we can ensure its type matches to
  // PolymorphicInvoke, to which CallbackType will cast back.
  using PolymorphicInvoke = typename CallbackType::PolymorphicInvoke;
  PolymorphicInvoke invoke_func;
  if constexpr (kIsOnce) {
    invoke_func = Invoker::RunOnce;
  } else {
    invoke_func = Invoker::Run;
  }

  using InvokeFuncStorage = BindStateBase::InvokeFuncStorage;
  return CallbackType(BindState::Create(
      reinterpret_cast<InvokeFuncStorage>(invoke_func),
      std::forward<Functor>(functor), std::forward<Args>(args)...));
}

// Special cases for binding to a base::{Once, Repeating}Callback without extra
// bound arguments. We CHECK() the validity of callback to guard against null
// pointers accidentally ending up in posted tasks, causing hard-to-debug
// crashes.
template <template <typename> class CallbackT,
          typename Signature,
          std::enable_if_t<std::is_same_v<CallbackT<Signature>,
                                          OnceCallback<Signature>>>* = nullptr>
OnceCallback<Signature> BindImpl(OnceCallback<Signature> callback) {
  CHECK(callback);
  return callback;
}

template <template <typename> class CallbackT,
          typename Signature,
          std::enable_if_t<std::is_same_v<CallbackT<Signature>,
                                          OnceCallback<Signature>>>* = nullptr>
OnceCallback<Signature> BindImpl(RepeatingCallback<Signature> callback) {
  CHECK(callback);
  return callback;
}

template <template <typename> class CallbackT,
          typename Signature,
          std::enable_if_t<std::is_same_v<CallbackT<Signature>,
                                          RepeatingCallback<Signature>>>* =
              nullptr>
RepeatingCallback<Signature> BindImpl(RepeatingCallback<Signature> callback) {
  CHECK(callback);
  return callback;
}

template <template <typename> class CallbackT, typename Signature>
auto BindImpl(absl::FunctionRef<Signature>, ...) {
  static_assert(
      AlwaysFalse<Signature>,
      "base::Bind{Once,Repeating} require strong ownership: non-owning "
      "function references may not bound as the functor due to potential "
      "lifetime issues.");
  return nullptr;
}

template <template <typename> class CallbackT, typename Signature>
auto BindImpl(FunctionRef<Signature>, ...) {
  static_assert(
      AlwaysFalse<Signature>,
      "base::Bind{Once,Repeating} require strong ownership: non-owning "
      "function references may not bound as the functor due to potential "
      "lifetime issues.");
  return nullptr;
}

}  // namespace internal

// An injection point to control |this| pointer behavior on a method invocation.
// If IsWeakReceiver<> is true_type for |T| and |T| is used for a receiver of a
// method, base::Bind cancels the method invocation if the receiver is tested as
// false.
// E.g. Foo::bar() is not called:
//   struct Foo : base::SupportsWeakPtr<Foo> {
//     void bar() {}
//   };
//
//   WeakPtr<Foo> oo = nullptr;
//   base::BindOnce(&Foo::bar, oo).Run();
template <typename T>
struct IsWeakReceiver : std::false_type {};

template <typename T>
struct IsWeakReceiver<std::reference_wrapper<T>> : IsWeakReceiver<T> {};

template <typename T>
struct IsWeakReceiver<WeakPtr<T>> : std::true_type {};

// An injection point to control how objects are checked for maybe validity,
// which is an optimistic thread-safe check for full validity.
template <typename>
struct MaybeValidTraits {
  template <typename T>
  static bool MaybeValid(const T& o) {
    return o.MaybeValid();
  }
};

// An injection point to control how bound objects passed to the target
// function. BindUnwrapTraits<>::Unwrap() is called for each bound objects right
// before the target function is invoked.
template <typename>
struct BindUnwrapTraits {
  template <typename T>
  static T&& Unwrap(T&& o) {
    return std::forward<T>(o);
  }
};

template <typename T, typename UnretainedTrait, RawPtrTraits PtrTraits>
struct BindUnwrapTraits<
    internal::UnretainedWrapper<T, UnretainedTrait, PtrTraits>> {
  static auto Unwrap(
      const internal::UnretainedWrapper<T, UnretainedTrait, PtrTraits>& o) {
    return o.get();
  }
};

template <typename T, typename UnretainedTrait, RawPtrTraits PtrTraits>
struct BindUnwrapTraits<
    internal::UnretainedRefWrapper<T, UnretainedTrait, PtrTraits>> {
  static T& Unwrap(
      const internal::UnretainedRefWrapper<T, UnretainedTrait, PtrTraits>& o) {
    return o.get();
  }
};

template <typename T>
struct BindUnwrapTraits<internal::RetainedRefWrapper<T>> {
  static T* Unwrap(const internal::RetainedRefWrapper<T>& o) { return o.get(); }
};

template <typename T, typename Deleter>
struct BindUnwrapTraits<internal::OwnedWrapper<T, Deleter>> {
  static T* Unwrap(const internal::OwnedWrapper<T, Deleter>& o) {
    return o.get();
  }
};

template <typename T>
struct BindUnwrapTraits<internal::OwnedRefWrapper<T>> {
  static T& Unwrap(const internal::OwnedRefWrapper<T>& o) { return o.get(); }
};

template <typename T>
struct BindUnwrapTraits<internal::PassedWrapper<T>> {
  static T Unwrap(const internal::PassedWrapper<T>& o) { return o.Take(); }
};

#if BUILDFLAG(IS_WIN)
template <typename T>
struct BindUnwrapTraits<Microsoft::WRL::ComPtr<T>> {
  static T* Unwrap(const Microsoft::WRL::ComPtr<T>& ptr) { return ptr.Get(); }
};
#endif

// CallbackCancellationTraits allows customization of Callback's cancellation
// semantics. By default, callbacks are not cancellable. A specialization should
// set is_cancellable = true and implement an IsCancelled() that returns if the
// callback should be cancelled.
template <typename Functor, typename BoundArgsTuple, typename SFINAE>
struct CallbackCancellationTraits {
  static constexpr bool is_cancellable = false;
};

// Specialization for method bound to weak pointer receiver.
template <typename Functor, typename... BoundArgs>
struct CallbackCancellationTraits<
    Functor,
    std::tuple<BoundArgs...>,
    std::enable_if_t<
        internal::IsWeakMethod<internal::FunctorTraits<Functor>::is_method,
                               BoundArgs...>::value>> {
  static constexpr bool is_cancellable = true;

  template <typename Receiver, typename... Args>
  static bool IsCancelled(const Functor&,
                          const Receiver& receiver,
                          const Args&...) {
    return !receiver;
  }

  template <typename Receiver, typename... Args>
  static bool MaybeValid(const Functor&,
                         const Receiver& receiver,
                         const Args&...) {
    return MaybeValidTraits<Receiver>::MaybeValid(receiver);
  }
};

// Specialization for a nested bind.
template <typename Signature, typename... BoundArgs>
struct CallbackCancellationTraits<OnceCallback<Signature>,
                                  std::tuple<BoundArgs...>> {
  static constexpr bool is_cancellable = true;

  template <typename Functor>
  static bool IsCancelled(const Functor& functor, const BoundArgs&...) {
    return functor.IsCancelled();
  }

  template <typename Functor>
  static bool MaybeValid(const Functor& functor, const BoundArgs&...) {
    return MaybeValidTraits<Functor>::MaybeValid(functor);
  }
};

template <typename Signature, typename... BoundArgs>
struct CallbackCancellationTraits<RepeatingCallback<Signature>,
                                  std::tuple<BoundArgs...>> {
  static constexpr bool is_cancellable = true;

  template <typename Functor>
  static bool IsCancelled(const Functor& functor, const BoundArgs&...) {
    return functor.IsCancelled();
  }

  template <typename Functor>
  static bool MaybeValid(const Functor& functor, const BoundArgs&...) {
    return MaybeValidTraits<Functor>::MaybeValid(functor);
  }
};

}  // namespace base

#endif  // BASE_FUNCTIONAL_BIND_INTERNAL_H_
