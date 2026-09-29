// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/headless_web_contents_manager.h"

#include <memory>

#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

class MockObserver : public HeadlessWebContentsManager::Observer {
 public:
  MOCK_METHOD(void,
              OnHeadlessContentsWillBeDestroyed,
              (content::WebContents * contents),
              (override));
};

// Sets up a TestingProfile and test renderer environment so the manager can
// create WebContents in unit tests.
class HeadlessWebContentsManagerTest : public testing::Test {
 public:
  void SetUp() override {
    manager_ = std::make_unique<HeadlessWebContentsManager>(&profile_);
    manager_->AddObserver(&observer_);
  }

  void TearDown() override { manager_.reset(); }

 protected:
  // Expects one notification for `contents`, arriving while it is still owned
  // by the manager.
  void ExpectNotifiedWhileValid(content::WebContents* contents) {
    // Not manager_.get(): unique_ptr::reset() nulls it before deleting.
    HeadlessWebContentsManager* manager = manager_.get();
    EXPECT_CALL(observer_, OnHeadlessContentsWillBeDestroyed(contents))
        .WillOnce([manager](content::WebContents* c) {
          EXPECT_EQ(c->GetDelegate(), manager);
        });
  }

  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  MockObserver observer_;
  std::unique_ptr<HeadlessWebContentsManager> manager_;
};

TEST_F(HeadlessWebContentsManagerTest, DestroyNotifiesThenDestroys) {
  content::WebContents* contents = manager_->Create();
  content::WebContentsDestroyedWatcher watcher(contents);
  ExpectNotifiedWhileValid(contents);

  manager_->Destroy(contents);

  EXPECT_TRUE(watcher.IsDestroyed());
}

// window.close() reaches the manager through WebContentsDelegate and must take
// the same notify-then-destroy path.
TEST_F(HeadlessWebContentsManagerTest, PageInitiatedCloseNotifiesThenDestroys) {
  content::WebContents* contents = manager_->Create();
  content::WebContentsDestroyedWatcher watcher(contents);
  ExpectNotifiedWhileValid(contents);

  contents->Close();

  EXPECT_TRUE(watcher.IsDestroyed());
}

TEST_F(HeadlessWebContentsManagerTest, ManagerDestructionNotifiesForEach) {
  content::WebContents* first = manager_->Create();
  content::WebContents* second = manager_->Create();
  content::WebContentsDestroyedWatcher first_watcher(first);
  content::WebContentsDestroyedWatcher second_watcher(second);
  ExpectNotifiedWhileValid(first);
  ExpectNotifiedWhileValid(second);

  manager_.reset();

  EXPECT_TRUE(first_watcher.IsDestroyed());
  EXPECT_TRUE(second_watcher.IsDestroyed());
}

}  // namespace
}  // namespace actor
