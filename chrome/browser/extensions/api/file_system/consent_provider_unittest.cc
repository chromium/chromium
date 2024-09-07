// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/extensions/api/file_system/consent_provider_impl.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/dialog_button.mojom.h"

using extensions::file_system_api::ConsentProviderImpl;
using extensions::mojom::ManifestLocation;

namespace extensions {
namespace {

// Configurations and results for TestingConsentProviderDelegate, with directly
// accessible fields.
struct TestDelegateState {
  // Used to assign a fake dialog response.
  ui::mojom::DialogButton dialog_button = ui::mojom::DialogButton::kNone;

  // Used to assign fake result of detection the auto launch kiosk mode.
  bool is_auto_launched = false;

  // Used to set allowlisted components list with a single id.
  std::string allowlisted_component_id;

  // Counters to record calls.
  int show_dialog_counter = 0;
  int show_notification_counter = 0;
};

// Test implementation of ConsentProviderImpl::DelegateInterface that exposes
// states to a TestDelegateState instance.
class TestingConsentProviderDelegate
    : public ConsentProviderImpl::DelegateInterface {
 public:
  explicit TestingConsentProviderDelegate(TestDelegateState* state)
      : state_(state) {}

  TestingConsentProviderDelegate(const TestingConsentProviderDelegate&) =
      delete;
  TestingConsentProviderDelegate& operator=(
      const TestingConsentProviderDelegate&) = delete;

  ~TestingConsentProviderDelegate() override = default;

 private:
  // ConsentProviderImpl::DelegateInterface:
  void ShowDialog(content::RenderFrameHost* host,
                  const extensions::ExtensionId& extension_id,
                  const std::string& extension_name,
                  const std::string& volume_id,
                  const std::string& volume_label,
                  bool writable,
                  ConsentProviderImpl::ShowDialogCallback callback) override {
    ++state_->show_dialog_counter;
    std::move(callback).Run(state_->dialog_button);
  }

  // ConsentProviderImpl::DelegateInterface:
  void ShowNotification(const extensions::ExtensionId& extension_id,
                        const std::string& extension_name,
                        const std::string& volume_id,
                        const std::string& volume_label,
                        bool writable) override {
    ++state_->show_notification_counter;
  }

  // ConsentProviderImpl::DelegateInterface:
  bool IsAutoLaunched(const extensions::Extension& extension) override {
    return state_->is_auto_launched;
  }

  // ConsentProviderImpl::DelegateInterface:
  bool IsAllowlistedComponent(const extensions::Extension& extension) override {
    return state_->allowlisted_component_id.compare(extension.id()) == 0;
  }

  // Use raw_ptr since |state| is owned by owner.
  raw_ptr<TestDelegateState> state_;
};

// Rewrites result of a consent request from |result| to |log|.
void OnConsentReceived(ConsentProviderImpl::Consent* log,
                       const ConsentProviderImpl::Consent result) {
  *log = result;
}

}  // namespace

class FileSystemApiConsentProviderTest : public testing::Test {
 public:
  FileSystemApiConsentProviderTest() {}

  void SetUp() override {
    testing_pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    TestingBrowserProcess::GetGlobal()->SetLocalState(
        testing_pref_service_.get());
    user_manager_ = new ash::FakeChromeUserManager;
    scoped_user_manager_enabler_ =
        std::make_unique<user_manager::ScopedUserManager>(
            base::WrapUnique(user_manager_.get()));
  }

  void TearDown() override {
    scoped_user_manager_enabler_.reset();
    user_manager_ = nullptr;
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
    testing_pref_service_.reset();
  }

