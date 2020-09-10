// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_SEQUENCE_BOUND_H_
#define BASE_THREADING_SEQUENCE_BOUND_H_

#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequence_bound_internal.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace base {

// Performing blocking work on a different task runner is a common pattern for
// improving responsiveness of foreground task runners. `SequenceBound<T>`
// provides an abstraction for an owner object living on the owner sequence, to
// construct, call methods on, and destroy an object of type T that lives on a
// different sequence (the bound sequence).
//
// This makes it natural for code running on different sequences to be
// partitioned along class boundaries, e.g.:
//
// class Tab {
//  private:
//   void OnScroll() {
//     // ...
//     io_helper_.AsyncCall(&IOHelper::SaveScrollPosition);
//   }
//   SequenceBound<IOHelper> io_helper_{GetBackgroundTaskRunner()};
// };
//
// Note: `SequenceBound<T>` intentionally does not expose a raw pointer to the
// managed `T` to ensure its internal sequence-safety invariants are not
// violated. As a result, `AsyncCall()` cannot simply use `base::OnceCallback`
//
// SequenceBound also supports replies:
//
//   class Database {
//    public:
//     int Query(int value) {
//       return value * value;
//     }
//   };
//
//   // SequenceBound itself is owned on `SequencedTaskRunnerHandle::Get()`.
//   // The managed Database instance managed by it is constructed and owned on
//   // `GetDBTaskRunner()`.
//   SequenceBound<Database> db(GetDBTaskRunner());
//
//   // `Database::Query()` runs on `GetDBTaskRunner()`, but
//   // `reply_callback` will run on the owner task runner.
//   auto reply_callback = [] (int result) {
//     LOG(ERROR) << result;  // Prints 25.
//   };
//   db.AsyncCall(&Database::Query).WithArgs(5)
//     .Then(base::BindOnce(reply_callback));
//
//   // When `db` goes out of scope, the Database instance will also be
//   // destroyed via a task posted to `GetDBTaskRunner()`.
//
// TODO(dcheng): SequenceBound should only be constructed, used, and destroyed
// on a single sequence. This enforcement will gradually be enabled over time.
template <typename T>
class SequenceBound {
 public:
  // Note: on construction, SequenceBound binds to the current sequence. Any
  // subsequent SequenceBound calls (including destruction) must run on that
  // same sequence.

  // Constructs a null SequenceBound with no managed `T`.
  // TODO(dcheng): Add an `Emplace()` method to go with `Reset()`.
  SequenceBound() = default;

