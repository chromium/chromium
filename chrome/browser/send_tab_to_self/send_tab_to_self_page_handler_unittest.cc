// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_page_handler.h"

#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/link_to_text/link_to_text.mojom.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

constexpr char kExampleUrl[] = "https://www.example.com";

std::unique_ptr<KeyedService> BuildStubSendTabToSelfSyncService(
    content::BrowserContext* context) {
  return std::make_unique<StubSendTabToSelfSyncService>();
}

using base::test::ScopedFeatureList;
using base::test::TestFuture;
using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::Return;

MATCHER(IsValidNavigationHistory, "") {
  return !arg.navigations.empty() && arg.current_navigation_index.has_value() &&
         *arg.current_navigation_index >= 0 &&
         *arg.current_navigation_index < std::ssize(arg.navigations);
}

class MockTextFragmentReceiver : public blink::mojom::TextFragmentReceiver {
 public:
  MockTextFragmentReceiver() = default;
  ~MockTextFragmentReceiver() override = default;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.reset();
    receiver_.Bind(mojo::PendingReceiver<blink::mojom::TextFragmentReceiver>(
        std::move(handle)));
  }

  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(void, RequestSelector, (RequestSelectorCallback), (override));
  MOCK_METHOD(void,
              RequestSelectorForSelection,
              (RequestSelectorForSelectionCallback),
              (override));
  MOCK_METHOD(void, RemoveFragments, (), (override));
  MOCK_METHOD(void,
              ExtractTextFragmentsMatches,
              (ExtractTextFragmentsMatchesCallback),
              (override));
  MOCK_METHOD(void,
              GetExistingSelectors,
              (GetExistingSelectorsCallback),
              (override));
  void RequestSelectorForViewportCenter(
      RequestSelectorForViewportCenterCallback callback) override {
    selector_callback_ = std::move(callback);
    if (on_request_selector_called_) {
      std::move(on_request_selector_called_).Run();
    }
  }
  MOCK_METHOD(void,
              ExtractFirstFragmentRect,
              (ExtractFirstFragmentRectCallback),
              (override));

  void WaitForRequestSelector() {
    if (selector_callback_) {
      return;
    }
    base::test::TestFuture<void> future;
    on_request_selector_called_ = future.GetCallback();
    EXPECT_TRUE(future.Wait());
  }

  void RespondToSelectorRequest(const std::string& selector) {
    if (selector_callback_) {
      std::move(selector_callback_)
          .Run(selector, shared_highlighting::LinkGenerationError::kNone,
               shared_highlighting::LinkGenerationReadyStatus::
                   kRequestedAfterReady);
    }
  }

  void RespondToSelectorRequestWithError(
      shared_highlighting::LinkGenerationError error) {
    if (selector_callback_) {
      std::move(selector_callback_)
          .Run("", error,
               shared_highlighting::LinkGenerationReadyStatus::
                   kRequestedAfterReady);
    }
  }

 private:
  mojo::Receiver<blink::mojom::TextFragmentReceiver> receiver_{this};
  RequestSelectorForViewportCenterCallback selector_callback_;
  base::OnceClosure on_request_selector_called_;
};


class SendTabToSelfPageHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  SendTabToSelfPageHandlerTest()
      : SendTabToSelfPageHandlerTest(std::vector<base::test::FeatureRef>{
            kSendTabToSelfPropagateScrollPosition}) {}

  explicit SendTabToSelfPageHandlerTest(
      const std::vector<base::test::FeatureRef>& enabled_features)
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitWithFeatures(enabled_features, {});
  }
  ~SendTabToSelfPageHandlerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildStubSendTabToSelfSyncService));

    NavigateAndCommit(GURL(kExampleUrl));

    // Override the interface provider to return our mock.
    content::RenderFrameHost* main_frame =
        web_contents()->GetPrimaryMainFrame();
    service_manager::InterfaceProvider::TestApi(
        main_frame->GetRemoteInterfaces())
        .SetBinderForName(
            blink::mojom::TextFragmentReceiver::Name_,
            base::BindRepeating(
                [](base::WeakPtr<SendTabToSelfPageHandlerTest> self,
                   mojo::ScopedMessagePipeHandle handle) {
                  if (self) {
                    self->mock_receiver_.Bind(std::move(handle));
                  }
                },
                weak_ptr_factory_.GetWeakPtr()));
  }

  void TearDown() override {
    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::NullCallback());
    ChromeRenderViewHostTestHarness::TearDown();
  }

  FakeSendTabToSelfModel* model() {
    return static_cast<StubSendTabToSelfSyncService*>(
               SendTabToSelfSyncServiceFactory::GetForProfile(profile()))
        ->GetFakeSendTabToSelfModel();
  }

 protected:
  ScopedFeatureList scoped_feature_list_;
  MockTextFragmentReceiver mock_receiver_;

 private:
  base::WeakPtrFactory<SendTabToSelfPageHandlerTest> weak_ptr_factory_{this};
};

TEST_F(SendTabToSelfPageHandlerTest,
       ShouldSendEntryWithScrollPositionWhenGenerationSucceeds) {
  SendTabToSelfPageHandler* handler =
      SendTabToSelfPageHandler::GetOrCreateForWebContents(web_contents());
  handler->SetSelectorGenerationTimeoutForTesting(base::Milliseconds(200));

  const GURL url(kExampleUrl);
  const std::string title = "Title";
  const std::string device_id = "device_id";

  // Prepare the model to capture the finalized entry once the generation
  // process completes.
  TestFuture<const SendTabToSelfEntry*> future;
  model()->SetSendEntryCallback(future.GetRepeatingCallback());

  // Initiate the send to device action. This will trigger an asynchronous
  // Mojo call to the renderer to generate the scroll position context.
  handler->SendTabToDevice(device_id, url, title, base::DoNothing());

  // Wait for the asynchronous Mojo request to reach our mock renderer.
  mock_receiver_.WaitForRequestSelector();

  // Simulate the renderer responding successfully with a text fragment.
  mock_receiver_.RespondToSelectorRequest("text");

  // Verify the model received the entry with the generated text fragment.
  EXPECT_EQ(
      "text",
      future.Get()->GetPageContext().scroll_position.text_fragment.text_start);
}

TEST_F(SendTabToSelfPageHandlerTest,
       ShouldSendEntryWithoutScrollPositionWhenBrowserTimesOut) {
  const GURL url(kExampleUrl);
  const std::string title = "Title";
  const std::string device_id = "device_id";

  SendTabToSelfPageHandler* handler =
      SendTabToSelfPageHandler::GetOrCreateForWebContents(web_contents());
  handler->SetSelectorGenerationTimeoutForTesting(base::Milliseconds(200));

  // Prepare the model to capture the entry when the handler falls back.
  TestFuture<const SendTabToSelfEntry*> future;
  model()->SetSendEntryCallback(future.GetRepeatingCallback());

  // Initiate the send to device action.
  handler->SendTabToDevice(device_id, url, title, base::DoNothing());

  // Wait for the asynchronous Mojo request to reach our mock renderer.
  mock_receiver_.WaitForRequestSelector();

  // Do NOT immediately respond from the mock renderer. Instead, trigger the
  // browser-side fallback by fast-forwarding time past the generation timeout.
  task_environment()->FastForwardBy(base::Milliseconds(200));

  // Verify the handler didn't wait indefinitely and proceeded to send the
  // tab without the scroll position context.
  EXPECT_TRUE(future.Get()
                  ->GetPageContext()
                  .scroll_position.text_fragment.text_start.empty());
}

