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
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace base {

namespace internal {

struct DefaultCrossThreadBindTraits {
  template <typename Signature>
  using CrossThreadTask = OnceCallback<Signature>;

  template <typename Functor, typename... Args>
  static inline auto BindOnce(Functor&& functor, Args&&... args) {
    return base::BindOnce(std::forward<Functor>(functor),
                          std::forward<Args>(args)...);
  }

  template <typename T>
  static inline auto Unretained(T* ptr) {
    return base::Unretained(ptr);
  }

  template <typename Signature>
  static inline bool PostTask(SequencedTaskRunner& task_runner,
                              const Location& location,
                              CrossThreadTask<Signature>&& task) {
    return task_runner.PostTask(location, std::move(task));
  }

  static inline bool PostTaskAndReply(SequencedTaskRunner& task_runner,
                                      const Location& location,
                                      OnceClosure&& task,
                                      OnceClosure&& reply) {
    return task_runner.PostTaskAndReply(location, std::move(task),
                                        std::move(reply));
  }

  template <typename TaskReturnType, typename ReplyArgType>
  static inline bool PostTaskAndReplyWithResult(
      SequencedTaskRunner& task_runner,
      const Location& location,
      OnceCallback<TaskReturnType()>&& task,
      OnceCallback<void(ReplyArgType)>&& reply) {
    return task_runner.PostTaskAndReplyWithResult(location, std::move(task),
                                                  std::move(reply));
  }

  // Accept RepeatingCallback here since it's convertible to a OnceCallback.
  template <template <typename> class CallbackType>
  using EnableIfIsCrossThreadTask = EnableIfIsBaseCallback<CallbackType>;
};

}  // namespace internal

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
// Sequence safety:
//
// Const-qualified methods may be used concurrently from multiple sequences,
// e.g. `AsyncCall()` or `is_null()`. Calls that are forwarded to the
// managed `T` will be posted to the bound sequence and executed serially
// there.
//
// Mutable methods (e.g. `Reset()`, destruction, or move assignment) require
// external synchronization if used concurrently with any other methods,
// including const-qualified methods.
template <typename T,
          class CrossThreadBindTraits = internal::DefaultCrossThreadBindTraits>
class SequenceBound {
 public:
  template <typename Signature>
  using CrossThreadTask =
      typename CrossThreadBindTraits::template CrossThreadTask<Signature>;

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
  explicit SequenceBound(scoped_refptr<SequencedTaskRunner> task_runner,
                         Args&&... args)
      : impl_task_runner_(std::move(task_runner)) {
    // Allocate space for but do not construct an instance of `T`.
    // AlignedAlloc() requires alignment be a multiple of sizeof(void*).
    storage_ = AlignedAlloc(
        sizeof(T), sizeof(void*) > alignof(T) ? sizeof(void*) : alignof(T));
    t_ = reinterpret_cast<T*>(storage_);

    // Ensure that `t_` will be initialized
    CrossThreadBindTraits::PostTask(
        *impl_task_runner_, FROM_HERE,
        CrossThreadBindTraits::BindOnce(&ConstructOwnerRecord<Args...>,
                                        CrossThreadBindTraits::Unretained(t_),
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
  // NOLINTNEXTLINE(google-explicit-constructor): Intentionally implicit.
  SequenceBound(SequenceBound<From, CrossThreadBindTraits>&& other) {
    MoveRecordFrom(other);
  }

  template <typename From>
  SequenceBound& operator=(SequenceBound<From, CrossThreadBindTraits>&& other) {
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
  // If `method` accepts arguments, use `WithArgs()` to bind them:
  //
  //   helper.AsyncCall(&IOHelper::DoWorkWithArgs)
  //       .WithArgs(args);
  //
  // Use `Then()` to run a callback on the owner sequence after `method`
  // completes:
  //
  //   helper.AsyncCall(&IOHelper::GetValue)
  //       .Then(std::move(process_result_callback));
  //
  // If a method returns a non-void type, use of `Then()` is required, and the
  // method's return value will be passed to the `Then()` callback. To ignore
  // the method's return value instead, wrap `method` in `base::IgnoreResult()`:
  //
  //   // Calling `GetPrefs` to force-initialize prefs.
  //   helper.AsyncCall(base::IgnoreResult(&IOHelper::GetPrefs));
  //
  // `WithArgs()` and `Then()` may also be combined:
  //
  //   // Ordering is important: `Then()` must come last.
  //   helper.AsyncCall(&IOHelper::GetValueWithArgs)
  //       .WithArgs(args)
  //       .Then(std::move(process_result_callback));
  //
  // Note: internally, `AsyncCall()` is implemented using a series of helper
  // classes that build the callback chain and post it on destruction. Capturing
  // the return value and passing it elsewhere or triggering lifetime extension
  // (e.g. by binding the return value to a reference) are both unsupported.
  template <typename R, typename... Args>
  auto AsyncCall(R (T::*method)(Args...),
                 const Location& location = Location::Current()) const {
    return AsyncCallBuilder<R (T::*)(Args...), R, std::tuple<Args...>>(
        this, &location, method);
  }

  template <typename R, typename... Args>
  auto AsyncCall(R (T::*method)(Args...) const,
                 const Location& location = Location::Current()) const {
    return AsyncCallBuilder<R (T::*)(Args...) const, R, std::tuple<Args...>>(
        this, &location, method);
  }

  template <typename R, typename... Args>
  auto AsyncCall(internal::IgnoreResultHelper<R (T::*)(Args...) const> method,
                 const Location& location = Location::Current()) const {
    return AsyncCallBuilder<
        internal::IgnoreResultHelper<R (T::*)(Args...) const>, void,
        std::tuple<Args...>>(this, &location, method);
  }

  template <typename R, typename... Args>
  auto AsyncCall(internal::IgnoreResultHelper<R (T::*)(Args...)> method,
                 const Location& location = Location::Current()) const {
    return AsyncCallBuilder<internal::IgnoreResultHelper<R (T::*)(Args...)>,
                            void, std::tuple<Args...>>(this, &location, method);
  }

  // Posts `task` to `impl_task_runner_`, passing it a reference to the wrapped
  // object. This allows arbitrary logic to be safely executed on the object's
  // task runner. The object is guaranteed to remain alive for the duration of
  // the task.
  // TODO(crbug.com/1182140): Consider checking whether the task runner can run
  // tasks in current sequence, and using "plain" binds and task posting (here
  // and other places that `CrossThreadBindTraits::PostTask`).
  using ConstPostTaskCallback = CrossThreadTask<void(const T&)>;
  void PostTaskWithThisObject(const Location& from_here,
                              ConstPostTaskCallback callback) const {
    DCHECK(!is_null());
    // Even though the lifetime of the object pointed to by `t_` may not have
    // begun yet, the storage has been allocated. Per [basic.life/6] and
    // [basic.life/7], "Indirection through such a pointer is permitted but the
    // resulting lvalue may only be used in limited ways, as described below."
    CrossThreadBindTraits::PostTask(
        *impl_task_runner_, from_here,
        CrossThreadBindTraits::BindOnce(std::move(callback), std::cref(*t_)));
  }

  // Same as above, but for non-const operations. The callback takes a pointer
  // to the wrapped object rather than a const ref.
  using PostTaskCallback = CrossThreadTask<void(T*)>;
  void PostTaskWithThisObject(const Location& from_here,
                              PostTaskCallback callback) const {
    DCHECK(!is_null());
    CrossThreadBindTraits::PostTask(
        *impl_task_runner_, from_here,
        CrossThreadBindTraits::BindOnce(std::move(callback),
                                        CrossThreadBindTraits::Unretained(t_)));
  }

  // TODO(liberato): Add PostOrCall(), to support cases where synchronous calls
  // are okay if it's the same task runner.

  // Resets `this` to null. If `this` is not currently null, posts destruction
  // of the managed `T` to `impl_task_runner_`.
  void Reset() {
    if (is_null())
      return;

    CrossThreadBindTraits::PostTask(
        *impl_task_runner_, FROM_HERE,
        CrossThreadBindTraits::BindOnce(
            &DeleteOwnerRecord, CrossThreadBindTraits::Unretained(t_),
            CrossThreadBindTraits::Unretained(storage_)));

    impl_task_runner_ = nullptr;
    t_ = nullptr;
    storage_ = nullptr;
  }

  // Resets `this` to null. If `this` is not currently null, posts destruction
  // of the managed `T` to `impl_task_runner_`. Blocks until the destructor has
  // run.
  void SynchronouslyResetForTest() {
    if (is_null())
      return;

    RunLoop run_loop;
    CrossThreadBindTraits::PostTask(
        *impl_task_runner_, FROM_HERE,
        CrossThreadBindTraits::BindOnce(
            [](T* t, void* storage) { DeleteOwnerRecord(t, storage); }, t_,
            storage_)
            .Then(run_loop.QuitClosure()));
    run_loop.Run();

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
  template <typename U, class Binder>
  friend class SequenceBound;

  template <template <typename> class CallbackType>
  using EnableIfIsCrossThreadTask =
      typename CrossThreadBindTraits::template EnableIfIsCrossThreadTask<
          CallbackType>;

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
  template <typename MethodRef>
  class AsyncCallBuilderBase {
   protected:
    AsyncCallBuilderBase(const SequenceBound* sequence_bound,
                         const Location* location,
                         MethodRef method)
        : sequence_bound_(sequence_bound),
          location_(location),
          method_(method) {
      // Common entry point for `AsyncCall()`, so check preconditions here.
      DCHECK(sequence_bound_);
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
    const SequenceBound* sequence_bound_;
    // Subtle: this typically points at a Location *temporary*. This is used to
    // try to detect errors resulting from lifetime extension of the async call
    // factory temporaries, since the factory destructors can perform work. If
    // the lifetime of the factory is incorrectly extended, dereferencing
    // `location_` will trigger a stack-use-after-scope when running with ASan.
    const Location* const location_;
    MethodRef method_;
  };

  template <typename MethodRef, typename ReturnType, typename ArgsTuple>
  class AsyncCallBuilderImpl;

  // Selected method has no arguments and returns void.
  template <typename MethodRef>
  class AsyncCallBuilderImpl<MethodRef, void, std::tuple<>>
      : public AsyncCallBuilderBase<MethodRef> {
   public:
    // Note: despite being here, this is actually still protected, since it is
    // protected on the base class.
    using AsyncCallBuilderBase<MethodRef>::AsyncCallBuilderBase;

    ~AsyncCallBuilderImpl() {
      if (this->sequence_bound_) {
        CrossThreadBindTraits::PostTask(
            *this->sequence_bound_->impl_task_runner_, *this->location_,
            CrossThreadBindTraits::BindOnce(
                this->method_,
                CrossThreadBindTraits::Unretained(this->sequence_bound_->t_)));
      }
    }

    void Then(OnceClosure then_callback) && {
      this->sequence_bound_->PostTaskAndThenHelper(
          *this->location_,
          CrossThreadBindTraits::BindOnce(
              this->method_,
              CrossThreadBindTraits::Unretained(this->sequence_bound_->t_)),
          std::move(then_callback));
      this->sequence_bound_ = nullptr;
    }

   private:
    friend SequenceBound;

    AsyncCallBuilderImpl(AsyncCallBuilderImpl&&) = default;
    AsyncCallBuilderImpl& operator=(AsyncCallBuilderImpl&&) = default;
  };

  // Selected method has no arguments and returns non-void.
  template <typename MethodRef, typename ReturnType>
  class AsyncCallBuilderImpl<MethodRef, ReturnType, std::tuple<>>
      : public AsyncCallBuilderBase<MethodRef> {
   public:
    // Note: despite being here, this is actually still protected, since it is
    // protected on the base class.
    using AsyncCallBuilderBase<MethodRef>::AsyncCallBuilderBase;

    ~AsyncCallBuilderImpl() {
      // Must use Then() since the method's return type is not void.
      // Should be optimized out if the code is bug-free.
      CHECK(!this->sequence_bound_)
          << "Then() not invoked for a method that returns a non-void type; "
          << "make sure to invoke Then() or use base::IgnoreResult()";
    }

    template <template <typename> class CallbackType,
              typename ThenArg,
              typename = EnableIfIsCrossThreadTask<CallbackType>>
    void Then(CallbackType<void(ThenArg)> then_callback) && {
      this->sequence_bound_->PostTaskAndThenHelper(
          *this->location_,
          CrossThreadBindTraits::BindOnce(
              this->method_,
              CrossThreadBindTraits::Unretained(this->sequence_bound_->t_)),
          std::move(then_callback));
      this->sequence_bound_ = nullptr;
    }

   private:
    friend SequenceBound;

    AsyncCallBuilderImpl(AsyncCallBuilderImpl&&) = default;
    AsyncCallBuilderImpl& operator=(AsyncCallBuilderImpl&&) = default;
  };

  // Selected method has arguments. Return type can be void or non-void.
  template <typename MethodRef, typename ReturnType, typename... Args>
  class AsyncCallBuilderImpl<MethodRef, ReturnType, std::tuple<Args...>>
      : public AsyncCallBuilderBase<MethodRef> {
   public:
    // Note: despite being here, this is actually still protected, since it is
    // protected on the base class.
    using AsyncCallBuilderBase<MethodRef>::AsyncCallBuilderBase;

    ~AsyncCallBuilderImpl() {
      // Must use WithArgs() since the method takes arguments.
      // Should be optimized out if the code is bug-free.
      CHECK(!this->sequence_bound_);
    }

    template <typename... BoundArgs>
    auto WithArgs(BoundArgs&&... bound_args) {
      const SequenceBound* const sequence_bound =
          std::exchange(this->sequence_bound_, nullptr);
      return AsyncCallWithBoundArgsBuilder<ReturnType>(
          sequence_bound, this->location_,
          CrossThreadBindTraits::BindOnce(
              this->method_,
              CrossThreadBindTraits::Unretained(sequence_bound->t_),
              std::forward<BoundArgs>(bound_args)...));
    }

   private:
    friend SequenceBound;

    AsyncCallBuilderImpl(AsyncCallBuilderImpl&&) = default;
    AsyncCallBuilderImpl& operator=(AsyncCallBuilderImpl&&) = default;
  };

  // `MethodRef` is either a member function pointer type or a member function
  //     pointer type wrapped with `internal::IgnoreResultHelper`.
  // `R` is the return type of `MethodRef`. This is always `void` if
  //     `MethodRef` is an `internal::IgnoreResultHelper` wrapper.
  // `ArgsTuple` is a `std::tuple` with template type arguments corresponding to
  //     the types of the method's parameters.
  template <typename MethodRef, typename R, typename ArgsTuple>
  using AsyncCallBuilder = AsyncCallBuilderImpl<MethodRef, R, ArgsTuple>;

  // Support factories when arguments are bound using `WithArgs()`. These
  // factories don't need to handle raw method pointers, since everything has
  // already been packaged into a base::OnceCallback.
  template <typename ReturnType>
  class AsyncCallWithBoundArgsBuilderBase {
   protected:
    AsyncCallWithBoundArgsBuilderBase(const SequenceBound* sequence_bound,
                                      const Location* location,
                                      CrossThreadTask<ReturnType()> callback)
        : sequence_bound_(sequence_bound),
          location_(location),
          callback_(std::move(callback)) {
      DCHECK(sequence_bound_);
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

    const SequenceBound* sequence_bound_;
    const Location* const location_;
    CrossThreadTask<ReturnType()> callback_;
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
              typename = EnableIfIsCrossThreadTask<CallbackType>>
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
        CrossThreadBindTraits::PostTask(
            *this->sequence_bound_->impl_task_runner_, *this->location_,
            std::move(this->callback_));
      }
    }

    void Then(CrossThreadTask<void()> then_callback) && {
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
                             CrossThreadTask<void()> callback,
                             CrossThreadTask<void()> then_callback) const {
    CrossThreadBindTraits::PostTaskAndReply(*impl_task_runner_, location,
                                            std::move(callback),
                                            std::move(then_callback));
  }

  template <typename ReturnType,
            template <typename>
            class CallbackType,
            typename ThenArg,
            typename = EnableIfIsCrossThreadTask<CallbackType>>
  void PostTaskAndThenHelper(const Location& location,
                             CrossThreadTask<ReturnType()> callback,
                             CallbackType<void(ThenArg)> then_callback) const {
    CrossThreadTask<void(ThenArg)>&& once_then_callback =
        std::move(then_callback);
    CrossThreadBindTraits::PostTaskAndReplyWithResult(
        *impl_task_runner_, location, std::move(callback),
        std::move(once_then_callback));
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

  // Pointer to the managed `T`.
  T* t_ = nullptr;

  // Storage originally allocated by `AlignedAlloc()`. Maintained separately
  // from  `t_` since the original, unadjusted pointer needs to be passed to
  // `AlignedFree()`.
  void* storage_ = nullptr;

  // Task runner which manages `t_`. `t_` is constructed, destroyed, and
  // dereferenced only on this task runner.
  scoped_refptr<SequencedTaskRunner> impl_task_runner_;

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
