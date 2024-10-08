// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_viewer_stream_manager.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/pdf/pdf_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "pdf/pdf_features.h"
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
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    feature_list_.InitAndEnableFeature(chrome_pdf::features::kPdfOopif);
  }

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
    PdfViewerStreamManager::Create(
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

// Verify adding and getting an `extensions::StreamContainer`.
TEST_F(PdfViewerStreamManagerTest, AddAndGetStreamContainer) {
  content::RenderFrameHost* embedder_host =
      NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  content::FrameTreeNodeId frame_tree_node_id =
      embedder_host->GetFrameTreeNodeId();

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
  content::FrameTreeNodeId frame_tree_node_id =
      embedder_host->GetFrameTreeNodeId();

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

// `PdfViewerStreamManager::IsPdfExtensionHost()` should correctly identify the
// PDF extension hosts.
TEST_F(PdfViewerStreamManagerTest, IsPdfExtensionHost) {
  auto* embedder_host = CreateChildRenderFrameHost(main_rfh(), "embedder host");
  embedder_host = NavigateAndCommit(embedder_host, GURL(kOriginalUrl1));

  // In a PDF load, there's an RFH for the extension frame for the initial
  // about:blank navigation. This RFH will always be replaced by
  // `extension_host`.
  auto* about_blank_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");
  auto* extension_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");
  auto* other_host = CreateChildRenderFrameHost(embedder_host, "other host");

  PdfViewerStreamManager* manager = pdf_viewer_stream_manager();
  manager->AddStreamContainer(embedder_host->GetFrameTreeNodeId(),
                              "internal_id",
                              pdf_test_util::GenerateSampleStreamContainer(1));
  manager->ClaimStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));

  // `about_blank_host` and `extension_host` should have the same frame tree
  // node ID, but this isn't possible with the current test infrastructure. For
  // testing purposes, it's okay to set the extension frame tree node ID to the
  // initial RFH.
  manager->SetExtensionFrameTreeNodeIdForTesting(
      embedder_host, about_blank_host->GetFrameTreeNodeId());

  // `about_blank_host` should be considered a PDF content host, even if it
  // isn't navigating to the original PDF URL.
  EXPECT_TRUE(manager->IsPdfExtensionHost(about_blank_host));

  // Now, set the extension frame tree node ID to the actual extension frame
  // tree node ID, which will be the same ID as `about_blank_host` in real
  // situations.
  manager->SetExtensionFrameTreeNodeIdForTesting(
      embedder_host, extension_host->GetFrameTreeNodeId());

  EXPECT_TRUE(manager->IsPdfExtensionHost(extension_host));

  // Unrelated hosts shouldn't be considered PDF content hosts.
  EXPECT_FALSE(manager->IsPdfExtensionHost(other_host));
}

// `PdfViewerStreamManager::IsPdfContentHost()` should correctly identify the
// PDF content hosts.
TEST_F(PdfViewerStreamManagerTest, IsPdfContentHost) {
  const GURL pdf_url = GURL(kOriginalUrl1);

  auto* embedder_host = CreateChildRenderFrameHost(main_rfh(), "embedder host");
  embedder_host = NavigateAndCommit(embedder_host, pdf_url);
  auto* extension_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");

  // In a PDF load, there's an RFH for the content frame for the initial
  // stream URL navigation. This RFH will always be replaced by
  // `content_host`.
  auto* stream_url_host =
      CreateChildRenderFrameHost(extension_host, "content host");
  auto* content_host =
      CreateChildRenderFrameHost(extension_host, "content host");
  content_host = NavigateAndCommit(content_host, pdf_url);
  auto* other_host = CreateChildRenderFrameHost(extension_host, "other host");

  PdfViewerStreamManager* manager = pdf_viewer_stream_manager();
  manager->AddStreamContainer(embedder_host->GetFrameTreeNodeId(),
                              "internal_id",
                              pdf_test_util::GenerateSampleStreamContainer(1));
  manager->ClaimStreamInfoForTesting(embedder_host);
  manager->SetExtensionFrameTreeNodeIdForTesting(
      embedder_host, extension_host->GetFrameTreeNodeId());
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));

  // `stream_url_host` and `content_host` should have the same frame tree node
  // ID, but this isn't possible with the current test infrastructure. For
  // testing purposes, it's okay to set the content frame tree node ID to the
  // initial RFH.
  manager->SetContentFrameTreeNodeIdForTesting(
      embedder_host, stream_url_host->GetFrameTreeNodeId());

  // `stream_url_host` should be considered a PDF content host, even if it isn't
  // navigating to the original PDF URL.
  EXPECT_TRUE(manager->IsPdfContentHost(stream_url_host));

  // Now, set the content frame tree node ID to the actual content frame tree
  // node ID, which will be the same as `stream_url_host` in real situations.
  manager->SetContentFrameTreeNodeIdForTesting(
      embedder_host, content_host->GetFrameTreeNodeId());

  EXPECT_TRUE(manager->IsPdfContentHost(content_host));

  // Unrelated hosts shouldn't be considered PDF content hosts.
  EXPECT_FALSE(manager->IsPdfContentHost(other_host));
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

