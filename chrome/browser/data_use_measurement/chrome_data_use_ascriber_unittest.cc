// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_use_measurement/chrome_data_use_ascriber.h"

#include <list>
#include <memory>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/data_use_measurement/core/data_use_recorder.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/process_type.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

int kRenderProcessId = 1;
int kRenderFrameId = 2;
int kRequestId = 3;
uint32_t kPageTransition = 5;
void* kNavigationHandle = &kNavigationHandle;

class MockPageLoadObserver
    : public data_use_measurement::DataUseAscriber::PageLoadObserver {
 public:
  MOCK_METHOD1(OnPageLoadCommit, void(data_use_measurement::DataUse* data_use));
  MOCK_METHOD2(OnPageResourceLoad,
               void(const net::URLRequest& request,
                    data_use_measurement::DataUse* data_use));
  MOCK_METHOD1(OnPageDidFinishLoad,
               void(data_use_measurement::DataUse* data_use));
  MOCK_METHOD1(OnPageLoadConcluded,
               void(data_use_measurement::DataUse* data_use));
  MOCK_METHOD2(OnNetworkBytesUpdate,
               void(const net::URLRequest& request,
                    data_use_measurement::DataUse* data_use));
};

}  // namespace

namespace data_use_measurement {

class ChromeDataUseAscriberTest : public testing::Test {
 protected:
  ChromeDataUseAscriberTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        resource_context_(new content::MockResourceContext(&context_)) {}

  void SetUp() override {}

  void TearDown() override { recorders().clear(); }

  void CreateAscriber() {
    ascriber_ = std::make_unique<ChromeDataUseAscriber>();
    // Enable ascriber for tests.
    ascriber_->disable_ascriber_ = false;
  }

  std::list<ChromeDataUseRecorder>& recorders() {
    return ascriber_->data_use_recorders_;
  }

  net::TestURLRequestContext* context() { return &context_; }

  content::MockResourceContext* resource_context() {
    return resource_context_.get();
  }

  ChromeDataUseAscriber* ascriber() { return ascriber_.get(); }

