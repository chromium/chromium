// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/os_feedback/os_feedback_screenshot_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/dlp_warn_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/mock_dlp_warn_notifier.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_observer.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/test/dlp_content_manager_test_helper.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_rules_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/reporting/client/mock_report_queue_provider.h"
#include "components/reporting/client/report_queue_provider_test_helper.h"
#include "components/reporting/util/status.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {

namespace {

using ::testing::_;
using ::testing::Return;

constexpr char kTestUserEmail[] = "test@example.com";
constexpr GaiaId::Literal kTestGaiaId("12345");
constexpr char kExampleUrl[] = "https://example.com/";
constexpr char kBlockedUrl[] = "https://blocked.example.com/";

}  // namespace

class OsFeedbackScreenshotManagerTest : public ::testing::Test {
 public:
  OsFeedbackScreenshotManagerTest() = default;
  ~OsFeedbackScreenshotManagerTest() override = default;

 protected:
  void SetTestPngData() {
    OsFeedbackScreenshotManager::GetInstance()->SetPngDataForTesting(
        CreateFakePngData());
  }

  scoped_refptr<base::RefCountedMemory> CreateFakePngData() {
    const unsigned char kData[] = {12, 11, 99};
    return base::MakeRefCounted<base::RefCountedBytes>(kData);
  }
};

// Test that OsFeedbackScreenshotManager is a Singleton.
TEST_F(OsFeedbackScreenshotManagerTest, Singleton) {
  EXPECT_EQ(nullptr, OsFeedbackScreenshotManager::GetIfExists());
  auto* manager = OsFeedbackScreenshotManager::GetInstance();

  EXPECT_EQ(manager, OsFeedbackScreenshotManager::GetInstance());
  EXPECT_EQ(manager, OsFeedbackScreenshotManager::GetIfExists());
}

// Test that DeleteScreenshotData removes the screenshot data.
TEST_F(OsFeedbackScreenshotManagerTest, DeleteScreenshotData) {
  auto* manager = OsFeedbackScreenshotManager::GetInstance();
  SetTestPngData();
  EXPECT_NE(nullptr, manager->GetScreenshotData());

  manager->DeleteScreenshotData();

  EXPECT_EQ(nullptr, manager->GetScreenshotData());
}

// Test that TakeScreenshot will skip if a screenshot exists already.
TEST_F(OsFeedbackScreenshotManagerTest, DoNotTakeScreenshotIfExists) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::test::TestFuture<bool> future;
  auto* manager = OsFeedbackScreenshotManager::GetInstance();
  SetTestPngData();
  EXPECT_NE(nullptr, manager->GetScreenshotData());
  EXPECT_EQ(3u, manager->GetScreenshotData()->size());

  manager->TakeScreenshot(future.GetCallback());

  EXPECT_FALSE(future.Get());
  EXPECT_EQ(3u, manager->GetScreenshotData()->size());
}

// Test that TakeScreenshot will skip if there is not a window/display.
TEST_F(OsFeedbackScreenshotManagerTest, DoNotTakeScreenshotIfNoWindow) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::test::TestFuture<bool> future;
  auto* manager = OsFeedbackScreenshotManager::GetInstance();

  manager->TakeScreenshot(future.GetCallback());

  EXPECT_FALSE(future.Get());
}

class OsFeedbackScreenshotManagerAshTest : public ChromeAshTestBase {
 public:
  OsFeedbackScreenshotManagerAshTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~OsFeedbackScreenshotManagerAshTest() override = default;

