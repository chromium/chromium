// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_tab_data.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/task/current_thread.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {
namespace {

using ::testing::_;
using ::testing::SaveArg;

// Helper to get the WebContents.
content::WebContents* GetActiveWebContents(Browser* browser) {
  return browser->tab_strip_model()->GetActiveWebContents();
}

class TabUpdateReceiver {
 public:
  base::RepeatingCallback<void(TabDataChange)> GetCallback() {
    return base::BindRepeating(&TabUpdateReceiver::OnUpdate, GetWeakPtr());
  }
  void OnUpdate(TabDataChange update) { updates_.push_back(std::move(update)); }
  const std::vector<TabDataChange>& GetUpdates() const { return updates_; }
  std::vector<TabDataChange> TakeUpdates() { return std::move(updates_); }

  friend std::ostream& operator<<(std::ostream& os,
                                  const TabUpdateReceiver& self) {
    os << "Received tab data list: {";
    for (auto& change : self.updates_) {
      os << change << ", ";
    }
    os << "}";
    return os;
  }

 private:
  base::WeakPtr<TabUpdateReceiver> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }
  std::vector<TabDataChange> updates_;
  base::WeakPtrFactory<TabUpdateReceiver> weak_ptr_factory_{this};
};

class GlicTabDataBrowserTest : public InProcessBrowserTest {
 public:
  GlicTabDataBrowserTest() = default;
  ~GlicTabDataBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    web_contents_ = GetActiveWebContents(browser());
    ASSERT_TRUE(web_contents_);

    observer_ = std::make_unique<TabDataObserver>(
        web_contents_, update_receiver_.GetCallback());
    observer_->SetTaskRunnerForTesting(mock_task_runner_);
  }

  void TearDownOnMainThread() override {
    observer_.reset();
    web_contents_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  const std::vector<TabDataChange>& GetUpdates() const {
    return update_receiver_.GetUpdates();
  }

 protected:
  content::WebContents* web_contents() { return web_contents_; }
  scoped_refptr<base::TestMockTimeTaskRunner> mock_task_runner_ =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<TabDataObserver> observer_;
  TabUpdateReceiver update_receiver_;
};

IN_PROC_BROWSER_TEST_F(GlicTabDataBrowserTest, NavigateSendsUpdate) {
  GURL url = embedded_test_server()->GetURL("/simple.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  SCOPED_TRACE(update_receiver_);
  ASSERT_THAT(
      GetUpdates(),
      testing::Contains(testing::Truly([](const TabDataChange& change) {
        return change.causes.Has(TabDataChangeCause::kCrossDocNavigation) &&
               change.tab_data->url.GetPath() == "/simple.html";
      })));
}

IN_PROC_BROWSER_TEST_F(GlicTabDataBrowserTest, NavigateToSamePageSendsUpdate) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/simple.html")));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/simple.html#1")));
  SCOPED_TRACE(update_receiver_);

  ASSERT_THAT(
      GetUpdates(),
      testing::Contains(testing::Truly([](const TabDataChange& change) {
        return change.causes.Has(TabDataChangeCause::kCrossDocNavigation) &&
               change.tab_data->url.GetPath() == "/simple.html" &&
               change.tab_data->url.GetRef() == "";
      })));
  ASSERT_THAT(
      GetUpdates(),
      testing::Contains(testing::Truly([](const TabDataChange& change) {
        return change.causes.Has(TabDataChangeCause::kSameDocNavigation) &&
               change.tab_data->url.GetRef() == "1";
      })));
}

IN_PROC_BROWSER_TEST_F(GlicTabDataBrowserTest, ChangeTitleSendsUpdate) {
  GURL url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(content::ExecJs(web_contents_->GetPrimaryMainFrame(),
                              "document.title = `hello world`;"));
  SCOPED_TRACE(update_receiver_);

  ASSERT_THAT(GetUpdates(),
              testing::Contains(testing::Truly([](const TabDataChange& change) {
                return change.causes.Has(TabDataChangeCause::kTitle) &&
                       change.tab_data->title == "hello world";
              })));
}

IN_PROC_BROWSER_TEST_F(GlicTabDataBrowserTest, ChangeTitleUpdatesAreThrottled) {
  GURL url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(content::ExecJs(web_contents_->GetPrimaryMainFrame(),
                              "document.title = `A`;"));
  ASSERT_TRUE(content::ExecJs(web_contents_->GetPrimaryMainFrame(),
                              "document.title = `B`;"));
  ASSERT_TRUE(content::ExecJs(web_contents_->GetPrimaryMainFrame(),
                              "document.title = `C`;"));
  ASSERT_TRUE(content::ExecJs(web_contents_->GetPrimaryMainFrame(),
                              "document.title = `D`;"));
  ASSERT_TRUE(content::ExecJs(web_contents_->GetPrimaryMainFrame(),
                              "document.title = `E`;"));
  ASSERT_TRUE(content::ExecJs(web_contents_->GetPrimaryMainFrame(),
                              "document.title = `F`;"));
  SCOPED_TRACE(update_receiver_);
  auto updates = update_receiver_.TakeUpdates();
  // Sometimes we get updates for about:blank, ignore them.
  while (updates[0].tab_data->url == GURL("about:blank")) {
    updates.erase(updates.begin());
  }
  ASSERT_EQ(updates.size(), 5ul);
  ASSERT_EQ(updates[0].tab_data->url.GetPath(), "/simple.html");
  ASSERT_EQ(updates[1].tab_data->title,
            "OK");  // actual page title, after load.
  ASSERT_EQ(updates[2].tab_data->title, "A");
  ASSERT_EQ(updates[3].tab_data->title, "B");
  ASSERT_EQ(updates[4].tab_data->title, "C");
  mock_task_runner_->FastForwardBy(base::Milliseconds(250));
  ASSERT_TRUE(base::test::RunUntil([&]() { return !GetUpdates().empty(); }));
  ASSERT_EQ(GetUpdates()[0].tab_data->title, "F");
}

}  // namespace
}  // namespace glic
