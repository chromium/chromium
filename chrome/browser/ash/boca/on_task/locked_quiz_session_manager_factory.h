// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_ON_TASK_LOCKED_QUIZ_SESSION_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_BOCA_ON_TASK_LOCKED_QUIZ_SESSION_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/ash/boca/on_task/locked_quiz_session_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace ash::boca {

// Singleton factory that builds and owns LockedQuizSessionManager.
class LockedQuizSessionManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  LockedQuizSessionManagerFactory(const LockedQuizSessionManagerFactory&) =
      delete;
  LockedQuizSessionManagerFactory& operator=(
      const LockedQuizSessionManagerFactory&) = delete;

  static LockedQuizSessionManagerFactory* GetInstance();
  static LockedQuizSessionManager* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend base::NoDestructor<LockedQuizSessionManagerFactory>;

  LockedQuizSessionManagerFactory();
  ~LockedQuizSessionManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace ash::boca

#endif  // CHROME_BROWSER_ASH_BOCA_ON_TASK_LOCKED_QUIZ_SESSION_MANAGER_FACTORY_H_