// Verify that unclaimed stream infos can be deleted.
TEST_F(PdfViewerStreamManagerTest, DeleteUnclaimedStreamInfo) {
  content::RenderFrameHost* unclaimed_embedder_host =
      NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  content::FrameTreeNodeId frame_tree_node_id =
      unclaimed_embedder_host->GetFrameTreeNodeId();

  PdfViewerStreamManager* manager = pdf_viewer_stream_manager();
  manager->AddStreamContainer(frame_tree_node_id, "internal_id",
                              pdf_test_util::GenerateSampleStreamContainer(1));
  EXPECT_FALSE(manager->GetStreamContainer(unclaimed_embedder_host));

  manager->DeleteUnclaimedStreamInfo(frame_tree_node_id);

  // There are no remaining streams, so `PdfViewerStreamManager` should delete
  // itself.
  EXPECT_FALSE(pdf_viewer_stream_manager());
}

// If the embedder render frame is deleted, the stream should be deleted.
TEST_F(PdfViewerStreamManagerTest, RenderFrameDeletedWithClaimedStream) {
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

  // `PdfViewerStreamManager::RenderFrameDeleted()` should cause the stream
  // associated with `actual_host` to be deleted.
  manager->RenderFrameDeleted(actual_host);

  // There are no remaining streams, so `PdfViewerStreamManager` should delete
  // itself.
  EXPECT_FALSE(pdf_viewer_stream_manager());
}

TEST_F(PdfViewerStreamManagerTest, RenderFrameDeletedWithUnclaimedStream) {
  auto* actual_host = CreateChildRenderFrameHost(main_rfh(), "actual host");
  actual_host = NavigateAndCommit(actual_host, GURL(kOriginalUrl1));

  PdfViewerStreamManager* manager = pdf_viewer_stream_manager();
  manager->AddStreamContainer(actual_host->GetFrameTreeNodeId(), "internal_id",
                              pdf_test_util::GenerateSampleStreamContainer(1));

  // The stream hasn't been claimed, so the stream container can't be retrieved.
  ASSERT_FALSE(manager->GetStreamContainer(actual_host));

  // Unrelated hosts should be ignored.
  manager->RenderFrameDeleted(main_rfh());
  ASSERT_EQ(manager, pdf_viewer_stream_manager());

  // `PdfViewerStreamManager::RenderFrameDeleted()` should cause the stream
  // associated with `actual_host` to be deleted.
  manager->RenderFrameDeleted(actual_host);

  // There are no remaining streams, so `PdfViewerStreamManager` should delete
  // itself.
  EXPECT_FALSE(pdf_viewer_stream_manager());
}

