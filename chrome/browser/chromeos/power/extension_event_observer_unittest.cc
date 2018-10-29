// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/extension_event_observer.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/common/extensions/api/gcm.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_renderer_host.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_observer.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/test_screen.h"
#include "ui/display/screen.h"

namespace chromeos {

class ExtensionEventObserverTest : public ::testing::Test {
 public:
  ExtensionEventObserverTest()
      : power_manager_client_(new FakePowerManagerClient()),
        test_screen_(aura::TestScreen::Create(gfx::Size())),
        fake_user_manager_(new FakeChromeUserManager()),
        scoped_user_manager_enabler_(base::WrapUnique(fake_user_manager_)) {
    DBusThreadManager::GetSetterForTesting()->SetPowerManagerClient(
        base::WrapUnique(power_manager_client_));

    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));

    extension_event_observer_.reset(new ExtensionEventObserver());
    test_api_ = extension_event_observer_->CreateTestApi();
  }

  ~ExtensionEventObserverTest() override {
    extension_event_observer_.reset();
    profile_manager_.reset();
    DBusThreadManager::Shutdown();
  }

  // ::testing::Test overrides.
  void SetUp() override {
    ::testing::Test::SetUp();

    display::Screen::SetScreenInstance(test_screen_.get());

    // Must be called from ::testing::Test::SetUp.
    ASSERT_TRUE(profile_manager_->SetUp());

    const char kUserProfile[] = "profile1@example.com";
    const AccountId account_id(AccountId::FromUserEmail(kUserProfile));
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);
    profile_ =
        profile_manager_->CreateTestingProfile(account_id.GetUserEmail());

    profile_manager_->SetLoggedIn(true);
  }
  void TearDown() override {
    profile_ = NULL;
    profile_manager_->DeleteAllTestingProfiles();
    display::Screen::SetScreenInstance(nullptr);
    ::testing::Test::TearDown();
  }

 protected:
  scoped_refptr<const extensions::Extension> CreateApp(const std::string& name,
                                                       bool uses_gcm) {
    scoped_refptr<const extensions::Extension> app =
        extensions::ExtensionBuilder()
            .SetManifest(
                extensions::DictionaryBuilder()
                    .Set("name", name)
                    .Set("version", "1.0.0")
                    .Set("manifest_version", 2)
                    .Set("app", extensions::DictionaryBuilder()
                                    .Set("background",
                                         extensions::DictionaryBuilder()
                                             .Set("scripts",
                                                  extensions::ListBuilder()
                                                      .Append("background.js")
                                                      .Build())
                                             .Build())
                                    .Build())
                    .Set("permissions", extensions::ListBuilder()
                                            .Append(uses_gcm ? "gcm" : "")
                                            .Build())
                    .Build())
            .Build();

    created_apps_.push_back(app);

    return app;
  }

  extensions::ExtensionHost* CreateHostForApp(
      Profile* profile,
      const extensions::Extension* app) {
    extensions::ProcessManager::Get(profile)->CreateBackgroundHost(
        app, extensions::BackgroundInfo::GetBackgroundURL(app));
    base::RunLoop().RunUntilIdle();

    return extensions::ProcessManager::Get(profile)
        ->GetBackgroundHostForExtension(app->id());
  }

  // Owned by DBusThreadManager.
  FakePowerManagerClient* power_manager_client_;

  std::unique_ptr<ExtensionEventObserver> extension_event_observer_;
  std::unique_ptr<ExtensionEventObserver::TestApi> test_api_;

  // Owned by |profile_manager_|.
  TestingProfile* profile_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

 private:
  std::unique_ptr<aura::TestScreen> test_screen_;
  content::TestBrowserThreadBundle browser_thread_bundle_;

  // Needed to ensure we don't end up creating actual RenderViewHosts
  // and RenderProcessHosts.
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;

  // Chrome OS needs the CrosSettings test helper.
  ScopedCrosSettingsTestHelper cros_settings_test_helper_;

  // Owned by |scoped_user_manager_enabler_|.
  FakeChromeUserManager* fake_user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_enabler_;

  std::vector<scoped_refptr<const extensions::Extension>> created_apps_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionEventObserverTest);
};