TEST_F(SendTabToSelfPageHandlerTest,
       ShouldSendEntryWithoutScrollPositionWhenRendererTimesOut) {
  const GURL url(kExampleUrl);
  const std::string title = "Title";
  const std::string device_id = "device_id";

  SendTabToSelfPageHandler* handler =
      SendTabToSelfPageHandler::GetOrCreateForWebContents(web_contents());
  handler->SetSelectorGenerationTimeoutForTesting(base::Milliseconds(200));

  // Prepare the model to capture the finalized entry.
  TestFuture<const SendTabToSelfEntry*> future;
  model()->SetSendEntryCallback(future.GetRepeatingCallback());

  // Initiate the send to device action.
  handler->SendTabToDevice(device_id, url, title, base::DoNothing());

  // Wait for the asynchronous Mojo request to reach our mock renderer.
  mock_receiver_.WaitForRequestSelector();

  // Simulate a timeout explicitly returned by the renderer.
  mock_receiver_.RespondToSelectorRequestWithError(
      shared_highlighting::LinkGenerationError::kTimeout);

  // Verify the model received the entry but without the scroll position
  // context since generation failed.
  EXPECT_TRUE(future.Get()
                  ->GetPageContext()
                  .scroll_position.text_fragment.text_start.empty());
}

TEST_F(SendTabToSelfPageHandlerTest,
       ShouldSendEntryWithoutScrollPositionWhenPageNavigatesDuringGeneration) {
  const GURL url(kExampleUrl);
  const std::string title = "Title";
  const std::string device_id = "device_id";

  SendTabToSelfPageHandler* handler =
      SendTabToSelfPageHandler::GetOrCreateForWebContents(web_contents());
  handler->SetSelectorGenerationTimeoutForTesting(base::Milliseconds(200));

  // Prepare the model to capture the entry when the fallback is triggered.
  TestFuture<const SendTabToSelfEntry*> future;
  model()->SetSendEntryCallback(future.GetRepeatingCallback());

  // Initiate the send to device action.
  handler->SendTabToDevice(device_id, url, title, base::DoNothing());

  // Wait for the asynchronous Mojo request to reach our mock renderer.
  mock_receiver_.WaitForRequestSelector();

  // Simulate the user navigating to another page while the capture is still
  // pending. This should instantly trigger a fallback.
  NavigateAndCommit(GURL("https://www.other.com"));

  // Verify the model received the entry but without the scroll position
  // context since generation was safely aborted by the navigation.
  EXPECT_TRUE(future.Get()
                  ->GetPageContext()
                  .scroll_position.text_fragment.text_start.empty());

  // Clean up the captured callback.
  mock_receiver_.RespondToSelectorRequest("");
}

TEST_F(SendTabToSelfPageHandlerTest,
       ShouldSendEntryWhenWebContentsIsDestroyedDuringGeneration) {
  const GURL url(kExampleUrl);
  const std::string title = "Title";
  const std::string device_id = "device_id";

  SendTabToSelfPageHandler* handler =
      SendTabToSelfPageHandler::GetOrCreateForWebContents(web_contents());
  handler->SetSelectorGenerationTimeoutForTesting(base::Milliseconds(200));

  // Prepare the model to capture the entry when the fallback is triggered by
  // the tab closure.
  TestFuture<const SendTabToSelfEntry*> future;
  model()->SetSendEntryCallback(future.GetRepeatingCallback());

  // Initiate the send to device action.
  handler->SendTabToDevice(device_id, url, title, base::DoNothing());

  // Wait for the asynchronous Mojo request to reach our mock renderer.
  mock_receiver_.WaitForRequestSelector();

  // Destroy the WebContents while the capture request is still pending.
  DeleteContents();

  // Verify the model received the entry but without the scroll position
  // context since generation was safely aborted by the tab closure.
  EXPECT_TRUE(future.Get()
                  ->GetPageContext()
                  .scroll_position.text_fragment.text_start.empty());

  // Clean up the captured callback.
  mock_receiver_.RespondToSelectorRequest("");
}

class SendTabToSelfPageHandlerWithNavigationHistoryTest
    : public SendTabToSelfPageHandlerTest {
 public:
  SendTabToSelfPageHandlerWithNavigationHistoryTest()
      : SendTabToSelfPageHandlerTest(std::vector<base::test::FeatureRef>{
            kSendTabToSelfPropagateNavigationHistory}) {}
};

