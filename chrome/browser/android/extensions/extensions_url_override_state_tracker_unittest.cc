// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/extensions/extensions_url_override_state_tracker_impl.h"
#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

constexpr char kNewTabUrlPath[] = "newtab";
constexpr char kHistoryUrlPath[] = "history";

std::unique_ptr<KeyedService> BuildOverrideRegistrar(
    content::BrowserContext* context) {
  return std::make_unique<ExtensionWebUIOverrideRegistrar>(context);
}

class MockStateListener
    : public ExtensionUrlOverrideStateTracker::StateListener {
 public:
  MOCK_METHOD(void,
              OnUrlOverrideEnabled,
              (const std::string&, bool),
              (override));
  MOCK_METHOD(void, OnUrlOverrideDisabled, (const std::string&), (override));
};

// Test suite to test ExtensionUrlOverrideStateTrackerImpl.
class ExtensionUrlOverrideStateTrackerImplTest : public testing::Test {
 public:
  ExtensionUrlOverrideStateTrackerImplTest() = default;
  ~ExtensionUrlOverrideStateTrackerImplTest() override = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();

    extension_system_ =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile_.get()));
    extension_system_->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);

    ExtensionWebUIOverrideRegistrar::GetFactoryInstance()->SetTestingFactory(
        profile_.get(), base::BindRepeating(&BuildOverrideRegistrar));
    ExtensionWebUIOverrideRegistrar::GetFactoryInstance()->Get(profile_.get());
    tracker_ = std::make_unique<ExtensionUrlOverrideStateTrackerImpl>(
        profile_.get(), &listener_);
  }

  void TearDown() override {
    tracker_.reset();
    profile_.reset();
  }

 protected:
  scoped_refptr<const Extension> CreateExtension(
      const std::string& name,
      const std::string& page_to_override,
      bool incognito_split_mode) {
    base::Value::Dict manifest;
    manifest.Set("name", name);
    manifest.Set("version", "1.0");
    manifest.Set("manifest_version", 2);
    if (incognito_split_mode) {
      manifest.Set("incognito", "split");
    }
    base::Value::Dict chrome_url_overrides;
    chrome_url_overrides.Set(page_to_override, "override.html");
    manifest.Set("chrome_url_overrides", std::move(chrome_url_overrides));
    return ExtensionBuilder()
        .SetManifest(std::move(manifest))
        .SetID(crx_file::id_util::GenerateId(name))
        .Build();
  }

  void SetIncognitoEnabled(const Extension* extension, bool enabled) {
    ExtensionPrefs::Get(profile_.get())
        ->SetIsIncognitoEnabled(extension->id(), enabled);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<TestExtensionSystem> extension_system_ = nullptr;
  testing::StrictMock<MockStateListener> listener_;
  std::unique_ptr<ExtensionUrlOverrideStateTrackerImpl> tracker_;
};

TEST_F(ExtensionUrlOverrideStateTrackerImplTest, TriggersOnUrlOverrideEnabled) {
  ExtensionRegistrar* registrar = ExtensionRegistrar::Get(profile_.get());
  scoped_refptr<const Extension> extension1 =
      CreateExtension("ext1", kNewTabUrlPath, false);
  scoped_refptr<const Extension> extension2 =
      CreateExtension("ext2", kNewTabUrlPath, false);

  EXPECT_CALL(listener_, OnUrlOverrideEnabled(kNewTabUrlPath, false)).Times(1);
  registrar->AddExtension(extension1.get());
  base::RunLoop().QuitWhenIdle();
  testing::Mock::VerifyAndClearExpectations(&listener_);

  EXPECT_CALL(listener_, OnUrlOverrideEnabled(kNewTabUrlPath, false)).Times(0);
  registrar->AddExtension(extension2.get());
  base::RunLoop().QuitWhenIdle();
}

TEST_F(ExtensionUrlOverrideStateTrackerImplTest,
       TriggersOnUrlOverrideDisabled) {
  ExtensionRegistrar* registrar = ExtensionRegistrar::Get(profile_.get());
  scoped_refptr<const Extension> extension1 =
      CreateExtension("ext1", kNewTabUrlPath, false);
  scoped_refptr<const Extension> extension2 =
      CreateExtension("ext2", kNewTabUrlPath, false);

  EXPECT_CALL(listener_, OnUrlOverrideEnabled(kNewTabUrlPath, false)).Times(1);
  registrar->AddExtension(extension1.get());
  registrar->AddExtension(extension2.get());
  base::RunLoop().QuitWhenIdle();

  EXPECT_CALL(listener_, OnUrlOverrideDisabled(kNewTabUrlPath)).Times(0);
  registrar->DisableExtension(extension1->id(),
                              {disable_reason::DISABLE_USER_ACTION});
  base::RunLoop().QuitWhenIdle();
  testing::Mock::VerifyAndClearExpectations(&listener_);

  EXPECT_CALL(listener_, OnUrlOverrideDisabled(kNewTabUrlPath)).Times(1);
  registrar->DisableExtension(extension2->id(),
                              {disable_reason::DISABLE_USER_ACTION});
  base::RunLoop().QuitWhenIdle();
}