// If the `content::RenderFrameHost` for the stream changes, then the stream
// should be deleted.
TEST_F(PdfViewerStreamManagerTest, EmbedderRenderFrameHostChanged) {
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

// If the PDF extension host changes to a different host, the stream should be
// deleted.
TEST_F(PdfViewerStreamManagerTest, ExtensionRenderFrameHostChanged) {
  auto* embedder_host = CreateChildRenderFrameHost(main_rfh(), "embedder host");
  embedder_host = NavigateAndCommit(embedder_host, GURL(kOriginalUrl1));

  // In a PDF load, there's an RFH for the extension frame for the initial
  // about:blank navigation. This RFH will always be replaced by
  // `extension_host` and shouldn't trigger stream deletion. Both hosts should
  // share the same frame name.
  auto* about_blank_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");
  about_blank_host = NavigateAndCommit(about_blank_host, GURL("about:blank"));
  auto* extension_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");
  auto* new_host = CreateChildRenderFrameHost(embedder_host, "new host");

  PdfViewerStreamManager* manager = pdf_viewer_stream_manager();
  manager->AddStreamContainer(embedder_host->GetFrameTreeNodeId(),
                              "internal_id",
                              pdf_test_util::GenerateSampleStreamContainer(1));
  manager->ClaimStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));

  // `about_blank_host` and `extension_host` should have the same frame tree
  // node ID, but this isn't possible with the current test infrastructure. For
  // testing purposes, it's okay to set the extension frame tree node ID to the
  // initial RFH.
  manager->SetExtensionFrameTreeNodeIdForTesting(
      embedder_host, about_blank_host->GetFrameTreeNodeId());

  // Changing `about_blank_host` to `extension_host` shouldn't delete the
  // stream.
  manager->RenderFrameHostChanged(about_blank_host, extension_host);

  ASSERT_TRUE(pdf_viewer_stream_manager());
  EXPECT_TRUE(manager->GetStreamContainer(embedder_host));

  // Now, set the extension frame tree node ID to the actual extension frame
  // tree node ID, which will be the same ID as `about_blank_host` in real
  // situations.
  manager->SetExtensionFrameTreeNodeIdForTesting(
      embedder_host, extension_host->GetFrameTreeNodeId());

  // Changing the extension host should delete the stream.
  manager->RenderFrameHostChanged(extension_host, new_host);

  // There are no remaining streams, so `PdfViewerStreamManager` should delete
  // itself.
  EXPECT_FALSE(pdf_viewer_stream_manager());
}

// If the PDF content host changes to a different host, the stream should be
// deleted.
TEST_F(PdfViewerStreamManagerTest, ContentRenderFrameHostChanged) {
  const GURL pdf_url = GURL(kOriginalUrl1);

  auto* embedder_host = CreateChildRenderFrameHost(main_rfh(), "embedder host");
  embedder_host = NavigateAndCommit(embedder_host, pdf_url);
  auto* extension_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");

  // In a PDF load, there's an RFH for the content frame for the initial
  // stream URL navigation. This RFH will always be replaced by
  // `content_host` and shouldn't trigger stream deletion.
  auto* stream_url_host =
      CreateChildRenderFrameHost(extension_host, "content host");
  auto* content_host =
      CreateChildRenderFrameHost(extension_host, "content host");
  content_host = NavigateAndCommit(content_host, pdf_url);
  auto* new_host = CreateChildRenderFrameHost(extension_host, "new host");

  PdfViewerStreamManager* manager = pdf_viewer_stream_manager();
  manager->AddStreamContainer(embedder_host->GetFrameTreeNodeId(),
                              "internal_id",
                              pdf_test_util::GenerateSampleStreamContainer(1));
  manager->ClaimStreamInfoForTesting(embedder_host);
  manager->SetExtensionFrameTreeNodeIdForTesting(
      embedder_host, extension_host->GetFrameTreeNodeId());
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));

  // The extension host needs to have the PDF extension origin so that
  // `pdf_frame_util::GetEmbedderHost()` can identify and get the embedder host
  // in `PdfViewerStreamManager::MaybeDeleteStreamOnPdfContentHostChanged()`.
  content::OverrideLastCommittedOrigin(
      extension_host,
      url::Origin::Create(GURL(
          "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html")));

  // `stream_url_host` and `content_host` should have the same frame tree node
  // ID, but this isn't possible with the current test infrastructure. For
  // testing purposes, it's okay to set the content frame tree node ID to the
  // initial RFH.
  manager->SetContentFrameTreeNodeIdForTesting(
      embedder_host, stream_url_host->GetFrameTreeNodeId());

  // Changing `stream_url_host` to `content_host` shouldn't delete the stream.
  manager->RenderFrameHostChanged(stream_url_host, content_host);

  ASSERT_TRUE(pdf_viewer_stream_manager());
  EXPECT_TRUE(manager->GetStreamContainer(embedder_host));

  // Now, set the content frame tree node ID to the actual content frame tree
  // node ID, which will be the same as `stream_url_host` in real situations.
  manager->SetContentFrameTreeNodeIdForTesting(
      embedder_host, content_host->GetFrameTreeNodeId());

  // Changing the content host should delete the stream.
  manager->RenderFrameHostChanged(content_host, new_host);

  // There are no remaining streams, so `PdfViewerStreamManager` should delete
  // itself.
  EXPECT_FALSE(pdf_viewer_stream_manager());
}

