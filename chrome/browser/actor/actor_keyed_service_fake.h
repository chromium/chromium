// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_FAKE_H_
#define CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_FAKE_H_

#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/enterprise_policy_url_checker.h"

class Profile;

namespace actor {

class ActorKeyedServiceFake : public ActorKeyedService {
 public:
  explicit ActorKeyedServiceFake(Profile* profile);
  ~ActorKeyedServiceFake() override;

  TaskId CreateTaskForTesting();
  void PauseTaskForTesting(TaskId task_id, bool from_actor);
  void StopTaskForTesting(TaskId task_id,
                          actor::ActorTask::StoppedReason stopped_reason);

 private:
  MockPolicyChecker no_enterprise_policy_checker_{
      EnterprisePolicyBlockReason::kNotBlocked};

  base::WeakPtrFactory<ActorKeyedServiceFake> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_FAKE_H_