TEST_F(ExtensionUrlOverrideStateTrackerImplTest, TriggersForMultipleUrls) {
  ExtensionRegistrar* registrar = ExtensionRegistrar::Get(profile_.get());
  scoped_refptr<const Extension> ntp_extension =
      CreateExtension("ntp", kNewTabUrlPath, false);
  scoped_refptr<const Extension> history_extension =
      CreateExtension("history", kHistoryUrlPath, false);

  EXPECT_CALL(listener_, OnUrlOverrideEnabled(kNewTabUrlPath, false)).Times(1);
  registrar->AddExtension(ntp_extension.get());
  base::RunLoop().QuitWhenIdle();

  EXPECT_CALL(listener_, OnUrlOverrideEnabled(kHistoryUrlPath, false)).Times(1);
  registrar->AddExtension(history_extension.get());
  base::RunLoop().QuitWhenIdle();

  EXPECT_CALL(listener_, OnUrlOverrideDisabled(kNewTabUrlPath)).Times(1);
  registrar->DisableExtension(ntp_extension->id(),
                              {disable_reason::DISABLE_USER_ACTION});
  base::RunLoop().QuitWhenIdle();

  EXPECT_CALL(listener_, OnUrlOverrideDisabled(kHistoryUrlPath)).Times(1);
  registrar->DisableExtension(history_extension->id(),
                              {disable_reason::DISABLE_USER_ACTION});
  base::RunLoop().QuitWhenIdle();
}

TEST_F(ExtensionUrlOverrideStateTrackerImplTest, RegisterWithIncognitoEnabled) {
  ExtensionRegistrar* registrar = ExtensionRegistrar::Get(profile_.get());
  scoped_refptr<const Extension> extension =
      CreateExtension("ext", kNewTabUrlPath, true);
  SetIncognitoEnabled(extension.get(), true);

  EXPECT_CALL(listener_, OnUrlOverrideEnabled(kNewTabUrlPath, true)).Times(1);
  registrar->AddExtension(extension.get());
  base::RunLoop().QuitWhenIdle();
}

TEST_F(ExtensionUrlOverrideStateTrackerImplTest,
       MultipleExtensionsAndIncognitoStatus) {
  ExtensionRegistrar* registrar = ExtensionRegistrar::Get(profile_.get());
  scoped_refptr<const Extension> ext1 =
      CreateExtension("ext1", kNewTabUrlPath, true);
  scoped_refptr<const Extension> ext2 =
      CreateExtension("ext2", kNewTabUrlPath, true);

  SetIncognitoEnabled(ext1.get(), true);
  SetIncognitoEnabled(ext2.get(), false);

  EXPECT_CALL(listener_, OnUrlOverrideEnabled(kNewTabUrlPath, true)).Times(1);
  registrar->AddExtension(ext1.get());
  base::RunLoop().QuitWhenIdle();
  testing::Mock::VerifyAndClearExpectations(&listener_);

  registrar->AddExtension(ext2.get());
  base::RunLoop().QuitWhenIdle();
  testing::Mock::VerifyAndClearExpectations(&listener_);

  // No more incognito overrides. We call this again.
  EXPECT_CALL(listener_, OnUrlOverrideEnabled(kNewTabUrlPath, false)).Times(1);
  registrar->DisableExtension(ext1->id(),
                              {disable_reason::DISABLE_USER_ACTION});
  base::RunLoop().QuitWhenIdle();
  testing::Mock::VerifyAndClearExpectations(&listener_);

  EXPECT_CALL(listener_, OnUrlOverrideDisabled(kNewTabUrlPath)).Times(1);
  registrar->DisableExtension(ext2->id(),
                              {disable_reason::DISABLE_USER_ACTION});
  base::RunLoop().QuitWhenIdle();
}

TEST_F(ExtensionUrlOverrideStateTrackerImplTest, CatchesPreexistingOverrides) {
  // Reset the tracker created in SetUp() to test construction with a
  // pre-existing override.
  tracker_.reset();

  ExtensionRegistrar* registrar = ExtensionRegistrar::Get(profile_.get());
  scoped_refptr<const Extension> extension =
      CreateExtension("ext", kNewTabUrlPath, false);
  registrar->AddExtension(extension.get());
  base::RunLoop().QuitWhenIdle();

  EXPECT_CALL(listener_, OnUrlOverrideEnabled(kNewTabUrlPath, false)).Times(1);
  tracker_ = std::make_unique<ExtensionUrlOverrideStateTrackerImpl>(
      profile_.get(), &listener_);
  base::RunLoop().QuitWhenIdle();
}

}  // namespace extensions
