// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_stream_focus_delegate.h"

#include <memory>

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Return;

namespace {

const content::DesktopMediaID kDesktopMediaIDWindow =
    content::DesktopMediaID(content::DesktopMediaID::TYPE_WINDOW,
                            content::DesktopMediaID::kNullId);

class MockWindowCapturer : public webrtc::DesktopCapturer {
 public:
  MockWindowCapturer() = default;
  ~MockWindowCapturer() override = default;

  MOCK_METHOD0(FocusOnSelectedSource, bool());
  MOCK_METHOD1(SelectSource, bool(webrtc::DesktopCapturer::SourceId));

  void Start(Callback* callback) override {}
  void CaptureFrame() override {}
};

}  // namespace

class MediaStreamFocusDelegateTest : public BrowserWithTestWindowTest {
 public:
  ~MediaStreamFocusDelegateTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    // Tabs are added at index 0, so add them in reverse order,
    // leading to a simple mapping of tab-N pointing at foo/N.
    AddTab(browser(), GURL("http://foo/2"));
    AddTab(browser(), GURL("http://foo/1"));
    AddTab(browser(), GURL("http://foo/0"));

    delegate_ = std::make_unique<MediaStreamFocusDelegate>(
        browser()->tab_strip_model()->GetWebContentsAt(0));
  }

  content::DesktopMediaID DesktopMediaIDForTabAt(int index) const {
    content::WebContents* const tab =
        browser()->tab_strip_model()->GetWebContentsAt(index);
    return content::DesktopMediaID(
        content::DesktopMediaID::TYPE_WEB_CONTENTS,
        content::DesktopMediaID::kNullId,
        content::WebContentsMediaCaptureId(
            tab->GetPrimaryMainFrame()->GetProcess()->GetID(),
            tab->GetPrimaryMainFrame()->GetRoutingID()));
  }

  void SetFocus(const content::DesktopMediaID& media_id,
                bool focus,
                bool is_from_microtask = false,
                bool is_from_timer = false) {
    delegate_->SetFocus(media_id, focus, is_from_microtask, is_from_timer);
  }

  void SetWindowCapturer(MockWindowCapturer* window_capturer) {
    delegate_->SetWindowCapturerForTesting(
        base::WrapUnique<MockWindowCapturer>(window_capturer));
  }

 protected:
  std::unique_ptr<MediaStreamFocusDelegate> delegate_;  // Unit-under-test.
};

TEST_F(MediaStreamFocusDelegateTest, FirstSetFocusTrueFocusesTab) {
  ASSERT_EQ(browser()->tab_strip_model()->active_index(), 0);
  SetFocus(DesktopMediaIDForTabAt(1), true);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 1);
}

TEST_F(MediaStreamFocusDelegateTest, SecondSetFocusTrueHasNoEffect) {
  // Setup - repeated from FirstSetFocusTrueFocusesTab, but as an assumption.
  ASSERT_EQ(browser()->tab_strip_model()->active_index(), 0);
  SetFocus(DesktopMediaIDForTabAt(1), true);
  ASSERT_EQ(browser()->tab_strip_model()->active_index(), 1);

  // Test - focus unchanged.
  SetFocus(DesktopMediaIDForTabAt(0), true);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 1);
}

TEST_F(MediaStreamFocusDelegateTest, SetFocusFalseClosesFocusWindow) {
  ASSERT_EQ(browser()->tab_strip_model()->active_index(), 0);

  // Calling SetFocus(false) does not change focus.
  SetFocus(DesktopMediaIDForTabAt(1), false);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 0);

  // Focus can no longer change by new calls to SetFocus(true).
  SetFocus(DesktopMediaIDForTabAt(1), true);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 0);
}

TEST_F(MediaStreamFocusDelegateTest, ChangeOfTabClosesFocusWindow) {
  // Setup.
  ASSERT_EQ(browser()->tab_strip_model()->active_index(), 0);
  browser()->tab_strip_model()->ActivateTabAt(2);
  ASSERT_EQ(browser()->tab_strip_model()->active_index(), 2);

  // Test - SetFocus has had no effect.
  SetFocus(DesktopMediaIDForTabAt(1), true);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 2);
}

TEST_F(MediaStreamFocusDelegateTest, SelectedSourceFocusesWindow) {
  // Setup.
  raw_ptr<MockWindowCapturer, DanglingUntriaged> window_capturer =
      new MockWindowCapturer();
  SetWindowCapturer(window_capturer);

  ON_CALL(*window_capturer, SelectSource(_)).WillByDefault(Return(true));
  EXPECT_CALL(*window_capturer, SelectSource(_)).Times(1);
  EXPECT_CALL(*window_capturer, FocusOnSelectedSource()).Times(1);
  SetFocus(kDesktopMediaIDWindow, true);
}

TEST_F(MediaStreamFocusDelegateTest, NotSelectedSourceDoesNotFocusWindow) {
  // Setup.
  raw_ptr<MockWindowCapturer, DanglingUntriaged> window_capturer =
      new MockWindowCapturer();
  SetWindowCapturer(window_capturer);

  ON_CALL(*window_capturer, SelectSource(_)).WillByDefault(Return(false));
  EXPECT_CALL(*window_capturer, SelectSource(_)).Times(1);
  EXPECT_CALL(*window_capturer, FocusOnSelectedSource()).Times(0);
  SetFocus(kDesktopMediaIDWindow, true);
}
