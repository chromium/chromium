// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_environment.h"

#include <utility>

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#endif

namespace extensions {

using content::BrowserThread;

namespace {

base::Value::Dict MakeExtensionManifest(
    const base::Value::Dict& manifest_extra) {
  base::Value::Dict manifest = base::Value::Dict()
                                   .Set("name", "Extension")
                                   .Set("version", "1.0")
                                   .Set("manifest_version", 2);
  manifest.Merge(manifest_extra.Clone());
  return manifest;
}

base::Value::Dict MakePackagedAppManifest() {
  return base::Value::Dict()
      .Set("name", "Test App Name")
      .Set("version", "2.0")
      .Set("manifest_version", 2)
      .Set("app",
           base::Value::Dict().Set(
               "background",
               base::Value::Dict().Set(
                   "scripts", base::Value::List().Append("background.js"))));
}

}  // namespace

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Extra environment state required for ChromeOS.
class TestExtensionEnvironment::ChromeOSEnv {
 public:
  ChromeOSEnv() = default;
  ChromeOSEnv(const ChromeOSEnv&) = delete;
  ChromeOSEnv& operator=(const ChromeOSEnv&) = delete;

 private:
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  user_manager::ScopedUserManager user_manager_{
      std::make_unique<user_manager::UserManagerImpl>(
          std::make_unique<ash::UserManagerDelegateImpl>(),
          g_browser_process->local_state(),
          ash::CrosSettings::Get())};
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// static
ExtensionService* TestExtensionEnvironment::CreateExtensionServiceForProfile(
    TestingProfile* profile) {
  TestExtensionSystem* extension_system =
      static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile));
  return extension_system->CreateExtensionService(
      base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
}

TestExtensionEnvironment::TestExtensionEnvironment(
    Type type,
    ProfileCreationType profile_creation_mode
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ,
    OSSetupType os_setup_mode
#endif
    )
    : task_environment_(
          type == Type::kWithTaskEnvironment
              ? std::make_unique<content::BrowserTaskEnvironment>()
              : nullptr),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      chromeos_env_(ash::DeviceSettingsService::IsInitialized() &&
                            os_setup_mode != OSSetupType::kSetUp
                        ? nullptr
                        : std::make_unique<ChromeOSEnv>()),
#endif
      profile_(profile_creation_mode != ProfileCreationType::kCreate
                   ? nullptr
                   : std::make_unique<TestingProfile>()),
      profile_ptr_(profile_.get()) {
}

TestExtensionEnvironment::~TestExtensionEnvironment() = default;

void TestExtensionEnvironment::SetProfile(TestingProfile* profile) {
  profile_ptr_ = profile;
}

TestingProfile* TestExtensionEnvironment::profile() const {
  return profile_ptr_.get();
}

TestExtensionSystem* TestExtensionEnvironment::GetExtensionSystem() {
  return static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));
}

ExtensionService* TestExtensionEnvironment::GetExtensionService() {
  if (!extension_service_) {
    extension_service_ = CreateExtensionServiceForProfile(profile());
  }
  return extension_service_;
}

ExtensionPrefs* TestExtensionEnvironment::GetExtensionPrefs() {
  return ExtensionPrefs::Get(profile());
}

const Extension* TestExtensionEnvironment::MakeExtension(
    const base::Value::Dict& manifest_extra) {
  base::Value::Dict manifest = MakeExtensionManifest(manifest_extra);
  scoped_refptr<const Extension> result =
      ExtensionBuilder().SetManifest(std::move(manifest)).Build();
  GetExtensionService()->AddExtension(result.get());
  return result.get();
}

const Extension* TestExtensionEnvironment::MakeExtension(
    const base::Value::Dict& manifest_extra,
    const std::string& id) {
  base::Value::Dict manifest = MakeExtensionManifest(manifest_extra);
  scoped_refptr<const Extension> result =
      ExtensionBuilder().SetManifest(std::move(manifest)).SetID(id).Build();
  GetExtensionService()->AddExtension(result.get());
  return result.get();
}

scoped_refptr<const Extension> TestExtensionEnvironment::MakePackagedApp(
    const std::string& id,
    bool install) {
  scoped_refptr<const Extension> result =
      ExtensionBuilder()
          .SetManifest(MakePackagedAppManifest())
          .AddFlags(Extension::FROM_WEBSTORE)
          .SetID(id)
          .Build();
  if (install) {
    GetExtensionService()->AddExtension(result.get());
  }
  return result;
}

std::unique_ptr<content::WebContents> TestExtensionEnvironment::MakeTab()
    const {
  std::unique_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  // Create a tab id.
  CreateSessionServiceTabHelper(contents.get());
  return contents;
}

void TestExtensionEnvironment::DeleteProfile() {
  profile_ptr_ = nullptr;
  profile_.reset();
  extension_service_ = nullptr;
}

}  // namespace extensions
