// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_ACTOR_NEW_GLIC_ACTOR_FUNCTIONAL_BROWSERTEST_H_
#define CHROME_BROWSER_GLIC_ACTOR_NEW_GLIC_ACTOR_FUNCTIONAL_BROWSERTEST_H_

#include "base/base64.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/test_support/new_glic_api_test.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/public/mojom/actor_types.mojom-forward.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"

namespace glic::actor {

using ::actor::ActorTask;
using ::actor::TaskId;
using ::base::test::TestFuture;

// Matches a base::expected<T, std::string> which has an error string
// that contains `expected_substring`.
MATCHER_P(ErrorHasSubstr, expected_substring, "") {
  return testing::Matches(
      base::test::ErrorIs(testing::HasSubstr(expected_substring)))(arg);
}

template <typename T>
class GlicActorFunctionalBrowserTestMixin : public T {
 public:
  static constexpr base::TimeDelta kShortWaitTime = base::Milliseconds(10);
  static constexpr base::TimeDelta kLongWaitTime = base::Minutes(2);

  template <typename... Args>
  explicit GlicActorFunctionalBrowserTestMixin(Args&&... args)
      : T(std::forward<Args>(args)...) {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{::actor::kActorBindCreatedTabToTask, {}},
                              {features::kGlicActor,
                               {{features::kGlicActorPolicyControlExemption
                                     .name,
                                 "true"}}}},
        /*disabled_features=*/{});
  }
  ~GlicActorFunctionalBrowserTestMixin() override = default;

  void SetUpOnMainThread() override {
    T::SetUpOnMainThread();
    ASSERT_OK(T::OpenGlicForActiveTab());
  }

  content::WebContents* web_contents() {
    return T::GetTabListInterface()->GetActiveTab()->GetContents();
  }

  tabs::TabInterface* active_tab() {
    return T::GetTabListInterface()->GetActiveTab();
  }

  ::actor::ActorKeyedService* actor_keyed_service() {
    return ::actor::ActorKeyedService::Get(T::GetProfile());
  }

  base::CallbackListSubscription CreateTaskCompletionSubscription(
      TaskId for_task_id,
      TestFuture<ActorTask::State>& future) {
    return actor_keyed_service()->AddTaskStateChangedCallback(
        base::BindLambdaForTesting([&future, for_task_id](ActorTask& task) {
          if (task.id() == for_task_id &&
              ActorTask::IsCompletedState(task.GetState())) {
            future.SetValue(task.GetState());
          }
        }));
  }

  ActorTask::State GetActorTaskState(TaskId task_id) {
    ActorTask* task = actor_keyed_service()->GetTask(task_id);
    CHECK_NE(task, nullptr) << "ActorTask " << task_id << " not found.";
    return task->GetState();
  }

  TaskId ExtractTaskIdFromStepData() {
    CHECK(T::step_data().has_value());
    CHECK(T::step_data()->is_dict());
    std::optional<int> task_id_opt =
        T::step_data()->GetDict().FindInt("taskId");
    CHECK(task_id_opt.has_value());
    return TaskId(*task_id_opt);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

using GlicActorFunctionalBrowserTestBase =
    GlicActorFunctionalBrowserTestMixin<GlicApiBrowserTest>;

}  // namespace glic::actor

#endif  // CHROME_BROWSER_GLIC_ACTOR_NEW_GLIC_ACTOR_FUNCTIONAL_BROWSERTEST_H_
