// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_viewer_stream_manager.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/pdf/pdf_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/transferrable_url_loader.mojom.h"
#include "url/gurl.h"

namespace pdf {

namespace {

using ::testing::NiceMock;
using ::testing::Return;

constexpr char kOriginalUrl1[] = "https://original_url1";
constexpr char kOriginalUrl2[] = "https://original_url2";

}  // namespace

class PdfViewerStreamManagerTest : public ChromeRenderViewHostTestHarness {
 protected:
  void TearDown() override {
    ChromeRenderViewHostTestHarness::web_contents()->RemoveUserData(
        PdfViewerStreamManager::UserDataKey());
    ChromeRenderViewHostTestHarness::TearDown();
  }

  PdfViewerStreamManager* pdf_viewer_stream_manager() {
    return PdfViewerStreamManager::FromWebContents(
        ChromeRenderViewHostTestHarness::web_contents());
  }

  // Simulate a navigation and commit on `host`. The last committed URL will be
  // `original_url`.
  content::RenderFrameHost* NavigateAndCommit(content::RenderFrameHost* host,
                                              const GURL& original_url) {
    content::RenderFrameHost* new_host =
        content::NavigationSimulator::NavigateAndCommitFromDocument(
            original_url, host);

    // Create `PdfViewerStreamManager` if it doesn't exist already. If `host` is
    // the primary main frame, then the previous `PdfViewerStreamManager` may
    // have been deleted as part of the above navigation.
    PdfViewerStreamManager::CreateForWebContents(
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
};

// Verify adding and getting an `extensions::StreamContainer`.
TEST_F(PdfViewerStreamManagerTest, AddAndGetStreamContainer) {
  content::RenderFrameHost* embedder_host =
      NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  int frame_tree_node_id = embedder_host->GetFrameTreeNodeId();

  PdfViewerStreamManager* manager = pdf_viewer_stream_manager();
  manager->AddStreamContainer(frame_tree_node_id, "internal_id",
                              pdf_test_util::GenerateSampleStreamContainer(1));
  EXPECT_TRUE(manager->ContainsUnclaimedStreamInfo(frame_tree_node_id));
  manager->ClaimStreamInfoForTesting(embedder_host);

  base::WeakPtr<extensions::StreamContainer> result =
      manager->GetStreamContainer(embedder_host);

  ASSERT_TRUE(result);
  blink::mojom::TransferrableURLLoaderPtr transferrable_loader =
      result->TakeTransferrableURLLoader();
  EXPECT_EQ(result->tab_id(), 1);
  EXPECT_EQ(result->embedded(), true);
  EXPECT_EQ(result->handler_url(), GURL("https://handler_url1"));
  EXPECT_EQ(result->extension_id(), "extension_id1");
  EXPECT_EQ(transferrable_loader->url, GURL("stream://url1"));
  EXPECT_EQ(transferrable_loader->head->mime_type, "application/pdf");
  EXPECT_EQ(result->original_url(), GURL("https://original_url1"));
  EXPECT_TRUE(pdf_viewer_stream_manager());
}

// Verify adding an `extensions::StreamContainer` under the same frame tree node
// ID replaces the original unclaimed `extensions::StreamContainer`.
TEST_F(PdfViewerStreamManagerTest,
       AddStreamContainerSameFrameTreeNodeIdUnclaimed) {
  content::RenderFrameHost* embedder_host =
      NavigateAndCommit(main_rfh(), GURL(kOriginalUrl2));
  int frame_tree_node_id = embedder_host->GetFrameTreeNodeId();

  PdfViewerStreamManager* manager = pdf_viewer_stream_manager();
  manager->AddStreamContainer(frame_tree_node_id, "internal_id1",
                              pdf_test_util::GenerateSampleStreamContainer(1));
  manager->AddStreamContainer(frame_tree_node_id, "internal_id2",
                              pdf_test_util::GenerateSampleStreamContainer(2));
  manager->ClaimStreamInfoForTesting(embedder_host);

  base::WeakPtr<extensions::StreamContainer> result =
      manager->GetStreamContainer(main_rfh());

  ASSERT_TRUE(result);
  blink::mojom::TransferrableURLLoaderPtr transferrable_loader =
      result->TakeTransferrableURLLoader();
  EXPECT_EQ(result->tab_id(), 2);
  EXPECT_EQ(result->embedded(), true);
  EXPECT_EQ(result->handler_url(), GURL("https://handler_url2"));
  EXPECT_EQ(result->extension_id(), "extension_id2");
  EXPECT_EQ(transferrable_loader->url, GURL("stream://url2"));
  EXPECT_EQ(transferrable_loader->head->mime_type, "application/pdf");
  EXPECT_EQ(result->original_url(), GURL("https://original_url2"));
  EXPECT_TRUE(pdf_viewer_stream_manager());
}

// Verify getting a `StreamContainer` with a non-matching URL returns nullptr;
TEST_F(PdfViewerStreamManagerTest, AddAndGetStreamInvalidURL) {
  content::RenderFrameHost* embedder_host =
      NavigateAndCommit(main_rfh(), GURL("https://nonmatching_url"));

  PdfViewerStreamManager* manager = pdf_viewer_stream_manager();
  manager->AddStreamContainer(embedder_host->GetFrameTreeNodeId(),
                              "internal_id",
                              pdf_test_util::GenerateSampleStreamContainer(1));
  manager->ClaimStreamInfoForTesting(embedder_host);

  EXPECT_FALSE(manager->GetStreamContainer(embedder_host));
  EXPECT_TRUE(pdf_viewer_stream_manager());
}

// Verify adding multiple `extensions::StreamContainer`s for different
// FrameTreeNodes at once.
TEST_F(PdfViewerStreamManagerTest, AddMultipleStreamContainers) {
  auto* embedder_host = NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  auto* child_host = CreateChildRenderFrameHost(embedder_host, "child host");
  child_host = NavigateAndCommit(child_host, GURL(kOriginalUrl2));

  PdfViewerStreamManager* manager = pdf_viewer_stream_manager();
  manager->AddStreamContainer(embedder_host->GetFrameTreeNodeId(),
                              "internal_id1",
                              pdf_test_util::GenerateSampleStreamContainer(1));
  manager->AddStreamContainer(child_host->GetFrameTreeNodeId(), "internal_id2",
                              pdf_test_util::GenerateSampleStreamContainer(2));
  manager->ClaimStreamInfoForTesting(embedder_host);
  manager->ClaimStreamInfoForTesting(child_host);

  base::WeakPtr<extensions::StreamContainer> result =
      manager->GetStreamContainer(embedder_host);

  ASSERT_TRUE(result);
  blink::mojom::TransferrableURLLoaderPtr transferrable_loader =
      result->TakeTransferrableURLLoader();
  EXPECT_EQ(result->tab_id(), 1);
  EXPECT_EQ(result->embedded(), true);
  EXPECT_EQ(result->handler_url(), GURL("https://handler_url1"));
  EXPECT_EQ(result->extension_id(), "extension_id1");
  EXPECT_EQ(transferrable_loader->url, GURL("stream://url1"));
  EXPECT_EQ(transferrable_loader->head->mime_type, "application/pdf");
  EXPECT_EQ(result->original_url(), GURL("https://original_url1"));

  result = manager->GetStreamContainer(child_host);

  ASSERT_TRUE(result);
  transferrable_loader = result->TakeTransferrableURLLoader();
  EXPECT_EQ(result->tab_id(), 2);
  EXPECT_EQ(result->embedded(), true);
  EXPECT_EQ(result->handler_url(), GURL("https://handler_url2"));
  EXPECT_EQ(result->extension_id(), "extension_id2");
  EXPECT_EQ(transferrable_loader->url, GURL("stream://url2"));
  EXPECT_EQ(transferrable_loader->head->mime_type, "application/pdf");
  EXPECT_EQ(result->original_url(), GURL("https://original_url2"));
  EXPECT_TRUE(pdf_viewer_stream_manager());
}

// If multiple `extensions::StreamContainer`s exist, then deleting one stream
// shouldn't delete the other stream.
TEST_F(PdfViewerStreamManagerTest, DeleteWithMultipleStreamContainers) {
  content::RenderFrameHost* embedder_host =
      NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  auto* child_host = CreateChildRenderFrameHost(embedder_host, "child host");
  child_host = NavigateAndCommit(child_host, GURL(kOriginalUrl2));

  PdfViewerStreamManager* manager = pdf_viewer_stream_manager();
  manager->AddStreamContainer(embedder_host->GetFrameTreeNodeId(),
                              "internal_id1",
                              pdf_test_util::GenerateSampleStreamContainer(1));
  manager->AddStreamContainer(child_host->GetFrameTreeNodeId(), "internal_id2",
                              pdf_test_util::GenerateSampleStreamContainer(2));
  manager->ClaimStreamInfoForTesting(embedder_host);
  manager->ClaimStreamInfoForTesting(child_host);
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));
  ASSERT_TRUE(manager->GetStreamContainer(child_host));

