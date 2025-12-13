// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_FAKE_H_
#define CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_FAKE_H_

#include "chrome/browser/actor/actor_keyed_service.h"

class Profile;

namespace actor {

class ActorKeyedServiceFake : public ActorKeyedService {
 public:
  explicit ActorKeyedServiceFake(Profile* profile);
  ~ActorKeyedServiceFake() override;

  TaskId CreateTaskForTesting();

 private:
  base::WeakPtrFactory<ActorKeyedServiceFake> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_FAKE_H_
