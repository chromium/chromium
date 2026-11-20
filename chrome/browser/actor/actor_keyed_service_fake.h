// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_FAKE_H_
#define CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_FAKE_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task_options.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/common/glic_enums.mojom.h"

class Profile;

namespace actor {

class ActorKeyedServiceFake : public ActorKeyedService {
 public:
  explicit ActorKeyedServiceFake(Profile* profile);
  ~ActorKeyedServiceFake() override;

  // Creates a task created by `source_info`. Uses default-duration options
  // if `options` is not set.
  TaskId CreateTaskForTesting(
      const TaskSourceInfo& source_info = TestTaskSourceInfo(),
      std::optional<ActorTaskOptions> options = std::nullopt);
  TaskId CreateTransientTaskForTesting();
  TaskId CreateExperimentalTriggeringTaskForTesting();
  // Creates a default-duration task created by `source_info`, e.g. `kGlic` with
  // a conversation id.
  TaskId CreateTaskWithSourceInfoForTesting(const TaskSourceInfo& source_info);
  // Same as CreateExperimentalTriggeringTaskForTesting(), but with the given
  // task `title` (which may be empty).
  TaskId CreateExperimentalTriggeringTaskWithTitleForTesting(
      const std::string& title);
  void PauseTaskForTesting(TaskId task_id, bool from_actor);
  void StopTaskForTesting(TaskId task_id,
                          actor::ActorTask::StoppedReason stopped_reason);

 private:
  TaskId CreateTaskWithDurationAndFeatureModeForTesting(
      actor::webui::mojom::TaskDuration duration,
      glic::mojom::FeatureMode feature_mode,
      const std::string& title = "Test Task",
      const TaskSourceInfo& source_info = TestTaskSourceInfo());

  TaskId CreateTaskWithDurationForTesting(
      actor::webui::mojom::TaskDuration duration);

  MockPolicyChecker no_enterprise_policy_checker_{
      EnterprisePolicyChecker::UrlBlockReason::kNotBlocked};

  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<ActorKeyedServiceFake> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_FAKE_H_
