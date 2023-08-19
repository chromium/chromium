// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_session_plugin_handler.h"

#include "chrome/browser/chromeos/app_mode/kiosk_session_plugin_handler_delegate.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;
using content::WebContentsObserver;

namespace chromeos {

namespace {

constexpr char kValidPluginPath[] = "/path/to/valid_plugin";
constexpr char kInvalidPluginPath[] = "/path/to/invalid_plugin";
constexpr char kProcessId = 2;
constexpr int kPluginChildId = 2;

}  // namespace

class TestKioskSessionPluginHandlerDelegate
    : public KioskSessionPluginHandlerDelegate {
 public:
  TestKioskSessionPluginHandlerDelegate() = default;
  ~TestKioskSessionPluginHandlerDelegate() override = default;

  bool ShouldHandlePlugin(const base::FilePath& plugin_path) const override {
    return plugin_path.AsUTF8Unsafe() == kValidPluginPath;
  }

  void OnPluginCrashed(const base::FilePath& plugin_path) override {
    has_crashed_ = true;
  }

  void OnPluginHung(const std::set<int>& hung_plugins) override {}

  bool has_crashed() const { return has_crashed_; }

 private:
  bool has_crashed_ = false;
};

class KioskSessionPluginHandlerTest : public testing::Test {
 public:
  KioskSessionPluginHandlerTest() = default;
  ~KioskSessionPluginHandlerTest() override = default;

  void SetUp() override {
    delegate_ = std::make_unique<TestKioskSessionPluginHandlerDelegate>();
    handler_ = std::make_unique<KioskSessionPluginHandler>(delegate_.get());
  }

  TestKioskSessionPluginHandlerDelegate* delegate() const {
    return delegate_.get();
  }

  KioskSessionPluginHandler* handler() const { return handler_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestKioskSessionPluginHandlerDelegate> delegate_;
  std::unique_ptr<KioskSessionPluginHandler> handler_;
};

TEST_F(KioskSessionPluginHandlerTest, ObserveAndDestroyWebContents) {
  // At the beginning, there is no watcher.
  EXPECT_EQ(handler()->GetWatchersForTesting().size(), 0U);

  // The number of watchers increases after a new WebContents instance is
  // observed.
  TestingProfile profile1;
  std::unique_ptr<WebContents> contents1 =
      WebContents::Create(WebContents::CreateParams(&profile1));
  handler()->Observe(contents1.get());
  EXPECT_EQ(handler()->GetWatchersForTesting().size(), 1U);

  // The number of watchers increases again after another new WebContents
  // instance is observed.
  TestingProfile profile2;
  std::unique_ptr<WebContents> contents2 =
      WebContents::Create(WebContents::CreateParams(&profile2));
  handler()->Observe(contents2.get());

  std::vector<KioskSessionPluginHandler::Observer*> watchers =
      handler()->GetWatchersForTesting();
  EXPECT_EQ(watchers.size(), 2U);

  // The number of watchers returns to zero after each WebContents instance is
  // destroyed.
  for (WebContentsObserver* observer : watchers) {
    observer->WebContentsDestroyed();
  }
  EXPECT_EQ(handler()->GetWatchersForTesting().size(), 0U);
}

TEST_F(KioskSessionPluginHandlerTest, PluginCrashed) {
  TestingProfile profile;
  std::unique_ptr<WebContents> contents =
      WebContents::Create(WebContents::CreateParams(&profile));
  handler()->Observe(contents.get());
  WebContentsObserver* watcher = handler()->GetWatchersForTesting().front();

  // At the beginning, no crash is notified to the delegate.
  EXPECT_FALSE(delegate()->has_crashed());

  // No crash is notified if the `plugin_path` is invalid.
  watcher->PluginCrashed(base::FilePath(kInvalidPluginPath),
                         base::ProcessId(kProcessId));
  EXPECT_FALSE(delegate()->has_crashed());

  // Crash is notified if the `plugin_path` is valid.
  watcher->PluginCrashed(base::FilePath(kValidPluginPath),
                         base::ProcessId(kProcessId));
  EXPECT_TRUE(delegate()->has_crashed());
}

TEST_F(KioskSessionPluginHandlerTest, PluginHungStatusChanged) {
  TestingProfile profile;
  std::unique_ptr<WebContents> contents =
      WebContents::Create(WebContents::CreateParams(&profile));
  handler()->Observe(contents.get());

  KioskSessionPluginHandler::Observer* observer =
      handler()->GetWatchersForTesting().front();
  WebContentsObserver* watcher = observer;

  // At the beginning, there is no hung plugin.
  EXPECT_EQ(observer->GetHungPluginsForTesting().size(), 0U);

  // The hung plugin is not stored if the `plugin_path` is invalid.
  watcher->PluginHungStatusChanged(kPluginChildId,
                                   base::FilePath(kInvalidPluginPath), true);
  EXPECT_EQ(observer->GetHungPluginsForTesting().size(), 0U);

  // The hung plugin is not stored if the `is_hung` is false.
  watcher->PluginHungStatusChanged(kPluginChildId,
                                   base::FilePath(kValidPluginPath), false);
  EXPECT_EQ(observer->GetHungPluginsForTesting().size(), 0U);

  // The hung plugin is store.
  watcher->PluginHungStatusChanged(kPluginChildId,
                                   base::FilePath(kValidPluginPath), true);
  EXPECT_EQ(observer->GetHungPluginsForTesting().size(), 1U);
}

}  // namespace chromeos