 protected:
  std::unique_ptr<TestingPrefServiceSimple> testing_pref_service_;
  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged>
      user_manager_;  // Owned by the scope enabler.
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_enabler_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(FileSystemApiConsentProviderTest, ForNonKioskApps) {
  // Component apps are not granted unless they are allowlisted.
  {
    scoped_refptr<const Extension> component_extension(
        ExtensionBuilder("Test", ExtensionBuilder::Type::PLATFORM_APP)
            .SetLocation(ManifestLocation::kComponent)
            .Build());
    TestDelegateState state;
    ConsentProviderImpl provider(
        std::make_unique<TestingConsentProviderDelegate>(&state));
    EXPECT_FALSE(provider.IsGrantable(*component_extension));
  }

  // Allowlisted component apps are instantly granted access without asking
  // user.
  {
    scoped_refptr<const Extension> allowlisted_component_extension(
        ExtensionBuilder("Test", ExtensionBuilder::Type::PLATFORM_APP)
            .SetLocation(ManifestLocation::kComponent)
            .Build());
    TestDelegateState state;
    state.allowlisted_component_id = allowlisted_component_extension->id();
    ConsentProviderImpl provider(
        std::make_unique<TestingConsentProviderDelegate>(&state));
    EXPECT_TRUE(provider.IsGrantable(*allowlisted_component_extension));

    ConsentProviderImpl::Consent result = ConsentProvider::CONSENT_IMPOSSIBLE;
    provider.RequestConsent(nullptr, *allowlisted_component_extension.get(),
                            "Volume ID 1", "Volume Label 1",
                            true /* writable */,
                            base::BindOnce(&OnConsentReceived, &result));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(0, state.show_dialog_counter);
    EXPECT_EQ(0, state.show_notification_counter);
    EXPECT_EQ(ConsentProvider::CONSENT_GRANTED, result);
  }

  // Non-component apps in non-kiosk mode will be rejected instantly, without
  // asking for user consent.
  {
    scoped_refptr<const Extension> non_component_extension(
        ExtensionBuilder("Test").Build());
    TestDelegateState state;
    ConsentProviderImpl provider(
        std::make_unique<TestingConsentProviderDelegate>(&state));
    EXPECT_FALSE(provider.IsGrantable(*non_component_extension));
  }
}

TEST_F(FileSystemApiConsentProviderTest, ForKioskApps) {
  // Non-component apps in auto-launch kiosk mode will be granted access
  // instantly without asking for user consent, but with a notification.
  {
    scoped_refptr<const Extension> auto_launch_kiosk_app(
        ExtensionBuilder("Test", ExtensionBuilder::Type::PLATFORM_APP)
            .SetManifestKey("kiosk_enabled", true)
            .SetManifestKey("kiosk_only", true)
            .Build());
    auto* auto_user = user_manager_->AddKioskAppUser(
        AccountId::FromUserEmail(auto_launch_kiosk_app->id()));
    user_manager_->LoginUser(auto_user->GetAccountId());

    TestDelegateState state;
    state.is_auto_launched = true;
    ConsentProviderImpl provider(
        std::make_unique<TestingConsentProviderDelegate>(&state));
    EXPECT_TRUE(provider.IsGrantable(*auto_launch_kiosk_app));

    ConsentProviderImpl::Consent result = ConsentProvider::CONSENT_IMPOSSIBLE;
    provider.RequestConsent(
        nullptr, *auto_launch_kiosk_app.get(), "Volume ID 2", "Volume Label 2",
        true /* writable */, base::BindOnce(&OnConsentReceived, &result));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(0, state.show_dialog_counter);
    EXPECT_EQ(1, state.show_notification_counter);
    EXPECT_EQ(ConsentProvider::CONSENT_GRANTED, result);
  }

  // Non-component apps in manual-launch kiosk mode will be granted access after
  // receiving approval from the user.
  scoped_refptr<const Extension> manual_launch_kiosk_app(
      ExtensionBuilder("Test", ExtensionBuilder::Type::PLATFORM_APP)
          .SetManifestKey("kiosk_enabled", true)
          .SetManifestKey("kiosk_only", true)
          .Build());
  auto* manual_user = user_manager_->AddKioskAppUser(
      AccountId::FromUserEmail(manual_launch_kiosk_app->id()));
  user_manager_->LoginUser(manual_user->GetAccountId());
  {
    TestDelegateState state;
    state.dialog_button = ui::mojom::DialogButton::kOk;
    ConsentProviderImpl provider(
        std::make_unique<TestingConsentProviderDelegate>(&state));
    EXPECT_TRUE(provider.IsGrantable(*manual_launch_kiosk_app));

    ConsentProviderImpl::Consent result = ConsentProvider::CONSENT_IMPOSSIBLE;
    provider.RequestConsent(nullptr, *manual_launch_kiosk_app.get(),
                            "Volume ID 3", "Volume Label 3",
                            true /* writable */,
                            base::BindOnce(&OnConsentReceived, &result));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1, state.show_dialog_counter);
    EXPECT_EQ(0, state.show_notification_counter);
    EXPECT_EQ(ConsentProvider::CONSENT_GRANTED, result);
  }

  // Non-component apps in manual-launch kiosk mode will be rejected access
  // after rejecting by a user.
  {
    TestDelegateState state;
    state.dialog_button = ui::mojom::DialogButton::kCancel;
    ConsentProviderImpl provider(
        std::make_unique<TestingConsentProviderDelegate>(&state));
    EXPECT_TRUE(provider.IsGrantable(*manual_launch_kiosk_app));

    ConsentProviderImpl::Consent result = ConsentProvider::CONSENT_IMPOSSIBLE;
    provider.RequestConsent(nullptr, *manual_launch_kiosk_app.get(),
                            "Volume ID 4", "Volume Label 4",
                            true /* writable */,
                            base::BindOnce(&OnConsentReceived, &result));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1, state.show_dialog_counter);
    EXPECT_EQ(0, state.show_notification_counter);
    EXPECT_EQ(ConsentProvider::CONSENT_REJECTED, result);
  }
}

}  // namespace extensions
