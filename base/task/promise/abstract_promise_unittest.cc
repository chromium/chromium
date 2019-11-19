// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/promise/abstract_promise.h"

#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/do_nothing_promise.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Errors from PROMISE_API_DCHECK are only observable in builds where DCHECKS
// are on.
#if DCHECK_IS_ON()
#define PROMISE_API_DCHECK_TEST(test_name) test_name
#else
#define PROMISE_API_DCHECK_TEST(test_name) DISABLED_##test_name
#endif

#define EXPECT_PROMISE_DCHECK_FAIL(code_that_should_fail)          \
  {                                                                \
    bool api_error_reported = false;                               \
    SetApiErrorObserver(                                           \
        BindLambdaForTesting([&] { api_error_reported = true; })); \
    code_that_should_fail;                                         \
    EXPECT_TRUE(api_error_reported);                               \
    SetApiErrorObserver(RepeatingClosure());                       \
  }

using testing::ElementsAre;

using ArgumentPassingType =
    base::internal::PromiseExecutor::ArgumentPassingType;

using PrerequisitePolicy = base::internal::PromiseExecutor::PrerequisitePolicy;

namespace base {
namespace internal {
namespace {

size_t CountTasksRunUntilIdle(
    const scoped_refptr<TestSimpleTaskRunner>& task_runner) {
  size_t count = 0;
  while (task_runner->HasPendingTask()) {
    count += task_runner->NumPendingTasks();
    task_runner->RunPendingTasks();
  }
  return count;
}

}  // namespace

template <PrerequisitePolicy PREREQUISITE_POLICY>
class TestExecutor {
 public:
  TestExecutor(
#if DCHECK_IS_ON()
      ArgumentPassingType resolve_executor_type,
      ArgumentPassingType reject_executor_type,
      bool can_resolve,
      bool can_reject,
#endif
      base::OnceCallback<void(AbstractPromise*)> callback)
      : callback_(std::move(callback))
#if DCHECK_IS_ON()
        ,
        resolve_argument_passing_type_(resolve_executor_type),
        reject_argument_passing_type_(reject_executor_type),
        resolve_flags_(can_resolve + (can_reject << 1))
#endif
  {
  }

#if DCHECK_IS_ON()
  ArgumentPassingType ResolveArgumentPassingType() const {
    return resolve_argument_passing_type_;
  }

  ArgumentPassingType RejectArgumentPassingType() const {
    return reject_argument_passing_type_;
  }

  bool CanResolve() const { return resolve_flags_ & 1; }

  bool CanReject() const { return resolve_flags_ & 2; }
#endif

  static constexpr PromiseExecutor::PrerequisitePolicy kPrerequisitePolicy =
      PREREQUISITE_POLICY;

  bool IsCancelled() const { return false; }

  void Execute(AbstractPromise* p) { std::move(callback_).Run(p); }

 private:
  base::OnceCallback<void(AbstractPromise*)> callback_;
#if DCHECK_IS_ON()
  const ArgumentPassingType resolve_argument_passing_type_;
  const ArgumentPassingType reject_argument_passing_type_;
  // On 32 bit platform we need to pack to fit in the space requirement of 3x
  // void*.
  uint8_t resolve_flags_;
#endif
};

class AbstractPromiseTest : public testing::Test {
 public:
  void SetApiErrorObserver(RepeatingClosure on_api_error_callback) {
#if DCHECK_IS_ON()
    AbstractPromise::SetApiErrorObserverForTesting(
        std::move(on_api_error_callback));
#endif
  }

  enum class CallbackResultType : uint8_t {
    kNoCallback,
    kCanResolve,
    kCanReject,
    kCanResolveOrReject,
  };

  struct PromiseSettings {
    PromiseSettings(
        Location from_here,
        std::unique_ptr<AbstractPromise::AdjacencyList> prerequisites)
        : from_here(from_here), prerequisites(std::move(prerequisites)) {}

    Location from_here;

    std::unique_ptr<AbstractPromise::AdjacencyList> prerequisites;

    PrerequisitePolicy prerequisite_policy =
        PromiseExecutor::PrerequisitePolicy::kAll;

    bool executor_can_resolve = true;

    bool executor_can_reject = false;

    ArgumentPassingType resolve_executor_type = ArgumentPassingType::kNormal;

    ArgumentPassingType reject_executor_type = ArgumentPassingType::kNoCallback;

    RejectPolicy reject_policy = RejectPolicy::kMustCatchRejection;

    base::OnceCallback<void(AbstractPromise*)> callback;

    scoped_refptr<TaskRunner> task_runner = ThreadTaskRunnerHandle::Get();
  };

  class PromiseSettingsBuilder {
   public:
    PromiseSettingsBuilder(
        Location from_here,
        std::unique_ptr<AbstractPromise::AdjacencyList> prerequisites)
        : settings(from_here, std::move(prerequisites)) {}

    PromiseSettingsBuilder& With(PrerequisitePolicy prerequisite_policy) {
      settings.prerequisite_policy = prerequisite_policy;
      return *this;
    }

    PromiseSettingsBuilder& With(const scoped_refptr<TaskRunner>& task_runner) {
      settings.task_runner = task_runner;
      return *this;
    }

    PromiseSettingsBuilder& With(RejectPolicy reject_policy) {
      settings.reject_policy = reject_policy;
      return *this;
    }

    PromiseSettingsBuilder& With(
        base::OnceCallback<void(AbstractPromise*)> callback) {
      settings.callback = std::move(callback);
      return *this;
    }

    PromiseSettingsBuilder& With(CallbackResultType callback_result_type) {
      switch (callback_result_type) {
        case CallbackResultType::kNoCallback:
          settings.executor_can_resolve = false;
          settings.executor_can_reject = false;
          break;
        case CallbackResultType::kCanResolve:
          settings.executor_can_resolve = true;
          settings.executor_can_reject = false;
          break;
        case CallbackResultType::kCanReject:
          settings.executor_can_resolve = false;
          settings.executor_can_reject = true;
          break;
        case CallbackResultType::kCanResolveOrReject:
          settings.executor_can_resolve = true;
          settings.executor_can_reject = true;
          break;
      };
      return *this;
    }

    PromiseSettingsBuilder& WithResolve(
        ArgumentPassingType resolve_executor_type) {
      settings.resolve_executor_type = resolve_executor_type;
      return *this;
    }

    PromiseSettingsBuilder& WithReject(
        ArgumentPassingType reject_executor_type) {
      settings.reject_executor_type = reject_executor_type;
      return *this;
    }

    operator scoped_refptr<AbstractPromise>() {
      switch (settings.prerequisite_policy) {
        case PrerequisitePolicy::kAll:
          return MakeAbstractPromise<PrerequisitePolicy::kAll>();

        case PrerequisitePolicy::kAny:
          return MakeAbstractPromise<PrerequisitePolicy::kAny>();

        case PrerequisitePolicy::kNever:
          return MakeAbstractPromise<PrerequisitePolicy::kNever>();
      }
    }

   private:
    template <PrerequisitePolicy POLICY>
    scoped_refptr<AbstractPromise> MakeAbstractPromise() {
      PromiseExecutor::Data executor_data(
          in_place_type_t<TestExecutor<POLICY>>(),
#if DCHECK_IS_ON()
          settings.resolve_executor_type, settings.reject_executor_type,
          settings.executor_can_resolve, settings.executor_can_reject,
#endif
          std::move(settings.callback));

      return WrappedPromise(AbstractPromise::Create(
                                settings.task_runner, settings.from_here,
                                std::move(settings.prerequisites),
                                settings.reject_policy,
                                DependentList::ConstructUnresolved(),
                                std::move(executor_data)))
          .TakeForTesting();
    }

    PromiseSettings settings;
  };

  PromiseSettingsBuilder ThenPromise(Location from_here,
                                     scoped_refptr<AbstractPromise> parent) {
    PromiseSettingsBuilder builder(
        from_here,
        parent ? std::make_unique<AbstractPromise::AdjacencyList>(parent.get())
               : std::make_unique<AbstractPromise::AdjacencyList>());
    builder.With(BindOnce([](AbstractPromise* p) {
      AbstractPromise* prerequisite = p->GetOnlyPrerequisite();
      if (prerequisite->IsResolved()) {
        p->emplace(Resolved<void>());
      } else if (prerequisite->IsRejected()) {
        // Consistent with BaseThenAndCatchExecutor::ProcessNullExecutor.
        p->emplace(scoped_refptr<AbstractPromise>(prerequisite));
      } else {
        NOTREACHED();
      }
    }));
    return builder;
  }

  PromiseSettingsBuilder CatchPromise(Location from_here,
                                      scoped_refptr<AbstractPromise> parent) {
    PromiseSettingsBuilder builder(
        from_here,
        parent ? std::make_unique<AbstractPromise::AdjacencyList>(parent.get())
               : std::make_unique<AbstractPromise::AdjacencyList>());
    builder.With(CallbackResultType::kNoCallback)
        .With(CallbackResultType::kCanResolve)
        .WithResolve(ArgumentPassingType::kNoCallback)
        .WithReject(ArgumentPassingType::kNormal)
        .With(BindOnce([](AbstractPromise* p) {
          AbstractPromise* prerequisite = p->GetOnlyPrerequisite();
          if (prerequisite->IsResolved()) {
            // Consistent with BaseThenAndCatchExecutor::ProcessNullExecutor.
            p->emplace(scoped_refptr<AbstractPromise>(prerequisite));
          } else if (prerequisite->IsRejected()) {
            p->emplace(Resolved<void>());
          } else {
            NOTREACHED();
          }
        }));
    return builder;
  }

  PromiseSettingsBuilder AllPromise(
      Location from_here,
      std::vector<DependentList::Node> prerequisite_list) {
    PromiseSettingsBuilder builder(
        from_here, std::make_unique<AbstractPromise::AdjacencyList>(
                       std::move(prerequisite_list)));
    builder.With(PrerequisitePolicy::kAll)
        .With(BindOnce([](AbstractPromise* p) {
          AbstractPromise* first_settled = p->GetFirstSettledPrerequisite();
          if (first_settled && first_settled->IsRejected()) {
            p->emplace(Rejected<void>());
            return;
          }

          p->emplace(Resolved<void>());
        }));
    return builder;
  }

