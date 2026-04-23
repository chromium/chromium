// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/pdf/mime_handler_stream_manager.h"
#include "chrome/browser/pdf/pdf_handler_stream_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "extensions/browser/mime_handler/mime_handler_test_helpers.h"
#include "extensions/browser/mime_handler/mock_mime_handler_stream_delegate.h"
#include "extensions/browser/mime_handler/stream_container.h"
#include "extensions/browser/mime_handler/stream_info.h"
#include "pdf/pdf_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/transferrable_url_loader.mojom.h"
#include "url/gurl.h"

namespace pdf {

namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

constexpr char kOriginalUrl1[] = "https://original_url1";
constexpr char kOriginalUrl2[] = "https://original_url2";

using extensions::mime_handler::MockMimeHandlerStreamDelegate;

}  // namespace

class MimeHandlerStreamManagerPdfTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    feature_list_.InitAndEnableFeature(chrome_pdf::features::kPdfOopif);
  }

  void TearDown() override {
    ChromeRenderViewHostTestHarness::web_contents()->RemoveUserData(
        MimeHandlerStreamManager::UserDataKey());
    ChromeRenderViewHostTestHarness::TearDown();
  }

  MimeHandlerStreamManager* mime_handler_stream_manager() {
    return MimeHandlerStreamManager::FromWebContents(
        ChromeRenderViewHostTestHarness::web_contents());
  }

  // Simulate a navigation and commit on `host`. The last committed URL will be
  // `original_url`.
  content::RenderFrameHost* NavigateAndCommit(content::RenderFrameHost* host,
                                              const GURL& original_url) {
    content::RenderFrameHost* new_host =
        content::NavigationSimulator::NavigateAndCommitFromDocument(
            original_url, host);

    // Create `MimeHandlerStreamManager` if it doesn't exist already. If `host`
    // is the primary main frame, then the previous `MimeHandlerStreamManager`
    // may have been deleted as part of the above navigation.
    MimeHandlerStreamManager::Create(
        ChromeRenderViewHostTestHarness::web_contents());
    return new_host;
  }

  content::RenderFrameHost* CreateChildRenderFrameHost(
      content::RenderFrameHost* parent_host,
      const std::string& frame_name) {
    auto* parent_host_tester = content::RenderFrameHostTester::For(parent_host);
    parent_host_tester->InitializeRenderFrameIfNeeded();
    return parent_host_tester->AppendChild(frame_name);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Starting the navigation for the content host should set the content host
// frame tree node ID.
TEST_F(MimeHandlerStreamManagerPdfTest,
       DidStartNavigationSetContentHostFrameTreeNodeId) {
  content::RenderFrameHost* embedder_host =
      NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  auto* extension_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");
  auto* pdf_host = CreateChildRenderFrameHost(extension_host, "pdf host");
  content::FrameTreeNodeId content_frame_tree_node_id =
      pdf_host->GetFrameTreeNodeId();

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<PdfHandlerStreamDelegate>());
  manager->ClaimStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));

  // Deleting the frame using frame tree node ID shouldn't trigger the stream
  // deletion, since the content host frame tree node ID hasn't been set.
  manager->FrameDeleted(pdf_host->GetFrameTreeNodeId());

  ASSERT_TRUE(mime_handler_stream_manager());

  NiceMock<content::MockNavigationHandle> navigation_handle(GURL(kOriginalUrl1),
                                                            pdf_host);

  // Set `navigation_handle`'s frame host to a grandchild frame host. This acts
  // as the content frame host.
  ON_CALL(navigation_handle, IsPdf).WillByDefault(Return(true));
  navigation_handle.set_render_frame_host(pdf_host);

  // Start the navigation. The content host frame tree node ID should now be
  // set.
  manager->DidStartNavigation(&navigation_handle);

  ASSERT_TRUE(mime_handler_stream_manager());

  // Deleting the frame should now trigger stream deletion, as the content host
  // frame tree node ID has been set.
  manager->FrameDeleted(content_frame_tree_node_id);

  // There are no remaining streams, so `MimeHandlerStreamManager` should delete
  // itself.
  EXPECT_FALSE(mime_handler_stream_manager());
}