// If the `content::RenderFrameHost` for the stream is deleted, then the stream
// should be deleted.
TEST_F(PdfViewerStreamManagerTest, EmbedderFrameDeleted) {
  content::RenderFrameHost* embedder_host =
      NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  content::FrameTreeNodeId frame_tree_node_id =
      embedder_host->GetFrameTreeNodeId();

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

// If the PDF extension frame is deleted, the stream should be deleted.
TEST_F(PdfViewerStreamManagerTest, ExtensionFrameDeleted) {
  auto* embedder_host = CreateChildRenderFrameHost(main_rfh(), "actual host");
  embedder_host = NavigateAndCommit(embedder_host, GURL(kOriginalUrl1));
  auto* extension_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");
  content::FrameTreeNodeId frame_tree_node_id =
      extension_host->GetFrameTreeNodeId();

  PdfViewerStreamManager* manager = pdf_viewer_stream_manager();
  manager->AddStreamContainer(embedder_host->GetFrameTreeNodeId(),
                              "internal_id",
                              pdf_test_util::GenerateSampleStreamContainer(1));
  manager->ClaimStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));

  // Set the extension frame tree node ID so the stream can be deleted when the
  // extension host is deleted.
  manager->SetExtensionFrameTreeNodeIdForTesting(embedder_host,
                                                 frame_tree_node_id);

  // Deleting the extension host should cause the stream to be deleted.
  manager->FrameDeleted(frame_tree_node_id);

  // There are no remaining streams, so `PdfViewerStreamManager` should delete
  // itself.
  EXPECT_FALSE(pdf_viewer_stream_manager());
}

// If the PDF content frame is deleted, the stream should be deleted.
TEST_F(PdfViewerStreamManagerTest, ContentFrameDeleted) {
  auto* embedder_host = CreateChildRenderFrameHost(main_rfh(), "embedder host");
  embedder_host = NavigateAndCommit(embedder_host, GURL(kOriginalUrl1));
  auto* extension_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");

  auto* content_host =
      CreateChildRenderFrameHost(extension_host, "content host");
  content::FrameTreeNodeId frame_tree_node_id =
      content_host->GetFrameTreeNodeId();

  PdfViewerStreamManager* manager = pdf_viewer_stream_manager();
  manager->AddStreamContainer(embedder_host->GetFrameTreeNodeId(),
                              "internal_id",
                              pdf_test_util::GenerateSampleStreamContainer(1));
  manager->ClaimStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));

  // Set the content frame tree node ID so the stream can be deleted when the
  // content host is deleted.
  manager->SetContentFrameTreeNodeIdForTesting(embedder_host,
                                               frame_tree_node_id);

  // Deleting the content host should cause the stream to be deleted.
  manager->FrameDeleted(frame_tree_node_id);

  // There are no remaining streams, so `PdfViewerStreamManager` should delete
  // itself.
  EXPECT_FALSE(pdf_viewer_stream_manager());
}