  PromiseSettingsBuilder AnyPromise(
      Location from_here,
      std::vector<internal::DependentList::Node> prerequisite_list) {
    PromiseSettingsBuilder builder(
        from_here, std::make_unique<AbstractPromise::AdjacencyList>(
                       std::move(prerequisite_list)));
    builder.With(PrerequisitePolicy::kAny)
        .With(BindOnce([](AbstractPromise* p) {
          AbstractPromise* first_settled = p->GetFirstSettledPrerequisite();
          if (first_settled && first_settled->IsRejected()) {
            p->emplace(Rejected<void>());
            return;
          }

          p->emplace(Resolved<void>());
        }));
    return builder;
  }

  // Convenience wrappers for calling private methods.
  static void OnCanceled(scoped_refptr<AbstractPromise> promise) {
    promise->OnCanceled();
  }

  static void OnResolved(scoped_refptr<AbstractPromise> promise) {
    promise->OnResolved();
  }

  static void OnRejected(scoped_refptr<AbstractPromise> promise) {
    promise->OnRejected();
  }

  static AbstractPromise* GetCurriedPromise(AbstractPromise* promise) {
    return promise->GetCurriedPromise();
  }

  test::TaskEnvironment task_environment_;
};

TEST_F(AbstractPromiseTest, UnfulfilledPromise) {
  scoped_refptr<AbstractPromise> promise =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  EXPECT_FALSE(promise->IsResolvedForTesting());
  EXPECT_FALSE(promise->IsRejectedForTesting());
  EXPECT_FALSE(promise->IsCanceled());
}

TEST_F(AbstractPromiseTest, OnResolve) {
  scoped_refptr<AbstractPromise> promise =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  EXPECT_FALSE(promise->IsResolvedForTesting());
  OnResolved(promise);
  EXPECT_TRUE(promise->IsResolvedForTesting());
}

TEST_F(AbstractPromiseTest, OnReject) {
  scoped_refptr<AbstractPromise> promise =
      DoNothingPromiseBuilder(FROM_HERE).SetCanReject(true).SetRejectPolicy(
          RejectPolicy::kCatchNotRequired);
  EXPECT_FALSE(promise->IsRejectedForTesting());
  OnRejected(promise);
  EXPECT_TRUE(promise->IsRejectedForTesting());
}

TEST_F(AbstractPromiseTest, ExecuteOnResolve) {
  scoped_refptr<AbstractPromise> promise =
      ThenPromise(FROM_HERE, nullptr).With(BindOnce([](AbstractPromise* p) {
        p->emplace(Resolved<void>());
      }));

  EXPECT_FALSE(promise->IsResolvedForTesting());
  promise->Execute();
  EXPECT_TRUE(promise->IsResolvedForTesting());
}

TEST_F(AbstractPromiseTest, ExecuteOnReject) {
  scoped_refptr<AbstractPromise> promise =
      ThenPromise(FROM_HERE, nullptr)
          .With(RejectPolicy::kCatchNotRequired)
          .With(CallbackResultType::kCanReject)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Rejected<void>());
          }));

  EXPECT_FALSE(promise->IsRejectedForTesting());
  promise->Execute();
  EXPECT_TRUE(promise->IsRejectedForTesting());
}

TEST_F(AbstractPromiseTest, ExecutionChain) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p2 = ThenPromise(FROM_HERE, p1);
  scoped_refptr<AbstractPromise> p3 = ThenPromise(FROM_HERE, p2);
  scoped_refptr<AbstractPromise> p4 = ThenPromise(FROM_HERE, p3);
  scoped_refptr<AbstractPromise> p5 = ThenPromise(FROM_HERE, p4);

  OnResolved(p1);

  EXPECT_FALSE(p2->IsResolvedForTesting());
  EXPECT_FALSE(p3->IsResolvedForTesting());
  EXPECT_FALSE(p4->IsResolvedForTesting());
  EXPECT_FALSE(p5->IsResolvedForTesting());
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(p1->IsResolvedForTesting());
  EXPECT_TRUE(p3->IsResolvedForTesting());
  EXPECT_TRUE(p4->IsResolvedForTesting());
  EXPECT_TRUE(p5->IsResolvedForTesting());
}

TEST_F(AbstractPromiseTest, MoveExecutionChain) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p2 =
      ThenPromise(FROM_HERE, p1).WithResolve(ArgumentPassingType::kMove);

  scoped_refptr<AbstractPromise> p3 =
      ThenPromise(FROM_HERE, p2).WithResolve(ArgumentPassingType::kMove);

  scoped_refptr<AbstractPromise> p4 =
      ThenPromise(FROM_HERE, p3).WithResolve(ArgumentPassingType::kMove);

  scoped_refptr<AbstractPromise> p5 =
      ThenPromise(FROM_HERE, p4).WithResolve(ArgumentPassingType::kMove);

  OnResolved(p1);

  EXPECT_FALSE(p2->IsResolvedForTesting());
  EXPECT_FALSE(p3->IsResolvedForTesting());
  EXPECT_FALSE(p4->IsResolvedForTesting());
  EXPECT_FALSE(p5->IsResolvedForTesting());
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(p1->IsResolvedForTesting());
  EXPECT_TRUE(p3->IsResolvedForTesting());
  EXPECT_TRUE(p4->IsResolvedForTesting());
  EXPECT_TRUE(p5->IsResolvedForTesting());
}

TEST_F(AbstractPromiseTest, MoveResolveCatchExecutionChain) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p2 =
      ThenPromise(FROM_HERE, p1)
          .With(CallbackResultType::kCanReject)
          .WithResolve(ArgumentPassingType::kMove)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Rejected<void>());
          }));

  scoped_refptr<AbstractPromise> p3 =
      CatchPromise(FROM_HERE, p2)
          .With(CallbackResultType::kCanResolve)
          .WithReject(ArgumentPassingType::kMove)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Resolved<void>());
          }));

  scoped_refptr<AbstractPromise> p4 =
      ThenPromise(FROM_HERE, p3)
          .With(CallbackResultType::kCanReject)
          .WithResolve(ArgumentPassingType::kMove)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Rejected<void>());
          }));

  scoped_refptr<AbstractPromise> p5 =
      CatchPromise(FROM_HERE, p4)
          .With(CallbackResultType::kCanResolve)
          .WithReject(ArgumentPassingType::kMove)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Resolved<void>());
          }));

  OnResolved(p1);

  EXPECT_FALSE(p2->IsRejectedForTesting());
  EXPECT_FALSE(p3->IsResolvedForTesting());
  EXPECT_FALSE(p4->IsRejectedForTesting());
  EXPECT_FALSE(p5->IsResolvedForTesting());
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(p2->IsRejectedForTesting());
  EXPECT_TRUE(p3->IsResolvedForTesting());
  EXPECT_TRUE(p4->IsRejectedForTesting());
  EXPECT_TRUE(p5->IsResolvedForTesting());
}

TEST_F(AbstractPromiseTest, MoveResolveCatchExecutionChainType2) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p2 =
      ThenPromise(FROM_HERE, p1)
          .With(CallbackResultType::kCanReject)
          .WithResolve(ArgumentPassingType::kMove)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Rejected<void>());
          }));

  scoped_refptr<AbstractPromise> p3 =
      CatchPromise(FROM_HERE, p2)
          .With(CallbackResultType::kCanReject)
          .WithReject(ArgumentPassingType::kMove)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Rejected<void>());
          }));

  scoped_refptr<AbstractPromise> p4 =
      CatchPromise(FROM_HERE, p3)
          .With(CallbackResultType::kCanResolve)
          .WithReject(ArgumentPassingType::kMove)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Resolved<void>());
          }));

  scoped_refptr<AbstractPromise> p5 =
      ThenPromise(FROM_HERE, p4)
          .With(CallbackResultType::kCanResolve)
          .WithResolve(ArgumentPassingType::kMove)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Resolved<void>());
          }));

  scoped_refptr<AbstractPromise> p6 =
      ThenPromise(FROM_HERE, p5)
          .With(CallbackResultType::kCanReject)
          .WithResolve(ArgumentPassingType::kMove)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Rejected<void>());
          }));

  scoped_refptr<AbstractPromise> p7 =
      CatchPromise(FROM_HERE, p6)
          .With(CallbackResultType::kCanReject)
          .WithReject(ArgumentPassingType::kMove)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Rejected<void>());
          }));

  scoped_refptr<AbstractPromise> p8 =
      CatchPromise(FROM_HERE, p7)
          .With(CallbackResultType::kCanResolve)
          .WithReject(ArgumentPassingType::kMove)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Resolved<void>());
          }));

  scoped_refptr<AbstractPromise> p9 =
      ThenPromise(FROM_HERE, p8)
          .With(CallbackResultType::kCanResolve)
          .WithResolve(ArgumentPassingType::kMove)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Resolved<void>());
          }));
  OnResolved(p1);

  EXPECT_FALSE(p2->IsRejectedForTesting());
  EXPECT_FALSE(p3->IsRejectedForTesting());
  EXPECT_FALSE(p4->IsResolvedForTesting());
  EXPECT_FALSE(p5->IsResolvedForTesting());
  EXPECT_FALSE(p6->IsRejectedForTesting());
  EXPECT_FALSE(p7->IsRejectedForTesting());
  EXPECT_FALSE(p8->IsResolvedForTesting());
  EXPECT_FALSE(p9->IsResolvedForTesting());
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(p2->IsRejectedForTesting());
  EXPECT_TRUE(p3->IsRejectedForTesting());
  EXPECT_TRUE(p4->IsResolvedForTesting());
  EXPECT_TRUE(p5->IsResolvedForTesting());
  EXPECT_TRUE(p6->IsRejectedForTesting());
  EXPECT_TRUE(p7->IsRejectedForTesting());
  EXPECT_TRUE(p8->IsResolvedForTesting());
  EXPECT_TRUE(p9->IsResolvedForTesting());
}

TEST_F(AbstractPromiseTest, MixedMoveAndNormalExecutionChain) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p2 =
      ThenPromise(FROM_HERE, p1).WithResolve(ArgumentPassingType::kMove);

  scoped_refptr<AbstractPromise> p3 = ThenPromise(FROM_HERE, p2);

  scoped_refptr<AbstractPromise> p4 =
      ThenPromise(FROM_HERE, p3).WithResolve(ArgumentPassingType::kMove);

  scoped_refptr<AbstractPromise> p5 = ThenPromise(FROM_HERE, p4);

  OnResolved(p1);

  EXPECT_FALSE(p2->IsResolvedForTesting());
  EXPECT_FALSE(p3->IsResolvedForTesting());
  EXPECT_FALSE(p4->IsResolvedForTesting());
  EXPECT_FALSE(p5->IsResolvedForTesting());
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(p1->IsResolvedForTesting());
  EXPECT_TRUE(p3->IsResolvedForTesting());
  EXPECT_TRUE(p4->IsResolvedForTesting());
  EXPECT_TRUE(p5->IsResolvedForTesting());
}

