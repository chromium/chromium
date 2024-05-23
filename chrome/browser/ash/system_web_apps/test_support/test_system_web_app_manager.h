// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TEST_SUPPORT_TEST_SYSTEM_WEB_APP_MANAGER_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TEST_SUPPORT_TEST_SYSTEM_WEB_APP_MANAGER_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/version.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}

namespace ash {

// This class is used in unit tests only, browser tests use real
// `SystemWebAppManager`.
class TestSystemWebAppManager : public SystemWebAppManager {
 public:
  // Used by the TestingProfile in unit tests.
  // Builds a stub `TestSystemWebAppManager` that needs to be manually started
  // by calling `ScheduleStart()`. Use `TestSystemWebAppManager::Get()` to use
  // testing methods.
  static std::unique_ptr<KeyedService> BuildDefault(
      content::BrowserContext* context);

  // Gets a TestSystemWebAppManager. Clients must call `ScheduleStart()` to make
  // `on_apps_synchronized()` event ready.
  static TestSystemWebAppManager* Get(Profile* profile);

  explicit TestSystemWebAppManager(Profile* profile);
  TestSystemWebAppManager(const TestSystemWebAppManager&) = delete;
  TestSystemWebAppManager& operator=(const TestSystemWebAppManager&) = delete;
  ~TestSystemWebAppManager() override;

  void SetUpdatePolicy(SystemWebAppManager::UpdatePolicy policy);

  void set_current_version(const base::Version& version) {
    current_version_ = version;
  }

  void set_current_locale(const std::string& locale) {
    current_locale_ = locale;
  }

  void set_icons_are_broken(bool broken) { icons_are_broken_ = broken; }

  // SystemWebAppManager:
  const base::Version& CurrentVersion() const override;
  const std::string& CurrentLocale() const override;
  bool PreviousSessionHadBrokenIcons() const override;

 private:
  base::Version current_version_{"0.0.0.0"};
  std::string current_locale_;
  bool icons_are_broken_ = false;
};

// Used in tests to ensure that the SystemWebAppManager that is created on
// profile startup is the TestSystemWebAppManager. Hooks into the
// BrowserContextKeyedService initialization pipeline.
class TestSystemWebAppManagerCreator {
 public:
  using CreateSystemWebAppManagerCallback =
      base::RepeatingCallback<std::unique_ptr<KeyedService>(Profile* profile)>;

  explicit TestSystemWebAppManagerCreator(
      CreateSystemWebAppManagerCallback callback);
  ~TestSystemWebAppManagerCreator();

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context);
  std::unique_ptr<KeyedService> CreateSystemWebAppManager(
      content::BrowserContext* context);

  CreateSystemWebAppManagerCallback callback_;

  base::CallbackListSubscription create_services_subscription_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TEST_SUPPORT_TEST_SYSTEM_WEB_APP_MANAGER_H_