// Tests that the ExtensionEventObserver reports readiness for suspend when
// there is nothing interesting going on.
TEST_F(ExtensionEventObserverTest, BasicSuspendAndDarkSuspend) {
  power_manager_client_->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(1, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());

  EXPECT_TRUE(test_api_->MaybeRunSuspendReadinessCallback());
  EXPECT_EQ(0, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());

  power_manager_client_->SendDarkSuspendImminent();
  EXPECT_EQ(1, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());

  EXPECT_TRUE(test_api_->MaybeRunSuspendReadinessCallback());
  EXPECT_EQ(0, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());
}

// Tests that the ExtensionEventObserver properly handles a canceled suspend
// attempt.
TEST_F(ExtensionEventObserverTest, CanceledSuspend) {
  power_manager_client_->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(1, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());

  power_manager_client_->SendSuspendDone();
  EXPECT_FALSE(test_api_->MaybeRunSuspendReadinessCallback());
}

// Tests that the ExtensionEventObserver delays suspends and dark suspends while
// there is a push message pending for an app that uses GCM.
TEST_F(ExtensionEventObserverTest, PushMessagesDelaySuspend) {
  scoped_refptr<const extensions::Extension> gcm_app =
      CreateApp("DelaysSuspendForPushMessages", true /* uses_gcm */);
  extensions::ExtensionHost* host = CreateHostForApp(profile_, gcm_app.get());
  ASSERT_TRUE(host);
  EXPECT_TRUE(test_api_->WillDelaySuspendForExtensionHost(host));

  // Test that a push message received before a suspend attempt delays the
  // attempt.
  const int kSuspendPushId = 23874;
  extension_event_observer_->OnBackgroundEventDispatched(
      host, extensions::api::gcm::OnMessage::kEventName, kSuspendPushId);
  power_manager_client_->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);

  EXPECT_TRUE(test_api_->MaybeRunSuspendReadinessCallback());
  EXPECT_EQ(1, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());

  extension_event_observer_->OnBackgroundEventAcked(host, kSuspendPushId);
  EXPECT_EQ(0, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());

  // Now test receiving the suspend attempt before the push message.
  const int kDarkSuspendPushId = 56674;
  power_manager_client_->SendDarkSuspendImminent();
  extension_event_observer_->OnBackgroundEventDispatched(
      host, extensions::api::gcm::OnMessage::kEventName, kDarkSuspendPushId);

  EXPECT_TRUE(test_api_->MaybeRunSuspendReadinessCallback());
  EXPECT_EQ(1, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());

  extension_event_observer_->OnBackgroundEventAcked(host, kDarkSuspendPushId);
  EXPECT_EQ(0, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());

  // Test that non-push messages do not delay the suspend.
  const int kNonPushId = 5687;
  power_manager_client_->SendDarkSuspendImminent();
  extension_event_observer_->OnBackgroundEventDispatched(host, "FakeMessage",
                                                         kNonPushId);

  EXPECT_TRUE(test_api_->MaybeRunSuspendReadinessCallback());
  EXPECT_EQ(0, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());
}

// Tests that messages sent for apps that don't use GCM are ignored.
TEST_F(ExtensionEventObserverTest, IgnoresNonGCMApps) {
  scoped_refptr<const extensions::Extension> app = CreateApp("Non-GCM", false);
  extensions::ExtensionHost* host = CreateHostForApp(profile_, app.get());
  ASSERT_TRUE(host);

  EXPECT_FALSE(test_api_->WillDelaySuspendForExtensionHost(host));

  power_manager_client_->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_TRUE(test_api_->MaybeRunSuspendReadinessCallback());
  EXPECT_EQ(0, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());
}

