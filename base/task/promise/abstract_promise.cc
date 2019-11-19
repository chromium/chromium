// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/promise/abstract_promise.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/sequenced_task_runner.h"
#include "base/task/promise/dependent_list.h"
#include "base/task/promise/post_task_executor.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace base {
namespace internal {

#if DCHECK_IS_ON()
namespace {

// static
RepeatingClosure& GetPromiseApiErrorCallback() {
  static NoDestructor<RepeatingClosure> on_api_error_callback;
  return *on_api_error_callback;
}

}  // namespace

// static
void AbstractPromise::SetApiErrorObserverForTesting(
    RepeatingClosure on_api_error_callback) {
  CheckedAutoLock lock(GetCheckedLock());
  GetPromiseApiErrorCallback() = std::move(on_api_error_callback);
}

// Like DCHECK except observable via
// AbstractPromise::SetApiErrorObserverForTesting. Exists to avoid DEATH_TESTs
// which are flaky with promises.
#define PROMISE_API_DCHECK(condition)                 \
  if (!(condition) && GetPromiseApiErrorCallback()) { \
    GetPromiseApiErrorCallback().Run();               \
    return;                                           \
  }                                                   \
  DCHECK(condition)

#endif  // DCHECK_IS_ON()

AbstractPromise::~AbstractPromise() {
#if DCHECK_IS_ON()
  {
    CheckedAutoLock lock(GetCheckedLock());

    PROMISE_API_DCHECK(!must_catch_ancestor_that_could_reject_ ||
                       passed_catch_responsibility_)
        << "Promise chain ending at " << from_here_.ToString()
        << " didn't have a catch for potentially rejecting promise here "
        << must_catch_ancestor_that_could_reject_->from_here().ToString();

    PROMISE_API_DCHECK(!this_must_catch_ || passed_catch_responsibility_)
        << "Potentially rejecting promise at " << from_here_.ToString()
        << " doesn't have a catch.";
  }
#endif

  // If we're not settled we might be retaining some promises which need to be
  // released to prevent memory leaks. If we are settled this does nothing.
  OnCanceled();
}

void AbstractPromise::EmplaceResolvedVoid() {
  emplace(Resolved<void>());
}

bool AbstractPromise::IsCanceled() const {
  if (dependents_.IsCanceled())
    return true;

  const PromiseExecutor* executor = GetExecutor();
  return executor && executor->IsCancelled();
}

void AbstractPromise::AddAsDependentForAllPrerequisites() {
  if (!prerequisites_)
    return;

  // Note a curried promise will eventually get to all its children and pass
  // them catch responsibility through AddAsDependentForAllPrerequisites,
  // although that'll be done lazily (only once they resolve/reject, so there
  // is a possibility the DCHECKs might be racy.

  for (DependentList::Node& node : *prerequisites_->prerequisite_list()) {
    node.dependent() = this;

    // If |node.prerequisite()| was canceled then early out because
    // |prerequisites_->prerequisite_list| will have been cleared.
    DCHECK(node.prerequisite());
    if (!node.prerequisite()->InsertDependentOnAnyThread(&node))
      break;
  }
}

bool AbstractPromise::InsertDependentOnAnyThread(DependentList::Node* node) {
  scoped_refptr<AbstractPromise>& dependent = node->dependent();

  // Used to ensure no reference to the dependent is kept in case the Promise is
  // already settled.
  scoped_refptr<AbstractPromise> dependent_to_release;

#if DCHECK_IS_ON()
  {
    CheckedAutoLock lock(GetCheckedLock());
    DCHECK(node->dependent()) << from_here_.ToString();
    node->dependent()->MaybeInheritChecks(this);
  }
#endif

  // If |dependents_| has been consumed (i.e. this promise has been resolved
  // or rejected) then |node| may be ready to run now.
  switch (dependents_.Insert(node)) {
    case DependentList::InsertResult::SUCCESS:
      break;

    case DependentList::InsertResult::FAIL_PROMISE_RESOLVED: {
      AbstractPromise* curried_promise = GetCurriedPromise();
      if (curried_promise) {
        // Try and reinsert |node| in the curried ancestor.
        node->SetPrerequisite(curried_promise);
        return curried_promise->InsertDependentOnAnyThread(node);
      } else {
        dependent_to_release = std::move(dependent);
        node->RetainSettledPrerequisite();
        dependent_to_release->OnPrerequisiteResolved(this);
      }
      break;
    }

    case DependentList::InsertResult::FAIL_PROMISE_REJECTED: {
      AbstractPromise* curried_promise = GetCurriedPromise();
      if (curried_promise) {
        // Try and reinsert |node| in the curried ancestor.
        node->SetPrerequisite(curried_promise);
        return curried_promise->InsertDependentOnAnyThread(node);
      } else {
        dependent_to_release = std::move(dependent);
        node->RetainSettledPrerequisite();
        dependent_to_release->OnPrerequisiteRejected(this);
      }
      break;
    }

    case DependentList::InsertResult::FAIL_PROMISE_CANCELED:
      dependent_to_release = std::move(dependent);
      return dependent_to_release->OnPrerequisiteCancelled(this);
  }

  return true;
}

void AbstractPromise::IgnoreUncaughtCatchForTesting() {
#if DCHECK_IS_ON()
  CheckedAutoLock lock(GetCheckedLock());
  passed_catch_responsibility_ = true;
#endif
}

#if DCHECK_IS_ON()

// static
CheckedLock& AbstractPromise::GetCheckedLock() {
  static base::NoDestructor<CheckedLock> instance;
  return *instance;
}

void AbstractPromise::DoubleMoveDetector::CheckForDoubleMoveErrors(
    const base::Location& new_dependent_location,
    PromiseExecutor::ArgumentPassingType new_dependent_executor_type) {
  switch (new_dependent_executor_type) {
    case PromiseExecutor::ArgumentPassingType::kNoCallback:
      return;

    case PromiseExecutor::ArgumentPassingType::kNormal:
      PROMISE_API_DCHECK(!dependent_move_only_promise_)
          << "Can't mix move only and non-move only " << callback_type_
          << "callback arguments for the same " << callback_type_
          << " prerequisite. See " << new_dependent_location.ToString()
          << " and " << dependent_move_only_promise_->ToString()
          << " with common ancestor " << from_here_.ToString();
      dependent_normal_promise_ =
          std::make_unique<Location>(new_dependent_location);
      return;

    case PromiseExecutor::ArgumentPassingType::kMove:
      PROMISE_API_DCHECK(!dependent_move_only_promise_ ||
                         *dependent_move_only_promise_ ==
                             new_dependent_location)
          << "Can't have multiple move only " << callback_type_
          << " callbacks for same " << callback_type_ << " prerequisite. See "
          << new_dependent_location.ToString() << " and "
          << dependent_move_only_promise_->ToString() << " with common "
          << callback_type_ << " prerequisite " << from_here_.ToString();
      PROMISE_API_DCHECK(!dependent_normal_promise_)
          << "Can't mix move only and non-move only " << callback_type_
          << " callback arguments for the same " << callback_type_
          << " prerequisite. See " << new_dependent_location.ToString()
          << " and " << dependent_normal_promise_->ToString() << " with common "
          << callback_type_ << " prerequisite " << from_here_.ToString();
      dependent_move_only_promise_ =
          std::make_unique<Location>(new_dependent_location);
      return;
  }
}

void AbstractPromise::MaybeInheritChecks(AbstractPromise* prerequisite) {
  if (!ancestor_that_could_resolve_) {
    // Inherit |prerequisite|'s resolve ancestor if it doesn't have a resolve
    // callback.
    if (prerequisite->resolve_argument_passing_type_ ==
        PromiseExecutor::ArgumentPassingType::kNoCallback) {
      ancestor_that_could_resolve_ = prerequisite->ancestor_that_could_resolve_;
    }

    // If |prerequisite| didn't have a resolve callback (but it's reject
    // callback could resolve) or if
    // |prerequisite->ancestor_that_could_resolve_| is null then assign
    // |prerequisite->this_resolve_|.
    if (!ancestor_that_could_resolve_ && prerequisite->executor_can_resolve_)
      ancestor_that_could_resolve_ = prerequisite->this_resolve_;
  }

  if (!ancestor_that_could_reject_) {
    // Inherit |prerequisite|'s reject ancestor if it doesn't have a Catch.
    if (prerequisite->reject_argument_passing_type_ ==
        PromiseExecutor::ArgumentPassingType::kNoCallback) {
      ancestor_that_could_reject_ = prerequisite->ancestor_that_could_reject_;
    }

    // If |prerequisite| didn't have a reject callback (but it's resolve
    // callback could reject) or if
    // |prerequisite->ancestor_that_could_resolve_| is null then assign
    // |prerequisite->this_reject_|.
    if (!ancestor_that_could_reject_ && prerequisite->executor_can_reject_)
      ancestor_that_could_reject_ = prerequisite->this_reject_;
  }

  if (!must_catch_ancestor_that_could_reject_) {
    // Inherit |prerequisite|'s must catch ancestor if it doesn't have a Catch.
    if (prerequisite->reject_argument_passing_type_ ==
        PromiseExecutor::ArgumentPassingType::kNoCallback) {
      must_catch_ancestor_that_could_reject_ =
          prerequisite->must_catch_ancestor_that_could_reject_;
    }

    // If |prerequisite| didn't have a reject callback (but it's resolve
    // callback could reject) or if
    // |prerequisite->must_catch_ancestor_that_could_reject_| is null then
    // assign |prerequisite->this_must_catch_|.
    if (!must_catch_ancestor_that_could_reject_ &&
        prerequisite->executor_can_reject_) {
      must_catch_ancestor_that_could_reject_ = prerequisite->this_must_catch_;
    }
  }

  if (ancestor_that_could_resolve_) {
    ancestor_that_could_resolve_->CheckForDoubleMoveErrors(
        from_here_, resolve_argument_passing_type_);
  }

  if (ancestor_that_could_reject_) {
    ancestor_that_could_reject_->CheckForDoubleMoveErrors(
        from_here_, reject_argument_passing_type_);
  }

  prerequisite->passed_catch_responsibility_ = true;
}

AbstractPromise::LocationRef::LocationRef(const Location& from_here)
    : from_here_(from_here) {}

AbstractPromise::LocationRef::~LocationRef() = default;

AbstractPromise::DoubleMoveDetector::DoubleMoveDetector(
    const Location& from_here,
    const char* callback_type)
    : from_here_(from_here), callback_type_(callback_type) {}

AbstractPromise::DoubleMoveDetector::~DoubleMoveDetector() = default;

#endif

AbstractPromise* AbstractPromise::GetCurriedPromise() {
  if (!value_.ContainsCurriedPromise())
    return nullptr;
  return value_.Get<scoped_refptr<AbstractPromise>>()->get();
}

const PromiseExecutor* AbstractPromise::GetExecutor() const {
  if (!value_.ContainsPromiseExecutor())
    return nullptr;
  return value_.Get<PromiseExecutor>();
}

PromiseExecutor::PrerequisitePolicy AbstractPromise::GetPrerequisitePolicy() {
  PromiseExecutor* executor = GetExecutor();
  if (!executor) {
    // If there's no executor it's because the promise has already run. We
    // can't run again however. The only circumstance in which we expect
    // GetPrerequisitePolicy() to be called after execution is when it was
    // resolved with a promise or we're already settled.
    DCHECK(IsSettled());
    return PromiseExecutor::PrerequisitePolicy::kNever;
  }
  return executor->GetPrerequisitePolicy();
}

AbstractPromise* AbstractPromise::GetFirstSettledPrerequisite() const {
  DCHECK(prerequisites_);
  return prerequisites_->GetFirstSettledPrerequisite();
}

void AbstractPromise::Execute() {
  const PromiseExecutor* executor = GetExecutor();
  DCHECK(executor || dependents_.IsCanceled()) << from_here_.ToString();

  if (!executor || executor->IsCancelled()) {
    OnCanceled();
    return;
  }

#if DCHECK_IS_ON()
  // Clear |must_catch_ancestor_that_could_reject_| if we can catch it.
  if (reject_argument_passing_type_ !=
      PromiseExecutor::ArgumentPassingType::kNoCallback) {
    CheckedAutoLock lock(GetCheckedLock());
    must_catch_ancestor_that_could_reject_ = nullptr;
  }
#endif

  DCHECK(!IsResolvedWithPromise());

  // To reduce template bloat the Executor machinery deals with AbstractPromise
  // raw pointers. This is fine as long as we ensure the reference count doesn't
  // go to zero when we run the promise callback (which could do anything
  // including releasing what might otherwise be the last reference to the
  // AbstractPromise).
  scoped_refptr<AbstractPromise> protect(this);

  // This is likely to delete the executor.
  GetExecutor()->Execute(this);

  if (value_.ContainsRejected()) {
    OnRejected();
  } else if (value_.ContainsResolved() || value_.ContainsCurriedPromise()) {
    OnResolved();
  }
}

void AbstractPromise::ReplaceCurriedPrerequisite(
    AbstractPromise* curried_prerequisite,
    AbstractPromise* replacement) {
  DCHECK(curried_prerequisite->IsResolved() ||
         curried_prerequisite->IsRejected());
  DCHECK(curried_prerequisite->IsResolvedWithPromise());
  DCHECK(replacement);
  for (DependentList::Node& node : *prerequisites_->prerequisite_list()) {
    if (node.prerequisite() == curried_prerequisite) {
      node.Reset(replacement, this);
      replacement->InsertDependentOnAnyThread(&node);
      return;
    }
  }
  NOTREACHED();
}

void AbstractPromise::OnPrerequisiteResolved(
    AbstractPromise* resolved_prerequisite) {
  DCHECK(resolved_prerequisite->IsResolved());

  switch (GetPrerequisitePolicy()) {
    case PromiseExecutor::PrerequisitePolicy::kAll:
      if (prerequisites_->DecrementPrerequisiteCountAndCheckIfZero())
        DispatchPromise();
      break;

    case PromiseExecutor::PrerequisitePolicy::kAny:
      // PrerequisitePolicy::kAny should resolve immediately.
      if (prerequisites_->MarkPrerequisiteAsSettling(resolved_prerequisite))
        DispatchPromise();
      break;

    case PromiseExecutor::PrerequisitePolicy::kNever:
      break;
  }
}

void AbstractPromise::OnPrerequisiteRejected(
    AbstractPromise* rejected_prerequisite) {
  DCHECK(rejected_prerequisite->IsRejected());

  // Promises::All (or Race if we add that) can have multiple prerequisites and
  // it will reject as soon as any prerequisite rejects. Multiple prerequisites
  // can reject, but we wish to record only the first one. Also we can only
  // invoke executors once.
  if (prerequisites_->MarkPrerequisiteAsSettling(rejected_prerequisite)) {
    DispatchPromise();
  }
}

bool AbstractPromise::OnPrerequisiteCancelled(
    AbstractPromise* canceled_prerequisite) {
  switch (GetPrerequisitePolicy()) {
    case PromiseExecutor::PrerequisitePolicy::kAll:
      // PrerequisitePolicy::kAll should cancel immediately.
      OnCanceled();
      return false;

    case PromiseExecutor::PrerequisitePolicy::kAny:
      // PrerequisitePolicy::kAny should only cancel if all if it's
      // pre-requisites have been canceled.
      if (prerequisites_->DecrementPrerequisiteCountAndCheckIfZero()) {
        OnCanceled();
        return false;
      } else {
        prerequisites_->RemoveCanceledPrerequisite(canceled_prerequisite);
      }
      return true;

    case PromiseExecutor::PrerequisitePolicy::kNever:
      // If we where resolved with a promise then we can't have had
      // PrerequisitePolicy::kAny or PrerequisitePolicy::kNever before the
      // executor was replaced with the curried promise, so pass on
      // cancellation.
      if (IsResolvedWithPromise())
        OnCanceled();
      return false;
  }
}

void AbstractPromise::OnResolveDispatchReadyDependents() {
  class Visitor : public DependentList::Visitor {
   public:
    explicit Visitor(AbstractPromise* resolved_prerequisite)
        : resolved_prerequisite_(resolved_prerequisite) {}

   private:
    void Visit(scoped_refptr<AbstractPromise> dependent) override {
      dependent->OnPrerequisiteResolved(resolved_prerequisite_);
    }
    AbstractPromise* const resolved_prerequisite_;
  };

  Visitor visitor(this);
  dependents_.ResolveAndConsumeAllDependents(&visitor);
}

void AbstractPromise::OnRejectDispatchReadyDependents() {
  class Visitor : public DependentList::Visitor {
   public:
    explicit Visitor(AbstractPromise* rejected_prerequisite)
        : rejected_prerequisite_(rejected_prerequisite) {}

   private:
    void Visit(scoped_refptr<AbstractPromise> dependent) override {
      dependent->OnPrerequisiteRejected(rejected_prerequisite_);
    }
    AbstractPromise* const rejected_prerequisite_;
  };

  Visitor visitor(this);
  dependents_.RejectAndConsumeAllDependents(&visitor);
}

void AbstractPromise::OnResolveMakeDependantsUseCurriedPrerequisite(
    AbstractPromise* non_curried_root) {
  class Visitor : public DependentList::Visitor {
   public:
    explicit Visitor(AbstractPromise* resolved_prerequisite,
                     AbstractPromise* non_curried_root)
        : resolved_prerequisite_(resolved_prerequisite),
          non_curried_root_(non_curried_root) {}

   private:
    void Visit(scoped_refptr<AbstractPromise> dependent) override {
      dependent->ReplaceCurriedPrerequisite(resolved_prerequisite_,
                                            non_curried_root_);
    }
    AbstractPromise* const resolved_prerequisite_;
    AbstractPromise* const non_curried_root_;
  };

  Visitor visitor(this, non_curried_root);
  dependents_.ResolveAndConsumeAllDependents(&visitor);
}

void AbstractPromise::OnRejectMakeDependantsUseCurriedPrerequisite(
    AbstractPromise* non_curried_root) {
  class Visitor : public DependentList::Visitor {
   public:
    explicit Visitor(AbstractPromise* rejected_prerequisite,
                     AbstractPromise* non_curried_root)
        : rejected_prerequisite_(rejected_prerequisite),
          non_curried_root_(non_curried_root) {}

   private:
    void Visit(scoped_refptr<AbstractPromise> dependent) override {
      dependent->ReplaceCurriedPrerequisite(rejected_prerequisite_,
                                            non_curried_root_);
    }
    AbstractPromise* const rejected_prerequisite_;
    AbstractPromise* const non_curried_root_;
  };

  Visitor visitor(this, non_curried_root);
  dependents_.RejectAndConsumeAllDependents(&visitor);
}

void AbstractPromise::DispatchPromise() {
  if (task_runner_) {
    task_runner_->PostPromiseInternal(WrappedPromise(this), TimeDelta());
  } else {
    Execute();
  }
}

void AbstractPromise::OnCanceled() {
  class Visitor : public DependentList::Visitor {
   public:
    explicit Visitor(AbstractPromise* canceled_prerequisite)
        : canceled_prerequisite_(canceled_prerequisite) {}

   private:
    void Visit(scoped_refptr<AbstractPromise> dependent) override {
      dependent->OnPrerequisiteCancelled(canceled_prerequisite_);
    }

    AbstractPromise* const canceled_prerequisite_;
  };

  Visitor visitor(this);
  if (!dependents_.CancelAndConsumeAllDependents(&visitor))
    return;

  // The executor could be keeping a promise alive, but it's never going to run
  // so clear it.
  value_.reset();

#if DCHECK_IS_ON()
  {
    CheckedAutoLock lock(GetCheckedLock());
    passed_catch_responsibility_ = true;
  }
#endif

  if (prerequisites_)
    prerequisites_->Clear();
}

AbstractPromise* AbstractPromise::FindCurriedAncestor() {
  AbstractPromise* promise = this;
  while (promise->IsSettled()) {
    if (promise->IsCanceled())
      return nullptr;

    if (!promise->value_.ContainsCurriedPromise())
      break;

    promise = promise->value_.Get<scoped_refptr<AbstractPromise>>()->get();
  }
  return promise;
}

void AbstractPromise::OnResolved() {
#if DCHECK_IS_ON()
  PROMISE_API_DCHECK(executor_can_resolve_ || IsResolvedWithPromise())
      << from_here_.ToString();
#endif
  if (AbstractPromise* curried_promise = GetCurriedPromise()) {
#if DCHECK_IS_ON()
    {
      CheckedAutoLock lock(GetCheckedLock());
      MaybeInheritChecks(curried_promise);
    }
#endif

    curried_promise = curried_promise->FindCurriedAncestor();
    if (!curried_promise) {
      OnCanceled();
      return;
    }

    OnResolveMakeDependantsUseCurriedPrerequisite(curried_promise);
  } else {
    OnResolveDispatchReadyDependents();
  }

  if (prerequisites_)
    prerequisites_->Clear();
}

void AbstractPromise::OnRejected() {
#if DCHECK_IS_ON()
  PROMISE_API_DCHECK(executor_can_reject_) << from_here_.ToString();
#endif

  if (AbstractPromise* curried_promise = GetCurriedPromise()) {
#if DCHECK_IS_ON()
    {
      CheckedAutoLock lock(GetCheckedLock());
      MaybeInheritChecks(curried_promise);
    }
#endif

    curried_promise = curried_promise->FindCurriedAncestor();

    // It shouldn't be possible for OnRejected to be called with a canceled
    // curried promise because AbstractPromise::Execute regards a curried as a
    // resolved promise.
    DCHECK(curried_promise);
    OnRejectMakeDependantsUseCurriedPrerequisite(curried_promise);
  } else {
    OnRejectDispatchReadyDependents();
  }

  if (prerequisites_)
    prerequisites_->Clear();
}

AbstractPromise::AdjacencyList::AdjacencyList() = default;

AbstractPromise::AdjacencyList::AdjacencyList(AbstractPromise* prerequisite)
    : prerequisite_list_(1), action_prerequisite_count_(1) {
  prerequisite_list_[0].SetPrerequisite(prerequisite);
}

AbstractPromise::AdjacencyList::AdjacencyList(
    std::vector<DependentList::Node> nodes)
    : prerequisite_list_(std::move(nodes)),
      action_prerequisite_count_(prerequisite_list_.size()) {}

AbstractPromise::AdjacencyList::~AdjacencyList() = default;

bool AbstractPromise::AdjacencyList::
    DecrementPrerequisiteCountAndCheckIfZero() {
  return action_prerequisite_count_.fetch_sub(1, std::memory_order_acq_rel) ==
         1;
}

// For PrerequisitePolicy::kAll this is called for the first rejected
// prerequisite. For PrerequisitePolicy:kAny this is called for the first
// resolving or rejecting prerequisite.
bool AbstractPromise::AdjacencyList::MarkPrerequisiteAsSettling(
    AbstractPromise* settled_prerequisite) {
  DCHECK(settled_prerequisite->IsSettled());
  uintptr_t expected = 0;
  return first_settled_prerequisite_.compare_exchange_strong(
      expected, reinterpret_cast<uintptr_t>(settled_prerequisite),
      std::memory_order_acq_rel);
}

void AbstractPromise::AdjacencyList::RemoveCanceledPrerequisite(
    AbstractPromise* canceled_prerequisite) {
  DCHECK(canceled_prerequisite->IsCanceled());
  for (DependentList::Node& node : prerequisite_list_) {
    if (node.prerequisite() == canceled_prerequisite) {
      node.ClearPrerequisite();
      return;
    }
  }
  NOTREACHED() << "Couldn't find canceled_prerequisite "
               << canceled_prerequisite->from_here().ToString();
}

void AbstractPromise::AdjacencyList::Clear() {
  // If there's only one prerequisite we can just clear |prerequisite_list_|
  // which deals with potential refcounting cycles due to curried promises.
  if (prerequisite_list_.size() == 1) {
    prerequisite_list_.clear();
  } else {
    // If there's multiple prerequisites we can't do that because the
    // DependentList::Nodes may still be in use by some of them.
    // Instead we release our prerequisite references and rely on refcounting to
    // release the owning AbstractPromise.
    for (DependentList::Node& node : prerequisite_list_) {
      node.ClearPrerequisite();
    }
  }
}

BasePromise::BasePromise() = default;

BasePromise::BasePromise(
    scoped_refptr<internal::AbstractPromise> abstract_promise)
    : abstract_promise_(std::move(abstract_promise)) {}

BasePromise::BasePromise(const BasePromise& other) = default;
BasePromise::BasePromise(BasePromise&& other) = default;

BasePromise& BasePromise::operator=(const BasePromise& other) = default;
BasePromise& BasePromise::operator=(BasePromise&& other) = default;

BasePromise::~BasePromise() = default;

}  // namespace internal

