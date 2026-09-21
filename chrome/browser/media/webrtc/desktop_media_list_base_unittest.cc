// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/desktop_media_list_base.h"

#include "base/time/time.h"
#include "chrome/browser/media/webrtc/tab_desktop_media_list_mock_observer.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestDesktopMediaListBase : public DesktopMediaListBase {
 public:
  explicit TestDesktopMediaListBase(DesktopMediaListObserver* observer)
      : DesktopMediaListBase(base::Milliseconds(100), observer) {}
  ~TestDesktopMediaListBase() override = default;

  using DesktopMediaListBase::SourceDescription;
  using DesktopMediaListBase::UpdateSourcesList;

  void Refresh(bool update_thumbnails) override {}
};

}  // namespace

class DesktopMediaListBaseTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(DesktopMediaListBaseTest,
       UpdateSourcesListPropagatesSharingBlockedAndNotifiesObserver) {
  testing::StrictMock<DesktopMediaListMockObserver> observer;
  TestDesktopMediaListBase media_list(&observer);

  content::DesktopMediaID id(content::DesktopMediaID::TYPE_WEB_CONTENTS, 1);

  EXPECT_CALL(observer, OnSourceAdded(0));
  media_list.UpdateSourcesList(
      {TestDesktopMediaListBase::SourceDescription(id, u"Tab 1", false)});
  ASSERT_EQ(media_list.GetSourceCount(), 1);
  EXPECT_FALSE(media_list.GetSource(0).is_sharing_blocked);

  // Flipping is_sharing_blocked preserves source identity (does not remove/add)
  // and notifies OnSourceThumbnailChanged.
  EXPECT_CALL(observer, OnSourceThumbnailChanged(0));
  media_list.UpdateSourcesList(
      {TestDesktopMediaListBase::SourceDescription(id, u"Tab 1", true)});
  ASSERT_EQ(media_list.GetSourceCount(), 1);
  EXPECT_TRUE(media_list.GetSource(0).is_sharing_blocked);
}
