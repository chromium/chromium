// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/dlp_ash.h"

#include "ash/test/ash_test_base.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager_factory.h"
#include "chrome/browser/ash/policy/dlp/test/mock_files_policy_notification_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_observer.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_content_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

namespace crosapi {

namespace {

const std::u16string kAppId = u"app_id";
constexpr char kScreenShareLabel[] = "label";

constexpr char kFilePath[] = "test.txt";

class MockStateChangeDelegate : public mojom::StateChangeDelegate {
 public:
  MOCK_METHOD(void, OnPause, (), (override));
  MOCK_METHOD(void, OnResume, (), (override));
  MOCK_METHOD(void, OnStop, (), (override));

  mojo::PendingRemote<mojom::StateChangeDelegate> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  mojo::Receiver<mojom::StateChangeDelegate> receiver_{this};
};

}  // namespace

class DlpAshTest : public ash::AshTestBase {
 public:
  explicit DlpAshTest(
      std::unique_ptr<base::test::TaskEnvironment> task_environment)
      : ash::AshTestBase(std::move(task_environment)) {}
  DlpAshTest() {}

  DlpAsh* dlp_ash() { return &dlp_ash_; }

 private:
  DlpAsh dlp_ash_;
};

TEST_F(DlpAshTest, CheckScreenShareRestrictionRootWindowAllowed) {
  testing::StrictMock<policy::MockDlpContentManager> mock_dlp_content_manager;
  policy::ScopedDlpContentObserverForTesting scoped_dlp_content_observer(
      &mock_dlp_content_manager);
  EXPECT_CALL(mock_dlp_content_manager, CheckScreenShareRestriction)
      .WillOnce([](const content::DesktopMediaID& media_id,
                   const std::u16string& application_title,
                   base::OnceCallback<void(bool)> callback) {
        EXPECT_EQ(content::DesktopMediaID::TYPE_SCREEN, media_id.type);
        EXPECT_EQ(kAppId, application_title);
        std::move(callback).Run(/*should_proceed=*/true);
      });

  mojom::ScreenShareAreaPtr area = mojom::ScreenShareArea::New();
  base::test::TestFuture<bool> is_allowed_response;
  dlp_ash()->CheckScreenShareRestriction(std::move(area), kAppId,
                                         is_allowed_response.GetCallback());
  EXPECT_TRUE(is_allowed_response.Take());
}

TEST_F(DlpAshTest, CheckScreenShareRestrictionRootWindowNotAllowed) {
  testing::StrictMock<policy::MockDlpContentManager> mock_dlp_content_manager;
  policy::ScopedDlpContentObserverForTesting scoped_dlp_content_observer(
      &mock_dlp_content_manager);
  EXPECT_CALL(mock_dlp_content_manager, CheckScreenShareRestriction)
      .WillOnce([](const content::DesktopMediaID& media_id,
                   const std::u16string& application_title,
                   base::OnceCallback<void(bool)> callback) {
        EXPECT_EQ(content::DesktopMediaID::TYPE_SCREEN, media_id.type);
        EXPECT_EQ(kAppId, application_title);
        std::move(callback).Run(/*should_proceed=*/false);
      });

  mojom::ScreenShareAreaPtr area = mojom::ScreenShareArea::New();
  base::test::TestFuture<bool> future;
  dlp_ash()->CheckScreenShareRestriction(std::move(area), kAppId,
                                         future.GetCallback());
  EXPECT_FALSE(future.Take());
}

TEST_F(DlpAshTest, CheckScreenShareRestrictionInvalidWindow) {
  testing::StrictMock<policy::MockDlpContentManager> mock_dlp_content_manager;
  policy::ScopedDlpContentObserverForTesting scoped_dlp_content_observer(
      &mock_dlp_content_manager);

  mojom::ScreenShareAreaPtr area = mojom::ScreenShareArea::New();
  area->window_id = "id";
  base::test::TestFuture<bool> future;
  dlp_ash()->CheckScreenShareRestriction(std::move(area), kAppId,
                                         future.GetCallback());
  EXPECT_TRUE(future.Take());
}

TEST_F(DlpAshTest, ScreenShareStarted) {
  testing::StrictMock<MockStateChangeDelegate> delegate;
  testing::StrictMock<policy::MockDlpContentManager> mock_dlp_content_manager;
  policy::ScopedDlpContentObserverForTesting scoped_dlp_content_observer(
      &mock_dlp_content_manager);

  base::RepeatingClosure stop_callback;
  content::MediaStreamUI::StateChangeCallback state_change_callback;
  content::DesktopMediaID media_id;

  EXPECT_CALL(mock_dlp_content_manager, OnScreenShareStarted)
      .WillOnce([&](const std::string& label,
                    std::vector<content::DesktopMediaID> ids,
                    const std::u16string& application_title,
                    base::RepeatingClosure stop_cb,
                    content::MediaStreamUI::StateChangeCallback state_change_cb,
                    content::MediaStreamUI::SourceCallback source_cb) {
        EXPECT_EQ(kScreenShareLabel, label);
        EXPECT_EQ(1u, ids.size());
        EXPECT_EQ(kAppId, application_title);
        stop_callback = std::move(stop_cb);
        state_change_callback = std::move(state_change_cb);
        media_id = ids[0];
        EXPECT_EQ(content::DesktopMediaID::TYPE_SCREEN, media_id.type);
      });

  mojom::ScreenShareAreaPtr area = mojom::ScreenShareArea::New();

  dlp_ash()->OnScreenShareStarted(kScreenShareLabel, std::move(area), kAppId,
                                  delegate.BindAndGetRemote());

  EXPECT_CALL(delegate, OnPause).Times(1);
  state_change_callback.Run(media_id,
                            blink::mojom::MediaStreamStateChange::PAUSE);

  EXPECT_CALL(delegate, OnResume).Times(1);
  state_change_callback.Run(media_id,
                            blink::mojom::MediaStreamStateChange::PLAY);

  EXPECT_CALL(delegate, OnStop).Times(1);
  stop_callback.Run();

  delegate.receiver_.FlushForTesting();
}

TEST_F(DlpAshTest, ScreenShareStartedInvalidWindow) {
  testing::StrictMock<MockStateChangeDelegate> delegate;
  testing::StrictMock<policy::MockDlpContentManager> mock_dlp_content_manager;
  policy::ScopedDlpContentObserverForTesting scoped_dlp_content_observer(
      &mock_dlp_content_manager);

  mojom::ScreenShareAreaPtr area = mojom::ScreenShareArea::New();
  area->window_id = "id";
  dlp_ash()->OnScreenShareStarted(kScreenShareLabel, std::move(area), kAppId,
                                  delegate.BindAndGetRemote());

  delegate.receiver_.FlushForTesting();
}

TEST_F(DlpAshTest, ScreenShareStopped) {
  testing::StrictMock<policy::MockDlpContentManager> mock_dlp_content_manager;
  policy::ScopedDlpContentObserverForTesting scoped_dlp_content_observer(
      &mock_dlp_content_manager);

  EXPECT_CALL(mock_dlp_content_manager, OnScreenShareStopped)
      .WillOnce([&](const std::string& label,
                    const content::DesktopMediaID& media_id) {
        EXPECT_EQ(kScreenShareLabel, label);
        EXPECT_EQ(content::DesktopMediaID::TYPE_SCREEN, media_id.type);
      });

  mojom::ScreenShareAreaPtr area = mojom::ScreenShareArea::New();
  dlp_ash()->OnScreenShareStopped(kScreenShareLabel, std::move(area));
}

TEST_F(DlpAshTest, ScreenShareStoppedInvalidWindow) {
  testing::StrictMock<policy::MockDlpContentManager> mock_dlp_content_manager;
  policy::ScopedDlpContentObserverForTesting scoped_dlp_content_observer(
      &mock_dlp_content_manager);

  mojom::ScreenShareAreaPtr area = mojom::ScreenShareArea::New();
  area->window_id = "id";
  dlp_ash()->OnScreenShareStopped(kScreenShareLabel, std::move(area));
}

class DlpAshBlockUITest
    : public DlpAshTest,
      public ::testing::WithParamInterface<
          std::pair<mojom::FileAction, policy::dlp::FileAction>> {
 public:
  DlpAshBlockUITest()
      : DlpAshTest(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::test::TaskEnvironment::TimeSource::MOCK_TIME))) {}

  void SetUp() override {
    DlpAshTest::SetUp();

    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    scoped_profile_ = std::make_unique<TestingProfile>();
    profile_ = scoped_profile_.get();
    AccountId account_id =
        AccountId::FromUserEmailGaiaId("test@example.com", "12345");
    profile_->SetIsNewProfile(true);
    user_manager::User* user =
        user_manager->AddUserWithAffiliationAndTypeAndProfile(
            account_id,
            /*is_affiliated=*/false, user_manager::UserType::kRegular,
            profile_);
    user_manager->UserLoggedIn(account_id, user->username_hash(),
                               /*browser_restart=*/false,
                               /*is_child=*/false);
    user_manager->SimulateUserProfileLoad(account_id);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    // Set FilesPolicyNotificationManager.
    policy::FilesPolicyNotificationManagerFactory::GetInstance()
        ->SetTestingFactory(
            profile_.get(),
            base::BindRepeating(
                &DlpAshBlockUITest::SetFilesPolicyNotificationManager,
                base::Unretained(this)));

    ASSERT_TRUE(
        policy::FilesPolicyNotificationManagerFactory::GetForBrowserContext(
            profile_.get()));
    ASSERT_TRUE(fpnm_);
  }

 protected:
  raw_ptr<policy::MockFilesPolicyNotificationManager,
          DisableDanglingPtrDetection>
      fpnm_ = nullptr;

 private:
  std::unique_ptr<KeyedService> SetFilesPolicyNotificationManager(
      content::BrowserContext* context) {
    auto fpnm = std::make_unique<
        testing::StrictMock<policy::MockFilesPolicyNotificationManager>>(
        profile_.get());
    fpnm_ = fpnm.get();

    return fpnm;
  }

  std::unique_ptr<TestingProfile> scoped_profile_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  raw_ptr<TestingProfile> profile_;
};