  void SetUp() override {
    ChromeAshTestBase::SetUp();
    rvh_test_enabler_ = std::make_unique<content::RenderViewHostTestEnabler>();
    ASSERT_TRUE(profile_manager_.SetUp());

    auto user_manager = std::make_unique<user_manager::UserManagerImpl>(
        std::make_unique<user_manager::FakeUserManagerDelegate>(),
        ash::Shell::Get()->local_state());
    auto* user_manager_ptr = user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    const std::string user_id_hash =
        user_manager::TestHelper::GetFakeUsernameHash(account_id_);
    ASSERT_TRUE(
        user_manager::TestHelper(user_manager_ptr).AddRegularUser(account_id_));
    user_manager_ptr->UserLoggedIn(account_id_, user_id_hash);
    profile_ = profile_manager_.CreateTestingProfile(kTestUserEmail);
    user_manager_ptr->OnUserProfileCreated(account_id_, profile_->GetPrefs());

    report_queue_provider_helper_ =
        std::make_unique<reporting::test::ReportQueueProviderTestHelper>();
    ON_CALL(*report_queue_provider_helper_->mock_provider(),
            CreateNewSpeculativeQueueMock)
        .WillByDefault([]() {
          return base::unexpected(
              reporting::Status(reporting::error::UNAVAILABLE, ""));
        });
    if (auto* manager = OsFeedbackScreenshotManager::GetIfExists()) {
      manager->DeleteScreenshotData();
    }
  }

  void TearDown() override {
    if (auto* manager = OsFeedbackScreenshotManager::GetIfExists()) {
      manager->DeleteScreenshotData();
    }
    ash::Shell::Get()->local_state()->SetBoolean(
        policy::policy_prefs::kDisableScreenshots, false);
    // Destroy any lazily-created DlpContentManagerAsh singleton before
    // ChromeAshTestBase::TearDown() tears down ash::Shell and its
    // ActivationClient.
    policy::ScopedDlpContentObserverForTesting reset_dlp(nullptr);
    report_queue_provider_helper_.reset();
    mock_rules_manager_ = nullptr;
    if (profile_) {
      user_manager::UserManager::Get()->OnUserProfileWillBeDestroyed(
          account_id_);
      profile_ = nullptr;
    }
    profile_manager_.DeleteAllTestingProfiles();
    scoped_user_manager_.reset();
    rvh_test_enabler_.reset();
    ChromeAshTestBase::TearDown();
  }

 protected:
  // RAII wrapper that clears the WebContents from DlpContentManagerAsh before
  // destroying it.
  class ScopedRestrictedWebContents {
   public:
    ScopedRestrictedWebContents(
        policy::DlpContentManagerTestHelper* dlp_helper,
        std::unique_ptr<content::WebContents> web_contents)
        : dlp_helper_(dlp_helper), web_contents_(std::move(web_contents)) {}
    ScopedRestrictedWebContents(const ScopedRestrictedWebContents&) = delete;
    ScopedRestrictedWebContents& operator=(const ScopedRestrictedWebContents&) =
        delete;
    ~ScopedRestrictedWebContents() {
      if (web_contents_) {
        dlp_helper_->DestroyWebContents(web_contents_.get());
        web_contents_.reset();
      }
    }

   private:
    raw_ptr<policy::DlpContentManagerTestHelper> dlp_helper_;
    std::unique_ptr<content::WebContents> web_contents_;
  };

  // Creates a visible test WebContents navigated to |url| and registers the
  // given DLP screenshot restriction level for it.
  ScopedRestrictedWebContents CreateRestrictedWebContents(
      policy::DlpContentManagerTestHelper& dlp_helper,
      const GURL& url,
      policy::DlpRulesManager::Level level) {
    auto web_contents =
        content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
    content::WebContentsTester::For(web_contents.get())->NavigateAndCommit(url);
    dlp_helper.ChangeConfidentiality(
        web_contents.get(),
        policy::DlpContentRestrictionSet(
            policy::DlpContentRestriction::kScreenshot, level));
    return ScopedRestrictedWebContents(&dlp_helper, std::move(web_contents));
  }

  // Installs a MockDlpWarnNotifier on |dlp_helper| that captures the pending
  // WarningCallback into |pending_callback| without immediately invoking it.
  policy::MockDlpWarnNotifier* InstallPendingWarnNotifier(
      policy::DlpContentManagerTestHelper& dlp_helper,
      policy::WarningCallback* pending_callback) {
    auto notifier = std::make_unique<policy::MockDlpWarnNotifier>();
    auto* notifier_ptr = notifier.get();
    ON_CALL(*notifier_ptr, ShowDlpWarningDialog)
        .WillByDefault(
            [pending_callback](policy::WarningCallback callback,
                               policy::DlpWarnDialog::DlpWarnDialogOptions) {
              *pending_callback = std::move(callback);
              return base::WeakPtr<views::Widget>();
            });
    dlp_helper.SetWarnNotifierForTesting(std::move(notifier));
    return notifier_ptr;
  }