// `MimeHandlerStreamManager` should register a subresource
// override if the navigation handle is for a PDF content frame.
TEST_F(MimeHandlerStreamManagerPdfTest,
       ReadyToCommitNavigationSubresourceOverride) {
  content::RenderFrameHost* embedder_host =
      NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  auto* extension_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");
  auto* pdf_host = CreateChildRenderFrameHost(extension_host, "pdf host");

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<PdfHandlerStreamDelegate>());
  manager->ClaimStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));

  NiceMock<content::MockNavigationHandle> navigation_handle;

  // Set `navigation_handle`'s frame host to a grandchild frame host. This acts
  // as the content frame host.
  ON_CALL(navigation_handle, IsPdf).WillByDefault(Return(true));
  navigation_handle.set_render_frame_host(pdf_host);
  navigation_handle.set_url(GURL("navigation_handle_url"));

  EXPECT_CALL(navigation_handle, RegisterSubresourceOverride);
  manager->ReadyToCommitNavigation(&navigation_handle);

  // The stream should persist after the load to provide postMessage support and
  // saving.
  ASSERT_EQ(manager, mime_handler_stream_manager());
  EXPECT_TRUE(manager->GetStreamContainer(embedder_host));
}

// `MimeHandlerStreamManager` should be able to handle registering multiple
// subresource override for multiple streams.
TEST_F(MimeHandlerStreamManagerPdfTest,
       ReadyToCommitNavigationSubresourceOverrideMultipleStreams) {
  content::RenderFrameHost* embedder_host1 =
      NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  auto* extension_host1 =
      CreateChildRenderFrameHost(embedder_host1, "extension host1");
  auto* pdf_host1 = CreateChildRenderFrameHost(extension_host1, "pdf host");

  auto* embedder_host2 =
      CreateChildRenderFrameHost(embedder_host1, "embedder host2");
  embedder_host2 = NavigateAndCommit(embedder_host2, GURL(kOriginalUrl2));
  auto* extension_host2 =
      CreateChildRenderFrameHost(embedder_host2, "extension host2");
  auto* pdf_host2 = CreateChildRenderFrameHost(extension_host2, "pdf host2");

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  manager->AddStreamContainer(
      embedder_host1->GetFrameTreeNodeId(), "internal_id1",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<PdfHandlerStreamDelegate>());
  manager->AddStreamContainer(
      embedder_host2->GetFrameTreeNodeId(), "internal_id2",
      extensions::mime_handler::GenerateSampleStreamContainer(2),
      std::make_unique<PdfHandlerStreamDelegate>());
  manager->ClaimStreamInfoForTesting(main_rfh());
  manager->ClaimStreamInfoForTesting(embedder_host2);
  ASSERT_TRUE(manager->GetStreamContainer(main_rfh()));
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host2));

  NiceMock<content::MockNavigationHandle> navigation_handle1;

  // Set `navigation_handle1`'s frame host to a grandchild frame host. This acts
  // as the content frame host.
  ON_CALL(navigation_handle1, IsPdf).WillByDefault(Return(true));
  navigation_handle1.set_render_frame_host(pdf_host1);
  navigation_handle1.set_url(GURL("navigation_handle_url"));

  EXPECT_CALL(navigation_handle1, RegisterSubresourceOverride);
  manager->ReadyToCommitNavigation(&navigation_handle1);

  NiceMock<content::MockNavigationHandle> navigation_handle2;

  // Set `navigation_handle2`'s frame host to a grandchild frame host. This acts
  // as the content frame host.
  ON_CALL(navigation_handle2, IsPdf).WillByDefault(Return(true));
  navigation_handle2.set_render_frame_host(pdf_host2);
  navigation_handle2.set_url(GURL("navigation_handle_url"));

  EXPECT_CALL(navigation_handle2, RegisterSubresourceOverride);
  manager->ReadyToCommitNavigation(&navigation_handle2);

  // The streams should persist after the load to provide postMessage support
  // and saving.
  ASSERT_EQ(manager, mime_handler_stream_manager());
  EXPECT_TRUE(manager->GetStreamContainer(embedder_host1));
  EXPECT_TRUE(manager->GetStreamContainer(embedder_host2));
}