  // `PdfViewerStreamManager::RenderFrameDeleted()` should cause the stream
  // associated with `child_host` to be deleted.
  manager->RenderFrameDeleted(child_host);

  EXPECT_TRUE(manager->GetStreamContainer(embedder_host));
  EXPECT_FALSE(manager->GetStreamContainer(child_host));
  EXPECT_TRUE(pdf_viewer_stream_manager());
}

// If the embedder render frame is deleted, the stream should be deleted.
TEST_F(PdfViewerStreamManagerTest, RenderFrameDeleted) {
  auto* actual_host = CreateChildRenderFrameHost(main_rfh(), "actual host");
  actual_host = NavigateAndCommit(actual_host, GURL(kOriginalUrl1));

  PdfViewerStreamManager* manager = pdf_viewer_stream_manager();
  manager->AddStreamContainer(actual_host->GetFrameTreeNodeId(), "internal_id",
                              pdf_test_util::GenerateSampleStreamContainer(1));
  manager->ClaimStreamInfoForTesting(actual_host);
  ASSERT_TRUE(manager->GetStreamContainer(actual_host));

  // Unrelated hosts should be ignored.
  manager->RenderFrameDeleted(main_rfh());
  ASSERT_EQ(manager, pdf_viewer_stream_manager());

  // There are no remaining streams, so `PdfViewerStreamManager` should delete
  // itself.
  manager->RenderFrameDeleted(actual_host);
  EXPECT_FALSE(pdf_viewer_stream_manager());
}

// If the `content::RenderFrameHost` for the stream changes, then the stream
// should be deleted.
TEST_F(PdfViewerStreamManagerTest, RenderFrameHostChanged) {
  content::RenderFrameHost* old_host =
      NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  auto* new_host = CreateChildRenderFrameHost(old_host, "new host");

  PdfViewerStreamManager* manager = pdf_viewer_stream_manager();
  manager->AddStreamContainer(old_host->GetFrameTreeNodeId(), "internal_id",
                              pdf_test_util::GenerateSampleStreamContainer(1));
  manager->ClaimStreamInfoForTesting(old_host);
  ASSERT_TRUE(manager->GetStreamContainer(old_host));

  // If the first parameter to RenderFrameHostChanged() is null, then it means a
  // subframe is being created and should be ignored.
  manager->RenderFrameHostChanged(nullptr, old_host);
  EXPECT_TRUE(manager->GetStreamContainer(old_host));

  // Unrelated hosts should be ignored.
  manager->RenderFrameHostChanged(new_host, new_host);
  EXPECT_TRUE(manager->GetStreamContainer(old_host));

  // There are no remaining streams, so `PdfViewerStreamManager` should delete
  // itself.
  manager->RenderFrameHostChanged(old_host, new_host);
  EXPECT_FALSE(pdf_viewer_stream_manager());
}

// If the `content::RenderFrameHost` for the stream is deleted, then the stream
// should be deleted.
TEST_F(PdfViewerStreamManagerTest, FrameDeleted) {
  content::RenderFrameHost* embedder_host =
      NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  int frame_tree_node_id = embedder_host->GetFrameTreeNodeId();

  PdfViewerStreamManager* manager = pdf_viewer_stream_manager();
  manager->AddStreamContainer(frame_tree_node_id, "internal_id",
                              pdf_test_util::GenerateSampleStreamContainer(1));
  manager->ClaimStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));

  // There are no remaining streams, so `PdfViewerStreamManager` should delete
  // itself.
  manager->FrameDeleted(frame_tree_node_id);
  EXPECT_FALSE(pdf_viewer_stream_manager());
}