  // Installs a MockDlpRulesManager for the primary profile so OnImageCapture()
  // reporting calls can be verified directly.
  policy::MockDlpRulesManager* SetUpMockDlpRulesManager() {
    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating(
                      &OsFeedbackScreenshotManagerAshTest::BuildRulesManager,
                      base::Unretained(this)));
    EXPECT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
    return mock_rules_manager_;
  }

 private:
  std::unique_ptr<KeyedService> BuildRulesManager(
      content::BrowserContext* context) {
    auto rules_manager = std::make_unique<policy::MockDlpRulesManager>(
        Profile::FromBrowserContext(context));
    mock_rules_manager_ = rules_manager.get();
    return rules_manager;
  }

  const AccountId account_id_ =
      AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestGaiaId);
  std::unique_ptr<content::RenderViewHostTestEnabler> rvh_test_enabler_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  raw_ptr<policy::MockDlpRulesManager> mock_rules_manager_ = nullptr;
  std::unique_ptr<reporting::test::ReportQueueProviderTestHelper>
      report_queue_provider_helper_;
};

// Test that TakeScreenshot does not instantiate DlpContentManagerAsh when no
// DlpContentObserver instance exists (e.g., on the login screen / OOBE).
TEST_F(OsFeedbackScreenshotManagerAshTest,
       DoesNotInstantiateDlpContentManagerWhenNoRestrictions) {
  ASSERT_FALSE(policy::DlpContentObserver::HasInstance());

  base::test::TestFuture<bool> future;
  auto* manager = OsFeedbackScreenshotManager::GetInstance();
  manager->TakeScreenshot(future.GetCallback());

  EXPECT_TRUE(future.Get());
  EXPECT_NE(nullptr, manager->GetScreenshotData());
  EXPECT_FALSE(policy::DlpContentObserver::HasInstance());
}

// Test that TakeScreenshot succeeds when allowed by policy and DLP.
TEST_F(OsFeedbackScreenshotManagerAshTest, TakeScreenshotAllowed) {
  policy::DlpContentManagerTestHelper dlp_helper;
  ScopedRestrictedWebContents web_contents = CreateRestrictedWebContents(
      dlp_helper, GURL(kExampleUrl), policy::DlpRulesManager::Level::kAllow);

  base::test::TestFuture<bool> future;
  auto* manager = OsFeedbackScreenshotManager::GetInstance();
  manager->TakeScreenshot(future.GetCallback());

  EXPECT_TRUE(future.Get());
  EXPECT_NE(nullptr, manager->GetScreenshotData());
}

// Test that TakeScreenshot will skip if screenshots are disabled by policy.
TEST_F(OsFeedbackScreenshotManagerAshTest,
       DoNotTakeScreenshotIfDisabledByPolicy) {
  ash::Shell::Get()->local_state()->SetBoolean(
      policy::policy_prefs::kDisableScreenshots, true);

  base::test::TestFuture<bool> future;
  auto* manager = OsFeedbackScreenshotManager::GetInstance();
  manager->TakeScreenshot(future.GetCallback());

  EXPECT_FALSE(future.Get());
  EXPECT_EQ(nullptr, manager->GetScreenshotData());
  EXPECT_FALSE(policy::DlpContentObserver::HasInstance());
}

