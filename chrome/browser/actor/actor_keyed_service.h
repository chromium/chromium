// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_H_
#define CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/actor/actor_task.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace actor {

// This class owns all ActorTasks for a given profile. ActorTasks are kept in
// memory until the process is destroyed.
class ActorKeyedService : public KeyedService {
 public:
  explicit ActorKeyedService(Profile* profile);
  ActorKeyedService(const ActorKeyedService&) = delete;
  ActorKeyedService& operator=(const ActorKeyedService&) = delete;
  ~ActorKeyedService() override;

  // Convenience method, may return nullptr.
  static ActorKeyedService* Get(content::BrowserContext* context);

  // Starts tracking a task.
  void AddTask(std::unique_ptr<ActorTask> task);

  // In the future we may want to return a more limited or const-version of
  // ActorTasks. The purpose of this method is to get information about tasks,
  // not to modify them.
  const std::vector<std::unique_ptr<ActorTask>>& GetTasks();

 private:
  // In the future we may want to divide this between active and inactive tasks.
  std::vector<std::unique_ptr<ActorTask>> tasks_;

  // Owns this.
  raw_ptr<Profile> profile_;
};

}  // namespace actor
#endif  // CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_H_