INSTANTIATE_TEST_SUITE_P(
    DlpAshBlockUI,
    DlpAshBlockUITest,
    ::testing::Values(std::make_tuple(mojom::FileAction::kUnknown,
                                      policy::dlp::FileAction::kUnknown),
                      std::make_tuple(mojom::FileAction::kDownload,
                                      policy::dlp::FileAction::kDownload),
                      std::make_tuple(mojom::FileAction::kTransfer,
                                      policy::dlp::FileAction::kTransfer),
                      std::make_tuple(mojom::FileAction::kUpload,
                                      policy::dlp::FileAction::kUpload),
                      std::make_tuple(mojom::FileAction::kCopy,
                                      policy::dlp::FileAction::kCopy),
                      std::make_tuple(mojom::FileAction::kMove,
                                      policy::dlp::FileAction::kMove),
                      std::make_tuple(mojom::FileAction::kOpen,
                                      policy::dlp::FileAction::kOpen),
                      std::make_tuple(mojom::FileAction::kShare,
                                      policy::dlp::FileAction::kShare)));

TEST_P(DlpAshBlockUITest, ShowBlockedFiles) {
  auto [mojo_action, dlp_action] = GetParam();

  std::optional<uint64_t> task_id = std::nullopt;
  base::FilePath path(kFilePath);

  EXPECT_CALL(*fpnm_,
              ShowDlpBlockedFiles(task_id, std::vector<base::FilePath>{path},
                                  dlp_action));

  dlp_ash()->ShowBlockedFiles(task_id, {path}, mojo_action);
}

}  // namespace crosapi
