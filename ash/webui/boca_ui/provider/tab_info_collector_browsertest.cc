// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/provider/tab_info_collector.h"

#include <memory>

#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "partition_alloc/pointers/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;
using ::testing::StrictMock;

namespace {
constexpr char kTabUrl1[] = "https://foo/1";
constexpr char kTabUrl2[] = "https://foo/2";
constexpr char kTabUrl3[] = "https://foo/3";
constexpr char kTabUrl4[] = "https://foo/4";

constexpr char kDataUrl[] = "url:image";

constexpr char kDefaultTitle[] = "foo";

}  // namespace

namespace ash::boca {

class MockImageGenerator : public TabInfoCollector::ImageGenerator {
 public:
  MockImageGenerator() = default;
  MOCK_METHOD(std::string, StringifyImage, (ui::ImageModel));
};

class TabInfoCollectorTest : public InProcessBrowserTest {
 public:
  TabInfoCollectorTest() = default;
  TabInfoCollectorTest(const TabInfoCollectorTest&) = delete;
  TabInfoCollectorTest operator=(const TabInfoCollectorTest&) = delete;
  ~TabInfoCollectorTest() override = default;

  Browser* CreateBrowser(const std::vector<GURL>& urls) {
    Browser::CreateParams params(Browser::TYPE_NORMAL,
                                 ProfileManager::GetActiveUserProfile(),
                                 /*user_gesture=*/false);
    Browser* browser = Browser::Create(params);
    // Create a new tab and make sure the urls have loaded.
    for (const auto& url : urls) {
      ui_test_utils::NavigateToURLWithDisposition(
          browser, url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    }
    return browser;
  }

  void SetUp() override {
    auto mock = std::make_unique<StrictMock<MockImageGenerator>>();
    image_generator_ = mock.get();
    tab_info_collector_ = std::make_unique<TabInfoCollector>(std::move(mock));
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    image_generator_ = nullptr;
    tab_info_collector_.reset();
  }

  TabInfoCollector* tabInfoCollector() { return tab_info_collector_.get(); }
  MockImageGenerator* imageGenerator() { return image_generator_.get(); }

 private:
  raw_ptr<StrictMock<MockImageGenerator>> image_generator_;
  std::unique_ptr<TabInfoCollector> tab_info_collector_;
};

IN_PROC_BROWSER_TEST_F(TabInfoCollectorTest, GetTabListForNonEmptyWindow) {
  ASSERT_EQ(1u, chrome::GetTotalBrowserCount());

  // Create browser 1 and navigate to url1 and then url2
  CreateBrowser({GURL(kTabUrl1), GURL(kTabUrl2)});

  // Create browser 2 and navigate to url3
  CreateBrowser({GURL(kTabUrl3)});

  // Create browser 3 and navigate to url4
  CreateBrowser({GURL(kTabUrl4)});
  EXPECT_CALL(*imageGenerator(), StringifyImage(_))
      .WillRepeatedly(Return(kDataUrl));
  base::test::TestFuture<std::vector<mojom::WindowPtr>> future;
  tabInfoCollector()->GetWindowTabInfo(future.GetCallback());
  auto window_list = future.Take();

  // Start with 1 existing window.
  ASSERT_EQ(4u, window_list.size());

  // Verify window is listed in non-ascending order based on last access time.
  ASSERT_EQ(1u, window_list[0]->tab_list.size());
  ASSERT_EQ(1u, window_list[1]->tab_list.size());
  ASSERT_EQ(2u, window_list[2]->tab_list.size());
  // Preexisting window.
  ASSERT_EQ(1u, window_list[3]->tab_list.size());

  // Verify tab is listed in non-ascending order inside window based on last
  // access time.
  EXPECT_EQ(kTabUrl2, window_list[2]->tab_list[0]->url);
  EXPECT_EQ(kTabUrl1, window_list[2]->tab_list[1]->url);

  EXPECT_EQ(kTabUrl4, window_list[0]->tab_list[0]->url);
  EXPECT_EQ(kTabUrl3, window_list[1]->tab_list[0]->url);

  // Verify image data url is generated
  EXPECT_EQ(kDataUrl, window_list[2]->tab_list[0]->favicon);
  EXPECT_EQ(kDataUrl, window_list[2]->tab_list[1]->favicon);
  EXPECT_EQ(kDataUrl, window_list[0]->tab_list[0]->favicon);
  EXPECT_EQ(kDataUrl, window_list[1]->tab_list[0]->favicon);

  // Verify title is set
  EXPECT_EQ(kDefaultTitle, window_list[2]->tab_list[0]->title);
  EXPECT_EQ(kDefaultTitle, window_list[2]->tab_list[1]->title);
  EXPECT_EQ(kDefaultTitle, window_list[0]->tab_list[0]->title);
  EXPECT_EQ(kDefaultTitle, window_list[1]->tab_list[0]->title);
}

IN_PROC_BROWSER_TEST_F(TabInfoCollectorTest, GetTabListForEmptyWindow) {
  // Close the browser and verify that all browser windows are closed.
  CloseBrowserSynchronously(browser());
  ASSERT_EQ(0u, chrome::GetTotalBrowserCount());

  EXPECT_CALL(*imageGenerator(), StringifyImage(_)).Times(0);

  base::test::TestFuture<std::vector<mojom::WindowPtr>> future;
  tabInfoCollector()->GetWindowTabInfo(future.GetCallback());
  auto window_list = future.Take();

  EXPECT_EQ(0u, window_list.size());
}
}  // namespace ash::boca