TEST_F(AbstractPromiseTest, MoveAtEndOfChain) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p2 = ThenPromise(FROM_HERE, p1);
  scoped_refptr<AbstractPromise> p3 =
      ThenPromise(FROM_HERE, p2).WithResolve(ArgumentPassingType::kMove);
}

TEST_F(AbstractPromiseTest, BranchedExecutionChain) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p2 = ThenPromise(FROM_HERE, p1);
  scoped_refptr<AbstractPromise> p3 = ThenPromise(FROM_HERE, p2);
  scoped_refptr<AbstractPromise> p4 = ThenPromise(FROM_HERE, p1);
  scoped_refptr<AbstractPromise> p5 = ThenPromise(FROM_HERE, p4);

  OnResolved(p1);

  EXPECT_FALSE(p2->IsResolvedForTesting());
  EXPECT_FALSE(p3->IsResolvedForTesting());
  EXPECT_FALSE(p4->IsResolvedForTesting());
  EXPECT_FALSE(p5->IsResolvedForTesting());
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(p2->IsResolvedForTesting());
  EXPECT_TRUE(p3->IsResolvedForTesting());
  EXPECT_TRUE(p4->IsResolvedForTesting());
  EXPECT_TRUE(p5->IsResolvedForTesting());
}

TEST_F(AbstractPromiseTest, PrerequisiteAlreadyResolved) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  OnResolved(p1);

  scoped_refptr<AbstractPromise> p2 = ThenPromise(FROM_HERE, p1);

  EXPECT_FALSE(p2->IsResolvedForTesting());
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(p2->IsResolvedForTesting());
}

TEST_F(AbstractPromiseTest, PrerequisiteAlreadyRejected) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanReject(true).SetCanResolve(
          false);
  OnRejected(p1);
  ;

  scoped_refptr<AbstractPromise> p2 =
      CatchPromise(FROM_HERE, p1)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            EXPECT_TRUE(
                p->GetFirstSettledPrerequisite()->IsRejectedForTesting());
            EXPECT_EQ(p->GetFirstSettledPrerequisite(), p1);
            p->emplace(Resolved<void>());
          }));

  EXPECT_FALSE(p2->IsResolvedForTesting());
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(p2->IsResolvedForTesting());
}

TEST_F(AbstractPromiseTest, MultipleResolvedPrerequisitePolicyALL) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p2 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p3 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p4 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  std::vector<internal::DependentList::Node> prerequisite_list(4);
  prerequisite_list[0].SetPrerequisite(p1.get());
  prerequisite_list[1].SetPrerequisite(p2.get());
  prerequisite_list[2].SetPrerequisite(p3.get());
  prerequisite_list[3].SetPrerequisite(p4.get());

  scoped_refptr<AbstractPromise> all_promise =
      AllPromise(FROM_HERE, std::move(prerequisite_list));

  OnResolved(p1);
  OnResolved(p2);
  OnResolved(p3);
  RunLoop().RunUntilIdle();

  EXPECT_FALSE(all_promise->IsResolvedForTesting());
  OnResolved(p4);

  RunLoop().RunUntilIdle();
  EXPECT_TRUE(all_promise->IsResolvedForTesting());
}

TEST_F(AbstractPromiseTest,
       MultithreadedMultipleResolvedPrerequisitePolicyALL) {
  constexpr int num_threads = 4;
  constexpr int num_promises = 1000;

  std::unique_ptr<Thread> thread[num_threads];
  for (int i = 0; i < num_threads; i++) {
    thread[i] = std::make_unique<Thread>("Test thread");
    thread[i]->Start();
  }

  scoped_refptr<AbstractPromise> promise[num_promises];
  std::vector<internal::DependentList::Node> prerequisite_list(num_promises);
  for (int i = 0; i < num_promises; i++) {
    promise[i] = DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
    prerequisite_list[i].SetPrerequisite(promise[i].get());
  }

  scoped_refptr<AbstractPromise> all_promise =
      AllPromise(FROM_HERE, std::move(prerequisite_list));

  RunLoop run_loop;
  scoped_refptr<AbstractPromise> p2 =
      ThenPromise(FROM_HERE, all_promise)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            run_loop.Quit();
            p->emplace(Resolved<void>());
          }));

  for (int i = 0; i < num_promises; i++) {
    thread[i % num_threads]->task_runner()->PostTask(
        FROM_HERE, BindOnce(
                       [](scoped_refptr<AbstractPromise> all_promise,
                          scoped_refptr<AbstractPromise> promise) {
                         EXPECT_FALSE(all_promise->IsResolvedForTesting());
                         AbstractPromiseTest::OnResolved(promise);
                       },
                       all_promise, promise[i]));
  }

  run_loop.Run();

  for (int i = 0; i < num_promises; i++) {
    EXPECT_TRUE(promise[i]->IsResolvedForTesting());
  }

  EXPECT_TRUE(all_promise->IsResolvedForTesting());

  for (int i = 0; i < num_threads; i++) {
    thread[i]->Stop();
  }
}

TEST_F(AbstractPromiseTest, SingleRejectPrerequisitePolicyALL) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanReject(true).SetCanResolve(
          false);
  scoped_refptr<AbstractPromise> p2 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanReject(true).SetCanResolve(
          false);
  scoped_refptr<AbstractPromise> p3 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanReject(true).SetCanResolve(
          false);
  scoped_refptr<AbstractPromise> p4 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanReject(true).SetCanResolve(
          false);

  std::vector<internal::DependentList::Node> prerequisite_list(4);
  prerequisite_list[0].SetPrerequisite(p1.get());
  prerequisite_list[1].SetPrerequisite(p2.get());
  prerequisite_list[2].SetPrerequisite(p3.get());
  prerequisite_list[3].SetPrerequisite(p4.get());

  scoped_refptr<AbstractPromise> all_promise =
      AllPromise(FROM_HERE, std::move(prerequisite_list))
          .With(CallbackResultType::kCanResolveOrReject)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            EXPECT_TRUE(
                p->GetFirstSettledPrerequisite()->IsRejectedForTesting());
            EXPECT_EQ(p->GetFirstSettledPrerequisite(), p3);
            p->emplace(Rejected<void>());
          }));

  scoped_refptr<AbstractPromise> p5 = CatchPromise(FROM_HERE, all_promise);

  OnRejected(p3);
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(all_promise->IsRejectedForTesting());
  EXPECT_TRUE(p5->IsResolvedForTesting());
}

TEST_F(AbstractPromiseTest, MultipleRejectPrerequisitePolicyALL) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanReject(true).SetCanResolve(
          false);
  scoped_refptr<AbstractPromise> p2 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanReject(true).SetCanResolve(
          false);
  scoped_refptr<AbstractPromise> p3 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanReject(true).SetCanResolve(
          false);
  scoped_refptr<AbstractPromise> p4 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanReject(true).SetCanResolve(
          false);

  std::vector<internal::DependentList::Node> prerequisite_list(4);
  prerequisite_list[0].SetPrerequisite(p1.get());
  prerequisite_list[1].SetPrerequisite(p2.get());
  prerequisite_list[2].SetPrerequisite(p3.get());
  prerequisite_list[3].SetPrerequisite(p4.get());

  scoped_refptr<AbstractPromise> all_promise =
      AllPromise(FROM_HERE, std::move(prerequisite_list))
          .With(CallbackResultType::kCanResolveOrReject)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            AbstractPromise* settled = p->GetFirstSettledPrerequisite();
            if (settled && settled->IsRejected()) {
              EXPECT_EQ(settled, p2);
              p->emplace(Rejected<void>());
            } else {
              FAIL() << "A prerequisite was rejected";
            }
          }));

  scoped_refptr<AbstractPromise> p5 =
      CatchPromise(FROM_HERE, all_promise)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            EXPECT_FALSE(p->IsSettled());  // Should only happen once.
            EXPECT_TRUE(
                p->GetFirstSettledPrerequisite()->IsRejectedForTesting());
            EXPECT_EQ(p->GetFirstSettledPrerequisite(), all_promise);
            p->emplace(Resolved<void>());
          }));

  OnRejected(p2);
  OnRejected(p1);
  OnRejected(p3);
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(all_promise->IsRejectedForTesting());
  EXPECT_TRUE(p5->IsResolvedForTesting());
}

TEST_F(AbstractPromiseTest, SingleResolvedPrerequisitePolicyANY) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p2 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p3 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p4 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  std::vector<internal::DependentList::Node> prerequisite_list(4);
  prerequisite_list[0].SetPrerequisite(p1.get());
  prerequisite_list[1].SetPrerequisite(p2.get());
  prerequisite_list[2].SetPrerequisite(p3.get());
  prerequisite_list[3].SetPrerequisite(p4.get());

  scoped_refptr<AbstractPromise> any_promise =
      AnyPromise(FROM_HERE, std::move(prerequisite_list));

  OnResolved(p2);
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(any_promise->IsResolvedForTesting());
}

TEST_F(AbstractPromiseTest, MultipleResolvedPrerequisitePolicyANY) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p2 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p3 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p4 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  std::vector<internal::DependentList::Node> prerequisite_list(4);
  prerequisite_list[0].SetPrerequisite(p1.get());
  prerequisite_list[1].SetPrerequisite(p2.get());
  prerequisite_list[2].SetPrerequisite(p3.get());
  prerequisite_list[3].SetPrerequisite(p4.get());

  scoped_refptr<AbstractPromise> any_promise =
      AnyPromise(FROM_HERE, std::move(prerequisite_list));

  OnResolved(p1);
  OnResolved(p2);
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(any_promise->IsResolvedForTesting());
}

