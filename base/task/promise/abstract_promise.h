// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_PROMISE_ABSTRACT_PROMISE_H_
#define BASE_TASK_PROMISE_ABSTRACT_PROMISE_H_

#include <utility>

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/task/common/checked_lock.h"
#include "base/task/promise/dependent_list.h"
#include "base/task/promise/promise_executor.h"
#include "base/task/promise/promise_value.h"
#include "base/thread_annotations.h"

namespace base {
class TaskRunner;

template <typename ResolveType, typename RejectType>
class ManualPromiseResolver;

template <typename ResolveType, typename RejectType>
class Promise;

// AbstractPromise Memory Management.
//
// Consider a chain of promises: P1, P2 & P3
//
// Before resolve:
// * P1 needs an external reference (such as a Promise<> handle or it has been
//   posted) to keep it alive
// * P2 is kept alive by P1
// * P3 is kept alive by P2
//
//                                      ------------
//  Key:                               |     P1     | P1 doesn't have an
//  ═ Denotes a reference is held      |            | AdjacencyList
//  ─ Denotes a raw pointer            | Dependants |
//                                      -----|------
//                                           |    ^
//                          ╔════════════╗   |    |
//                          ↓            ║   ↓    |
//                    ------------    ---║--------|--
//                   |     P2     |  | dependent_ |  |
//                   |            |==|            |  | P2's AdjacencyList
//                   | Dependants |  | prerequisite_ |
//                    -----|------    ---------------
//                         |    ^
//        ╔════════════╗   |    |
//        ↓            ║   ↓    |
//  ------------    ---║--------|--
// |     P3     |  | dependent_ |  |
// |            |==|            |  | P3's AdjacencyList
// | Dependants |  | prerequisite_ |
//  ---|--------    ---------------
//     |
//     ↓
//    null
//
//
// After P1's executor runs, P2's |prerequisite_| link is upgraded by
// OnResolveDispatchReadyDependents (which incirectly calls
// RetainSettledPrerequisite) from a raw pointer to a reference. This is done to
// ensure P1's |value_| is available when P2's executor runs.
//
//                                      ------------
//                                     |     P1     | P1 doesn't have an
//                                     |            | AdjacencyList
//                                     | Dependants |
//                                      -----|------
//                                           |    ^
//                          ╔════════════╗   |    ║
//                          ↓            ║   ↓    ║
//                    ------------    ---║--------║--
//                   |     P2     |  | dependent_ ║  |
//                   |            |==|            ║  | P2's AdjacencyList
//                   | Dependants |  | prerequisite_ |
//                    -----|------    ---------------
//                         |    ^
//        ╔════════════╗   |    |
//        ↓            ║   ↓    |
//  ------------    ---║--------|--
// |     P3     |  | dependent_ |  |
// |            |==|            |  | P3's AdjacencyList
// | Dependants |  | prerequisite_ |
//  ---|--------    ---------------
//     |
//     ↓
//    null
//
//
// After P2's executor runs, it's AdjacencyList is cleared. Unless there's
// external references, at this stage P1 will be deleted. P3's |prerequisite_|
// is from a raw pointer to a reference to ensure P2's |value_| is available
// when P3's executor runs.
//
//                                      ------------
//                                     |     P1     | P1 doesn't have an
//                                     |            | AdjacencyList
//                                     | Dependants |
//                                      -----|------
//                                           |
//                                     null  |  null
//                                       ^   ↓    ^
//                    ------------    ---|--------|--
//                   |     P2     |  | dependent_ |  |
//                   |            |==|            |  | P2's AdjacencyList
//                   | Dependants |  | prerequisite_ |
//                    -----|------    ---------------
//                         |    ^
//        ╔════════════╗   |    ║
//        ↓            ║   ↓    ║
//  ------------    ---║--------║--
// |     P3     |  | dependent_ ║  |
// |            |==|            ║  | P3's AdjacencyList
// | Dependants |  | prerequisite_ |
//  ---|--------    ---------------
//     |
//     ↓
//    null
//
// =============================================================================
// Consider a promise P1 that is resolved with an unresolved promise P2, and P3
// which depends on P1.
//
// 1) Initially P1 doesn't have an AdjacencyList and must be kept alive by an
//    external reference.  P1 keeps P3 alive.
//
// 2) P1's executor resolves with P2 and P3 is modified to have P2 as a
//    dependent instead of P1. P1 has a reference to P2, but it needs an
//    external reference to keep alive.
//
// 3) When P2's executor runs, P3's executor is scheduled and P3's
//    |prerequisite_| link to P2 is upgraded to a reference. So P3 keeps P2
//    alive.
//
// 4) When P3's executor runs, its AdjacencyList is cleared. At this stage
//    unless there are external referecnes P2 and P3 will be deleted.
//
//
// 1.                   --------------
//                     | P1  value_   |
//                     |              | P1 doesn't have an AdjacencyList
//                     | Dependants   |
//                      ---|----------
//                         |        ^
//                         ↓        ║
//      ------------    ------------║---
//     | P3         |  | dependent_ ║   |
//     |            |==|            ║   | P3's AdjacencyList
//     | Dependants |  | prerequisite_  |
//      ---|--------     ---------------
//         |
//         ↓
//        null
//
// 2.                                  ------------
//                                    |     P2     |
//                            ╔══════>|            | P2 doesn't have an
//                            ║       | Dependants | AdjacencyList
//                            ║        -----|------
//                            ║             |  ^
//                      ------║-------      |  |
//                     | P1  value_   |     |  |
//                     |              |     |  |
//                     | Dependants   |     |  |
//                      --------------      |  |
//               ╔═════════╗   ┌────────────┘  |
//               ↓         ║   ↓    ┌──────────┘
//      ------------    ---║--------|---
//     | P3         |  | dependent_ |   |
//     |            |==|            |   | P3's AdjacencyList
//     | Dependants |  | prerequisite_  |
//      ---|--------     ---------------
//         |
//         ↓
//        null
// 3.                                  ------------
//                                    |     P2     |
//                                    |            | P2 doesn't have an
//                                    | Dependants | AdjacencyList
//                                     -----|------
//                                          |  ^
//                                          |  ║
//                                          |  ║
//                                          |  ║
//                                          |  ║
//                                          |  ║
//               ╔═════════╗   ┌────────────┘  ║
//               ↓         ║   ↓    ╔══════════╝
//      ------------    ---║--------║---
//     | P3         |  | dependent_ ║   |
//     |            |==|            ║   | P3's AdjacencyList
//     | Dependants |  | prerequisite_  |
//      ---|--------     ---------------
//         |
//         ↓
//        null
//
//
// 4.                                  ------------
//                                    |     P2     |
//                                    |            | P2 doesn't have an
//                                    | Dependants | AdjacencyList
//                                     ------------
//
//
//
//
//
//
//
//
//      ------------
//     | P3         |  P3 doesn't have an AdjacencyList anymore.
//     |            |
//     | Dependants |
//      ---|--------
//         |
//         ↓
//        null
//
// =============================================================================
// Consider an all promise Pall with dependents P1, P2 & P3:
//
// Before resolve P1, P2 & P3 keep Pall alive. If say P2 rejects then Pall
// keeps P2 alive, however all the dependents in Pall's AdjacencyList are
// cleared. When there are no external references to P1, P2 & P3 then Pall
// will get deleted too if it has no external references.
//
//                                Pall's AdjacencyList
//  ------------             ----------------
// |     P1     |           |                |
// |            | <─────────── prerequisite_ |
// | Dependants────────────>|  dependent_══════════════════╗
//  ------------            |                |             ↓
//                          |----------------|         ---------
//  ------------            |                |        |         |
// |     P2     | <─────────── prerequisite_ |        |  Pall   |
// |            |           |  dependent_════════════>|         |
// | Dependants────────────>|                |        |         |
//  ------------            |                |         ---------
//                          |----------------|             ^
//  ------------            |                |             ║
// |     P3     | <─────────── prerequisite_ |             ║
// |            |           |  dependent_══════════════════╝
// | Dependants────────────>|                |
//  ------------             ----------------
//
//
// In general a promise's AdjacencyList's only retains prerequisites after the
// promise has resolved. It is necessary to retain the prerequisites because a
// ThenOn or CatchOn can be added after the promise has resolved.

// A promise for either |ResolveType| if successful or |RejectType| on error.
template <typename ResolveType, typename RejectType>
class Promise;

// This enum is used to configure AbstractPromise's uncaught reject detection.
// Usually not catching a reject reason is a coding error, but at times that can
// become onerous. When that happens kCatchNotRequired should be used.
enum class RejectPolicy {
  kMustCatchRejection,
  kCatchNotRequired,
};

class WrappedPromise;

namespace internal {

template <typename T, typename... Args>
class PromiseCallbackHelper;

class AbstractPromise;
class AbstractPromiseTest;
class BasePromise;

// A binary size optimization to reduce the overhead of passing a scoped_refptr
// to Promise<> returned by PostTask. There are many thousands of PostTasks so
// even a single extra instruction (such as the scoped_refptr move constructor
// clearing the pointer) adds up. This is why we're not constructing a Promise<>
// with a scoped_refptr.
//
// The constructor calls AddRef, it's up to the owner of this object to either
// call Clear (which calls Release) or AbstractPromise in order to pass
// ownership onto a WrappedPromise.
class BASE_EXPORT PassedPromise {
 public:
  explicit inline PassedPromise(const scoped_refptr<AbstractPromise>& promise);