// Starting the navigation for the content host should set the content host
// frame tree node ID.
TEST_F(PdfViewerStreamManagerTest,
       DidStartNavigationSetContentHostFrameTreeNodeId) {
  content::RenderFrameHost* embedder_host =
      NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  auto* extension_host =
      CreateChildRenderFrameHost(embedder_host, "extension host");
  auto* pdf_host = CreateChildRenderFrameHost(extension_host, "pdf host");
  content::FrameTreeNodeId content_frame_tree_node_id =
      pdf_host->GetFrameTreeNodeId();

  PdfViewerStreamManager* manager = pdf_viewer_stream_manager();
  manager->AddStreamContainer(embedder_host->GetFrameTreeNodeId(),
                              "internal_id",
                              pdf_test_util::GenerateSampleStreamContainer(1));
  manager->ClaimStreamInfoForTesting(embedder_host);
  ASSERT_TRUE(manager->GetStreamContainer(embedder_host));

  // Deleting the frame using frame tree node ID shouldn't trigger the stream
  // deletion, since the content host frame tree node ID hasn't been set.
  manager->FrameDeleted(pdf_host->GetFrameTreeNodeId());

  ASSERT_TRUE(pdf_viewer_stream_manager());

  NiceMock<content::MockNavigationHandle> navigation_handle(GURL(kOriginalUrl1),
                                                            pdf_host);

  // Set `navigation_handle`'s frame host to a grandchild frame host. This acts
  // as the PDF frame host.
  ON_CALL(navigation_handle, IsPdf).WillByDefault(Return(true));
  navigation_handle.set_render_frame_host(pdf_host);

  // Start the navigation. The content host frame tree node ID should now be
  // set.
  manager->DidStartNavigation(&navigation_handle);

  ASSERT_TRUE(pdf_viewer_stream_manager());

  // Deleting the frame should now trigger stream deletion, as the content host
  // frame tree node ID has been set.
  manager->FrameDeleted(content_frame_tree_node_id);

  // There are no remaining streams, so `PdfViewerStreamManager` should delete
  // itself.
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

  EXPECT_CALL(navigation_handle, RegisterSubresourceOverride);
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

  NiceMock<content::MockNavigationHandle> navigation_handle1;

  // Set `navigation_handle1`'s frame host to a grandchild frame host. This acts
  // as the PDF frame host.
  ON_CALL(navigation_handle1, IsPdf).WillByDefault(Return(true));
  navigation_handle1.set_render_frame_host(pdf_host1);
  navigation_handle1.set_url(GURL("navigation_handle_url"));

  EXPECT_CALL(navigation_handle1, RegisterSubresourceOverride);
  manager->ReadyToCommitNavigation(&navigation_handle1);

  NiceMock<content::MockNavigationHandle> navigation_handle2;

  // Set `navigation_handle2`'s frame host to a grandchild frame host. This acts
  // as the PDF frame host.
  ON_CALL(navigation_handle2, IsPdf).WillByDefault(Return(true));
  navigation_handle2.set_render_frame_host(pdf_host2);
  navigation_handle2.set_url(GURL("navigation_handle_url"));

  EXPECT_CALL(navigation_handle2, RegisterSubresourceOverride);
  manager->ReadyToCommitNavigation(&navigation_handle2);

  // The streams should persist after the PDF load to provide postMessage
  // support and PDF saving.
  ASSERT_EQ(manager, pdf_viewer_stream_manager());
  EXPECT_TRUE(manager->GetStreamContainer(embedder_host1));
  EXPECT_TRUE(manager->GetStreamContainer(embedder_host2));
}

// The initial load should claim the stream.
TEST_F(PdfViewerStreamManagerTest, ReadyToCommitNavigationClaimAndReplace) {
  content::RenderFrameHost* embedder_host =
      NavigateAndCommit(main_rfh(), GURL(kOriginalUrl1));
  PdfViewerStreamManager* manager = pdf_viewer_stream_manager();
  manager->AddStreamContainer(embedder_host->GetFrameTreeNodeId(),
                              "internal_id",
                              pdf_test_util::GenerateSampleStreamContainer(1));
  EXPECT_FALSE(manager->GetStreamContainer(embedder_host));

  NiceMock<content::MockNavigationHandle> navigation_handle1;
  navigation_handle1.set_render_frame_host(embedder_host);

  // The initial load should cause the embedder host to claim the stream.
  manager->ReadyToCommitNavigation(&navigation_handle1);
  base::WeakPtr<extensions::StreamContainer> original_stream =
      manager->GetStreamContainer(embedder_host);
  EXPECT_TRUE(original_stream);
  EXPECT_TRUE(pdf_viewer_stream_manager());

  NiceMock<content::MockNavigationHandle> navigation_handle2;
  navigation_handle2.set_render_frame_host(embedder_host);

  // Committing a navigation again shouldn't try to claim a stream again if
  // there isn't a new stream. The stream should remain the same. This can occur
  // if a page contains an embed to a PDF, and the embed later navigates to
  // another URL.
  manager->ReadyToCommitNavigation(&navigation_handle2);
  base::WeakPtr<extensions::StreamContainer> same_stream =
      manager->GetStreamContainer(embedder_host);
  ASSERT_TRUE(original_stream);
  ASSERT_TRUE(same_stream);
  EXPECT_EQ(original_stream.get(), same_stream.get());
  EXPECT_TRUE(pdf_viewer_stream_manager());

  // Re-add a duplicate stream.
  manager->AddStreamContainer(embedder_host->GetFrameTreeNodeId(),
                              "internal_id",
                              pdf_test_util::GenerateSampleStreamContainer(1));

  NiceMock<content::MockNavigationHandle> navigation_handle3;
  navigation_handle3.set_render_frame_host(embedder_host);

  // If a new stream exists for the same frame tree node ID, allow claiming the
  // new stream. This can occur if a full page PDF viewer refreshes.
  manager->ReadyToCommitNavigation(&navigation_handle3);
  EXPECT_TRUE(manager->GetStreamContainer(embedder_host));
  EXPECT_FALSE(original_stream);
  EXPECT_TRUE(pdf_viewer_stream_manager());
}

}  // namespace pdf