TEST_F(AbstractPromiseTest, SingleRejectPrerequisitePolicyANY) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanReject(true).SetCanResolve(
          false);
  scoped_refptr<AbstractPromise> p2 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanReject(true).SetCanResolve(
          false);
  scoped_refptr<AbstractPromise> p3 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanReject(true).SetCanResolve(
          false);
  scoped_refptr<AbstractPromise> p4 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanReject(true).SetCanResolve(
          false);

  std::vector<internal::DependentList::Node> prerequisite_list(4);
  prerequisite_list[0].SetPrerequisite(p1.get());
  prerequisite_list[1].SetPrerequisite(p2.get());
  prerequisite_list[2].SetPrerequisite(p3.get());
  prerequisite_list[3].SetPrerequisite(p4.get());

  scoped_refptr<AbstractPromise> any_promise =
      AnyPromise(FROM_HERE, std::move(prerequisite_list))
          .With(CallbackResultType::kCanResolveOrReject);

  scoped_refptr<AbstractPromise> p5 = CatchPromise(FROM_HERE, any_promise);

  OnRejected(p3);
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(any_promise->IsRejectedForTesting());
  EXPECT_TRUE(p5->IsResolvedForTesting());
}

TEST_F(AbstractPromiseTest, SingleResolvePrerequisitePolicyANY) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p2 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p3 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p4 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  std::vector<internal::DependentList::Node> prerequisite_list(4);
  prerequisite_list[0].SetPrerequisite(p1.get());
  prerequisite_list[1].SetPrerequisite(p2.get());
  prerequisite_list[2].SetPrerequisite(p3.get());
  prerequisite_list[3].SetPrerequisite(p4.get());

  scoped_refptr<AbstractPromise> any_promise =
      AnyPromise(FROM_HERE, std::move(prerequisite_list));

  scoped_refptr<AbstractPromise> p5 = CatchPromise(FROM_HERE, any_promise);

  OnResolved(p3);
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(any_promise->IsResolvedForTesting());
  EXPECT_TRUE(p5->IsResolvedForTesting());
}

TEST_F(AbstractPromiseTest, IsCanceled) {
  scoped_refptr<AbstractPromise> promise = ThenPromise(FROM_HERE, nullptr);
  EXPECT_FALSE(promise->IsCanceled());
  OnCanceled(promise);
  EXPECT_TRUE(promise->IsCanceled());
}

TEST_F(AbstractPromiseTest, OnCanceledPreventsExecution) {
  scoped_refptr<AbstractPromise> promise =
      ThenPromise(FROM_HERE, nullptr).With(BindOnce([](AbstractPromise* p) {
        FAIL() << "Should not be called";
      }));
  OnCanceled(promise);
  promise->Execute();
}

TEST_F(AbstractPromiseTest, CancelationStopsExecutionChain) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p2 =
      ThenPromise(FROM_HERE, p1).With(BindOnce([](AbstractPromise* p) {
        OnCanceled(p);
        OnCanceled(p);  // NOP shouldn't crash.
      }));
  scoped_refptr<AbstractPromise> p3 = ThenPromise(FROM_HERE, p2);
  scoped_refptr<AbstractPromise> p4 = ThenPromise(FROM_HERE, p3);

  OnResolved(p1);

  RunLoop().RunUntilIdle();
  EXPECT_TRUE(p3->IsCanceled());
  EXPECT_TRUE(p4->IsCanceled());
}

TEST_F(AbstractPromiseTest, CancelationStopsBranchedExecutionChain) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p2 =
      ThenPromise(FROM_HERE, p1).With(BindOnce([](AbstractPromise* p) {
        // This cancellation should get propagated down the chain which is
        // registered below.
        OnCanceled(p);
      }));

  // Branch one
  scoped_refptr<AbstractPromise> p3 = ThenPromise(FROM_HERE, p2);
  scoped_refptr<AbstractPromise> p4 = ThenPromise(FROM_HERE, p3);

  // Branch two
  scoped_refptr<AbstractPromise> p5 = ThenPromise(FROM_HERE, p2);
  scoped_refptr<AbstractPromise> promise6 = ThenPromise(FROM_HERE, p5);

  OnResolved(p1);

  RunLoop().RunUntilIdle();
  EXPECT_TRUE(p3->IsCanceled());
  EXPECT_TRUE(p4->IsCanceled());
  EXPECT_TRUE(p5->IsCanceled());
  EXPECT_TRUE(promise6->IsCanceled());
}

TEST_F(AbstractPromiseTest, CancelChainCanReject) {
  scoped_refptr<AbstractPromise> p0 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p1 =
      ThenPromise(FROM_HERE, p0)
          .With(CallbackResultType::kCanReject)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Rejected<void>());
          }));

  scoped_refptr<AbstractPromise> p2 = ThenPromise(FROM_HERE, p1);
  scoped_refptr<AbstractPromise> p3 = CatchPromise(FROM_HERE, p2);

  OnCanceled(p0);
  RunLoop().RunUntilIdle();
}

TEST_F(AbstractPromiseTest, CancelationPrerequisitePolicyALL) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p2 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p3 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  std::vector<internal::DependentList::Node> prerequisite_list(3);
  prerequisite_list[0].SetPrerequisite(p1.get());
  prerequisite_list[1].SetPrerequisite(p2.get());
  prerequisite_list[2].SetPrerequisite(p3.get());

  scoped_refptr<AbstractPromise> all_promise =
      AllPromise(FROM_HERE, std::move(prerequisite_list));

  OnCanceled(p2);
  EXPECT_TRUE(all_promise->IsCanceled());
}

TEST_F(AbstractPromiseTest, CancelationPrerequisitePolicyANY) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p2 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p3 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  std::vector<internal::DependentList::Node> prerequisite_list(3);
  prerequisite_list[0].SetPrerequisite(p1.get());
  prerequisite_list[1].SetPrerequisite(p2.get());
  prerequisite_list[2].SetPrerequisite(p3.get());

  scoped_refptr<AbstractPromise> any_promise =
      AnyPromise(FROM_HERE, std::move(prerequisite_list));

  OnCanceled(p3);
  OnCanceled(p2);
  EXPECT_FALSE(any_promise->IsCanceled());

  OnCanceled(p1);
  EXPECT_TRUE(any_promise->IsCanceled());
}

TEST_F(AbstractPromiseTest, AlreadyCanceledPrerequisitePolicyALL) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p2 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p3 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  std::vector<internal::DependentList::Node> prerequisite_list(3);
  prerequisite_list[0].SetPrerequisite(p1.get());
  prerequisite_list[1].SetPrerequisite(p2.get());
  prerequisite_list[2].SetPrerequisite(p3.get());
  OnCanceled(p2);

  scoped_refptr<AbstractPromise> all_promise =
      AllPromise(FROM_HERE, std::move(prerequisite_list));

  EXPECT_TRUE(all_promise->IsCanceled());
}

TEST_F(AbstractPromiseTest, SomeAlreadyCanceledPrerequisitePolicyANY) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p2 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p3 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  std::vector<internal::DependentList::Node> prerequisite_list(3);
  prerequisite_list[0].SetPrerequisite(p1.get());
  prerequisite_list[1].SetPrerequisite(p2.get());
  prerequisite_list[2].SetPrerequisite(p3.get());
  OnCanceled(p2);

  scoped_refptr<AbstractPromise> any_promise =
      AnyPromise(FROM_HERE, std::move(prerequisite_list));

  EXPECT_FALSE(any_promise->IsCanceled());
}

TEST_F(AbstractPromiseTest, AllAlreadyCanceledPrerequisitePolicyANY) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p2 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p3 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  std::vector<internal::DependentList::Node> prerequisite_list(3);
  prerequisite_list[0].SetPrerequisite(p1.get());
  prerequisite_list[1].SetPrerequisite(p2.get());
  prerequisite_list[2].SetPrerequisite(p3.get());
  OnCanceled(p1);
  OnCanceled(p2);
  OnCanceled(p3);

  scoped_refptr<AbstractPromise> any_promise =
      AnyPromise(FROM_HERE, std::move(prerequisite_list));

  EXPECT_TRUE(any_promise->IsCanceled());
}

TEST_F(AbstractPromiseTest, CurriedResolvedPromiseAny) {
  scoped_refptr<AbstractPromise> p0 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p2 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p1 =
      ThenPromise(FROM_HERE, p0)
          .With(BindOnce(
              [](scoped_refptr<AbstractPromise> p2, AbstractPromise* p) {
                p->emplace(std::move(p2));
              },
              p2))
          .With(PrerequisitePolicy::kAny);

  scoped_refptr<TestSimpleTaskRunner> task_runner =
      MakeRefCounted<TestSimpleTaskRunner>();
  scoped_refptr<AbstractPromise> p3 =
      ThenPromise(FROM_HERE, p1).With(task_runner);

  OnResolved(p0);
  OnResolved(p2);
  RunLoop().RunUntilIdle();

  // |p3| should run.
  EXPECT_EQ(1u, CountTasksRunUntilIdle(task_runner));
}

TEST_F(AbstractPromiseTest, CurriedRejectedPromiseAny) {
  scoped_refptr<AbstractPromise> p0 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  scoped_refptr<AbstractPromise> p2 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanReject(true);
  scoped_refptr<AbstractPromise> p1 =
      ThenPromise(FROM_HERE, p0)
          .With(BindOnce(
              [](scoped_refptr<AbstractPromise> p2, AbstractPromise* p) {
                p->emplace(std::move(p2));
              },
              p2))
          .With(PrerequisitePolicy::kAny);

  scoped_refptr<TestSimpleTaskRunner> task_runner =
      MakeRefCounted<TestSimpleTaskRunner>();
  scoped_refptr<AbstractPromise> p3 =
      CatchPromise(FROM_HERE, p1).With(task_runner);

  OnResolved(p0);
  OnRejected(p2);
  RunLoop().RunUntilIdle();

  // |p3| should run.
  EXPECT_EQ(1u, CountTasksRunUntilIdle(task_runner));
}

TEST_F(AbstractPromiseTest,
       PROMISE_API_DCHECK_TEST(DetectResolveDoubleMoveHazard)) {
  scoped_refptr<AbstractPromise> p0 = ThenPromise(FROM_HERE, nullptr);

  scoped_refptr<AbstractPromise> p1 =
      ThenPromise(FROM_HERE, p0).WithResolve(ArgumentPassingType::kMove);

  EXPECT_PROMISE_DCHECK_FAIL({
    scoped_refptr<AbstractPromise> p2 =
        ThenPromise(FROM_HERE, p0).WithResolve(ArgumentPassingType::kMove);
  });
}

