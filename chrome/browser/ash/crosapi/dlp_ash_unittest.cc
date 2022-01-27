// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/dlp_ash.h"

#include "ash/test/ash_test_base.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash.h"
#include "chrome/browser/ash/policy/dlp/mock_dlp_content_manager_ash.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

namespace crosapi {

namespace {
const std::u16string app_id = u"app_id";
}  // namespace

class DlpAshTest : public ash::AshTestBase {
 public:
  DlpAsh* dlp_ash() { return &dlp_ash_; }

 private:
  DlpAsh dlp_ash_;
};

TEST_F(DlpAshTest, CheckScreenShareRestrictionRootWindowAllowed) {
  testing::StrictMock<policy::MockDlpContentManagerAsh>
      mock_dlp_content_manager;
  policy::ScopedDlpContentManagerAshForTesting scoped_dlp_content_manager(
      &mock_dlp_content_manager);
  EXPECT_CALL(mock_dlp_content_manager, CheckScreenShareRestriction)
      .WillOnce([](const content::DesktopMediaID& media_id,
                   const std::u16string& application_title,
                   base::OnceCallback<void(bool)> callback) {
        EXPECT_EQ(content::DesktopMediaID::TYPE_SCREEN, media_id.type);
        EXPECT_EQ(app_id, application_title);
        std::move(callback).Run(/*should_proceed=*/true);
      });

  mojom::ScreenShareAreaPtr area = mojom::ScreenShareArea::New();
  base::RunLoop run_loop;
  dlp_ash()->CheckScreenShareRestriction(
      std::move(area), app_id, base::BindLambdaForTesting([&](bool allowed) {
        EXPECT_TRUE(allowed);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(DlpAshTest, CheckScreenShareRestrictionRootWindowNotAllowed) {
  testing::StrictMock<policy::MockDlpContentManagerAsh>
      mock_dlp_content_manager;
  policy::ScopedDlpContentManagerAshForTesting scoped_dlp_content_manager(
      &mock_dlp_content_manager);
  EXPECT_CALL(mock_dlp_content_manager, CheckScreenShareRestriction)
      .WillOnce([](const content::DesktopMediaID& media_id,
                   const std::u16string& application_title,
                   base::OnceCallback<void(bool)> callback) {
        EXPECT_EQ(content::DesktopMediaID::TYPE_SCREEN, media_id.type);
        EXPECT_EQ(app_id, application_title);
        std::move(callback).Run(/*should_proceed=*/false);
      });

  mojom::ScreenShareAreaPtr area = mojom::ScreenShareArea::New();
  base::RunLoop run_loop;
  dlp_ash()->CheckScreenShareRestriction(
      std::move(area), app_id, base::BindLambdaForTesting([&](bool allowed) {
        EXPECT_FALSE(allowed);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(DlpAshTest, CheckScreenShareRestrictionInvalidWindow) {
  testing::StrictMock<policy::MockDlpContentManagerAsh>
      mock_dlp_content_manager;
  policy::ScopedDlpContentManagerAshForTesting scoped_dlp_content_manager(
      &mock_dlp_content_manager);

  mojom::ScreenShareAreaPtr area = mojom::ScreenShareArea::New();
  area->window_id = "id";
  base::RunLoop run_loop;
  dlp_ash()->CheckScreenShareRestriction(
      std::move(area), app_id, base::BindLambdaForTesting([&](bool allowed) {
        EXPECT_TRUE(allowed);
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace crosapi