  PassedPromise() : promise_(nullptr) {}

  PassedPromise(const PassedPromise&) = delete;
  PassedPromise& operator=(const PassedPromise&) = delete;

#if DCHECK_IS_ON()
  PassedPromise(PassedPromise&& other) noexcept : promise_(other.promise_) {
    DCHECK(promise_);
    other.promise_ = nullptr;
  }

  PassedPromise& operator=(PassedPromise&& other) noexcept {
    DCHECK(!promise_);
    promise_ = other.promise_;
    DCHECK(promise_);
    other.promise_ = nullptr;
    return *this;
  }

  ~PassedPromise() {
    DCHECK(!promise_) << "The PassedPromise must be Cleared or passed onto a "
                         "Wrapped Promise";
  }
#else
  PassedPromise(PassedPromise&&) noexcept = default;
  PassedPromise& operator=(PassedPromise&&) noexcept = default;
#endif

  AbstractPromise* Release() {
    AbstractPromise* promise = promise_;
#if DCHECK_IS_ON()
    promise_ = nullptr;
#endif
    return promise;
  }

  AbstractPromise* get() const { return promise_; }

 private:
  AbstractPromise* promise_;
};

// Internal promise representation, maintains a graph of dependencies and posts
// promises as they become ready. In debug builds various sanity checks are
// performed to catch common errors such as double move or forgetting to catch a
// potential reject (NB this last check can be turned off with
// RejectPolicy::kCatchNotRequired).
class BASE_EXPORT AbstractPromise
    : public RefCountedThreadSafe<AbstractPromise> {
 public:
  class AdjacencyList;

  template <typename ConstructType>
  static scoped_refptr<AbstractPromise> Create(
      const scoped_refptr<TaskRunner>& task_runner,
      const Location& from_here,
      std::unique_ptr<AdjacencyList> prerequisites,
      RejectPolicy reject_policy,
      ConstructType tag,
      PromiseExecutor::Data&& executor_data) noexcept {
    scoped_refptr<AbstractPromise> promise = subtle::AdoptRefIfNeeded(
        new AbstractPromise(task_runner, from_here, std::move(prerequisites),
                            reject_policy, tag, std::move(executor_data)),
        AbstractPromise::kRefCountPreference);
    // It's important this is called after |promise| has been initialized
    // because otherwise it could trigger a scoped_refptr destructor on another
    // thread before this thread has had a chance to increment the refcount.
    promise->AddAsDependentForAllPrerequisites();
    return promise;
  }

  template <typename ConstructType>
  static scoped_refptr<AbstractPromise> CreateNoPrerequisitePromise(
      const Location& from_here,
      RejectPolicy reject_policy,
      ConstructType tag,
      PromiseExecutor::Data&& executor_data) noexcept {
    return subtle::AdoptRefIfNeeded(
        new internal::AbstractPromise(nullptr, from_here, nullptr,
                                      reject_policy, tag,
                                      std::move(executor_data)),
        AbstractPromise::kRefCountPreference);
  }

  AbstractPromise(const AbstractPromise&) = delete;
  AbstractPromise& operator=(const AbstractPromise&) = delete;

  const Location& from_here() const { return from_here_; }

  bool IsSettled() const { return dependents_.IsSettled(); }
  bool IsCanceled() const;

  // It's an error (result will be racy) to call these if unsettled.
  bool IsRejected() const { return dependents_.IsRejected(); }
  bool IsResolved() const { return dependents_.IsResolved(); }

  bool IsRejectedForTesting() const {
    return dependents_.IsRejectedForTesting();
  }

  bool IsResolvedForTesting() const {
    return dependents_.IsResolvedForTesting();
  }

  bool IsResolvedWithPromise() const { return value_.ContainsCurriedPromise(); }

  const PromiseValue& value() const {
    DCHECK(!IsResolvedWithPromise());
    return value_;
  }

  class ValueHandle {
   public:
    PromiseValue& value() { return value_; }

#if DCHECK_IS_ON()
    ~ValueHandle() { value_.reset(); }
#endif

   private:
    friend class AbstractPromise;

    explicit ValueHandle(PromiseValue& value) : value_(value) {}

    PromiseValue& value_;
  };

  // Used for promise results that require move semantics.  E.g. a promise chain
  // involving a std::unique_ptr<>.
  ValueHandle TakeValue() { return ValueHandle(value_); }

  // Returns nullptr if there isn't a curried promise.
  const AbstractPromise* GetCurriedPromise() const;

  // Sets the |value_| to |t|. The caller should call OnResolved() or
  // OnRejected() afterwards.
  template <typename T>
  void emplace(T&& t) {
    DCHECK(GetExecutor() != nullptr) << "Only valid to emplace once";
    value_ = std::forward<T>(t);
    static_assert(!std::is_same<std::decay_t<T>, AbstractPromise*>::value,
                  "Use scoped_refptr<AbstractPromise> instead");
  }

  template <typename T, typename... Args>
  void emplace(in_place_type_t<T> tag, Args&&... args) {
    DCHECK(GetExecutor() != nullptr) << "Only valid to emplace once";
    value_.emplace(tag, std::forward<Args>(args)...);
    static_assert(!std::is_same<std::decay_t<T>, AbstractPromise*>::value,
                  "Use scoped_refptr<AbstractPromise> instead");
  }

  // An out-of line emplace(Resolved<void>()); Useful for reducing binary
  // bloat in executor templates.
  void EmplaceResolvedVoid();

  // This is separate from AbstractPromise to reduce the memory footprint of
  // regular PostTask without promise chains.
  class BASE_EXPORT AdjacencyList {
   public:
    AdjacencyList();
    ~AdjacencyList();

    explicit AdjacencyList(AbstractPromise* prerequisite);
    explicit AdjacencyList(std::vector<DependentList::Node> prerequisite_list);

    bool DecrementPrerequisiteCountAndCheckIfZero();

    // Called for each prerequisites that resolves or rejects for
    // PrerequisitePolicy::kAny and each prerequisite that rejects for
    // PrerequisitePolicy::kAll. This saves |settled_prerequisite| and returns
    // true iff called for the first time.
    bool MarkPrerequisiteAsSettling(AbstractPromise* settled_prerequisite);

    // Invoked when this promise is notified that |canceled_prerequisite| is
    // cancelled. Clears the reference to |canceled_prerequisite| in this
    // AdjacencyList to ensure the it is not accessed later when Clear() is
    // called.
    void RemoveCanceledPrerequisite(AbstractPromise* canceled_prerequisite);

    std::vector<DependentList::Node>* prerequisite_list() {
      return &prerequisite_list_;
    }

    AbstractPromise* GetFirstSettledPrerequisite() const {
      return reinterpret_cast<AbstractPromise*>(
          first_settled_prerequisite_.load(std::memory_order_acquire));
    }

    void Clear();

   private:
    std::vector<DependentList::Node> prerequisite_list_;

    // PrerequisitePolicy::kAny waits for at most 1 resolve or N cancellations.
    // PrerequisitePolicy::kAll waits for N resolves or at most 1 cancellation.
    // PrerequisitePolicy::kNever doesn't use this.
    std::atomic_int action_prerequisite_count_;

    // For PrerequisitePolicy::kAll the address of the first rejected
    // prerequisite if any.
    // For PrerequisitePolicy::kAll the address of the first rejected or
    // resolved rerequsite if any.
    std::atomic<uintptr_t> first_settled_prerequisite_{0};
  };

  const std::vector<DependentList::Node>* prerequisite_list() const {
    if (!prerequisites_)
      return nullptr;
    return prerequisites_->prerequisite_list();
  }

  // Returns the first and only prerequisite AbstractPromise.  It's an error to
  // call this if the number of prerequisites isn't exactly one.
  AbstractPromise* GetOnlyPrerequisite() const {
    DCHECK(prerequisites_);
    const std::vector<DependentList::Node>* prerequisite_list =
        prerequisites_->prerequisite_list();
    DCHECK_EQ(prerequisite_list->size(), 1u);
    return (*prerequisite_list)[0].prerequisite();
  }

  // For PrerequisitePolicy::kAll returns the first rejected prerequisite if
  // any. For PrerequisitePolicy::kAny returns the first rejected or resolved
  // rerequsite if any.
  AbstractPromise* GetFirstSettledPrerequisite() const;

  // Calls |RunExecutor()| or posts a task to do so if |from_here_| is not
  // nullopt.
  void Execute();

  void IgnoreUncaughtCatchForTesting();

  // Signals that this promise was cancelled. If executor hasn't run yet, this
  // will prevent it from running and cancels any dependent promises unless they
  // have PrerequisitePolicy::kAny, in which case they will only be canceled if
  // all of their prerequisites are canceled. If OnCanceled() or OnResolved() or
  // OnRejected() has already run, this does nothing.
  void OnCanceled();

 private:
  friend base::RefCountedThreadSafe<AbstractPromise>;

  friend class AbstractPromiseTest;

  template <typename ResolveType, typename RejectType>
  friend class base::ManualPromiseResolver;

  template <typename T, typename... Args>
  friend class PromiseCallbackHelper;

  template <typename ConstructType>
  AbstractPromise(const scoped_refptr<TaskRunner>& task_runner,
                  const Location& from_here,
                  std::unique_ptr<AdjacencyList> prerequisites,
                  RejectPolicy reject_policy,
                  ConstructType tag,
                  PromiseExecutor::Data&& executor_data) noexcept
      : task_runner_(task_runner),
        from_here_(std::move(from_here)),
        value_(in_place_type_t<PromiseExecutor>(), std::move(executor_data)),
#if DCHECK_IS_ON()
        reject_policy_(reject_policy),
        resolve_argument_passing_type_(
            GetExecutor()->ResolveArgumentPassingType()),
        reject_argument_passing_type_(
            GetExecutor()->RejectArgumentPassingType()),
        executor_can_resolve_(GetExecutor()->CanResolve()),
        executor_can_reject_(GetExecutor()->CanReject()),
#endif
        dependents_(tag),
        prerequisites_(std::move(prerequisites)) {
#if DCHECK_IS_ON()
    {
      CheckedAutoLock lock(GetCheckedLock());
      if (executor_can_resolve_) {
        this_resolve_ =
            MakeRefCounted<DoubleMoveDetector>(from_here_, "resolve");
      }

      if (executor_can_reject_) {
        this_reject_ = MakeRefCounted<DoubleMoveDetector>(from_here_, "reject");

        if (reject_policy_ == RejectPolicy::kMustCatchRejection) {
          this_must_catch_ = MakeRefCounted<LocationRef>(from_here_);
        }
      }
    }
#endif
  }

  NOINLINE ~AbstractPromise();

  // Follows the chain of CurriedPromises attempting to find the non-curried
  // root. This isn't always possible because some nodes may not have settled
  // yet, in which case the non-settled ancestor is returned. A node may also
  // have been canceled, in which case null is returned.
  AbstractPromise* FindCurriedAncestor();

  // Signals that |value_| now contains a resolve value. Dependent promises may
  // scheduled for execution.
  void OnResolved();

  // Signals that |value_| now contains a reject value. Dependent promises may
  // scheduled for execution.
  void OnRejected();

  // Returns the curried promise if there is one or null otherwise.
  AbstractPromise* GetCurriedPromise();

  // Returns the associated PromiseExecutor if there is one.
  const PromiseExecutor* GetExecutor() const;

  PromiseExecutor* GetExecutor() {
    return const_cast<PromiseExecutor*>(
        const_cast<const AbstractPromise*>(this)->GetExecutor());
  }

  // With the exception of curried promises, this may only be called before the
  // executor has run.
  PromiseExecutor::PrerequisitePolicy GetPrerequisitePolicy();

  void AddAsDependentForAllPrerequisites();

  // If the promise hasn't executed then |node| is added to the list. If it has
  // and it was resolved or rejected then the corresponding promise is scheduled
  // for execution if necessary. If this promise was canceled this is a NOP.
  // Returns false if this operation failed because this promise became canceled
  // as a result of adding a dependency on a canceled |node|.
  bool InsertDependentOnAnyThread(DependentList::Node* node);

  // Checks if the promise is now ready to be executed and if so posts it on the
  // given task runner.
  void OnPrerequisiteResolved(AbstractPromise* resolved_prerequisite);

  // Schedules the promise for execution.
  void OnPrerequisiteRejected(AbstractPromise* rejected_prerequisite);

  // Returns true if we are still potentially eligible to run despite the
  // cancellation.
  bool OnPrerequisiteCancelled(AbstractPromise* canceled_prerequisite);

  // This promise was resolved, post any dependent promises that are now ready
  // as a result.
  void OnResolveDispatchReadyDependents();

  // This promise was rejected, post any dependent promises that are now ready
  // as a result.
  void OnRejectDispatchReadyDependents();

  // This promise was resolved with a curried promise, make any dependent
  // promises depend on |non_curried_root| instead.
  void OnResolveMakeDependantsUseCurriedPrerequisite(
      AbstractPromise* non_curried_root);

  // This promise was rejected with a curried promise, make any dependent
  // promises depend on |non_curried_root| instead.
  void OnRejectMakeDependantsUseCurriedPrerequisite(
      AbstractPromise* non_curried_root);

  void DispatchPromise();

  // Reverses |list| so dependents can be dispatched in the order they where
  // added. Assumes no other thread is accessing |list|.
  static DependentList::Node* NonThreadSafeReverseList(
      DependentList::Node* list);

  void ReplaceCurriedPrerequisite(AbstractPromise* curried_prerequisite,
                                  AbstractPromise* replacement);

  scoped_refptr<TaskRunner> task_runner_;

  const Location from_here_;

  // To save memory |value_| contains Executor (which is stored inline) before
  // it has run and afterwards it contains one of:
  // * Resolved<T>
  // * Rejected<T>
  // * scoped_refptr<AbstractPromise> (for curried promises - i.e. a promise
  //   which is resolved with a promise).
  //
  // The state transitions which occur during Execute() (which is once only) are
  // like so:
  //
  //      ┌────────── Executor ─────────┐
  //      |               |             │
  //      |               |             │
  //      ↓               |             ↓
  // Resolved<T>          |        Rejected<T>
  //                      ↓
  //        scoped_refptr<AbstractPromise>
  //
  PromiseValue value_;

#if DCHECK_IS_ON()
  // |on_api_error_callback| is called when an API usage error is spotted.
  static void SetApiErrorObserverForTesting(
      RepeatingClosure on_api_error_callback);

  void MaybeInheritChecks(AbstractPromise* source)
      EXCLUSIVE_LOCKS_REQUIRED(GetCheckedLock());

  // Controls how we deal with unhandled rejection.
  const RejectPolicy reject_policy_;

  // Cached because we need to access these values after the Executor they came
  // from has gone away.
  const PromiseExecutor::ArgumentPassingType resolve_argument_passing_type_;
  const PromiseExecutor::ArgumentPassingType reject_argument_passing_type_;
  const bool executor_can_resolve_;
  const bool executor_can_reject_;

  // Whether responsibility for catching rejected promise has been passed on to
  // this promise's dependents.
  bool passed_catch_responsibility_ GUARDED_BY(GetCheckedLock()) = false;

  static CheckedLock& GetCheckedLock();

  // Used to avoid refcounting cycles.
  class BASE_EXPORT LocationRef : public RefCountedThreadSafe<LocationRef> {
   public:
    explicit LocationRef(const Location& from_here);

    const Location& from_here() const { return from_here_; }

   private:
    Location from_here_;

    friend class RefCountedThreadSafe<LocationRef>;
    ~LocationRef();
  };

  // For catching missing catches.
  scoped_refptr<LocationRef> must_catch_ancestor_that_could_reject_
      GUARDED_BY(GetCheckedLock());

  // Used to supply all child nodes with a single LocationRef.
  scoped_refptr<LocationRef> this_must_catch_ GUARDED_BY(GetCheckedLock());

  class BASE_EXPORT DoubleMoveDetector
      : public RefCountedThreadSafe<DoubleMoveDetector> {
   public:
    DoubleMoveDetector(const Location& from_here, const char* callback_type);

    void CheckForDoubleMoveErrors(
        const base::Location& new_dependent_location,
        PromiseExecutor::ArgumentPassingType new_dependent_executor_type);

   private:
    const Location from_here_;
    const char* callback_type_;
    std::unique_ptr<Location> dependent_move_only_promise_;
    std::unique_ptr<Location> dependent_normal_promise_;

    friend class RefCountedThreadSafe<DoubleMoveDetector>;
    ~DoubleMoveDetector();
  };

  // Used to supply all child nodes with a single DoubleMoveDetector.
  scoped_refptr<DoubleMoveDetector> this_resolve_ GUARDED_BY(GetCheckedLock());

  // Used to supply all child nodes with a single DoubleMoveDetector.
  scoped_refptr<DoubleMoveDetector> this_reject_ GUARDED_BY(GetCheckedLock());

  // Validates that the value of this promise, or the value of the closest
  // ancestor that can resolve if this promise can't resolve, is not
  // double-moved.
  scoped_refptr<DoubleMoveDetector> ancestor_that_could_resolve_
      GUARDED_BY(GetCheckedLock());

  // Validates that the value of this promise, or the value of the closest
  // ancestor that can reject if this promise can't reject, is not
  // double-moved.
  scoped_refptr<DoubleMoveDetector> ancestor_that_could_reject_
      GUARDED_BY(GetCheckedLock());
#endif

  // List of promises which are dependent on this one.
  DependentList dependents_;

  // Details of any promises this promise is dependent on. If there are none
  // |prerequisites_| will be null. This is a space optimization for the common
  // case of a non-chained PostTask.
  std::unique_ptr<AdjacencyList> prerequisites_;
};

PassedPromise::PassedPromise(const scoped_refptr<AbstractPromise>& promise)
    : promise_(promise.get()) {
  promise_->AddRef();
}

// Non-templatized base class of the Promise<> template. This is a binary size
// optimization, letting us use an out of line destructor in the template
// instead of the more complex scoped_refptr<> destructor.
class BASE_EXPORT BasePromise {
 public:
  BasePromise();