TEST_F(
    AbstractPromiseTest,
    PROMISE_API_DCHECK_TEST(DetectMixedResolveCallbackMoveAndNonMoveHazard)) {
  scoped_refptr<AbstractPromise> p0 = ThenPromise(FROM_HERE, nullptr);

  scoped_refptr<AbstractPromise> p1 =
      ThenPromise(FROM_HERE, p0).WithResolve(ArgumentPassingType::kMove);

  EXPECT_PROMISE_DCHECK_FAIL(
      { scoped_refptr<AbstractPromise> p2 = ThenPromise(FROM_HERE, p0); });
}

TEST_F(AbstractPromiseTest, MultipleNonMoveCatchCallbacksAreOK) {
  /*
   * Key:  T = Then, C = Catch
   *
   *      P0
   *   T      T
   *   |      |
   *  \|      |/
   *  P1      P2
   *   T      T
   *   |      |
   *  \|      |/
   *  E3      E4
   *   C      C
   */
  scoped_refptr<AbstractPromise> p0 =
      ThenPromise(FROM_HERE, nullptr).With(CallbackResultType::kCanReject);

  scoped_refptr<AbstractPromise> p1 = ThenPromise(FROM_HERE, p0);
  scoped_refptr<AbstractPromise> p2 = ThenPromise(FROM_HERE, p0);
  scoped_refptr<AbstractPromise> p3 = CatchPromise(FROM_HERE, p1);
  scoped_refptr<AbstractPromise> p4 = CatchPromise(FROM_HERE, p2);
}

TEST_F(AbstractPromiseTest,
       PROMISE_API_DCHECK_TEST(DetectCatchCallbackDoubleMoveHazard)) {
  /*
   * Key:  T = Then, C = Catch
   *
   *      P0
   *   T      T
   *   |      |
   *  \|      |/
   *  P1      P2
   *   C      C
   *
   * We need to make sure P3 & P4's reject callback don't both use move
   * semantics since they share a common ancestor with no intermediate catches.
   */
  scoped_refptr<AbstractPromise> p0 =
      ThenPromise(FROM_HERE, nullptr).With(CallbackResultType::kCanReject);

  scoped_refptr<AbstractPromise> p1 =
      CatchPromise(FROM_HERE, p0).WithReject(ArgumentPassingType::kMove);

  EXPECT_PROMISE_DCHECK_FAIL({
    scoped_refptr<AbstractPromise> p2 =
        CatchPromise(FROM_HERE, p0).WithReject(ArgumentPassingType::kMove);
  });
}

TEST_F(AbstractPromiseTest,
       PROMISE_API_DCHECK_TEST(DetectCatchCallbackDoubleMoveHazardInChain)) {
  /*
   * Key:  T = Then, C = Catch
   *
   *      P0
   *   T      T
   *   |      |
   *  \|      |/
   *  P1      P2
   *   T      T
   *   |      |
   *  \|      |/
   *  P3      P4
   *   C      C
   *
   * We need to make sure P3 & P4's reject callback don't both use move
   * semantics since they share a common ancestor with no intermediate catches.
   */
  scoped_refptr<AbstractPromise> p0 =
      ThenPromise(FROM_HERE, nullptr).With(CallbackResultType::kCanReject);

  scoped_refptr<AbstractPromise> p1 = ThenPromise(FROM_HERE, p0);
  scoped_refptr<AbstractPromise> p2 = ThenPromise(FROM_HERE, p0);

  scoped_refptr<AbstractPromise> p3 =
      CatchPromise(FROM_HERE, p1).WithReject(ArgumentPassingType::kMove);

  EXPECT_PROMISE_DCHECK_FAIL({
    scoped_refptr<AbstractPromise> p4 =
        CatchPromise(FROM_HERE, p2).WithReject(ArgumentPassingType::kMove);
  });
}

TEST_F(
    AbstractPromiseTest,
    PROMISE_API_DCHECK_TEST(
        DetectCatchCallbackDoubleMoveHazardInChainIntermediateThensCanReject)) {
  /*
   * Key:  T = Then, C = Catch
   *
   *      P0
   *   T      T
   *   |      |
   *  \|      |/
   *  P1      P2
   *   T      T
   *   |      |
   *  \|      |/
   *  P3      P4
   *   C      C
   *
   * We need to make sure P3 & P4's reject callback don't both use move
   * semantics since they share a common ancestor with no intermediate catches.
   */
  scoped_refptr<AbstractPromise> p0 =
      ThenPromise(FROM_HERE, nullptr).With(CallbackResultType::kCanReject);

  scoped_refptr<AbstractPromise> p1 =
      ThenPromise(FROM_HERE, p0).With(CallbackResultType::kCanReject);

  scoped_refptr<AbstractPromise> p2 =
      ThenPromise(FROM_HERE, p0).With(CallbackResultType::kCanReject);

  scoped_refptr<AbstractPromise> p3 =
      CatchPromise(FROM_HERE, p1).WithReject(ArgumentPassingType::kMove);

  EXPECT_PROMISE_DCHECK_FAIL({
    scoped_refptr<AbstractPromise> p4 =
        CatchPromise(FROM_HERE, p2).WithReject(ArgumentPassingType::kMove);
  });
}

TEST_F(AbstractPromiseTest,
       PROMISE_API_DCHECK_TEST(DetectMixedCatchCallbackMoveAndNonMoveHazard)) {
  /*
   * Key:  T = Then, C = Catch
   *
   *      P0
   *   T      T
   *   |      |
   *  \|      |/
   *  P1      P2
   *   T      T
   *   |      |
   *  \|      |/
   *  P3      P4
   *   C      C
   *
   * We can't guarantee the order in which P3 and P4's reject callbacks run so
   * we need to need to catch the case where move and non-move semantics are
   * mixed.
   */
  scoped_refptr<AbstractPromise> p0 =
      ThenPromise(FROM_HERE, nullptr).With(CallbackResultType::kCanReject);

  scoped_refptr<AbstractPromise> p1 = ThenPromise(FROM_HERE, p0);
  scoped_refptr<AbstractPromise> p2 = ThenPromise(FROM_HERE, p0);

  scoped_refptr<AbstractPromise> p3 =
      CatchPromise(FROM_HERE, p1).WithReject(ArgumentPassingType::kMove);

  EXPECT_PROMISE_DCHECK_FAIL(
      { scoped_refptr<AbstractPromise> p4 = CatchPromise(FROM_HERE, p2); });
}

TEST_F(AbstractPromiseTest,
       PROMISE_API_DCHECK_TEST(DetectThenCallbackDoubleMoveHazardInChain)) {
  /*
   * Key:  T = Then, C = Catch
   *
   *      P0
   *   C      C
   *   |      |
   *  \|      |/
   *  P1      P2
   *   C      C
   *   |      |
   *  \|      |/
   *  P3      P4
   *   T      T
   *
   * We need to make sure P3 & P4's resolve callback don't both use move
   * semantics since they share a common ancestor with no intermediate then's.
   */
  scoped_refptr<AbstractPromise> p0 = ThenPromise(FROM_HERE, nullptr);
  scoped_refptr<AbstractPromise> p1 = CatchPromise(FROM_HERE, p0);
  scoped_refptr<AbstractPromise> p2 = CatchPromise(FROM_HERE, p0);

  scoped_refptr<AbstractPromise> p3 =
      ThenPromise(FROM_HERE, p1).WithResolve(ArgumentPassingType::kMove);

  EXPECT_PROMISE_DCHECK_FAIL({
    scoped_refptr<AbstractPromise> p4 =
        ThenPromise(FROM_HERE, p2).WithResolve(ArgumentPassingType::kMove);
  });
}

TEST_F(AbstractPromiseTest, PROMISE_API_DCHECK_TEST(SimpleMissingCatch)) {
  scoped_refptr<AbstractPromise> p0 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p1 =
      ThenPromise(FROM_HERE, p0)
          .With(CallbackResultType::kCanReject)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Rejected<void>());
          }));

  OnResolved(p0);
  RunLoop().RunUntilIdle();

  // An error should be reported when |p1| is deleted.
  EXPECT_PROMISE_DCHECK_FAIL({ p1 = nullptr; });
}

TEST_F(AbstractPromiseTest, PROMISE_API_DCHECK_TEST(MissingCatch)) {
  scoped_refptr<AbstractPromise> p0 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p1 =
      ThenPromise(FROM_HERE, p0)
          .With(CallbackResultType::kCanReject)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Rejected<void>());
          }));

  // The missing catch here will get noticed.
  scoped_refptr<AbstractPromise> p2 = ThenPromise(FROM_HERE, p1);

  OnResolved(p0);
  RunLoop().RunUntilIdle();

  // An error should be reported when |p2| is deleted.
  EXPECT_PROMISE_DCHECK_FAIL({ p2 = nullptr; });
}

TEST_F(AbstractPromiseTest, MissingCatchNotRequired) {
  scoped_refptr<AbstractPromise> p0 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p1 =
      ThenPromise(FROM_HERE, p0)
          .With(CallbackResultType::kCanReject)
          .With(RejectPolicy::kCatchNotRequired)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Rejected<void>());
          }));

  // The missing catch here will gets ignored.
  scoped_refptr<AbstractPromise> p2 = ThenPromise(FROM_HERE, p1);

  OnResolved(p0);

  RunLoop().RunUntilIdle();
}

TEST_F(AbstractPromiseTest,
       PROMISE_API_DCHECK_TEST(MissingCatchFromCurriedPromise)) {
  scoped_refptr<AbstractPromise> p0 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p1 =
      ThenPromise(FROM_HERE, nullptr)
          .With(CallbackResultType::kCanReject)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Rejected<void>());
          }));

  scoped_refptr<AbstractPromise> p2 =
      ThenPromise(FROM_HERE, p0)
          .With(BindOnce(
              [](scoped_refptr<AbstractPromise> p1, AbstractPromise* p) {
                // Resolve with a promise that can and does reject.
                ThreadTaskRunnerHandle::Get()->PostTask(
                    FROM_HERE, BindOnce(&AbstractPromise::Execute, p1));
                p->emplace(std::move(p1));
              },
              std::move(p1)));

  OnResolved(p0);
  RunLoop().RunUntilIdle();

  // An error should be reported when |p2| is deleted.
  EXPECT_PROMISE_DCHECK_FAIL({ p2 = nullptr; });
}