  // Schedules asynchronous construction of a new instance of `T` on
  // `task_runner`.
  //
  // Once the SequenceBound constructor completes, the caller can immediately
  // use `AsyncCall()`, et cetera, to schedule work after the construction of
  // `T` on `task_runner`.
  //
  // Marked NO_SANITIZE because cfi doesn't like casting uninitialized memory to
  // `T*`. However, this is safe here because:
  //
  // 1. The cast is well-defined (see https://eel.is/c++draft/basic.life#6) and
  // 2. The resulting pointer is only ever dereferenced on `impl_task_runner_`.
  //    By the time SequenceBound's constructor returns, the task to construct
  //    `T` will already be posted; thus, subsequent dereference of `t_` on
  //    `impl_task_runner_` are safe.
  template <typename... Args>
  NO_SANITIZE("cfi-unrelated-cast")
  SequenceBound(scoped_refptr<base::SequencedTaskRunner> task_runner,
                Args&&... args)
      : impl_task_runner_(std::move(task_runner)) {
    // Allocate space for but do not construct an instance of `T`.
    // AlignedAlloc() requires alignment be a multiple of sizeof(void*).
    storage_ = AlignedAlloc(
        sizeof(T), sizeof(void*) > alignof(T) ? sizeof(void*) : alignof(T));
    t_ = reinterpret_cast<T*>(storage_);

    // Ensure that `t_` will be initialized
    impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ConstructOwnerRecord<Args...>, base::Unretained(t_),
                       std::forward<Args>(args)...));
  }

  // If non-null, destruction of the managed `T` is posted to
  // `impl_task_runner_`.`
  ~SequenceBound() { Reset(); }

  // Disallow copy or assignment. SequenceBound has single ownership of the
  // managed `T`.
  SequenceBound(const SequenceBound&) = delete;
  SequenceBound& operator=(const SequenceBound&) = delete;

  // Move construction and assignment.
  SequenceBound(SequenceBound&& other) { MoveRecordFrom(other); }

  SequenceBound& operator=(SequenceBound&& other) {
    Reset();
    MoveRecordFrom(other);
    return *this;
  }

  // Move conversion helpers: allows upcasting from SequenceBound<Derived> to
  // SequenceBound<Base>.
  template <typename From>
  SequenceBound(SequenceBound<From>&& other) {
    MoveRecordFrom(other);
  }

  template <typename From>
  SequenceBound<T>& operator=(SequenceBound<From>&& other) {
    Reset();
    MoveRecordFrom(other);
    return *this;
  }

  // Invokes `method` of the managed `T` on `impl_task_runner_`. May only be
  // used when `is_null()` is false.
  //
  // Basic usage:
  //
  //   helper.AsyncCall(&IOHelper::DoWork);
  //
  // If `method` accepts arguments, use of `WithArgs()` to bind them is
  // mandatory:
  //
  //   helper.AsyncCall(&IOHelper::DoWorkWithArgs).WithArgs(args);
  //
  // Optionally, use `Then()` to chain to a callback on the owner sequence after
  // `method` completes. If `method` returns a non-void type, the return value
  // will be passed to the chained callback.
  //
  //   helper.AsyncCall(&IOHelper::GetValue).Then(std::move(process_result));
  //
  // `WithArgs()` and `Then()` may also be combined:
  //
  //   helper.AsyncCall(&IOHelper::GetValueWithArgs).WithArgs(args)
  //         .Then(std::move(process_result));
  //
  // but note that ordering is strict: `Then()` must always be last.
  //
  // Note: internally, this is implemented using a series of templated builders.
  // Destruction of the builder may trigger task posting; as a result, using the
  // builder as anything other than a temporary is not allowed.
  //
  // Similarly, triggering lifetime extension of the temporary (e.g. by binding
  // to a const lvalue reference) is not allowed.
  template <typename R, typename... Args>
  auto AsyncCall(R (T::*method)(Args...),
                 const Location& location = Location::Current()) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return AsyncCallBuilder<R (T::*)(Args...)>(this, &location, method);
  }

  template <typename R, typename... Args>
  auto AsyncCall(R (T::*method)(Args...) const,
                 const Location& location = Location::Current()) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return AsyncCallBuilder<R (T::*)(Args...) const>(this, &location, method);
  }

  // Post a call to `method` to `impl_task_runner_`.
  // TODO(dcheng): Deprecate this in favor of `AsyncCall()`.
  template <typename... MethodArgs, typename... Args>
  void Post(const base::Location& from_here,
            void (T::*method)(MethodArgs...),
            Args&&... args) const {
    DCHECK(t_);
    impl_task_runner_->PostTask(from_here,
                                base::BindOnce(method, base::Unretained(t_),
                                               std::forward<Args>(args)...));
  }

  // Posts `task` to `impl_task_runner_`, passing it a reference to the wrapped
  // object. This allows arbitrary logic to be safely executed on the object's
  // task runner. The object is guaranteed to remain alive for the duration of
  // the task.
  using ConstPostTaskCallback = base::OnceCallback<void(const T&)>;
  void PostTaskWithThisObject(const base::Location& from_here,
                              ConstPostTaskCallback callback) const {
    DCHECK(t_);
    impl_task_runner_->PostTask(
        from_here,
        base::BindOnce([](ConstPostTaskCallback callback,
                          const T* t) { std::move(callback).Run(*t); },
                       std::move(callback), t_));
  }

  // Same as above, but for non-const operations. The callback takes a pointer
  // to the wrapped object rather than a const ref.
  using PostTaskCallback = base::OnceCallback<void(T*)>;
  void PostTaskWithThisObject(const base::Location& from_here,
                              PostTaskCallback callback) const {
    DCHECK(t_);
    impl_task_runner_->PostTask(from_here,
                                base::BindOnce(std::move(callback), t_));
  }

  // TODO(liberato): Add PostOrCall(), to support cases where synchronous calls
  // are okay if it's the same task runner.

  // Resets `this` to null. If `this` is not currently null, posts destruction
  // of the managed `T` to `impl_task_runner_`.
  void Reset() {
    if (is_null())
      return;

    impl_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DeleteOwnerRecord, base::Unretained(t_),
                                  base::Unretained(storage_)));

    impl_task_runner_ = nullptr;
    t_ = nullptr;
    storage_ = nullptr;
  }

  // Same as above, but allows the caller to provide a closure to be invoked
  // immediately after destruction. The Closure is invoked on
  // `impl_task_runner_`, iff the owned object was non-null.
  //
  // TODO(dcheng): Consider removing this; this appears to be used for test
  // synchronization, but that could be achieved by posting
  // `run_loop.QuitClosure()` to the destination sequence after calling
  // `Reset()`.
  void ResetWithCallbackAfterDestruction(base::OnceClosure callback) {
    if (is_null())
      return;

    impl_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::OnceClosure callback, T* t, void* storage) {
                         DeleteOwnerRecord(t, storage);
                         std::move(callback).Run();
                       },
                       std::move(callback), t_, storage_));

    impl_task_runner_ = nullptr;
    t_ = nullptr;
    storage_ = nullptr;
  }

  // Return true if `this` is logically null; otherwise, returns false.
  //
  // A SequenceBound is logically null if there is no managed `T`; it is only
  // valid to call `AsyncCall()` on a non-null SequenceBound.
  //
  // Note that the concept of 'logically null' here does not exactly match the
  // lifetime of `T`, which lives on `impl_task_runner_`. In particular, when
  // SequenceBound is first constructed, `is_null()` may return false, even
  // though the lifetime of `T` may not have begun yet on `impl_task_runner_`.
  // Similarly, after `SequenceBound::Reset()`, `is_null()` may return true,
  // even though the lifetime of `T` may not have ended yet on
  // `impl_task_runner_`.
  bool is_null() const { return !t_; }

  // True if `this` is not logically null. See `is_null()`.
  explicit operator bool() const { return !is_null(); }

 private:
  // For move conversion.
  template <typename U>
  friend class SequenceBound;

  // Support helpers for `AsyncCall()` implementation.
  //
  // Several implementation notes:
  // 1. Tasks are posted via destroying the builder or an explicit call to
  //    `Then()`.
  //
  // 2. A builder may be consumed by:
  //
  //    - calling `Then()`, which immediately posts the task chain
  //    - calling `WithArgs()`, which returns a new builder with the captured
  //      arguments
  //
  //    Builders that are consumed have the internal `sequence_bound_` field
  //    nulled out; the hope is the compiler can see this and use it to
  //    eliminate dead branches (e.g. correctness checks that aren't needed
  //    since the code can be statically proved correct).
  //
  // 3. Builder methods are rvalue-qualified to try to enforce that the builder
  //    is only used as a temporary. Note that this only helps so much; nothing
  //    prevents a determined caller from using `std::move()` to force calls to
  //    a non-temporary instance.
  //
  // TODO(dcheng): It might also be possible to use Gmock-style matcher
  // composition, e.g. something like:
  //
  //   sb.AsyncCall(&Helper::DoWork, WithArgs(args),
  //                Then(std::move(process_result));
  //
  // In theory, this might allow the elimination of magic destructors and
  // better static checking by the compiler.
  template <typename MethodPtrType>
  class AsyncCallBuilderBase {
   protected:
    AsyncCallBuilderBase(SequenceBound* sequence_bound,
                         const Location* location,
                         MethodPtrType method)
        : sequence_bound_(sequence_bound),
          location_(location),
          method_(method) {
      // Common entry point for `AsyncCall()`, so check preconditions here.
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_bound_->sequence_checker_);
      DCHECK(sequence_bound_->t_);
    }

    AsyncCallBuilderBase(AsyncCallBuilderBase&&) = default;
    AsyncCallBuilderBase& operator=(AsyncCallBuilderBase&&) = default;

    // `sequence_bound_` is consumed and set to `nullptr` when `Then()` is
    // invoked. This is used as a flag for two potential states
    //
    // - if a method returns void, invoking `Then()` is optional. The destructor
    //   will check if `sequence_bound_` is null; if it is, `Then()` was
    //   already invoked and the task chain has already been posted, so the
    //   destructor does not need to do anything. Otherwise, the destructor
    //   needs to post the task to make the async call. In theory, the compiler
    //   should be able to eliminate this branch based on the presence or
    //   absence of a call to `Then()`.
    //
    // - if a method returns a non-void type, `Then()` *must* be invoked. The
    //   destructor will `CHECK()` if `sequence_bound_` is non-null, since that
    //   indicates `Then()` was not invoked. Similarly, note this branch should
    //   be eliminated by the optimizer if the code is free of bugs. :)
    SequenceBound* sequence_bound_;
    // Subtle: this typically points at a Location *temporary*. This is used to
    // try to detect errors resulting from lifetime extension of the async call
    // factory temporaries, since the factory destructors can perform work. If
    // the lifetime of the factory is incorrectly extended, dereferencing
    // `location_` will trigger a stack-use-after-scope when running with ASan.
    const Location* const location_;
    MethodPtrType method_;
  };

  template <typename MethodPtrType, typename ReturnType, typename... Args>
  class AsyncCallBuilderImpl;

  // Selected method has no arguments and returns void.
  template <typename MethodPtrType>
  class AsyncCallBuilderImpl<MethodPtrType, void, std::tuple<>>
      : public AsyncCallBuilderBase<MethodPtrType> {
   public:
    // Note: despite being here, this is actually still protected, since it is
    // protected on the base class.
    using AsyncCallBuilderBase<MethodPtrType>::AsyncCallBuilderBase;

    ~AsyncCallBuilderImpl() {
      if (this->sequence_bound_) {
        this->sequence_bound_->impl_task_runner_->PostTask(
            *this->location_,
            BindOnce(this->method_, Unretained(this->sequence_bound_->t_)));
      }
    }

    void Then(OnceClosure then_callback) && {
      this->sequence_bound_->PostTaskAndThenHelper(
          *this->location_,
          BindOnce(this->method_, Unretained(this->sequence_bound_->t_)),
          std::move(then_callback));
      this->sequence_bound_ = nullptr;
    }

   private:
    friend SequenceBound;

    AsyncCallBuilderImpl(AsyncCallBuilderImpl&&) = default;
    AsyncCallBuilderImpl& operator=(AsyncCallBuilderImpl&&) = default;
  };

  // Selected method has no arguments and returns non-void.
  template <typename MethodPtrType, typename ReturnType>
  class AsyncCallBuilderImpl<MethodPtrType, ReturnType, std::tuple<>>
      : public AsyncCallBuilderBase<MethodPtrType> {
   public:
    // Note: despite being here, this is actually still protected, since it is
    // protected on the base class.
    using AsyncCallBuilderBase<MethodPtrType>::AsyncCallBuilderBase;

    ~AsyncCallBuilderImpl() {
      // Must use Then() since the method's return type is not void.
      // Should be optimized out if the code is bug-free.
      CHECK(!this->sequence_bound_)
          << "Then() not invoked for a method that returns a non-void type; "
          << "make sure to invoke Then() or use base::IgnoreResult()";
    }

    template <template <typename> class CallbackType,
              typename ThenArg,
              typename = EnableIfIsBaseCallback<CallbackType>>
    void Then(CallbackType<void(ThenArg)> then_callback) && {
      this->sequence_bound_->PostTaskAndThenHelper(
          *this->location_,
          BindOnce(this->method_, Unretained(this->sequence_bound_->t_)),
          std::move(then_callback));
      this->sequence_bound_ = nullptr;
    }

   private:
    friend SequenceBound;

    AsyncCallBuilderImpl(AsyncCallBuilderImpl&&) = default;
    AsyncCallBuilderImpl& operator=(AsyncCallBuilderImpl&&) = default;
  };

  // Selected method has arguments. Return type can be void or non-void.
  template <typename MethodPtrType, typename ReturnType, typename... Args>
  class AsyncCallBuilderImpl<MethodPtrType, ReturnType, std::tuple<Args...>>
      : public AsyncCallBuilderBase<MethodPtrType> {
   public:
    // Note: despite being here, this is actually still protected, since it is
    // protected on the base class.
    using AsyncCallBuilderBase<MethodPtrType>::AsyncCallBuilderBase;

    ~AsyncCallBuilderImpl() {
      // Must use WithArgs() since the method takes arguments.
      // Should be optimized out if the code is bug-free.
      CHECK(!this->sequence_bound_);
    }

    template <typename... BoundArgs>
    auto WithArgs(BoundArgs&&... bound_args) {
      SequenceBound* const sequence_bound =
          std::exchange(this->sequence_bound_, nullptr);
      return AsyncCallWithBoundArgsBuilder<ReturnType>(
          sequence_bound, this->location_,
          BindOnce(this->method_, Unretained(sequence_bound->t_),
                   std::forward<BoundArgs>(bound_args)...));
    }

   private:
    friend SequenceBound;

    AsyncCallBuilderImpl(AsyncCallBuilderImpl&&) = default;
    AsyncCallBuilderImpl& operator=(AsyncCallBuilderImpl&&) = default;
  };

  template <typename MethodPtrType,
            typename R = internal::ExtractMethodReturnType<MethodPtrType>,
            typename ArgsTuple =
                internal::ExtractMethodArgsTuple<MethodPtrType>>
  using AsyncCallBuilder = AsyncCallBuilderImpl<MethodPtrType, R, ArgsTuple>;

  // Support factories when arguments are bound using `WithArgs()`. These
  // factories don't need to handle raw method pointers, since everything has
  // already been packaged into a base::OnceCallback.
  template <typename ReturnType>
  class AsyncCallWithBoundArgsBuilderBase {
   protected:
    AsyncCallWithBoundArgsBuilderBase(SequenceBound* sequence_bound,
                                      const Location* location,
                                      base::OnceCallback<ReturnType()> callback)
        : sequence_bound_(sequence_bound),
          location_(location),
          callback_(std::move(callback)) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_bound_->sequence_checker_);
      DCHECK(sequence_bound_->t_);
    }

    // Subtle: the internal helpers rely on move elision. Preventing move
    // elision (e.g. using `std::move()` when returning the temporary) will
    // trigger a `CHECK()` since `sequence_bound_` is not reset to nullptr on
    // move.
    AsyncCallWithBoundArgsBuilderBase(
        AsyncCallWithBoundArgsBuilderBase&&) noexcept = default;
    AsyncCallWithBoundArgsBuilderBase& operator=(
        AsyncCallWithBoundArgsBuilderBase&&) noexcept = default;

    SequenceBound* sequence_bound_;
    const Location* const location_;
    base::OnceCallback<ReturnType()> callback_;
  };

  // Note: this doesn't handle a void return type, which has an explicit
  // specialization below.
  template <typename ReturnType>
  class AsyncCallWithBoundArgsBuilderDefault
      : public AsyncCallWithBoundArgsBuilderBase<ReturnType> {
   public:
    ~AsyncCallWithBoundArgsBuilderDefault() {
      // Must use Then() since the method's return type is not void.
      // Should be optimized out if the code is bug-free.
      CHECK(!this->sequence_bound_);
    }

    template <template <typename> class CallbackType,
              typename ThenArg,
              typename = EnableIfIsBaseCallback<CallbackType>>
    void Then(CallbackType<void(ThenArg)> then_callback) && {
      this->sequence_bound_->PostTaskAndThenHelper(*this->location_,
                                                   std::move(this->callback_),
                                                   std::move(then_callback));
      this->sequence_bound_ = nullptr;
    }

   protected:
    using AsyncCallWithBoundArgsBuilderBase<
        ReturnType>::AsyncCallWithBoundArgsBuilderBase;

   private:
    friend SequenceBound;

    AsyncCallWithBoundArgsBuilderDefault(
        AsyncCallWithBoundArgsBuilderDefault&&) = default;
    AsyncCallWithBoundArgsBuilderDefault& operator=(
        AsyncCallWithBoundArgsBuilderDefault&&) = default;
  };

  class AsyncCallWithBoundArgsBuilderVoid
      : public AsyncCallWithBoundArgsBuilderBase<void> {
   public:
    // Note: despite being here, this is actually still protected, since it is
    // protected on the base class.
    using AsyncCallWithBoundArgsBuilderBase<
        void>::AsyncCallWithBoundArgsBuilderBase;

    ~AsyncCallWithBoundArgsBuilderVoid() {
      if (this->sequence_bound_) {
        this->sequence_bound_->impl_task_runner_->PostTask(
            *this->location_, std::move(this->callback_));
      }
    }

    void Then(OnceClosure then_callback) && {
      this->sequence_bound_->PostTaskAndThenHelper(*this->location_,
                                                   std::move(this->callback_),
                                                   std::move(then_callback));
      this->sequence_bound_ = nullptr;
    }

   private:
    friend SequenceBound;

    AsyncCallWithBoundArgsBuilderVoid(AsyncCallWithBoundArgsBuilderVoid&&) =
        default;
    AsyncCallWithBoundArgsBuilderVoid& operator=(
        AsyncCallWithBoundArgsBuilderVoid&&) = default;
  };

  template <typename ReturnType>
  using AsyncCallWithBoundArgsBuilder = typename std::conditional<
      std::is_void<ReturnType>::value,
      AsyncCallWithBoundArgsBuilderVoid,
      AsyncCallWithBoundArgsBuilderDefault<ReturnType>>::type;

  void PostTaskAndThenHelper(const Location& location,
                             OnceCallback<void()> callback,
                             OnceClosure then_callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    impl_task_runner_->PostTaskAndReply(location, std::move(callback),
                                        std::move(then_callback));
  }

  template <typename ReturnType,
            template <typename>
            class CallbackType,
            typename ThenArg,
            typename = EnableIfIsBaseCallback<CallbackType>>
  void PostTaskAndThenHelper(const Location& location,
                             OnceCallback<ReturnType()> callback,
                             CallbackType<void(ThenArg)> then_callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    OnceCallback<void(ThenArg)>&& once_then_callback = std::move(then_callback);
    impl_task_runner_->PostTaskAndReplyWithResult(
        location, std::move(callback), std::move(once_then_callback));
  }

  // Helper to support move construction and move assignment.
  //
  // Marked NO_SANITIZE since:
  // 1. SequenceBound can be moved before `t_` is constructed on
  //    `impl_task_runner_` but
  // 2. Implicit conversions to non-virtual base classes are allowed before the
  //    lifetime of `t_` has started (see https://eel.is/c++draft/basic.life#6).
  template <typename From>
  void NO_SANITIZE("cfi-unrelated-cast") MoveRecordFrom(From&& other) {
    // TODO(dcheng): Consider adding a static_assert to provide a friendlier
    // error message.
    impl_task_runner_ = std::move(other.impl_task_runner_);

    // Subtle: this must not use static_cast<>, since the lifetime of the
    // managed `T` may not have begun yet. However, the standard explicitly
    // still allows implicit conversion to a non-virtual base class.
    t_ = std::exchange(other.t_, nullptr);
    storage_ = std::exchange(other.storage_, nullptr);
  }

  // Pointer to the managed `T`. This field is only read and written on
  // the sequence associated with `sequence_checker_`.
  T* t_ = nullptr;

  // Storage originally allocated by `AlignedAlloc()`. Maintained separately
  // from  `t_` since the original, unadjusted pointer needs to be passed to
  // `AlignedFree()`.
  void* storage_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);

  // Task runner which manages `t_`. `t_` is constructed, destroyed, and
  // dereferenced only on this task runner.
  scoped_refptr<base::SequencedTaskRunner> impl_task_runner_;

  // Helpers for constructing and destroying `T` on `impl_task_runner_`.
  template <typename... Args>
  static void ConstructOwnerRecord(T* t, std::decay_t<Args>&&... args) {
    new (t) T(std::move(args)...);
  }

  static void DeleteOwnerRecord(T* t, void* storage) {
    t->~T();
    AlignedFree(storage);
  }
};

}  // namespace base

#endif  // BASE_THREADING_SEQUENCE_BOUND_H_