// Test that TakeScreenshot will skip if restricted by DLP.
TEST_F(OsFeedbackScreenshotManagerAshTest,
       DoNotTakeScreenshotIfRestrictedByDlp) {
  policy::DlpContentManagerTestHelper dlp_helper;
  ScopedRestrictedWebContents web_contents = CreateRestrictedWebContents(
      dlp_helper, GURL(kExampleUrl), policy::DlpRulesManager::Level::kBlock);

  base::test::TestFuture<bool> future;
  auto* manager = OsFeedbackScreenshotManager::GetInstance();
  manager->TakeScreenshot(future.GetCallback());

  EXPECT_FALSE(future.Get());
  EXPECT_EQ(nullptr, manager->GetScreenshotData());
}

// Test that TakeScreenshot succeeds and notifies OnImageCapture when the user
// proceeds past an async DLP warning dialog.
TEST_F(OsFeedbackScreenshotManagerAshTest, TakeScreenshotWarnProceed) {
  auto* mock_rules_manager = SetUpMockDlpRulesManager();
  ASSERT_TRUE(mock_rules_manager);
  policy::DlpContentManagerTestHelper dlp_helper;

  policy::WarningCallback warn_callback;
  auto* warn_notifier = InstallPendingWarnNotifier(dlp_helper, &warn_callback);
  EXPECT_CALL(*warn_notifier, ShowDlpWarningDialog).Times(1);
  // Called once by CheckScreenshotRestriction() for the warning event and a
  // second time by OnImageCapture() for the warning-proceeded event.
  EXPECT_CALL(
      *mock_rules_manager,
      GetSourceUrlPattern(_, policy::DlpRulesManager::Restriction::kScreenshot,
                          policy::DlpRulesManager::Level::kWarn, _))
      .Times(2)
      .WillRepeatedly(Return("example.com"));

  ScopedRestrictedWebContents web_contents = CreateRestrictedWebContents(
      dlp_helper, GURL(kExampleUrl), policy::DlpRulesManager::Level::kWarn);

  base::test::TestFuture<bool> future;
  auto* manager = OsFeedbackScreenshotManager::GetInstance();
  manager->TakeScreenshot(future.GetCallback());

  ASSERT_TRUE(warn_callback);
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(nullptr, manager->GetScreenshotData());

  std::move(warn_callback).Run(/*should_proceed=*/true);

  EXPECT_TRUE(future.Get());
  EXPECT_NE(nullptr, manager->GetScreenshotData());
}

// Test that TakeScreenshot aborts and does not call OnImageCapture when the
// user cancels an async DLP warning dialog.
TEST_F(OsFeedbackScreenshotManagerAshTest, TakeScreenshotWarnCancel) {
  auto* mock_rules_manager = SetUpMockDlpRulesManager();
  ASSERT_TRUE(mock_rules_manager);
  policy::DlpContentManagerTestHelper dlp_helper;

  policy::WarningCallback warn_callback;
  auto* warn_notifier = InstallPendingWarnNotifier(dlp_helper, &warn_callback);
  EXPECT_CALL(*warn_notifier, ShowDlpWarningDialog).Times(1);
  // Called only once by CheckScreenshotRestriction(); OnImageCapture() must not
  // be called when the user cancels.
  EXPECT_CALL(
      *mock_rules_manager,
      GetSourceUrlPattern(_, policy::DlpRulesManager::Restriction::kScreenshot,
                          policy::DlpRulesManager::Level::kWarn, _))
      .Times(1)
      .WillOnce(Return("example.com"));

  ScopedRestrictedWebContents web_contents = CreateRestrictedWebContents(
      dlp_helper, GURL(kExampleUrl), policy::DlpRulesManager::Level::kWarn);

  base::test::TestFuture<bool> future;
  auto* manager = OsFeedbackScreenshotManager::GetInstance();
  manager->TakeScreenshot(future.GetCallback());

  ASSERT_TRUE(warn_callback);
  EXPECT_FALSE(future.IsReady());

  std::move(warn_callback).Run(/*should_proceed=*/false);

  EXPECT_FALSE(future.Get());
  EXPECT_EQ(nullptr, manager->GetScreenshotData());
}