WrappedPromise::WrappedPromise() = default;

WrappedPromise::WrappedPromise(scoped_refptr<internal::AbstractPromise> promise)
    : promise_(std::move(promise)) {}

WrappedPromise::WrappedPromise(internal::PassedPromise&& passed_promise)
    : promise_(passed_promise.Release(), subtle::kAdoptRefTag) {
  DCHECK(promise_);
}

WrappedPromise::WrappedPromise(const Location& from_here, OnceClosure task)
    : WrappedPromise(internal::AbstractPromise::CreateNoPrerequisitePromise(
          from_here,
          RejectPolicy::kMustCatchRejection,
          internal::DependentList::ConstructUnresolved(),
          internal::PromiseExecutor::Data(
              base::in_place_type_t<internal::PostTaskExecutor<void>>(),
              std::move(task)))) {}

WrappedPromise::WrappedPromise(const WrappedPromise& other) = default;
WrappedPromise::WrappedPromise(WrappedPromise&& other) = default;

WrappedPromise& WrappedPromise::operator=(const WrappedPromise& other) =
    default;
WrappedPromise& WrappedPromise::operator=(WrappedPromise&& other) = default;

WrappedPromise::~WrappedPromise() = default;

void WrappedPromise::Execute() {
  DCHECK(promise_);
  promise_->Execute();
}

void WrappedPromise::Clear() {
  promise_ = nullptr;
}

}  // namespace base
