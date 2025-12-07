// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/locked_quiz_session_manager_factory.h"

#include "chrome/browser/ash/boca/on_task/locked_quiz_session_manager.h"
#include "chrome/browser/ash/boca/on_task/locked_session_window_tracker_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace ash::boca {

// static
LockedQuizSessionManagerFactory*
LockedQuizSessionManagerFactory::GetInstance() {
  static base::NoDestructor<LockedQuizSessionManagerFactory> instance;
  return instance.get();
}

// static
LockedQuizSessionManager* LockedQuizSessionManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<LockedQuizSessionManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

LockedQuizSessionManagerFactory::LockedQuizSessionManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "LockedQuizSessionManager",
          BrowserContextDependencyManager::GetInstance()) {
  // Add an explicit dependency on `LockedSessionWindowTrackerFactory` to ensure
  // the window tracker is destroyed after locked_quiz_session_manager is
  // destroyed.
  DependsOn(LockedSessionWindowTrackerFactory::GetInstance());
}

LockedQuizSessionManagerFactory::~LockedQuizSessionManagerFactory() = default;

std::unique_ptr<KeyedService>
LockedQuizSessionManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<LockedQuizSessionManager>(context);
}

content::BrowserContext*
LockedQuizSessionManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace ash::boca
