// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_environment.h"

#include <utility>

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#endif

namespace extensions {

using content::BrowserThread;

namespace {

std::unique_ptr<base::DictionaryValue> MakeExtensionManifest(
    const base::Value& manifest_extra) {
  std::unique_ptr<base::DictionaryValue> manifest =
      DictionaryBuilder()
          .Set("name", "Extension")
          .Set("version", "1.0")
          .Set("manifest_version", 2)
          .Build();
  const base::DictionaryValue* manifest_extra_dict;
  if (manifest_extra.GetAsDictionary(&manifest_extra_dict)) {
    manifest->MergeDictionary(manifest_extra_dict);
  } else {
    std::string manifest_json;
    base::JSONWriter::Write(manifest_extra, &manifest_json);
    ADD_FAILURE() << "Expected dictionary; got \"" << manifest_json << "\"";
  }
  return manifest;
}

std::unique_ptr<base::DictionaryValue> MakePackagedAppManifest() {
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

#if defined(OS_CHROMEOS)
// Extra environment state required for ChromeOS.
class TestExtensionEnvironment::ChromeOSEnv {
 public:
  ChromeOSEnv() {}

 private:
  chromeos::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  chromeos::ScopedTestUserManager test_user_manager_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOSEnv);
};
#endif  // defined(OS_CHROMEOS)

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
#if defined(OS_CHROMEOS)
      chromeos_env_(chromeos::DeviceSettingsService::IsInitialized()
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
    const base::Value& manifest_extra) {
  std::unique_ptr<base::DictionaryValue> manifest =
      MakeExtensionManifest(manifest_extra);
  scoped_refptr<const Extension> result =
      ExtensionBuilder().SetManifest(std::move(manifest)).Build();
  GetExtensionService()->AddExtension(result.get());
  return result.get();
}

const Extension* TestExtensionEnvironment::MakeExtension(
    const base::Value& manifest_extra,
    const std::string& id) {
  std::unique_ptr<base::DictionaryValue> manifest =
      MakeExtensionManifest(manifest_extra);
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
  SessionTabHelper::CreateForWebContents(contents.get());
  return contents;
}

void TestExtensionEnvironment::DeleteProfile() {
  profile_.reset();
  extension_service_ = nullptr;
}

}  // namespace extensions