  BasePromise(const BasePromise& other);
  BasePromise(BasePromise&& other) noexcept;

  BasePromise& operator=(const BasePromise& other);
  BasePromise& operator=(BasePromise&& other) noexcept;

  // We want an out of line destructor to reduce binary size.
  ~BasePromise();

  // Returns true if the promise is not null.
  operator bool() const { return abstract_promise_.get(); }

 protected:
  struct InlineConstructor {};

  explicit BasePromise(
      scoped_refptr<internal::AbstractPromise> abstract_promise);

  // We want this to be inlined to reduce binary size for the Promise<>
  // constructor. Its a template to bypass ChromiumStyle plugin which otherwise
  // insists this is out of line.
  template <typename T>
  explicit BasePromise(internal::PassedPromise&& passed_promise,
                       T InlineConstructor)
      : abstract_promise_(passed_promise.Release(), subtle::kAdoptRefTag) {}

  scoped_refptr<internal::AbstractPromise> abstract_promise_;
};

}  // namespace internal

// Wrapper around scoped_refptr<base::internal::AbstractPromise> which is
// intended for use by TaskRunner implementations.
class BASE_EXPORT WrappedPromise {
 public:
  WrappedPromise();

  explicit WrappedPromise(scoped_refptr<internal::AbstractPromise> promise);