  std::unique_ptr<net::URLRequest> CreateNewRequest(std::string url,
                                                    bool is_main_frame,
                                                    int request_id,
                                                    int render_process_id,
                                                    int render_frame_id) {
    std::unique_ptr<net::URLRequest> request = context()->CreateRequest(
        GURL(url), net::IDLE, nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);
    // TODO(kundaji): Allow request_id to be specified in AllocateForTesting.
    content::ResourceRequestInfo::AllocateForTesting(
        request.get(),
        is_main_frame ? content::RESOURCE_TYPE_MAIN_FRAME
                      : content::RESOURCE_TYPE_SCRIPT,
        resource_context(), render_process_id,
        /*render_view_id=*/-1, render_frame_id, is_main_frame,
        /*allow_download=*/false,
        /*is_async=*/true, content::PREVIEWS_OFF,
        /*navigation_ui_data*/ nullptr);
    return request;
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<ChromeDataUseAscriber> ascriber_;
  net::TestURLRequestContext context_;
  std::unique_ptr<content::MockResourceContext> resource_context_;
};

TEST_F(ChromeDataUseAscriberTest, NoRecorderWithoutFrame) {
  CreateAscriber();
  std::unique_ptr<net::URLRequest> request = CreateNewRequest(
      "http://test.com", true, kRequestId, kRenderProcessId, kRenderFrameId);

  // Main frame request causes a recorder to be created.
  ascriber()->OnBeforeUrlRequest(request.get());
  EXPECT_EQ(1u, recorders().size());

  // Frame is created.
  ascriber()->RenderFrameCreated(kRenderProcessId, kRenderFrameId, -1, -1);
  EXPECT_EQ(2u, recorders().size());

  // Same mainframe request should not cause another recorder to be created.
  ascriber()->OnBeforeUrlRequest(request.get());
  EXPECT_EQ(2u, recorders().size());

  ascriber()->OnUrlRequestDestroyed(request.get());

  ascriber()->ReadyToCommitMainFrameNavigation(
      content::GlobalRequestID(kRenderProcessId, 0), kRenderProcessId,
      kRenderFrameId);
  ascriber()->DidFinishMainFrameNavigation(
      kRenderProcessId, kRenderFrameId, GURL("http://test.com"), false,
      kPageTransition, base::TimeTicks::Now());
  EXPECT_EQ(1u, recorders().size());

  ascriber()->RenderFrameDeleted(kRenderProcessId, kRenderFrameId, -1, -1);
  EXPECT_EQ(0u, recorders().size());
}

TEST_F(ChromeDataUseAscriberTest, RenderFrameShownAndHidden) {
  CreateAscriber();
  std::unique_ptr<net::URLRequest> request = CreateNewRequest(
      "http://test.com", true, kRequestId, kRenderProcessId, kRenderFrameId);

  ascriber()->RenderFrameCreated(kRenderProcessId, kRenderFrameId, -1, -1);
  ascriber()->OnBeforeUrlRequest(request.get());
  ascriber()->ReadyToCommitMainFrameNavigation(
      content::GlobalRequestID(kRenderProcessId, 0), kRenderProcessId,
      kRenderFrameId);
  ascriber()->DidFinishMainFrameNavigation(
      kRenderProcessId, kRenderFrameId, GURL("http://test.com"), false,
      kPageTransition, base::TimeTicks::Now());
  ascriber()->WasShownOrHidden(kRenderProcessId, kRenderFrameId, true);

  EXPECT_TRUE(ascriber()->GetDataUseRecorder(*request)->is_visible());

  // Hide the frame, and the visibility should be updated.
  ascriber()->WasShownOrHidden(kRenderProcessId, kRenderFrameId, false);
  EXPECT_FALSE(ascriber()->GetDataUseRecorder(*request)->is_visible());

  ascriber()->RenderFrameDeleted(kRenderProcessId, kRenderFrameId, -1, -1);

  ascriber()->OnUrlRequestDestroyed(request.get());
  EXPECT_EQ(0u, recorders().size());
}

TEST_F(ChromeDataUseAscriberTest, RenderFrameHiddenAndShown) {
  CreateAscriber();
  std::unique_ptr<net::URLRequest> request = CreateNewRequest(
      "http://test.com", true, kRequestId, kRenderProcessId, kRenderFrameId);

  ascriber()->RenderFrameCreated(kRenderProcessId, kRenderFrameId, -1, -1);
  ascriber()->OnBeforeUrlRequest(request.get());
  ascriber()->ReadyToCommitMainFrameNavigation(
      content::GlobalRequestID(kRenderProcessId, 0), kRenderProcessId,
      kRenderFrameId);
  ascriber()->DidFinishMainFrameNavigation(
      kRenderProcessId, kRenderFrameId, GURL("http://test.com"), false,
      kPageTransition, base::TimeTicks::Now());
  ascriber()->WasShownOrHidden(kRenderProcessId, kRenderFrameId, false);

  EXPECT_FALSE(ascriber()->GetDataUseRecorder(*request)->is_visible());

  // Show the frame, and the visibility should be updated.
  ascriber()->WasShownOrHidden(kRenderProcessId, kRenderFrameId, true);
  EXPECT_TRUE(ascriber()->GetDataUseRecorder(*request)->is_visible());

  ascriber()->RenderFrameDeleted(kRenderProcessId, kRenderFrameId, -1, -1);

  ascriber()->OnUrlRequestDestroyed(request.get());
  EXPECT_EQ(0u, recorders().size());
}

TEST_F(ChromeDataUseAscriberTest, RenderFrameHostChanged) {
  CreateAscriber();
  std::unique_ptr<net::URLRequest> request = CreateNewRequest(
      "http://test.com", true, kRequestId, kRenderProcessId, kRenderFrameId);

  ascriber()->RenderFrameCreated(kRenderProcessId, kRenderFrameId, -1, -1);
  ascriber()->OnBeforeUrlRequest(request.get());
  ascriber()->ReadyToCommitMainFrameNavigation(
      content::GlobalRequestID(kRenderProcessId, 0), kRenderProcessId,
      kRenderFrameId);
  ascriber()->DidFinishMainFrameNavigation(
      kRenderProcessId, kRenderFrameId, GURL("http://test.com"), false,
      kPageTransition, base::TimeTicks::Now());
  ascriber()->WasShownOrHidden(kRenderProcessId, kRenderFrameId, true);
  EXPECT_TRUE(ascriber()->GetDataUseRecorder(*request)->is_visible());

  // Create a new render frame and swap it.
  ascriber()->RenderFrameCreated(kRenderProcessId + 1, kRenderFrameId + 1, -1,
                                 -1);
  ascriber()->RenderFrameHostChanged(kRenderProcessId, kRenderFrameId,
                                     kRenderProcessId + 1, kRenderFrameId + 1);
  ascriber()->RenderFrameDeleted(kRenderProcessId, kRenderFrameId, -1, -1);

  ascriber()->WasShownOrHidden(kRenderProcessId + 1, kRenderFrameId + 1, true);
  ascriber()->RenderFrameDeleted(kRenderProcessId + 1, kRenderFrameId + 1, -1,
                                 -1);

  ascriber()->OnUrlRequestDestroyed(request.get());
  EXPECT_EQ(0u, recorders().size());
}

TEST_F(ChromeDataUseAscriberTest, MainFrameNavigation) {
  CreateAscriber();
  std::unique_ptr<net::URLRequest> request = CreateNewRequest(
      "http://test.com", true, kRequestId, kRenderProcessId, kRenderFrameId);

  // Mainframe is created.
  ascriber()->RenderFrameCreated(kRenderProcessId, kRenderFrameId, -1, -1);
  EXPECT_EQ(1u, recorders().size());

  // Request should cause a recorder to be created.
  ascriber()->OnBeforeUrlRequest(request.get());
  EXPECT_EQ(2u, recorders().size());

  // Navigation commits.
  ascriber()->ReadyToCommitMainFrameNavigation(
      content::GlobalRequestID(kRenderProcessId, 0), kRenderProcessId,
      kRenderFrameId);
  ascriber()->DidFinishMainFrameNavigation(
      kRenderProcessId, kRenderFrameId, GURL("http://mobile.test.com"), false,
      kPageTransition, base::TimeTicks::Now());

  // Navigation commit should merge the two data use recorder entries.
  EXPECT_EQ(1u, recorders().size());
  auto& recorder_entry = recorders().front();
  EXPECT_EQ(content::GlobalFrameRoutingId(kRenderProcessId, kRenderFrameId),
            recorder_entry.main_frame_id());
  EXPECT_EQ(content::GlobalRequestID(kRenderProcessId, 0),
            recorder_entry.main_frame_request_id());
  EXPECT_EQ(GURL("http://mobile.test.com"), recorder_entry.data_use().url());
  EXPECT_EQ(DataUse::TrafficType::USER_TRAFFIC,
            recorder_entry.data_use().traffic_type());

  ascriber()->RenderFrameDeleted(kRenderProcessId, kRenderFrameId, -1, -1);
  ascriber()->OnUrlRequestDestroyed(request.get());

  EXPECT_EQ(0u, recorders().size());
}

TEST_F(ChromeDataUseAscriberTest, SubResourceRequestsAttributed) {
  CreateAscriber();
  // A regression test that verifies that subframe requests in the second page
  // load in the same frame get attributed to the entry correctly.
  std::unique_ptr<net::URLRequest> page_load_a_main_frame_request =
      CreateNewRequest("http://test.com", true, kRequestId, kRenderProcessId,
                       kRenderFrameId);

  ascriber()->RenderFrameCreated(kRenderProcessId, kRenderFrameId, -1, -1);

  // Start the main frame reuqest.
  ascriber()->OnBeforeUrlRequest(page_load_a_main_frame_request.get());

  // Commit the page load.
  ascriber()->ReadyToCommitMainFrameNavigation(
      content::GlobalRequestID(kRenderProcessId, 0), kRenderProcessId,
      kRenderFrameId);
  ascriber()->DidFinishMainFrameNavigation(
      kRenderProcessId, kRenderFrameId, GURL("http://mobile.test.com"), false,
      kPageTransition, base::TimeTicks::Now());

  std::unique_ptr<net::URLRequest> page_load_b_main_frame_request =
      CreateNewRequest("http://test_2.com", true, kRequestId + 1,
                       kRenderProcessId, kRenderFrameId);
  std::unique_ptr<net::URLRequest> page_load_b_sub_request =
      CreateNewRequest("http://test_2.com/s", false, kRequestId + 2,
                       kRenderProcessId, kRenderFrameId);

  // Start the second main frame request.
  ascriber()->OnBeforeUrlRequest(page_load_b_main_frame_request.get());

  // Commit the second page load.
  ascriber()->ReadyToCommitMainFrameNavigation(
      content::GlobalRequestID(kRenderProcessId, 0), kRenderProcessId,
      kRenderFrameId);
  ascriber()->DidFinishMainFrameNavigation(
      kRenderProcessId, kRenderFrameId, GURL("http://mobile.test_2.com"), false,
      kPageTransition, base::TimeTicks::Now());

  // Delete the first main frame request.
  ascriber()->OnUrlRequestDestroyed(page_load_a_main_frame_request.get());

  // Start the sub resource request for the second page load.
  ascriber()->OnBeforeUrlRequest(page_load_b_sub_request.get());
  ascriber()->OnNetworkBytesReceived(page_load_b_sub_request.get(), 100);

  EXPECT_EQ(1u, recorders().size());
  auto& recorder_entry = recorders().front();
  EXPECT_EQ(content::GlobalFrameRoutingId(kRenderProcessId, kRenderFrameId),
            recorder_entry.main_frame_id());
  EXPECT_EQ(content::GlobalRequestID(kRenderProcessId, 0),
            recorder_entry.main_frame_request_id());
  EXPECT_EQ(GURL("http://mobile.test_2.com"), recorder_entry.data_use().url());
  EXPECT_EQ(DataUse::TrafficType::USER_TRAFFIC,
            recorder_entry.data_use().traffic_type());
  // Verify that the data usage for the sub-resource was counted towards the
  // entry.
  EXPECT_EQ(100, recorder_entry.data_use().total_bytes_received());

  ascriber()->OnUrlRequestDestroyed(page_load_b_sub_request.get());
  ascriber()->OnUrlRequestDestroyed(page_load_b_main_frame_request.get());
  ascriber()->RenderFrameDeleted(kRenderProcessId, kRenderFrameId, -1, -1);
  EXPECT_EQ(0u, recorders().size());
}

// Verifies that navigation finish acts as the event that separates pageloads.
// subresource requests started before the navigation commit has finished are
// ascribed to the previous page load at the time of the commit, and requests
// started after are ascribed to the next page load.
TEST_F(ChromeDataUseAscriberTest, SubResourceRequestsAfterNavigationFinish) {
  CreateAscriber();
  std::unique_ptr<net::URLRequest> page_load_a_mainresource = CreateNewRequest(
      "http://test.com", true, kRequestId, kRenderProcessId, kRenderFrameId);
  std::unique_ptr<net::URLRequest> page_load_a_subresource =
      CreateNewRequest("http://test.com/subresource", false, kRequestId + 1,
                       kRenderProcessId, kRenderFrameId);

  // First page load 'a'.
  ascriber()->RenderFrameCreated(kRenderProcessId, kRenderFrameId, -1, -1);
  ascriber()->OnBeforeUrlRequest(page_load_a_mainresource.get());
  ascriber()->ReadyToCommitMainFrameNavigation(
      content::GlobalRequestID(kRenderProcessId, 0), kRenderProcessId,
      kRenderFrameId);
  ascriber()->DidFinishMainFrameNavigation(
      kRenderProcessId, kRenderFrameId, GURL("http://mobile.test.com"), false,
      kPageTransition, base::TimeTicks::Now());

  ascriber()->OnBeforeUrlRequest(page_load_a_subresource.get());
  ascriber()->OnUrlRequestDestroyed(page_load_a_mainresource.get());

  ascriber()->OnNetworkBytesReceived(page_load_a_subresource.get(), 100);

  EXPECT_EQ(1u, recorders().size());
  auto& page_load_a_recorder = recorders().front();
  EXPECT_EQ(content::GlobalFrameRoutingId(kRenderProcessId, kRenderFrameId),
            page_load_a_recorder.main_frame_id());
  EXPECT_EQ(content::GlobalRequestID(kRenderProcessId, 0),
            page_load_a_recorder.main_frame_request_id());
  EXPECT_EQ(GURL("http://mobile.test.com"),
            page_load_a_recorder.data_use().url());
  EXPECT_EQ(DataUse::TrafficType::USER_TRAFFIC,
            page_load_a_recorder.data_use().traffic_type());

  std::unique_ptr<net::URLRequest> page_load_b_mainresource =
      CreateNewRequest("http://test_2.com", true, kRequestId + 2,
                       kRenderProcessId, kRenderFrameId);
  std::unique_ptr<net::URLRequest> page_load_b_subresource =
      CreateNewRequest("http://test_2.com/subresource", false, kRequestId + 3,
                       kRenderProcessId, kRenderFrameId);

  // Second page load 'b' on the same main render frame.
  ascriber()->OnBeforeUrlRequest(page_load_b_mainresource.get());
  EXPECT_EQ(100, page_load_a_recorder.data_use().total_bytes_received());
  ascriber()->ReadyToCommitMainFrameNavigation(
      content::GlobalRequestID(kRenderProcessId, 0), kRenderProcessId,
      kRenderFrameId);
  ascriber()->DidFinishMainFrameNavigation(
      kRenderProcessId, kRenderFrameId, GURL("http://mobile.test_2.com"), false,
      kPageTransition, base::TimeTicks::Now());

  ascriber()->OnBeforeUrlRequest(page_load_b_subresource.get());
  ascriber()->OnNetworkBytesReceived(page_load_b_subresource.get(), 200);
  ascriber()->OnNetworkBytesReceived(page_load_a_subresource.get(), 100);

  // Previous page recorder is gone.
  EXPECT_EQ(1u, recorders().size());
  auto& page_load_b_recorder = recorders().back();
  EXPECT_EQ(content::GlobalFrameRoutingId(kRenderProcessId, kRenderFrameId),
            page_load_b_recorder.main_frame_id());
  EXPECT_EQ(content::GlobalRequestID(kRenderProcessId, 0),
            page_load_b_recorder.main_frame_request_id());
  EXPECT_EQ(GURL("http://mobile.test_2.com"),
            page_load_b_recorder.data_use().url());
  EXPECT_EQ(DataUse::TrafficType::USER_TRAFFIC,
            page_load_b_recorder.data_use().traffic_type());

  // Verify the data usage.
  EXPECT_EQ(200, page_load_b_recorder.data_use().total_bytes_received());

  std::unique_ptr<net::URLRequest> page_load_c_mainresource =
      CreateNewRequest("http://test_c.com", true, kRequestId + 2,
                       kRenderProcessId, kRenderFrameId);
  std::unique_ptr<net::URLRequest> page_load_c_subresource =
      CreateNewRequest("http://test_c.com/subresource", false, kRequestId + 3,
                       kRenderProcessId, kRenderFrameId);

  // Third page load 'c' on the same main render frame with
  // same_document_navigation set.
  ascriber()->OnBeforeUrlRequest(page_load_c_mainresource.get());
  ascriber()->ReadyToCommitMainFrameNavigation(
      content::GlobalRequestID(kRenderProcessId, 0), kRenderProcessId,
      kRenderFrameId);
  ascriber()->DidFinishMainFrameNavigation(
      kRenderProcessId, kRenderFrameId, GURL("http://mobile.test_c.com"), true,
      kPageTransition, base::TimeTicks::Now());

  ascriber()->OnBeforeUrlRequest(page_load_c_subresource.get());
  ascriber()->OnNetworkBytesReceived(page_load_c_subresource.get(), 2000);
  ascriber()->OnNetworkBytesReceived(page_load_b_subresource.get(), 1000);

  // Data usage of page load 'c' should get merged to page load 'b'.
  EXPECT_EQ(3200, page_load_b_recorder.data_use().total_bytes_received());

  ascriber()->OnUrlRequestDestroyed(page_load_a_subresource.get());
  ascriber()->OnUrlRequestDestroyed(page_load_b_subresource.get());
  ascriber()->OnUrlRequestDestroyed(page_load_b_mainresource.get());
  ascriber()->OnUrlRequestDestroyed(page_load_c_subresource.get());
  ascriber()->OnUrlRequestDestroyed(page_load_c_mainresource.get());
  ascriber()->RenderFrameDeleted(kRenderProcessId, kRenderFrameId, -1, -1);
  EXPECT_EQ(0u, recorders().size());
}

TEST_F(ChromeDataUseAscriberTest, FailedMainFrameNavigation) {
  CreateAscriber();
  std::unique_ptr<net::URLRequest> request = CreateNewRequest(
      "http://test.com", true, kRequestId, kRenderProcessId, kRenderFrameId);

  // Mainframe is created.
  ascriber()->RenderFrameCreated(kRenderProcessId, kRenderFrameId, -1, -1);
  EXPECT_EQ(1u, recorders().size());

  // Request should cause a recorder to be created.
  ascriber()->OnBeforeUrlRequest(request.get());
  EXPECT_EQ(2u, recorders().size());

  // Failed request will remove the pending entry.
  request->Cancel();
  ascriber()->OnUrlRequestCompleted(request.get(), false);

  ascriber()->RenderFrameDeleted(kRenderProcessId, kRenderFrameId, -1, -1);
  ascriber()->OnUrlRequestDestroyed(request.get());

  EXPECT_EQ(0u, recorders().size());
}

TEST_F(ChromeDataUseAscriberTest, PageLoadObserverNotified) {
  CreateAscriber();
  // TODO(rajendrant): Handle PlzNavigate (http://crbug/664233).
  MockPageLoadObserver mock_observer;
  ascriber()->AddObserver(&mock_observer);

  std::unique_ptr<net::URLRequest> request = CreateNewRequest(
      "http://test.com", true, kRequestId, kRenderProcessId, kRenderFrameId);

  // Mainframe is created.
  ascriber()->RenderFrameCreated(kRenderProcessId, kRenderFrameId, -1, -1);
  EXPECT_EQ(1u, recorders().size());

  ascriber()->OnBeforeUrlRequest(request.get());

  // Navigation starts and is ready to commit.
  ascriber()->ReadyToCommitMainFrameNavigation(
      content::GlobalRequestID(kRenderProcessId, 0), kRenderProcessId,
      kRenderFrameId);

  EXPECT_EQ(2u, recorders().size());
  DataUse* data_use = &recorders().back().data_use();

  EXPECT_CALL(mock_observer, OnNetworkBytesUpdate(testing::_, data_use))
      .Times(2);
  ascriber()->OnNetworkBytesSent(request.get(), 2);
  ascriber()->OnNetworkBytesReceived(request.get(), 2);

  EXPECT_CALL(mock_observer, OnPageResourceLoad(testing::_, data_use)).Times(1);
  ascriber()->OnUrlRequestCompleted(request.get(), false);

  EXPECT_CALL(mock_observer, OnPageLoadCommit(data_use)).Times(1);
  EXPECT_CALL(mock_observer, OnPageLoadConcluded(testing::_)).Times(1);
  ascriber()->DidFinishMainFrameNavigation(
      kRenderProcessId, kRenderFrameId, GURL("http://mobile.test.com"), false,
      kPageTransition, base::TimeTicks::Now());

  EXPECT_EQ(1u, recorders().size());
  auto& recorder_entry = recorders().front();
  EXPECT_EQ(content::GlobalFrameRoutingId(kRenderProcessId, kRenderFrameId),
            recorder_entry.main_frame_id());
  EXPECT_EQ(content::GlobalRequestID(kRenderProcessId, 0),
            recorder_entry.main_frame_request_id());
  EXPECT_EQ(GURL("http://mobile.test.com"), recorder_entry.data_use().url());

  EXPECT_CALL(mock_observer, OnPageDidFinishLoad(&recorder_entry.data_use()))
      .Times(1);
  ascriber()->DidFinishLoad(kRenderProcessId, kRenderFrameId,
                            GURL("http://mobile.test.com"));

  EXPECT_CALL(mock_observer, OnPageLoadConcluded(&recorder_entry.data_use()))
      .Times(1);
  ascriber()->RenderFrameDeleted(kRenderProcessId, kRenderFrameId, -1, -1);
  ascriber()->OnUrlRequestDestroyed(request.get());

  EXPECT_EQ(0u, recorders().size());
}

TEST_F(ChromeDataUseAscriberTest, PageLoadObserverForErrorPageValidatedURL) {
  CreateAscriber();
  MockPageLoadObserver mock_observer;
  ascriber()->AddObserver(&mock_observer);

  std::unique_ptr<net::URLRequest> request = CreateNewRequest(
      "http://test.com", true, kRequestId, kRenderProcessId, kRenderFrameId);

  // Mainframe is created.
  ascriber()->RenderFrameCreated(kRenderProcessId, kRenderFrameId, -1, -1);
  EXPECT_EQ(1u, recorders().size());

  ascriber()->OnBeforeUrlRequest(request.get());

  // Navigation starts and is ready to commit.
  ascriber()->ReadyToCommitMainFrameNavigation(
      content::GlobalRequestID(kRenderProcessId, 0), kRenderProcessId,
      kRenderFrameId);

  EXPECT_EQ(2u, recorders().size());
  DataUse* data_use = &recorders().back().data_use();

  EXPECT_CALL(mock_observer, OnNetworkBytesUpdate(testing::_, data_use))
      .Times(2);
  ascriber()->OnNetworkBytesSent(request.get(), 2);
  ascriber()->OnNetworkBytesReceived(request.get(), 2);

  EXPECT_CALL(mock_observer, OnPageResourceLoad(testing::_, data_use)).Times(1);
  ascriber()->OnUrlRequestCompleted(request.get(), false);

  EXPECT_CALL(mock_observer, OnPageLoadCommit(data_use)).Times(1);
  EXPECT_CALL(mock_observer, OnPageLoadConcluded(testing::_)).Times(1);
  ascriber()->DidFinishMainFrameNavigation(
      kRenderProcessId, kRenderFrameId, GURL("http://mobile.test.com"), false,
      kPageTransition, base::TimeTicks::Now());

  EXPECT_EQ(1u, recorders().size());
  auto& recorder_entry = recorders().front();
  EXPECT_EQ(content::GlobalFrameRoutingId(kRenderProcessId, kRenderFrameId),
            recorder_entry.main_frame_id());
  EXPECT_EQ(content::GlobalRequestID(kRenderProcessId, 0),
            recorder_entry.main_frame_request_id());
  EXPECT_EQ(GURL("http://mobile.test.com"), recorder_entry.data_use().url());

  // Now expect no DidFinishLoad observer call for "about:blank" validated URL.
  EXPECT_CALL(mock_observer, OnPageDidFinishLoad(&recorder_entry.data_use()))
      .Times(0);
  ascriber()->DidFinishLoad(kRenderProcessId, kRenderFrameId,
                            GURL("about:blank"));

  EXPECT_CALL(mock_observer, OnPageLoadConcluded(&recorder_entry.data_use()))
      .Times(1);
  ascriber()->RenderFrameDeleted(kRenderProcessId, kRenderFrameId, -1, -1);
  ascriber()->OnUrlRequestDestroyed(request.get());

  EXPECT_EQ(0u, recorders().size());
}

}  // namespace data_use_measurement