TEST_F(AbstractPromiseTest,
       PROMISE_API_DCHECK_TEST(MissingCatchFromCurriedPromiseWithDependent)) {
  scoped_refptr<AbstractPromise> p0 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p1 =
      ThenPromise(FROM_HERE, nullptr)
          .With(CallbackResultType::kCanReject)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Rejected<void>());
          }));

  scoped_refptr<AbstractPromise> p2 =
      ThenPromise(FROM_HERE, p0)
          .With(BindOnce(
              [&](scoped_refptr<AbstractPromise> p1, AbstractPromise* p) {
                // Resolve with a promise that can and does reject.
                ThreadTaskRunnerHandle::Get()->PostTask(
                    FROM_HERE, BindOnce(&AbstractPromise::Execute, p1));
                p->emplace(std::move(p1));
              },
              std::move(p1)));

  scoped_refptr<AbstractPromise> p3 = ThenPromise(FROM_HERE, p2);

  OnResolved(p0);
  RunLoop().RunUntilIdle();

  // An error should be reported when |p3| is deleted.
  EXPECT_PROMISE_DCHECK_FAIL({ p3 = nullptr; });
}

TEST_F(AbstractPromiseTest,
       PROMISE_API_DCHECK_TEST(
           MissingCatchFromCurriedPromiseWithDependentAddedAfterExecution)) {
  scoped_refptr<AbstractPromise> p0 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p1 =
      ThenPromise(FROM_HERE, nullptr)
          .With(CallbackResultType::kCanReject)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Rejected<void>());
          }));

  scoped_refptr<AbstractPromise> p2 =
      ThenPromise(FROM_HERE, p0)
          .With(BindOnce(
              [&](scoped_refptr<AbstractPromise> p1, AbstractPromise* p) {
                // Resolve with a promise that can and does reject.
                ThreadTaskRunnerHandle::Get()->PostTask(
                    FROM_HERE, BindOnce(&AbstractPromise::Execute, p1));
                p->emplace(std::move(p1));
              },
              std::move(p1)));

  OnResolved(p0);
  RunLoop().RunUntilIdle();

  scoped_refptr<AbstractPromise> p3 = ThenPromise(FROM_HERE, p2);
  RunLoop().RunUntilIdle();

  // An error should be reported when |p3| is deleted.
  EXPECT_PROMISE_DCHECK_FAIL({ p3 = nullptr; });
}

TEST_F(AbstractPromiseTest, PROMISE_API_DCHECK_TEST(MissingCatchLongChain)) {
  scoped_refptr<AbstractPromise> p0 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p1 =
      ThenPromise(FROM_HERE, p0)
          .With(CallbackResultType::kCanReject)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Rejected<void>());
          }));

  scoped_refptr<AbstractPromise> p2 = ThenPromise(FROM_HERE, p1);
  scoped_refptr<AbstractPromise> p3 = ThenPromise(FROM_HERE, p2);
  scoped_refptr<AbstractPromise> p4 = ThenPromise(FROM_HERE, p3);

  OnResolved(p0);
  RunLoop().RunUntilIdle();

  // An error should be reported when |p4| is deleted.
  EXPECT_PROMISE_DCHECK_FAIL({ p4 = nullptr; });
}

TEST_F(AbstractPromiseTest,
       PROMISE_API_DCHECK_TEST(
           ThenAddedToSettledPromiseWithMissingCatchAndSeveralDependents)) {
    scoped_refptr<AbstractPromise> p0 =
        DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

    scoped_refptr<AbstractPromise> p1 =
        ThenPromise(FROM_HERE, p0)
            .With(CallbackResultType::kCanReject)
            .With(BindOnce([](AbstractPromise* p) {
              p->emplace(Rejected<void>());
            }));

    scoped_refptr<AbstractPromise> p2 = ThenPromise(FROM_HERE, p1);
    scoped_refptr<AbstractPromise> p3 = ThenPromise(FROM_HERE, p2);
    scoped_refptr<AbstractPromise> p4 = ThenPromise(FROM_HERE, p2);

    OnResolved(p0);
    RunLoop().RunUntilIdle();

    scoped_refptr<AbstractPromise> p5 = ThenPromise(FROM_HERE, p2);

    RunLoop().RunUntilIdle();

    // An error should be reported when |p5| is deleted.
    EXPECT_PROMISE_DCHECK_FAIL({ p5 = nullptr; });

    p3->IgnoreUncaughtCatchForTesting();
    p4->IgnoreUncaughtCatchForTesting();
}

TEST_F(AbstractPromiseTest,
       PROMISE_API_DCHECK_TEST(ThenAddedAfterChainExecutionWithMissingCatch)) {
  scoped_refptr<AbstractPromise> p0 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p1 =
      ThenPromise(FROM_HERE, p0)
          .With(CallbackResultType::kCanReject)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Rejected<void>());
          }));

  scoped_refptr<AbstractPromise> p2 = ThenPromise(FROM_HERE, p1);
  scoped_refptr<AbstractPromise> p3 = ThenPromise(FROM_HERE, p2);

  OnResolved(p0);
  RunLoop().RunUntilIdle();

  // The missing catch here will get noticed.
  scoped_refptr<AbstractPromise> p4 = ThenPromise(FROM_HERE, p3);
  RunLoop().RunUntilIdle();

  // An error should be reported when |p4| is deleted.
  EXPECT_PROMISE_DCHECK_FAIL({ p4 = nullptr; });
}

TEST_F(AbstractPromiseTest, CatchAddedAfterChainExecution) {
  scoped_refptr<AbstractPromise> p0 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p1 =
      ThenPromise(FROM_HERE, p0)
          .With(CallbackResultType::kCanReject)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Rejected<void>());
          }));

  scoped_refptr<AbstractPromise> p2 = ThenPromise(FROM_HERE, p1);
  scoped_refptr<AbstractPromise> p3 = ThenPromise(FROM_HERE, p2);

  OnResolved(p0);
  RunLoop().RunUntilIdle();

  scoped_refptr<AbstractPromise> p4 = CatchPromise(FROM_HERE, p3);

  // We shouldn't get a DCHECK failure because |p4| catches the rejection
  // from |p1|.
  RunLoop().RunUntilIdle();
}

TEST_F(AbstractPromiseTest,
       PROMISE_API_DCHECK_TEST(MultipleThensAddedAfterChainExecution)) {
  scoped_refptr<AbstractPromise> p0 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p1 =
      ThenPromise(FROM_HERE, p0)
          .With(CallbackResultType::kCanReject)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Rejected<void>());
          }));

  scoped_refptr<AbstractPromise> p2 = ThenPromise(FROM_HERE, p1);
  scoped_refptr<AbstractPromise> p3 = ThenPromise(FROM_HERE, p2);

  // |p5| - |p7| should still inherit catch responsibility despite this.
  scoped_refptr<AbstractPromise> p4 = CatchPromise(FROM_HERE, p3);

  OnResolved(p0);
  RunLoop().RunUntilIdle();

  // The missing catches will get noticed.
  scoped_refptr<AbstractPromise> p5 = ThenPromise(FROM_HERE, p3);
  scoped_refptr<AbstractPromise> p6 = ThenPromise(FROM_HERE, p3);
  scoped_refptr<AbstractPromise> p7 = ThenPromise(FROM_HERE, p3);
  RunLoop().RunUntilIdle();

  // An error should be reported when |p5|, |p6| or |p7| are deleted.
  EXPECT_PROMISE_DCHECK_FAIL({ p5 = nullptr; });
  EXPECT_PROMISE_DCHECK_FAIL({ p6 = nullptr; });
  EXPECT_PROMISE_DCHECK_FAIL({ p7 = nullptr; });
}

TEST_F(AbstractPromiseTest, MultipleDependentsAddedAfterChainExecution) {
  scoped_refptr<AbstractPromise> p0 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p1 =
      ThenPromise(FROM_HERE, p0)
          .With(CallbackResultType::kCanReject)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Rejected<void>());
          }));

  scoped_refptr<AbstractPromise> p2 = ThenPromise(FROM_HERE, p1);
  scoped_refptr<AbstractPromise> p3 = ThenPromise(FROM_HERE, p2);

  OnResolved(p0);
  RunLoop().RunUntilIdle();

  scoped_refptr<AbstractPromise> p4 = ThenPromise(FROM_HERE, p3);
  scoped_refptr<AbstractPromise> p5 = CatchPromise(FROM_HERE, p4);
  scoped_refptr<AbstractPromise> p6 = ThenPromise(FROM_HERE, p3);
  scoped_refptr<AbstractPromise> p7 = CatchPromise(FROM_HERE, p6);

  // We shouldn't get a DCHECK failure because |p6| and |p7| catch the rejection
  // from |p1|.
  RunLoop().RunUntilIdle();
}

TEST_F(AbstractPromiseTest, CatchAfterLongChain) {
  scoped_refptr<AbstractPromise> p0 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p1 =
      ThenPromise(FROM_HERE, p0)
          .With(CallbackResultType::kCanReject)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Rejected<void>());
          }));

  scoped_refptr<AbstractPromise> p2 = ThenPromise(FROM_HERE, p1);
  scoped_refptr<AbstractPromise> p3 = ThenPromise(FROM_HERE, p2);
  scoped_refptr<AbstractPromise> p4 = CatchPromise(FROM_HERE, p3);

  OnResolved(p0);

  RunLoop().RunUntilIdle();
}

TEST_F(AbstractPromiseTest,
       PROMISE_API_DCHECK_TEST(MissingCatchOneSideOfBranchedExecutionChain)) {
  /*
   * Key:  T = Then, C = Catch
   *
   *      P0
   *   T      T
   *   |      |
   *  \|      |/
   *  P1      P2
   *   T      T
   *   |      |
   *  \|      |/
   *  P3      P4
   *   C      T
   *
   * The missing catch for P4 should get noticed.
   */
  scoped_refptr<AbstractPromise> p0 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanReject(true).SetCanResolve(
          false);

  scoped_refptr<AbstractPromise> p1 = ThenPromise(FROM_HERE, p0);
  scoped_refptr<AbstractPromise> p2 = ThenPromise(FROM_HERE, p0);
  scoped_refptr<AbstractPromise> p3 = CatchPromise(FROM_HERE, p1);
  scoped_refptr<AbstractPromise> p4 = ThenPromise(FROM_HERE, p2);

  OnRejected(p0);

  RunLoop().RunUntilIdle();

  // An error should be reported when |p4| is deleted.
  EXPECT_PROMISE_DCHECK_FAIL({ p4 = nullptr; });
}

