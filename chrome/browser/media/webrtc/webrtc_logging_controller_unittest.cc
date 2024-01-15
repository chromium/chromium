// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_logging_controller.h"

#include <list>
#include <map>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_store.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webrtc_text_log {

using ::testing::_;

using BrowserContext = content::BrowserContext;
using MockRenderProcessHost = content::MockRenderProcessHost;
using RenderProcessHost = content::RenderProcessHost;

class WebRtcLoggingControllerTest : public ::testing::Test {
 public:
  WebRtcLoggingControllerTest()
      : browser_context_(nullptr),
        test_shared_url_loader_factory_(
            test_url_loader_factory_.GetSafeWeakWrapper()) {
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        test_shared_url_loader_factory_);

    EXPECT_TRUE(profiles_dir_.CreateUniqueTempDir());
  }

  WebRtcLoggingControllerTest(const WebRtcLoggingControllerTest&) = delete;
  WebRtcLoggingControllerTest& operator=(const WebRtcLoggingControllerTest&) =
      delete;

  ~WebRtcLoggingControllerTest() override {
    webrtc_logging_controller_ = nullptr;
    if (browser_context_) {
      UnloadMainTestProfile();
    }
  }

  void LoadMainTestProfile(std::optional<bool> text_log_collection_allowed) {
    browser_context_ = CreateBrowserContext("browser_context_", true,
                                            text_log_collection_allowed);
    CreateRenderHost();
  }

  void UnloadMainTestProfile() {
    TestingBrowserProcess::GetGlobal()->webrtc_log_uploader()->Shutdown();
    TestingBrowserProcess::GetGlobal()->SetWebRtcLogUploader(nullptr);
    rph_.reset();
    browser_context_.reset();
  }

  void CreateRenderHost() {
    rph_ = std::make_unique<MockRenderProcessHost>(browser_context_.get());
    auto webrtc_log_uploader = std::make_unique<WebRtcLogUploader>();
    TestingBrowserProcess::GetGlobal()->SetWebRtcLogUploader(
        std::move(webrtc_log_uploader));
    WebRtcLoggingController::AttachToRenderProcessHost(rph_.get());
    webrtc_logging_controller_ =
        WebRtcLoggingController::FromRenderProcessHost(rph_.get());
  }

  void CreateUnManagedProfile() {
    browser_context_ =
        CreateBrowserContext("browser_context_", false, std::nullopt);
    CreateRenderHost();
  }

  std::unique_ptr<TestingProfile> CreateBrowserContext(
      std::string profile_name,
      bool is_managed_profile,
      std::optional<bool> text_log_collection_allowed) {
    // If profile name not specified, select a unique name.
    if (profile_name.empty()) {
      static size_t index = 0;
      profile_name = base::NumberToString(++index);
    }

    // Set a directory for the profile, derived from its name, so that
    // recreating the profile will get the same directory.
    const base::FilePath profile_path =
        profiles_dir_.GetPath().AppendASCII(profile_name);
    if (base::PathExists(profile_path)) {
      EXPECT_TRUE(base::DirectoryExists(profile_path));
    } else {
      EXPECT_TRUE(base::CreateDirectory(profile_path));
    }

    // Prepare to specify preferences for the profile.
    sync_preferences::PrefServiceMockFactory factory;
    factory.set_user_prefs(base::WrapRefCounted(new TestingPrefStore()));
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    sync_preferences::PrefServiceSyncable* regular_prefs =
        factory.CreateSyncable(registry.get()).release();

    // Set the preference associated with the policy for
    // WebRtcTextLogCollectionAllowed
    RegisterUserProfilePrefs(registry.get());
    if (text_log_collection_allowed.has_value()) {
      regular_prefs->SetBoolean(prefs::kWebRtcTextLogCollectionAllowed,
                                text_log_collection_allowed.value());
    }

    // Build the profile.
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(profile_name);
    profile_builder.SetPath(profile_path);
    profile_builder.SetPrefService(base::WrapUnique(regular_prefs));
    profile_builder.OverridePolicyConnectorIsManagedForTesting(
        is_managed_profile);

    std::unique_ptr<TestingProfile> profile = profile_builder.Build();

    return profile;
  }

  // The directory which will contain all profiles.
  base::ScopedTempDir profiles_dir_;

  // Default BrowserContext
  std::unique_ptr<TestingProfile> browser_context_;
  std::unique_ptr<MockRenderProcessHost> rph_;

  // Class under test.
  raw_ptr<WebRtcLoggingController> webrtc_logging_controller_ = nullptr;

  // Testing utilities.
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
};

TEST_F(WebRtcLoggingControllerTest, ManagedProfileWithTruePolicy) {
  LoadMainTestProfile(true);
  EXPECT_TRUE(webrtc_logging_controller_->IsWebRtcTextLogAllowed(
      browser_context_.get()));
}

TEST_F(WebRtcLoggingControllerTest, ManagedProfileWithFalsePolicy) {
  LoadMainTestProfile(false);
  EXPECT_FALSE(webrtc_logging_controller_->IsWebRtcTextLogAllowed(
      browser_context_.get()));
}

TEST_F(WebRtcLoggingControllerTest, ManagedProfileWithUnsetPolicy) {
  LoadMainTestProfile(std::nullopt);
  EXPECT_TRUE(webrtc_logging_controller_->IsWebRtcTextLogAllowed(
      browser_context_.get()));
}

TEST_F(WebRtcLoggingControllerTest, IncognitoWithUnsetPolicy) {
  LoadMainTestProfile(std::nullopt);
  Profile* incognito_profile =
      browser_context_->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  EXPECT_TRUE(
      webrtc_logging_controller_->IsWebRtcTextLogAllowed(incognito_profile));

  browser_context_->DestroyOffTheRecordProfile(incognito_profile);
}

TEST_F(WebRtcLoggingControllerTest, UnmanagedProfileWithUnsetPolicy) {
  CreateUnManagedProfile();
  EXPECT_TRUE(webrtc_logging_controller_->IsWebRtcTextLogAllowed(
      browser_context_.get()));
}

TEST_F(WebRtcLoggingControllerTest, NullBrowserContext) {
  EXPECT_TRUE(webrtc_logging_controller_->IsWebRtcTextLogAllowed(nullptr));
}
}  // namespace webrtc_text_log
