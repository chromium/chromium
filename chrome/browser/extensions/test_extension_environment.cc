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
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/scoped_test_user_manager.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#endif

namespace extensions {

using content::BrowserThread;

namespace {

base::Value::Dict MakeExtensionManifest(
    const base::Value::Dict& manifest_extra) {
  base::Value::Dict manifest = DictionaryBuilder()
                                   .Set("name", "Extension")
                                   .Set("version", "1.0")
                                   .Set("manifest_version", 2)
                                   .Build();
  manifest.Merge(manifest_extra.Clone());
  return manifest;
}

base::Value::Dict MakePackagedAppManifest() {
  return extensions::DictionaryBuilder()
      .Set("name", "Test App Name")
      .Set("version", "2.0")
      .Set("manifest_version", 2)
      .Set("app", extensions::DictionaryBuilder()
                      .Set("background",
                           extensions::DictionaryBuilder()
                               .Set("scripts", extensions::ListBuilder()
                                                   .Append("background.js")
                                                   .Build())
                               .Build())
                      .Build())
      .Build();
}

}  // namespace

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Extra environment state required for ChromeOS.
class TestExtensionEnvironment::ChromeOSEnv {
 public:
  ChromeOSEnv() {}

  ChromeOSEnv(const ChromeOSEnv&) = delete;
  ChromeOSEnv& operator=(const ChromeOSEnv&) = delete;

 private:
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  ash::ScopedTestUserManager test_user_manager_;
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

TestExtensionEnvironment::TestExtensionEnvironment(Type type)
    : task_environment_(
          type == Type::kWithTaskEnvironment
              ? std::make_unique<content::BrowserTaskEnvironment>()
              : nullptr),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      chromeos_env_(ash::DeviceSettingsService::IsInitialized()
                        ? nullptr
                        : std::make_unique<ChromeOSEnv>()),
#endif
      profile_(std::make_unique<TestingProfile>()) {
}

TestExtensionEnvironment::~TestExtensionEnvironment() {
}

TestingProfile* TestExtensionEnvironment::profile() const {
  return profile_.get();
}

TestExtensionSystem* TestExtensionEnvironment::GetExtensionSystem() {
  return static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));
}

ExtensionService* TestExtensionEnvironment::GetExtensionService() {
  if (!extension_service_)
    extension_service_ = CreateExtensionServiceForProfile(profile());
  return extension_service_;
}

ExtensionPrefs* TestExtensionEnvironment::GetExtensionPrefs() {
  return ExtensionPrefs::Get(profile_.get());
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
  if (install)
    GetExtensionService()->AddExtension(result.get());
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
  profile_.reset();
  extension_service_ = nullptr;
}

}  // namespace extensions
