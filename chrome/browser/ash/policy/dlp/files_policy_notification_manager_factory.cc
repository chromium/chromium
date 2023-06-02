// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace policy {

// static
FilesPolicyNotificationManagerFactory*
FilesPolicyNotificationManagerFactory::GetInstance() {
  static base::NoDestructor<FilesPolicyNotificationManagerFactory> instance;
  return instance.get();
}

// static
FilesPolicyNotificationManager*
FilesPolicyNotificationManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<FilesPolicyNotificationManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

FilesPolicyNotificationManagerFactory::FilesPolicyNotificationManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "FilesPolicyNotificationManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(DlpRulesManagerFactory::GetInstance());
  DependsOn(enterprise_connectors::ConnectorsServiceFactory::GetInstance());
}

FilesPolicyNotificationManagerFactory::
    ~FilesPolicyNotificationManagerFactory() = default;

std::unique_ptr<KeyedService>
FilesPolicyNotificationManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto files_notification_manager =
      std::make_unique<FilesPolicyNotificationManager>(
          Profile::FromBrowserContext(context));
  return files_notification_manager;
}

}  // namespace policy