// Test that TakeScreenshot aborts cleanly if DisableScreenshots becomes true
// while an async DLP warning dialog is pending.
TEST_F(OsFeedbackScreenshotManagerAshTest,
       TakeScreenshotAbortsWhenPolicyDisabledDuringPendingWarn) {
  policy::DlpContentManagerTestHelper dlp_helper;

  policy::WarningCallback warn_callback;
  auto* warn_notifier = InstallPendingWarnNotifier(dlp_helper, &warn_callback);
  EXPECT_CALL(*warn_notifier, ShowDlpWarningDialog).Times(1);

  ScopedRestrictedWebContents web_contents = CreateRestrictedWebContents(
      dlp_helper, GURL(kExampleUrl), policy::DlpRulesManager::Level::kWarn);

  base::test::TestFuture<bool> future;
  auto* manager = OsFeedbackScreenshotManager::GetInstance();
  manager->TakeScreenshot(future.GetCallback());

  ASSERT_TRUE(warn_callback);
  EXPECT_FALSE(future.IsReady());

  ash::Shell::Get()->local_state()->SetBoolean(
      policy::policy_prefs::kDisableScreenshots, true);
  std::move(warn_callback).Run(/*should_proceed=*/true);

  EXPECT_FALSE(future.Get());
  EXPECT_EQ(nullptr, manager->GetScreenshotData());
}

// Test that TakeScreenshot aborts cleanly without hitting OnImageCapture's
// CHECK(!IsBlocked) if a kBlock window appears while a DLP warning is pending.
TEST_F(OsFeedbackScreenshotManagerAshTest,
       TakeScreenshotAbortsWhenBlockedWindowAppearsDuringPendingWarn) {
  policy::DlpContentManagerTestHelper dlp_helper;

  policy::WarningCallback warn_callback;
  auto* warn_notifier = InstallPendingWarnNotifier(dlp_helper, &warn_callback);
  EXPECT_CALL(*warn_notifier, ShowDlpWarningDialog).Times(1);

  ScopedRestrictedWebContents warn_contents = CreateRestrictedWebContents(
      dlp_helper, GURL(kExampleUrl), policy::DlpRulesManager::Level::kWarn);

  base::test::TestFuture<bool> future;
  auto* manager = OsFeedbackScreenshotManager::GetInstance();
  manager->TakeScreenshot(future.GetCallback());

  ASSERT_TRUE(warn_callback);
  EXPECT_FALSE(future.IsReady());

  ScopedRestrictedWebContents block_contents = CreateRestrictedWebContents(
      dlp_helper, GURL(kBlockedUrl), policy::DlpRulesManager::Level::kBlock);
  std::move(warn_callback).Run(/*should_proceed=*/true);

  EXPECT_FALSE(future.Get());
  EXPECT_EQ(nullptr, manager->GetScreenshotData());
}

// Test that concurrent TakeScreenshot calls while an async DLP warning check is
// in flight are coalesced without spawning duplicate warning dialogs.
TEST_F(OsFeedbackScreenshotManagerAshTest,
       CoalescesConcurrentTakeScreenshotDuringPendingWarn) {
  policy::DlpContentManagerTestHelper dlp_helper;

  policy::WarningCallback warn_callback;
  auto* warn_notifier = InstallPendingWarnNotifier(dlp_helper, &warn_callback);
  EXPECT_CALL(*warn_notifier, ShowDlpWarningDialog).Times(1);

  ScopedRestrictedWebContents web_contents = CreateRestrictedWebContents(
      dlp_helper, GURL(kExampleUrl), policy::DlpRulesManager::Level::kWarn);

  base::test::TestFuture<bool> future1;
  base::test::TestFuture<bool> future2;
  auto* manager = OsFeedbackScreenshotManager::GetInstance();
  manager->TakeScreenshot(future1.GetCallback());
  manager->TakeScreenshot(future2.GetCallback());

  ASSERT_TRUE(warn_callback);
  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());

  std::move(warn_callback).Run(/*should_proceed=*/true);

  EXPECT_TRUE(future1.Get());
  EXPECT_TRUE(future2.Get());
  EXPECT_NE(nullptr, manager->GetScreenshotData());
}

}  // namespace ash
