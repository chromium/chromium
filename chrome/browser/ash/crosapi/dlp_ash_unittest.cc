// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/dlp_ash.h"

#include "ash/test/ash_test_base.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_observer.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_content_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

namespace crosapi {

namespace {

const std::u16string kAppId = u"app_id";
constexpr char kScreenShareLabel[] = "label";

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
  base::RunLoop run_loop;
  dlp_ash()->CheckScreenShareRestriction(
      std::move(area), kAppId, base::BindLambdaForTesting([&](bool allowed) {
        EXPECT_TRUE(allowed);
        run_loop.Quit();
      }));
  run_loop.Run();
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
  base::RunLoop run_loop;
  dlp_ash()->CheckScreenShareRestriction(
      std::move(area), kAppId, base::BindLambdaForTesting([&](bool allowed) {
        EXPECT_FALSE(allowed);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(DlpAshTest, CheckScreenShareRestrictionInvalidWindow) {
  testing::StrictMock<policy::MockDlpContentManager> mock_dlp_content_manager;
  policy::ScopedDlpContentObserverForTesting scoped_dlp_content_observer(
      &mock_dlp_content_manager);

  mojom::ScreenShareAreaPtr area = mojom::ScreenShareArea::New();
  area->window_id = "id";
  base::RunLoop run_loop;
  dlp_ash()->CheckScreenShareRestriction(
      std::move(area), kAppId, base::BindLambdaForTesting([&](bool allowed) {
        EXPECT_TRUE(allowed);
        run_loop.Quit();
      }));
  run_loop.Run();
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

}  // namespace crosapi
