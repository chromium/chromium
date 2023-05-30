// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_FILES_POLICY_NOTIFICATION_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_FILES_POLICY_NOTIFICATION_MANAGER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace policy {
class FilesPolicyNotificationManager;

// Singleton that owns all FilesPolicyNotificationManager instances and
// associates them with Profiles.
class FilesPolicyNotificationManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  FilesPolicyNotificationManagerFactory(
      const FilesPolicyNotificationManagerFactory&) = delete;
  FilesPolicyNotificationManagerFactory& operator=(
      const FilesPolicyNotificationManagerFactory&) = delete;

  static FilesPolicyNotificationManagerFactory* GetInstance();
  static FilesPolicyNotificationManager* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend base::NoDestructor<FilesPolicyNotificationManagerFactory>;

  FilesPolicyNotificationManagerFactory();
  ~FilesPolicyNotificationManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_FILES_POLICY_NOTIFICATION_MANAGER_FACTORY_H_