// `PdfViewerStreamManager` should register a subresource
// override if the navigation handle is for a PDF content frame.
TEST_F(PdfViewerStreamManagerTest, ReadyToCommitNavigationSubresourceOverride) {
  content::RenderFrameHost* embedder_host =
      NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  auto* extension_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");
  auto* pdf_host = CreateChildRenderFrameHost(extension_host, "pdf host");

  PdfViewerStreamManager* manager = pdf_viewer_stream_manager();
  manager->AddStreamContainer(embedder_host->GetFrameTreeNodeId(),
                              "internal_id",
                              pdf_test_util::GenerateSampleStreamContainer(1));
  manager->ClaimStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));

  NiceMock<content::MockNavigationHandle> navigation_handle;

  // Set `navigation_handle`'s frame host to a grandchild frame host. This acts
  // as the PDF frame host.
  ON_CALL(navigation_handle, IsPdf).WillByDefault(Return(true));
  navigation_handle.set_render_frame_host(pdf_host);
  navigation_handle.set_url(GURL("navigation_handle_url"));

  EXPECT_CALL(navigation_handle, RegisterSubresourceOverride).Times(1);
  manager->ReadyToCommitNavigation(&navigation_handle);

  // The stream should persist after the PDF load to provide postMessage support
  // and PDF saving.
  ASSERT_EQ(manager, pdf_viewer_stream_manager());
  EXPECT_TRUE(manager->GetStreamContainer(embedder_host));
}

