// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_test_helper.h"

#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "third_party/skia/include/core/SkColor.h"

using vm_tools::apps::App;
using vm_tools::apps::ApplicationList;

namespace crostini {

constexpr SkColor kTestContainerBadgeColor = SK_ColorBLUE;

CrostiniTestHelper::CrostiniTestHelper(TestingProfile* profile,
                                       bool enable_crostini)
    : fake_user_manager_(std::make_unique<ash::FakeChromeUserManager>()),
      profile_(profile) {
  scoped_feature_list_.InitAndEnableFeature(features::kCrostini);

  ash::ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(true);
  auto account = AccountId::FromUserEmailGaiaId("test@example.com", "12345");
  fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
      account, false, user_manager::UserType::kRegular, profile);
  fake_user_manager_->LoginUser(account);

  if (enable_crostini) {
    EnableCrostini(profile);
  }

  current_apps_.set_vm_name(kCrostiniDefaultVmName);
  current_apps_.set_container_name(kCrostiniDefaultContainerName);

  guest_os::AddContainerToPrefs(profile_, DefaultContainerId(), {});
  SetContainerBadgeColor(profile_, DefaultContainerId(),
                         kTestContainerBadgeColor);
}

CrostiniTestHelper::~CrostiniTestHelper() {
  ash::ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(false);
  DisableCrostini(profile_);
}

void CrostiniTestHelper::SetupDummyApps() {
  // This updates the registry for us.
  AddApp(BasicApp("dummy1"));
  AddApp(BasicApp("dummy2"));
}

App CrostiniTestHelper::GetApp(int i) {
  return current_apps_.apps(i);
}

void CrostiniTestHelper::AddApp(const vm_tools::apps::App& app) {
  for (int i = 0; i < current_apps_.apps_size(); ++i) {
    if (current_apps_.apps(i).desktop_file_id() == app.desktop_file_id()) {
      *current_apps_.mutable_apps(i) = app;
      UpdateRegistry();
      return;
    }
  }
  *current_apps_.add_apps() = app;
  UpdateRegistry();
}

void CrostiniTestHelper::RemoveApp(int i) {
  auto* apps = current_apps_.mutable_apps();
  apps->erase(apps->begin() + i);
  UpdateRegistry();
}

void CrostiniTestHelper::ReInitializeAppServiceIntegration() {
  // Some Crostini-related tests add apps to the registry, which queues
  // (asynchronous) icon loading requests, which depends on the Cicerone D-Bus
  // client. These requests are merely queued, not executed, so without further
  // action, D-Bus can be ignored.
  //
  // It is simpler if those RunUntilIdle calls are unconditional, so we require
  // Cicerone to be initialized by this point, whether or not the App Service
  // is enabled. Note that we can't initialize it ourselves here because once it
  // has been initialized it must be shutdown, but it can't be shutdown until
  // after the profile (and all keyed services) have been destroyed, which we
  // can't manage because we don't own the profile.
  CHECK(ash::CiceroneClient::Get())
      << "CiceroneClient must be initialized before calling "
         "ReInitializeAppServiceIntegration";

  // The App Service is originally initialized when the Profile is created,
  // but this class' constructor takes the Profile* as an argument, which
  // means that the fake user (created during that constructor) is
  // necessarily configured after the App Service's initialization.
  //
  // Without further action, in tests (but not in production which looks at
  // real users, not fakes), the App Service serves no Crostini apps, as at
  // the time it looked, the profile/user doesn't have Crostini enabled.
  //
  // We therefore manually have the App Service re-examine whether Crostini
  // is enabled for this profile.
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  proxy->ReInitializeCrostiniForTesting();
}

void CrostiniTestHelper::UpdateAppKeywords(
    vm_tools::apps::App& app,
    const std::map<std::string, std::set<std::string>>& keywords) {
  for (const auto& keywords_entry : keywords) {
    auto* strings_with_locale = app.mutable_keywords()->add_values();
    strings_with_locale->set_locale(keywords_entry.first);
    for (const auto& curr_keyword : keywords_entry.second) {
      strings_with_locale->add_value(curr_keyword);
    }
  }
}

// static
void CrostiniTestHelper::EnableCrostini(TestingProfile* profile) {
  profile->GetPrefs()->SetBoolean(crostini::prefs::kCrostiniEnabled, true);
}
// static
void CrostiniTestHelper::DisableCrostini(TestingProfile* profile) {
  profile->GetPrefs()->SetBoolean(crostini::prefs::kCrostiniEnabled, false);
}

// static
std::string CrostiniTestHelper::GenerateAppId(
    const std::string& desktop_file_id,
    const std::string& vm_name,
    const std::string& container_name) {
  return guest_os::GuestOsRegistryService::GenerateAppId(
      desktop_file_id, vm_name, container_name);
}

// static
App CrostiniTestHelper::BasicApp(const std::string& desktop_file_id,
                                 const std::string& name,
                                 bool no_display) {
  App app;
  app.set_desktop_file_id(desktop_file_id);
  App::LocaleString::Entry* entry = app.mutable_name()->add_values();
  entry->set_locale(std::string());
  entry->set_value(name.empty() ? desktop_file_id : name);
  app.set_no_display(no_display);
  return app;
}

// static
ApplicationList CrostiniTestHelper::BasicAppList(
    const std::string& desktop_file_id,
    const std::string& vm_name,
    const std::string& container_name) {
  ApplicationList app_list;
  app_list.set_vm_name(vm_name);
  app_list.set_container_name(container_name);
  *app_list.add_apps() = BasicApp(desktop_file_id);
  return app_list;
}

void CrostiniTestHelper::UpdateRegistry() {
  guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_)
      ->UpdateApplicationList(current_apps_);
}

}  // namespace crostini
