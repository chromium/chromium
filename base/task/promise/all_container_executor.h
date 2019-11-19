// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_PROMISE_ALL_CONTAINER_EXECUTOR_H_
#define BASE_TASK_PROMISE_ALL_CONTAINER_EXECUTOR_H_

#include <tuple>

#include "base/task/promise/abstract_promise.h"
#include "base/task/promise/helpers.h"
#include "base/task/promise/no_op_promise_executor.h"

namespace base {
namespace internal {

// For Promises::All(std::vector<Promise<T>& promises)
template <typename ResolveType, typename RejectType>
class AllContainerPromiseExecutor {
 public:
  bool IsCancelled() const { return false; }

  static constexpr PromiseExecutor::PrerequisitePolicy kPrerequisitePolicy =
      PromiseExecutor::PrerequisitePolicy::kAll;

  struct VoidResolveType {};
  struct NonVoidResolveType {};

  using ResolveTypeTag = std::conditional_t<std::is_void<ResolveType>::value,
                                            VoidResolveType,
                                            NonVoidResolveType>;

  void Execute(AbstractPromise* promise) {
    // All is rejected if any prerequisites are rejected.
    AbstractPromise* first_settled = promise->GetFirstSettledPrerequisite();
    if (first_settled && first_settled->IsRejected()) {
      AllPromiseRejectHelper<Rejected<RejectType>>::Reject(promise,
                                                           first_settled);
      return;
    }

    ResolveInternal(promise, ResolveTypeTag());
  }

#if DCHECK_IS_ON()
  PromiseExecutor::ArgumentPassingType ResolveArgumentPassingType() const {
    return UseMoveSemantics<ResolveType>::argument_passing_type;
  }

  PromiseExecutor::ArgumentPassingType RejectArgumentPassingType() const {
    return UseMoveSemantics<RejectType>::argument_passing_type;
  }

  bool CanResolve() const {
    return !std::is_same<ResolveType, NoResolve>::value;
  }

  bool CanReject() const { return !std::is_same<RejectType, NoReject>::value; }
#endif

 private:
  // For containers of Promise<void> there is no point resolving with
  // std::vector<Void>.
  void ResolveInternal(AbstractPromise* promise, VoidResolveType) {
    promise->emplace(Resolved<void>());
  }

  void ResolveInternal(AbstractPromise* promise, NonVoidResolveType) {
    using NonVoidResolveType = ToNonVoidT<ResolveType>;
    Resolved<std::vector<NonVoidResolveType>> result;

    const std::vector<DependentList::Node>* prerequisite_list =
        promise->prerequisite_list();
    DCHECK(prerequisite_list);
    result.value.reserve(prerequisite_list->size());

    for (const auto& node : *prerequisite_list) {
      DCHECK(node.prerequisite()->IsResolved());
      result.value.push_back(
          ArgMoveSemanticsHelper<
              NonVoidResolveType,
              Resolved<NonVoidResolveType>>::Get(node.prerequisite()));
    }

    promise->emplace(std::move(result));
  }
};

template <typename Container, typename ContainerT>
struct AllContainerHelper;

template <typename Container, typename ResolveType, typename RejectType>
struct AllContainerHelper<Container, Promise<ResolveType, RejectType>> {
  using PromiseResolve = std::vector<ToNonVoidT<ResolveType>>;

  // As an optimization we don't return std::vector<ResolveType> for void
  // ResolveType.
  using PromiseType = std::conditional_t<std::is_void<ResolveType>::value,
                                         Promise<void, RejectType>,
                                         Promise<PromiseResolve, RejectType>>;

  static PromiseType All(const Location& from_here, const Container& promises) {
    size_t i = 0;
    std::vector<DependentList::Node> prerequisite_list(promises.size());
    // TODO(alexclarke): Move construction of this list and AbstractPromise out
    // of line to reduce template bloat.
    for (auto& promise : promises) {
      prerequisite_list[i++].SetPrerequisite(promise.abstract_promise_.get());
    }

    internal::PromiseExecutor::Data executor_data(
        (in_place_type_t<
            AllContainerPromiseExecutor<ResolveType, RejectType>>()));

    return PromiseType(AbstractPromise::Create(
        nullptr, from_here,
        std::make_unique<AbstractPromise::AdjacencyList>(
            std::move(prerequisite_list)),
        RejectPolicy::kMustCatchRejection, DependentList::ConstructUnresolved(),
        std::move(executor_data)));
  }
};

// TODO(alexclarke): Maybe specalize to support containers of variants.
// E.g. Promises::All(std::vector<Variant<Promise<T>...>>& promises)

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_PROMISE_ALL_CONTAINER_EXECUTOR_H_