TEST_F(SendTabToSelfPageHandlerWithNavigationHistoryTest,
       ShouldSendEntryWithNavigationHistoryWhenFeatureEnabled) {
  SendTabToSelfPageHandler* handler =
      SendTabToSelfPageHandler::GetOrCreateForWebContents(web_contents());

  const GURL url("https://www.example.com");
  const std::string title = "Title";
  const std::string device_id = "device_id";

  // Navigation history should have at least the current page.
  TestFuture<const SendTabToSelfEntry*> future;
  model()->SetSendEntryCallback(future.GetRepeatingCallback());

  handler->SendTabToDevice(device_id, url, title, base::DoNothing());

  EXPECT_THAT(future.Get()->GetNavigationHistory(), IsValidNavigationHistory());
}

TEST_F(SendTabToSelfPageHandlerTest,
       ShouldInvokeErrorCallbackWhenModelIsNotReady) {
  const GURL url(kExampleUrl);
  const std::string title = "Title";
  const std::string device_id = "device_id";

  SendTabToSelfPageHandler* handler =
      SendTabToSelfPageHandler::GetOrCreateForWebContents(web_contents());
  handler->SetSelectorGenerationTimeoutForTesting(base::Milliseconds(200));

  // Simulate the model not being ready (e.g. Sync paused or disabled).
  model()->SetIsReady(false);

  // Initiate the send to device action, providing a result callback.
  TestFuture<SendTabToSelfResult> result_future;
  handler->SendTabToDevice(device_id, url, title, result_future.GetCallback());

  // Verify the callback is invoked immediately with
  // kFailureNotTrackingMetadata, bypassing the entire generation flow.
  EXPECT_EQ(SendTabToSelfResult::kFailureNotTrackingMetadata,
            result_future.Get());
}

TEST_F(SendTabToSelfPageHandlerTest, ShouldInvokeCallbackOnSuccess) {
  const GURL url(kExampleUrl);
  const std::string title = "Title";
  const std::string device_id = "device_id";

  auto* handler =
      SendTabToSelfPageHandler::GetOrCreateForWebContents(web_contents());

  // Prepare the model to accept the entry.
  TestFuture<const SendTabToSelfEntry*> future;
  model()->SetSendEntryCallback(future.GetRepeatingCallback());

  // Initiate the send to device action, providing a result callback.
  TestFuture<SendTabToSelfResult> result_future;
  handler->SendTabToDevice(device_id, url, title, result_future.GetCallback());

  // Fast-forward to skip selector generation (since it's not the focus of
  // this test).
  mock_receiver_.WaitForRequestSelector();
  mock_receiver_.RespondToSelectorRequest("");

  // Verify the callback is invoked with kSuccess.
  EXPECT_EQ(SendTabToSelfResult::kSuccess, result_future.Get());
}

TEST_F(SendTabToSelfPageHandlerTest,
       ShouldSendEntryWithoutContextWhenSharingLink) {
  // This is different from the current page URL.
  const GURL link_url("https://www.other.com");
  const std::string title = "Title";
  const std::string device_id = "device_id";

  SendTabToSelfPageHandler* handler =
      SendTabToSelfPageHandler::GetOrCreateForWebContents(web_contents());

  // Prepare the model to capture the entry.
  TestFuture<const SendTabToSelfEntry*> future;
  model()->SetSendEntryCallback(future.GetRepeatingCallback());

  // We don't expect any Mojo calls to the renderer since this is link sharing.
  EXPECT_CALL(mock_receiver_, RequestSelector(_)).Times(0);

  // Initiate the send to device action for a DIFFERENT URL than the current
  // page (which is `kExampleUrl`).
  handler->SendTabToDevice(device_id, link_url, title, base::DoNothing());

  // Verify the model received the entry but without any context.
  EXPECT_TRUE(future.Get()
                  ->GetPageContext()
                  .scroll_position.text_fragment.text_start.empty());
  EXPECT_TRUE(future.Get()->GetPageContext().form_field_info.fields.empty());
}

}  // namespace

}  // namespace send_tab_to_self