TEST_F(AbstractPromiseTest,
       PROMISE_API_DCHECK_TEST(CantResolveIfPromiseDeclaredAsNonResolving)) {
  scoped_refptr<AbstractPromise> p = DoNothingPromiseBuilder(FROM_HERE);

  EXPECT_PROMISE_DCHECK_FAIL({ AbstractPromiseTest::OnResolved(p); });
}

TEST_F(AbstractPromiseTest,
       PROMISE_API_DCHECK_TEST(CantRejectIfPromiseDeclaredAsNonRejecting)) {
  scoped_refptr<AbstractPromise> p = DoNothingPromiseBuilder(FROM_HERE);

  EXPECT_PROMISE_DCHECK_FAIL({ AbstractPromiseTest::OnRejected(p); });
}

TEST_F(AbstractPromiseTest,
       PROMISE_API_DCHECK_TEST(DoubleMoveDoNothingPromise)) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p2 =
      ThenPromise(FROM_HERE, p1)
          .WithResolve(ArgumentPassingType::kMove)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Resolved<int>(42));
          }));

  EXPECT_PROMISE_DCHECK_FAIL({
    scoped_refptr<AbstractPromise> p3 =
        ThenPromise(FROM_HERE, p1)
            .WithResolve(ArgumentPassingType::kMove)
            .With(BindOnce([](AbstractPromise* p) {
              p->emplace(Resolved<int>(42));
            }));
  });
}

TEST_F(AbstractPromiseTest, CatchBothSidesOfBranchedExecutionChain) {
  /*
   * Key:  T = Then, C = Catch
   *
   *      P0
   *   T      T
   *   |      |
   *  \|      |/
   *  P1      P2
   *   T      T
   *   |      |
   *  \|      |/
   *  P3      P4
   *   C      C
   *
   * This should execute without DCHECKS.
   */
  scoped_refptr<AbstractPromise> p0 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanReject(true).SetCanResolve(
          false);

  scoped_refptr<AbstractPromise> p1 = ThenPromise(FROM_HERE, p0);
  scoped_refptr<AbstractPromise> p2 = ThenPromise(FROM_HERE, p0);
  scoped_refptr<AbstractPromise> p3 = CatchPromise(FROM_HERE, p1);
  scoped_refptr<AbstractPromise> p4 = CatchPromise(FROM_HERE, p2);

  OnRejected(p0);

  RunLoop().RunUntilIdle();
}

TEST_F(AbstractPromiseTest, ResolvedCurriedPromise) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  std::vector<int> run_order;

  // Promise |p3| will be resolved with.
  scoped_refptr<AbstractPromise> p2 =
      ThenPromise(FROM_HERE, nullptr)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            run_order.push_back(2);
            p->emplace(Resolved<void>());
          }));

  scoped_refptr<AbstractPromise> p3 =
      ThenPromise(FROM_HERE, p1)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            run_order.push_back(3);
            // Resolve with a promise.
            ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, BindOnce(&AbstractPromise::Execute, p2));
            p->emplace(std::move(p2));

            EXPECT_TRUE(p3->IsResolvedWithPromise());
          }));

  scoped_refptr<AbstractPromise> p4 =
      ThenPromise(FROM_HERE, p3)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            run_order.push_back(4);
            p->emplace(Resolved<void>());
          }));

  OnResolved(p1);
  RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(3, 2, 4));
}

TEST_F(AbstractPromiseTest, UnresolvedCurriedPromise) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  std::vector<int> run_order;

  // Promise |p3| will be resolved with.
  scoped_refptr<AbstractPromise> p2 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p3 =
      ThenPromise(FROM_HERE, p1)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            run_order.push_back(3);
            // Resolve with a promise.
            ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, BindOnce(&AbstractPromise::Execute, p2));
            p->emplace(p2);

            EXPECT_TRUE(p3->IsResolvedWithPromise());
          }));

  scoped_refptr<AbstractPromise> p4 =
      ThenPromise(FROM_HERE, p3)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            run_order.push_back(4);
            p->emplace(Resolved<void>());
          }));

  OnResolved(p1);
  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(3));

  OnResolved(p2);
  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(3, 4));
}

TEST_F(AbstractPromiseTest, NeverResolvedCurriedPromise) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  std::vector<int> run_order;

  // Promise |p3| will be resolved with.
  scoped_refptr<AbstractPromise> p2 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p3 =
      ThenPromise(FROM_HERE, p1)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            run_order.push_back(3);
            // Resolve with a promise.
            ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, BindOnce(&AbstractPromise::Execute, p2));
            p->emplace(p2);

            EXPECT_TRUE(p3->IsResolvedWithPromise());
          }));

  scoped_refptr<AbstractPromise> p4 =
      ThenPromise(FROM_HERE, p3)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            run_order.push_back(4);
            p->emplace(Resolved<void>());
          }));

  OnResolved(p1);
  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(3));

  // This shouldn't leak.
}

TEST_F(AbstractPromiseTest, CanceledCurriedPromise) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  // Promise |p3| will be resolved with.
  scoped_refptr<AbstractPromise> p2 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  OnCanceled(p2);

  scoped_refptr<AbstractPromise> p3 =
      ThenPromise(FROM_HERE, p1)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            // Resolve with a promise.
            ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, BindOnce(&AbstractPromise::Execute, p2));
            EXPECT_TRUE(p2->IsCanceled());
            p->emplace(p2);
          }));

  scoped_refptr<AbstractPromise> p4 =
      ThenPromise(FROM_HERE, p3)
          .With(BindLambdaForTesting(
              [&](AbstractPromise* p) { FAIL() << "Should not get here"; }));

  OnResolved(p1);
  RunLoop().RunUntilIdle();

  EXPECT_TRUE(p3->IsCanceled());
  EXPECT_TRUE(p4->IsCanceled());
}

TEST_F(AbstractPromiseTest, CurriedPromiseChain) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);
  std::vector<int> run_order;

  // Promise |p3| will be resolved with.
  scoped_refptr<AbstractPromise> p2 =
      ThenPromise(FROM_HERE, nullptr)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            run_order.push_back(2);
            p->emplace(Resolved<void>());
          }));

  // Promise |p4| will be resolved with.
  scoped_refptr<AbstractPromise> p3 =
      ThenPromise(FROM_HERE, nullptr)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            run_order.push_back(3);
            // Resolve with a promise.
            ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, BindOnce(&AbstractPromise::Execute, p2));
            p->emplace(std::move(p2));
          }));

  // Promise |p5| will be resolved with.
  scoped_refptr<AbstractPromise> p4 =
      ThenPromise(FROM_HERE, nullptr)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            run_order.push_back(4);
            // Resolve with a promise.
            ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, BindOnce(&AbstractPromise::Execute, p3));
            p->emplace(std::move(p3));
          }));

  scoped_refptr<AbstractPromise> p5 =
      ThenPromise(FROM_HERE, p1)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            run_order.push_back(5);
            // Resolve with a promise.
            ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, BindOnce(&AbstractPromise::Execute, p4));
            p->emplace(std::move(p4));
          }));

  scoped_refptr<AbstractPromise> p6 =
      ThenPromise(FROM_HERE, p3)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            run_order.push_back(6);
            p->emplace(Resolved<void>());
          }));

  OnResolved(p1);
  RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(5, 4, 3, 2, 6));
}

TEST_F(AbstractPromiseTest, RejectedCurriedPromiseChain) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanReject(true);
  std::vector<int> run_order;

  // Promise |p3| will be resolved with.
  scoped_refptr<AbstractPromise> p2 =
      CatchPromise(FROM_HERE, nullptr)
          .With(CallbackResultType::kCanReject)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            run_order.push_back(2);
            p->emplace(Rejected<void>());
          }));

  // Promise |p4| will be resolved with.
  scoped_refptr<AbstractPromise> p3 =
      CatchPromise(FROM_HERE, nullptr)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            run_order.push_back(3);
            // Resolve with a promise.
            ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, BindOnce(&AbstractPromise::Execute, p2));
            p->emplace(std::move(p2));
          }));

  // Promise |p5| will be resolved with.
  scoped_refptr<AbstractPromise> p4 =
      CatchPromise(FROM_HERE, nullptr)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            run_order.push_back(4);
            // Resolve with a promise.
            ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, BindOnce(&AbstractPromise::Execute, p3));
            p->emplace(std::move(p3));
          }));

  scoped_refptr<AbstractPromise> p5 =
      CatchPromise(FROM_HERE, p1)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            run_order.push_back(5);
            // Resolve with a promise.
            ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, BindOnce(&AbstractPromise::Execute, p4));
            p->emplace(std::move(p4));
          }));

  scoped_refptr<AbstractPromise> p6 =
      CatchPromise(FROM_HERE, p3)
          .With(RejectPolicy::kCatchNotRequired)
          .With(CallbackResultType::kCanReject)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            run_order.push_back(6);
            p->emplace(Rejected<void>());
          }));

  OnRejected(p1);
  RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(5, 4, 3, 2, 6));
}

TEST_F(AbstractPromiseTest, CurriedPromiseChainType2) {
  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p2 =
      ThenPromise(FROM_HERE, p1)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            p->emplace(p1);
          }));

  scoped_refptr<AbstractPromise> p3 =
      ThenPromise(FROM_HERE, p2)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            p->emplace(p2);
          }));

  scoped_refptr<AbstractPromise> p4 =
      ThenPromise(FROM_HERE, p3)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            p->emplace(p3);
          }));

  OnResolved(p1);
  RunLoop().RunUntilIdle();

  EXPECT_TRUE(p4->IsResolvedForTesting());
  EXPECT_EQ(p1.get(),
            GetCurriedPromise(GetCurriedPromise(GetCurriedPromise(p4.get()))));
}

TEST_F(AbstractPromiseTest, CurriedPromiseMoveArg) {
  scoped_refptr<AbstractPromise> p0 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p1 =
      ThenPromise(FROM_HERE, nullptr)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Resolved<void>());
          }))
          .WithResolve(ArgumentPassingType::kMove);

  scoped_refptr<AbstractPromise> p2 =
      ThenPromise(FROM_HERE, p0)
          .With(BindOnce(
              [](scoped_refptr<AbstractPromise> p1, AbstractPromise* p) {
                ThreadTaskRunnerHandle::Get()->PostTask(
                    FROM_HERE, BindOnce(&AbstractPromise::Execute, p1));
                p->emplace(std::move(p1));
              },
              std::move(p1)))
          .WithResolve(ArgumentPassingType::kMove);

  OnResolved(p0);
  RunLoop().RunUntilIdle();
}

