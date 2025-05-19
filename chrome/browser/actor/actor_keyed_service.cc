// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_keyed_service.h"

#include <utility>

#include "chrome/browser/actor/actor_keyed_service_factory.h"

namespace actor {

ActorKeyedService::ActorKeyedService(Profile* profile) : profile_(profile) {}

ActorKeyedService::~ActorKeyedService() = default;

// static
ActorKeyedService* ActorKeyedService::Get(content::BrowserContext* context) {
  return ActorKeyedServiceFactory::GetActorKeyedService(context);
}

void ActorKeyedService::AddTask(std::unique_ptr<ActorTask> task) {
  tasks_.push_back(std::move(task));
}

const std::vector<std::unique_ptr<ActorTask>>& ActorKeyedService::GetTasks() {
  return tasks_;
}

}  // namespace actor