// If the navigation handle is not semantically a content-frame navigation
// (e.g. a non-PDF subframe of the PDF extension host), the structural shape
// `embedder -> extension -> child` should not trigger subresource override
// registration. The anonymous-namespace `IsContentFrameNavigation()` helper
// in `MimeHandlerStreamManager` reads `IsPdf()` to gate this work.
TEST_F(MimeHandlerStreamManagerPdfTest,
       StructuralMatchButGateFalseSkipsContentFrameWork) {
  auto* embedder_host = NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  auto* extension_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");
  auto* pdf_host = CreateChildRenderFrameHost(extension_host, "pdf host");

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  auto delegate = std::make_unique<NiceMock<MockMimeHandlerStreamDelegate>>();
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::move(delegate));
  manager->ClaimStreamInfoForTesting(embedder_host);

  auto* stream_info = manager->GetClaimedStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(stream_info);

  NiceMock<content::MockNavigationHandle> navigation_handle(GURL(kOriginalUrl1),
                                                            pdf_host);
  navigation_handle.set_render_frame_host(pdf_host);
  ON_CALL(navigation_handle, IsPdf).WillByDefault(Return(false));

  EXPECT_CALL(navigation_handle, RegisterSubresourceOverride).Times(0);

  manager->ReadyToCommitNavigation(&navigation_handle);
  manager->DidFinishNavigation(&navigation_handle);
}

// If the URL is reloaded during a load, `MimeHandlerStreamManager` should
// ignore the initial content frame navigation.
TEST_F(MimeHandlerStreamManagerPdfTest,
       DidFinishNavigationReloadOverlappingNavigations) {
  const GURL pdf_url(kOriginalUrl1);

  // Set up the first load and initial content navigation handle.
  content::RenderFrameHost* embedder_host =
      NavigateAndCommit(main_rfh(), pdf_url);
  auto* extension_host1 =
      CreateChildRenderFrameHost(embedder_host, "extension host1");
  auto* pdf_host1 = CreateChildRenderFrameHost(extension_host1, "pdf host1");

  MimeHandlerStreamManager* manager = mime_handler_stream_manager();
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id1",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<PdfHandlerStreamDelegate>());
  manager->ClaimStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));

  manager->SetContentFrameTreeNodeIdForTesting(embedder_host,
                                               pdf_host1->GetFrameTreeNodeId());

  NiceMock<content::MockNavigationHandle> navigation_handle1(pdf_url,
                                                             pdf_host1);
  ON_CALL(navigation_handle1, IsPdf).WillByDefault(Return(true));

  // Before processing the initial content navigation handle, "reload" the URL.
  // The embedder host will stay the same, but there will be a new content frame
  // and navigation handle. The stream info will be overridden.
  // Do not use `NavigateAndCommit()` here, as it would delete `manager` (which
  // does not happen in production code).
  auto* extension_host2 =
      CreateChildRenderFrameHost(embedder_host, "extension host2");
  auto* pdf_host2 = CreateChildRenderFrameHost(extension_host2, "pdf host2");
  manager->AddStreamContainer(
      embedder_host->GetFrameTreeNodeId(), "internal_id1",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<PdfHandlerStreamDelegate>());
  manager->ClaimStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));

  manager->SetContentFrameTreeNodeIdForTesting(embedder_host,
                                               pdf_host2->GetFrameTreeNodeId());

  NiceMock<content::MockNavigationHandle> navigation_handle2(pdf_url,
                                                             pdf_host2);
  ON_CALL(navigation_handle2, IsPdf).WillByDefault(Return(true));

  EXPECT_FALSE(manager->DidPdfContentNavigate(embedder_host));

  // The initial content navigation should not update any stream state.
  manager->DidFinishNavigation(&navigation_handle1);
  EXPECT_FALSE(manager->DidPdfContentNavigate(embedder_host));

  // The new, correct content navigation should update the stream state.
  manager->DidFinishNavigation(&navigation_handle2);
  EXPECT_TRUE(manager->DidPdfContentNavigate(embedder_host));
}

}  // namespace pdf
