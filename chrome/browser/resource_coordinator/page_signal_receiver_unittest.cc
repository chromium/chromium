// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/page_signal_receiver.h"

#include "base/task/post_task.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

class PageSignalReceiverUnitTest : public ChromeRenderViewHostTestHarness {
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    page_cu_id_ = {CoordinationUnitType::kPage, CoordinationUnitID::RANDOM_ID};
    page_signal_receiver_ = std::make_unique<PageSignalReceiver>();
  }

  void TearDown() override {
    page_signal_receiver_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  CoordinationUnitID page_cu_id_;
  std::unique_ptr<PageSignalReceiver> page_signal_receiver_;
};

enum class Action { kObserve, kRemoveCoordinationUnitID };
class TestPageSignalObserver : public PageSignalObserver {
 public:
  TestPageSignalObserver(Action action,
                         CoordinationUnitID page_cu_id,
                         PageSignalReceiver* page_signal_receiver)
      : action_(action),
        page_cu_id_(page_cu_id),
        page_signal_receiver_(page_signal_receiver) {
    page_signal_receiver_->AddObserver(this);
  }
  ~TestPageSignalObserver() override {
    page_signal_receiver_->RemoveObserver(this);
  }
  // PageSignalObserver:
  void OnLifecycleStateChanged(content::WebContents* contents,
                               const PageNavigationIdentity& page_navigation_id,
                               mojom::LifecycleState state) override {
    if (action_ == Action::kRemoveCoordinationUnitID)
      page_signal_receiver_->RemoveCoordinationUnitID(page_cu_id_);
  }

 private:
  Action action_;
  CoordinationUnitID page_cu_id_;
  PageSignalReceiver* page_signal_receiver_;
  DISALLOW_COPY_AND_ASSIGN(TestPageSignalObserver);
};

// This test models the scenario that can happen with tab discarding.
// 1) Multiple observers are subscribed to the page signal receiver.
// 2) The page signal receiver sends OnLifecycleStateChanged to observers.
// 3) The first observer is TabLifecycleUnitSource, which calls
//    TabLifecycleUnit::FinishDiscard that destroys the old WebContents.
// 4) The destructor of the WebContents calls
//    ResourceCoordinatorTabHelper::WebContentsDestroyed, which deletes the
//    page_cu_id entry from the page signal receiver map.
// 5) The next observer is invoked with the deleted entry.
TEST_F(PageSignalReceiverUnitTest,
       NotifyObserversThatCanRemoveCoordinationUnitID) {
  page_signal_receiver_->AssociateCoordinationUnitIDWithWebContents(
      page_cu_id_, web_contents());
  TestPageSignalObserver observer1(Action::kObserve, page_cu_id_,
                                   page_signal_receiver_.get());
  TestPageSignalObserver observer2(Action::kRemoveCoordinationUnitID,
                                   page_cu_id_, page_signal_receiver_.get());
  TestPageSignalObserver observer3(Action::kObserve, page_cu_id_,
                                   page_signal_receiver_.get());
  page_signal_receiver_->NotifyObserversIfKnownCu(
      {page_cu_id_, 1, ""}, &PageSignalObserver::OnLifecycleStateChanged,
      mojom::LifecycleState::kDiscarded);
}

// Regression test for crbug.com/855114.
TEST_F(PageSignalReceiverUnitTest, ConstructMojoChannelOnce) {
  // Create a dummy service manager.
  service_manager::mojom::ServicePtr service;
  content::ServiceManagerConnection::SetForProcess(
      content::ServiceManagerConnection::Create(
          mojo::MakeRequest(&service),
          base::CreateSingleThreadTaskRunnerWithTraits(
              {content::BrowserThread::IO})));
  // Add and remove an observer.
  {
    TestPageSignalObserver observer1(Action::kObserve, page_cu_id_,
                                     page_signal_receiver_.get());
  }
  // Add and remove another observer. This causes the page signal receiver to
  // construct the mojo channel again because the observer list is empty.
  {
    TestPageSignalObserver observer2(Action::kObserve, page_cu_id_,
                                     page_signal_receiver_.get());
  }
  content::ServiceManagerConnection::DestroyForProcess();
}

TEST_F(PageSignalReceiverUnitTest, GetNavigationIDForWebContents) {
  page_signal_receiver_->AssociateCoordinationUnitIDWithWebContents(
      page_cu_id_, web_contents());

  // Starts unset.
  EXPECT_EQ(
      0, page_signal_receiver_->GetNavigationIDForWebContents(web_contents()));

  // Follows updates.
  page_signal_receiver_->SetNavigationID(web_contents(), 10);
  EXPECT_EQ(
      10, page_signal_receiver_->GetNavigationIDForWebContents(web_contents()));
  page_signal_receiver_->SetNavigationID(web_contents(), 11);
  EXPECT_EQ(
      11, page_signal_receiver_->GetNavigationIDForWebContents(web_contents()));

  // Unset on removal.
  page_signal_receiver_->RemoveCoordinationUnitID(page_cu_id_);
  EXPECT_EQ(
      0, page_signal_receiver_->GetNavigationIDForWebContents(web_contents()));
}

}  // namespace resource_coordinator