  WrappedPromise(const WrappedPromise& other);
  WrappedPromise(WrappedPromise&& other) noexcept;

  WrappedPromise& operator=(const WrappedPromise& other);
  WrappedPromise& operator=(WrappedPromise&& other) noexcept;

  explicit WrappedPromise(internal::PassedPromise&& passed_promise);

  // Constructs a promise to run |task|.
  WrappedPromise(const Location& from_here, OnceClosure task);

  // If the WrappedPromise hasn't been executed, cleared or taken by
  // TakeForTesting, it will be canceled to prevent memory leaks of dependent
  // tasks that will never run.
  ~WrappedPromise();

  // Returns true if the promise is not null.
  operator bool() const { return promise_.get(); }

  bool IsCanceled() const {
    DCHECK(promise_);
    return promise_->IsCanceled();
  }

  void OnCanceled() {
    DCHECK(promise_);
    promise_->OnCanceled();
  }

  // Can only be called once, clears |promise_| after execution.
  void Execute();

  // Clears |promise_|.
  void Clear();

  const Location& from_here() const {
    DCHECK(promise_);
    return promise_->from_here();
  }

  scoped_refptr<internal::AbstractPromise>& GetForTesting() { return promise_; }

  scoped_refptr<internal::AbstractPromise> TakeForTesting() {
    return std::move(promise_);
  }

 private:
  template <typename ResolveType, typename RejectType>
  friend class Promise;

  template <typename T, typename... Args>
  friend class internal::PromiseCallbackHelper;

  friend class Promises;

  scoped_refptr<internal::AbstractPromise> promise_;
};

}  // namespace base

#endif  // BASE_TASK_PROMISE_ABSTRACT_PROMISE_H_