TEST_F(AbstractPromiseTest, CatchCurriedPromise) {
  scoped_refptr<AbstractPromise> p0 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p1 =
      ThenPromise(FROM_HERE, nullptr)
          .With(CallbackResultType::kCanReject)
          .With(BindOnce([](AbstractPromise* p) {
            p->emplace(Rejected<void>());
          }));

  scoped_refptr<AbstractPromise> p2 =
      ThenPromise(FROM_HERE, p0)
          .With(BindOnce(
              [](scoped_refptr<AbstractPromise> p1, AbstractPromise* p) {
                // Resolve with a promise that can and does reject.
                ThreadTaskRunnerHandle::Get()->PostTask(
                    FROM_HERE, BindOnce(&AbstractPromise::Execute, p1));
                p->emplace(std::move(p1));
              },
              std::move(p1)));

  scoped_refptr<AbstractPromise> p3 = CatchPromise(FROM_HERE, p2);

  OnResolved(p0);
  EXPECT_FALSE(p3->IsResolvedForTesting());

  RunLoop().RunUntilIdle();
  EXPECT_TRUE(p3->IsResolvedForTesting());
}

TEST_F(AbstractPromiseTest, ManuallyResolveWithNonSettledCurriedPromise) {
  scoped_refptr<AbstractPromise> p0 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p2 = ThenPromise(FROM_HERE, p1);

  p1->emplace(p0);
  OnResolved(p1);
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(p1->IsResolvedForTesting());
  EXPECT_FALSE(p2->IsResolvedForTesting());

  OnResolved(p0);
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(p2->IsResolvedForTesting());
}

TEST_F(AbstractPromiseTest, ExecuteCalledOnceForLateResolvedCurriedPromise) {
  scoped_refptr<AbstractPromise> p0 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<TestSimpleTaskRunner> task_runner =
      MakeRefCounted<TestSimpleTaskRunner>();
  scoped_refptr<AbstractPromise> p2 =
      ThenPromise(FROM_HERE, p0)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            p->emplace(p1);
          }))
          .With(task_runner);

  scoped_refptr<AbstractPromise> p3 =
      ThenPromise(FROM_HERE, p1).With(task_runner);

  OnResolved(p0);
  // |p2| should run but not |p3|.
  EXPECT_EQ(1u, CountTasksRunUntilIdle(task_runner));
  EXPECT_FALSE(p3->IsResolvedForTesting());

  OnResolved(p1);
  // |p3| should run.
  EXPECT_EQ(1u, CountTasksRunUntilIdle(task_runner));
  EXPECT_TRUE(p3->IsResolvedForTesting());
}

TEST_F(AbstractPromiseTest, ExecuteCalledOnceForLateRejectedCurriedPromise) {
  scoped_refptr<AbstractPromise> p0 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanReject(true).SetRejectPolicy(
          RejectPolicy::kCatchNotRequired);

  scoped_refptr<TestSimpleTaskRunner> task_runner =
      MakeRefCounted<TestSimpleTaskRunner>();
  scoped_refptr<AbstractPromise> p2 =
      ThenPromise(FROM_HERE, p0)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            p->emplace(p1);
          }))
          .With(task_runner);

  scoped_refptr<AbstractPromise> p3 =
      CatchPromise(FROM_HERE, p1).With(task_runner);

  OnResolved(p0);
  // |p2| should run but not |p3|.
  EXPECT_EQ(1u, CountTasksRunUntilIdle(task_runner));
  EXPECT_FALSE(p3->IsResolvedForTesting());

  OnRejected(p1);
  // |p3| should run.
  EXPECT_EQ(1u, CountTasksRunUntilIdle(task_runner));
  EXPECT_TRUE(p3->IsResolvedForTesting());
}

TEST_F(AbstractPromiseTest, ThreadHopping) {
  std::unique_ptr<Thread> thread_a(new Thread("AbstractPromiseTest_Thread_A"));
  std::unique_ptr<Thread> thread_b(new Thread("AbstractPromiseTest_Thread_B"));
  std::unique_ptr<Thread> thread_c(new Thread("AbstractPromiseTest_Thread_C"));
  thread_a->Start();
  thread_b->Start();
  thread_c->Start();
  RunLoop run_loop;

  scoped_refptr<AbstractPromise> p1 =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  scoped_refptr<AbstractPromise> p2 =
      ThenPromise(FROM_HERE, p1)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            p->emplace(Resolved<void>());
            CHECK(thread_a->task_runner()->BelongsToCurrentThread());
          }))
          .With(thread_a->task_runner());

  scoped_refptr<AbstractPromise> p3 =
      ThenPromise(FROM_HERE, p2)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            p->emplace(Resolved<void>());
            CHECK(thread_b->task_runner()->BelongsToCurrentThread());
          }))
          .With(thread_b->task_runner());

  scoped_refptr<AbstractPromise> p4 =
      ThenPromise(FROM_HERE, p3)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            p->emplace(Resolved<void>());
            CHECK(thread_c->task_runner()->BelongsToCurrentThread());
          }))
          .With(thread_c->task_runner());

  scoped_refptr<SingleThreadTaskRunner> main_thread =
      ThreadTaskRunnerHandle::Get();
  scoped_refptr<AbstractPromise> p5 =
      ThenPromise(FROM_HERE, p4)
          .With(BindLambdaForTesting([&](AbstractPromise* p) {
            p->emplace(Resolved<void>());
            run_loop.Quit();
            CHECK(main_thread->BelongsToCurrentThread());
          }))
          .With(main_thread);

  OnResolved(p1);

  EXPECT_FALSE(p5->IsResolvedForTesting());
  run_loop.Run();
  EXPECT_TRUE(p2->IsResolvedForTesting());
  EXPECT_TRUE(p3->IsResolvedForTesting());
  EXPECT_TRUE(p4->IsResolvedForTesting());
  EXPECT_TRUE(p5->IsResolvedForTesting());

  thread_a->Stop();
  thread_b->Stop();
  thread_c->Stop();
}

TEST_F(AbstractPromiseTest, MutipleThreadsAddingDependants) {
  constexpr int num_threads = 4;
  constexpr int num_promises = 100000;

  std::unique_ptr<Thread> thread[num_threads];
  for (int i = 0; i < num_threads; i++) {
    thread[i] = std::make_unique<Thread>("Test thread");
    thread[i]->Start();
  }

  scoped_refptr<AbstractPromise> root =
      DoNothingPromiseBuilder(FROM_HERE).SetCanResolve(true);

  RunLoop run_loop;
  std::atomic<int> pending_count(num_promises);

  // After being called |num_promises| times |decrement_cb| will quit |run_loop|
  auto decrement_cb = BindLambdaForTesting([&](AbstractPromise* p) {
    int count = pending_count.fetch_sub(1, std::memory_order_acq_rel);
    if (count == 1)
      run_loop.Quit();
    OnResolved(p);
  });

  // Post a bunch of tasks on multiple threads that create Then promises
  // dependent on |root| which call |decrement_cb| when resolved.
  for (int i = 0; i < num_promises; i++) {
    thread[i % num_threads]->task_runner()->PostTask(
        FROM_HERE, BindLambdaForTesting([&]() {
          scoped_refptr<AbstractPromise> p =
              ThenPromise(FROM_HERE, root).With(decrement_cb);
        }));

    // Mid way through post a task to resolve |root|.
    if (i == num_promises / 2) {
      thread[i % num_threads]->task_runner()->PostTask(
          FROM_HERE, BindOnce(&AbstractPromiseTest::OnResolved, root));
    }
  }

  // This should exit cleanly without any TSan errors.
  run_loop.Run();

  for (int i = 0; i < num_threads; i++) {
    thread[i]->Stop();
  }
}

TEST_F(AbstractPromiseTest, SingleRejectPrerequisitePolicyALLModified) {
  // Regression test to ensure cross thread rejection works as intended. Loop
  // increaces chances of catching any bugs.
  for (size_t i = 0; i < 1000; ++i) {
    scoped_refptr<AbstractPromise> p1 =
        DoNothingPromiseBuilder(FROM_HERE).SetCanReject(true).SetCanResolve(
            false);
    scoped_refptr<AbstractPromise> p2 =
        DoNothingPromiseBuilder(FROM_HERE).SetCanReject(true).SetCanResolve(
            false);
    scoped_refptr<AbstractPromise> p3 =
        DoNothingPromiseBuilder(FROM_HERE).SetCanReject(true).SetCanResolve(
            false);
    scoped_refptr<AbstractPromise> p4 =
        DoNothingPromiseBuilder(FROM_HERE).SetCanReject(true).SetCanResolve(
            false);

    std::vector<internal::DependentList::Node> prerequisite_list(4);
    prerequisite_list[0].SetPrerequisite(p1.get());
    prerequisite_list[1].SetPrerequisite(p2.get());
    prerequisite_list[2].SetPrerequisite(p3.get());
    prerequisite_list[3].SetPrerequisite(p4.get());

    scoped_refptr<AbstractPromise> all_promise =
        AllPromise(FROM_HERE, std::move(prerequisite_list))
            .With(CallbackResultType::kCanResolveOrReject)
            .With(BindLambdaForTesting([&](AbstractPromise* p) {
              p->emplace(Rejected<void>());
            }));

    base::PostTask(FROM_HERE, {base::ThreadPool()},
                   base::BindOnce(
                       [](scoped_refptr<AbstractPromise> p2) {
                         p2->emplace(Rejected<void>());
                       },
                       p2));

    RunLoop run_loop;
    scoped_refptr<AbstractPromise> p5 =
        CatchPromise(FROM_HERE, all_promise)
            .With(BindLambdaForTesting([&](AbstractPromise* p) {
              p->emplace(Resolved<void>());
              run_loop.Quit();
            }));

    OnRejected(p3);
    run_loop.Run();
    EXPECT_TRUE(all_promise->IsRejected());
    EXPECT_TRUE(p5->IsResolved());
    task_environment_.RunUntilIdle();
  }
}

}  // namespace internal
}  // namespace base