// `PdfViewerStreamManager` should be able to handle registering multiple
// subresource override for multiple PDF streams.
TEST_F(PdfViewerStreamManagerTest,
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

  PdfViewerStreamManager* manager = pdf_viewer_stream_manager();
  manager->AddStreamContainer(embedder_host1->GetFrameTreeNodeId(),
                              "internal_id1",
                              pdf_test_util::GenerateSampleStreamContainer(1));
  manager->AddStreamContainer(embedder_host2->GetFrameTreeNodeId(),
                              "internal_id2",
                              pdf_test_util::GenerateSampleStreamContainer(2));
  manager->ClaimStreamInfoForTesting(main_rfh());
  manager->ClaimStreamInfoForTesting(embedder_host2);
  ASSERT_TRUE(manager->GetStreamContainer(main_rfh()));
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host2));

  NiceMock<content::MockNavigationHandle> navigation_handle;

  // Set `navigation_handle`'s frame host to a grandchild frame host. This acts
  // as the PDF frame host.
  ON_CALL(navigation_handle, IsPdf).WillByDefault(Return(true));
  navigation_handle.set_render_frame_host(pdf_host1);
  navigation_handle.set_url(GURL("navigation_handle_url"));

  EXPECT_CALL(navigation_handle, RegisterSubresourceOverride).Times(2);
  manager->ReadyToCommitNavigation(&navigation_handle);

  navigation_handle.set_render_frame_host(pdf_host2);

  manager->ReadyToCommitNavigation(&navigation_handle);

  // The streams should persist after the PDF load to provide postMessage
  // support and PDF saving.
  ASSERT_EQ(manager, pdf_viewer_stream_manager());
  EXPECT_TRUE(manager->GetStreamContainer(embedder_host1));
  EXPECT_TRUE(manager->GetStreamContainer(embedder_host2));
}

// The initial load should claim the stream. If the top level frame is
// committing a navigation to a different document, the stream should be
// deleted.
TEST_F(PdfViewerStreamManagerTest, ReadyToCommitNavigationClaimAndDelete) {
  content::RenderFrameHost* embedder_host =
      NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  PdfViewerStreamManager* manager = pdf_viewer_stream_manager();
  manager->AddStreamContainer(embedder_host->GetFrameTreeNodeId(),
                              "internal_id",
                              pdf_test_util::GenerateSampleStreamContainer(1));
  EXPECT_FALSE(manager->GetStreamContainer(embedder_host));

  NiceMock<content::MockNavigationHandle> navigation_handle;
  navigation_handle.set_render_frame_host(embedder_host);

  // The initial load should cause the embedder host to claim the stream.
  manager->ReadyToCommitNavigation(&navigation_handle);
  EXPECT_TRUE(manager->GetStreamContainer(embedder_host));
  EXPECT_TRUE(pdf_viewer_stream_manager());
}

}  // namespace pdf
