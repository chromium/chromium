// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/headless_web_contents_manager.h"

#include <memory>

#include "build/build_config.h"
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

// Android sizes a headless page through its view tree, not
// `WebContents::Resize()`, so this only holds on desktop.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(HeadlessWebContentsManagerTest, CreateSizesToViewport) {
  content::WebContents* contents = manager_->Create();

  EXPECT_EQ(contents->GetContainerBounds().size(),
            HeadlessWebContentsManager::kViewportSize);

  ExpectNotifiedWhileValid(contents);
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(HeadlessWebContentsManagerTest, DestroyNotifiesThenDestroys) {
  content::WebContents* contents = manager_->Create();
  content::WebContentsDestroyedWatcher watcher(contents);
  ExpectNotifiedWhileValid(contents);

  manager_->Destroy(contents);

  EXPECT_TRUE(watcher.IsDestroyed());
}

// The page can't close itself: window.close() reaches the manager through
// CloseContents(), which leaves the page alive.
TEST_F(HeadlessWebContentsManagerTest, PageInitiatedCloseKeepsContents) {
  content::WebContents* contents = manager_->Create();
  content::WebContentsDestroyedWatcher watcher(contents);
  EXPECT_CALL(observer_, OnHeadlessContentsWillBeDestroyed).Times(0);

  contents->Close();

  EXPECT_FALSE(watcher.IsDestroyed());
  testing::Mock::VerifyAndClearExpectations(&observer_);

  // Destroy it explicitly before teardown.
  ExpectNotifiedWhileValid(contents);
  manager_->Destroy(contents);
  EXPECT_TRUE(watcher.IsDestroyed());
}

// TakeContents() hands the page over intact: no notification, no destruction,
// and the manager no longer owns or delegates for it.
TEST_F(HeadlessWebContentsManagerTest,
       TakeContentsTransfersOwnershipWithoutDestroying) {
  content::WebContents* contents = manager_->Create();
  content::WebContentsDestroyedWatcher watcher(contents);
  EXPECT_CALL(observer_, OnHeadlessContentsWillBeDestroyed).Times(0);

  std::unique_ptr<content::WebContents> owned =
      manager_->TakeContents(contents);

  EXPECT_EQ(owned.get(), contents);
  EXPECT_EQ(owned->GetDelegate(), nullptr);
  // Destroying the manager leaves the released page alone.
  manager_.reset();
  EXPECT_FALSE(watcher.IsDestroyed());
  testing::Mock::VerifyAndClearExpectations(&observer_);

  owned.reset();
  EXPECT_TRUE(watcher.IsDestroyed());
}

// AdoptContents() makes the manager own and delegate for a page it didn't
// create, without notifying observers; the page is then destroyed like any
// other headless page.
TEST_F(HeadlessWebContentsManagerTest, AdoptContentsTakesOwnership) {
  std::unique_ptr<content::WebContents> owned = content::WebContents::Create(
      content::WebContents::CreateParams(&profile_));
  content::WebContents* contents = owned.get();
  content::WebContentsDestroyedWatcher watcher(contents);
  EXPECT_CALL(observer_, OnHeadlessContentsWillBeDestroyed).Times(0);

  manager_->AdoptContents(std::move(owned));

  EXPECT_EQ(contents->GetDelegate(), manager_.get());
  EXPECT_FALSE(watcher.IsDestroyed());
  testing::Mock::VerifyAndClearExpectations(&observer_);

  ExpectNotifiedWhileValid(contents);
  manager_->Destroy(contents);
  EXPECT_TRUE(watcher.IsDestroyed());
}

// TakeContents() and AdoptContents() round-trip the same page intact.
TEST_F(HeadlessWebContentsManagerTest, TakeThenAdoptKeepsPage) {
  content::WebContents* contents = manager_->Create();
  content::WebContentsDestroyedWatcher watcher(contents);

  manager_->AdoptContents(manager_->TakeContents(contents));

  EXPECT_FALSE(watcher.IsDestroyed());
  EXPECT_EQ(contents->GetDelegate(), manager_.get());
  // Still owned: destroying the manager destroys the page.
  ExpectNotifiedWhileValid(contents);
  manager_.reset();
  EXPECT_TRUE(watcher.IsDestroyed());
}

// Contains() follows the page through Create(), TakeContents(),
// AdoptContents() and Destroy().
TEST_F(HeadlessWebContentsManagerTest, ContainsTracksOwnership) {
  content::WebContents* contents = manager_->Create();
  EXPECT_TRUE(manager_->Contains(contents));

  std::unique_ptr<content::WebContents> owned =
      manager_->TakeContents(contents);
  EXPECT_FALSE(manager_->Contains(contents));

  manager_->AdoptContents(std::move(owned));
  EXPECT_TRUE(manager_->Contains(contents));

  manager_->Destroy(contents);
  EXPECT_FALSE(manager_->Contains(contents));
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
