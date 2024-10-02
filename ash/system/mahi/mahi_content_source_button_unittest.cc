// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_content_source_button.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/test/mock_mahi_manager.h"
#include "ash/system/mahi/test/mock_mahi_media_app_content_manager.h"
#include "ash/test/ash_test_base.h"
#include "base/unguessable_token.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {

namespace {

using ::testing::_;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;

class MockNewWindowDelegate : public NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};

class MahiContentSourceButtonTest : public AshTestBase {
 public:
  MahiContentSourceButtonTest() = default;

  ~MahiContentSourceButtonTest() override = default;

  MockMahiManager& mock_mahi_manager() { return mock_mahi_manager_; }

  MockMahiMediaAppContentManager& mock_mahi_media_app_content_manager() {
    return mock_mahi_media_app_content_manager_;
  }

  MockNewWindowDelegate& GetMockNewWindowDelegate() {
    return new_window_delegate_;
  }

 private:
  NiceMock<MockMahiManager> mock_mahi_manager_;
  chromeos::ScopedMahiManagerSetter scoped_mahi_manager_setter_{
      &mock_mahi_manager_};

  NiceMock<MockMahiMediaAppContentManager> mock_mahi_media_app_content_manager_;
  chromeos::ScopedMahiMediaAppContentManagerSetter
      scoped_mahi_media_app_content_manager_{
          &mock_mahi_media_app_content_manager_};

  MockNewWindowDelegate new_window_delegate_;
};

TEST_F(MahiContentSourceButtonTest, InitialContentSourceTitle) {
  const std::u16string kTestTitle(u"Initial content title");
  ON_CALL(mock_mahi_manager(), GetContentTitle)
      .WillByDefault(Return(kTestTitle));
  MahiContentSourceButton content_source_button;

  EXPECT_EQ(content_source_button.GetText(), kTestTitle);
}

TEST_F(MahiContentSourceButtonTest, ContentSourceTitleAfterRefresh) {
  ON_CALL(mock_mahi_manager(), GetContentTitle)
      .WillByDefault(Return(u"Initial content title"));
  MahiContentSourceButton content_source_button;

  const std::u16string kRefreshedTitle(u"Refreshed content title");
  ON_CALL(mock_mahi_manager(), GetContentTitle)
      .WillByDefault(Return(kRefreshedTitle));
  content_source_button.RefreshContentSourceInfo();

  EXPECT_EQ(content_source_button.GetText(), kRefreshedTitle);
}

TEST_F(MahiContentSourceButtonTest, InitialContentSourceIcon) {
  const auto kInitialIcon =
      gfx::test::CreateImageSkia(/*size=*/128, SK_ColorRED);
  ON_CALL(mock_mahi_manager(), GetContentIcon)
      .WillByDefault(Return(kInitialIcon));
  MahiContentSourceButton content_source_button;

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      *content_source_button.GetImage(views::Button::STATE_NORMAL).bitmap(),
      *image_util::ResizeAndCropImage(kInitialIcon,
                                      mahi_constants::kContentIconSize)
           .bitmap()));
}

TEST_F(MahiContentSourceButtonTest, ContentSourceIconAfterRefresh) {
  ON_CALL(mock_mahi_manager(), GetContentIcon)
      .WillByDefault(
          Return(gfx::test::CreateImageSkia(/*size=*/128, SK_ColorRED)));
  MahiContentSourceButton content_source_button;

  const auto kRefreshedIcon =
      gfx::test::CreateImageSkia(/*size=*/128, SK_ColorBLUE);
  ON_CALL(mock_mahi_manager(), GetContentIcon)
      .WillByDefault(Return(kRefreshedIcon));
  content_source_button.RefreshContentSourceInfo();

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      *content_source_button.GetImage(views::Button::STATE_NORMAL).bitmap(),
      *image_util::ResizeAndCropImage(kRefreshedIcon,
                                      mahi_constants::kContentIconSize)
           .bitmap()));
}

TEST_F(MahiContentSourceButtonTest, InitialContentSourceButtonUrl) {
  const GURL kInitialUrl("https://www.google.com");
  ON_CALL(mock_mahi_manager(), GetContentUrl)
      .WillByDefault(Return(kInitialUrl));
  auto widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  auto* content_source_button =
      widget->SetContentsView(std::make_unique<MahiContentSourceButton>());

  EXPECT_CALL(
      GetMockNewWindowDelegate(),
      OpenUrl(kInitialUrl, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
              NewWindowDelegate::Disposition::kSwitchToTab));
  LeftClickOn(content_source_button);
  Mock::VerifyAndClearExpectations(&GetMockNewWindowDelegate());
}

TEST_F(MahiContentSourceButtonTest, ContentSourceButtonUrlAfterRefresh) {
  ON_CALL(mock_mahi_manager(), GetContentUrl)
      .WillByDefault(Return(GURL("https://www.google.com")));
  auto widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  auto* content_source_button =
      widget->SetContentsView(std::make_unique<MahiContentSourceButton>());

  const GURL kRefreshedUrl("https://en.wikipedia.org");
  ON_CALL(mock_mahi_manager(), GetContentUrl)
      .WillByDefault(Return(kRefreshedUrl));
  content_source_button->RefreshContentSourceInfo();

  EXPECT_CALL(
      GetMockNewWindowDelegate(),
      OpenUrl(kRefreshedUrl, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
              NewWindowDelegate::Disposition::kSwitchToTab));
  LeftClickOn(content_source_button);
  Mock::VerifyAndClearExpectations(&GetMockNewWindowDelegate());
}

TEST_F(MahiContentSourceButtonTest, ContentSourceButtonActivateMediaAppWindow) {
  ON_CALL(mock_mahi_manager(), GetContentUrl)
      .WillByDefault(Return(GURL("https://www.google.com")));
  const base::UnguessableToken media_app_client_id =
      base::UnguessableToken::Create();
  ON_CALL(mock_mahi_manager(), GetMediaAppPDFClientId)
      .WillByDefault(Return(std::make_optional(media_app_client_id)));
  auto widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  auto* content_source_button =
      widget->SetContentsView(std::make_unique<MahiContentSourceButton>());

  EXPECT_CALL(mock_mahi_media_app_content_manager(),
              ActivateClientWindow(media_app_client_id))
      .Times(1);
  EXPECT_CALL(GetMockNewWindowDelegate(), OpenUrl(_, _, _)).Times(0);
  LeftClickOn(content_source_button);
  Mock::VerifyAndClearExpectations(&GetMockNewWindowDelegate());
}

}  // namespace

}  // namespace ash