// Tests that network requests started by an app while it is processing a push
// message delay any suspend attempt.
TEST_F(ExtensionEventObserverTest, NetworkRequestsMayDelaySuspend) {
  scoped_refptr<const extensions::Extension> app =
      CreateApp("NetworkRequests", true);
  extensions::ExtensionHost* host = CreateHostForApp(profile_, app.get());
  ASSERT_TRUE(host);
  EXPECT_TRUE(test_api_->WillDelaySuspendForExtensionHost(host));

  // Test that network requests started while there is no pending push message
  // are ignored.
  const uint64_t kNonPushRequestId = 5170725;
  extension_event_observer_->OnNetworkRequestStarted(host, kNonPushRequestId);
  power_manager_client_->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);

  EXPECT_TRUE(test_api_->MaybeRunSuspendReadinessCallback());
  EXPECT_EQ(0, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());

  // Test that network requests started while a push message is pending delay
  // the suspend even after the push message has been acked.
  const int kPushMessageId = 178674;
  const uint64_t kNetworkRequestId = 78917089;
  power_manager_client_->SendDarkSuspendImminent();
  extension_event_observer_->OnBackgroundEventDispatched(
      host, extensions::api::gcm::OnMessage::kEventName, kPushMessageId);

  EXPECT_TRUE(test_api_->MaybeRunSuspendReadinessCallback());
  EXPECT_EQ(1, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());

  extension_event_observer_->OnNetworkRequestStarted(host, kNetworkRequestId);
  extension_event_observer_->OnBackgroundEventAcked(host, kPushMessageId);
  EXPECT_EQ(1, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());

  extension_event_observer_->OnNetworkRequestDone(host, kNetworkRequestId);
  EXPECT_EQ(0, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());
}

// Tests that any outstanding push messages or network requests for an
// ExtensionHost that is destroyed do not end up blocking system suspend.
TEST_F(ExtensionEventObserverTest, DeletedExtensionHostDoesNotBlockSuspend) {
  scoped_refptr<const extensions::Extension> app =
      CreateApp("DeletedExtensionHost", true);

  // The easiest way to delete an extension host is to delete the Profile it is
  // associated with so we create a new Profile here.
  const char kProfileName[] = "DeletedExtensionHostProfile";
  Profile* new_profile = profile_manager_->CreateTestingProfile(kProfileName);

  extensions::ExtensionHost* host = CreateHostForApp(new_profile, app.get());
  ASSERT_TRUE(host);
  EXPECT_TRUE(test_api_->WillDelaySuspendForExtensionHost(host));

  const int kPushId = 156178;
  const uint64_t kNetworkId = 791605;
  extension_event_observer_->OnBackgroundEventDispatched(
      host, extensions::api::gcm::OnMessage::kEventName, kPushId);
  extension_event_observer_->OnNetworkRequestStarted(host, kNetworkId);

  // Now delete the Profile.  This has the side-effect of also deleting all the
  // ExtensionHosts.
  profile_manager_->DeleteTestingProfile(kProfileName);

  power_manager_client_->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_TRUE(test_api_->MaybeRunSuspendReadinessCallback());
  EXPECT_EQ(0, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());
}

// Tests that the ExtensionEventObserver does not delay suspend attempts when it
// is disabled.
TEST_F(ExtensionEventObserverTest, DoesNotDelaySuspendWhenDisabled) {
  scoped_refptr<const extensions::Extension> app =
      CreateApp("NoDelayWhenDisabled", true);
  extensions::ExtensionHost* host = CreateHostForApp(profile_, app.get());
  ASSERT_TRUE(host);
  EXPECT_TRUE(test_api_->WillDelaySuspendForExtensionHost(host));

  // Test that disabling the suspend delay while a suspend is pending will cause
  // the ExtensionEventObserver to immediately report readiness.
  const int kPushId = 416753;
  extension_event_observer_->OnBackgroundEventDispatched(
      host, extensions::api::gcm::OnMessage::kEventName, kPushId);
  power_manager_client_->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(1, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());

  extension_event_observer_->SetShouldDelaySuspend(false);
  EXPECT_FALSE(test_api_->MaybeRunSuspendReadinessCallback());
  EXPECT_EQ(0, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());

  // Test that the ExtensionEventObserver does not delay suspend attempts when
  // it is disabled.
  power_manager_client_->SendDarkSuspendImminent();
  EXPECT_EQ(0, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());
}

}  // namespace chromeos
