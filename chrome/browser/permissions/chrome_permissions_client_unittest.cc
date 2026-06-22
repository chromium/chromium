// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/chrome_permissions_client.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_request.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS) && !BUILDFLAG(IS_ANDROID)
#include <optional>
#include <string>

#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "extensions/browser/mime_handler/mime_handler_stream_manager.h"
#include "extensions/browser/mime_handler/mime_handler_test_helpers.h"
#include "extensions/browser/mime_handler/mock_mime_handler_stream_delegate.h"
#include "extensions/browser/mime_handler/stream_container.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"
#include "url/origin.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && !BUILDFLAG(IS_ANDROID)

class ChromePermissionsClientTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
  }
};

#if BUILDFLAG(IS_ANDROID)
TEST_F(ChromePermissionsClientTest,
       MaybeCreateMessageUINeverQuietForUpgradeToPrecise) {
  auto request = std::make_unique<permissions::MockPermissionRequest>(
      GURL(permissions::MockPermissionRequest::kDefaultOrigin),
      permissions::RequestType::kGeolocation,
      permissions::PermissionRequestGestureType::GESTURE,
      permissions::GeolocationPromptType::kUpgradeToPrecise);

  base::WeakPtr<permissions::PermissionPromptAndroid> dummy_prompt;

  auto* client = ChromePermissionsClient::GetInstance();
  auto message_ui =
      client->MaybeCreateMessageUI(web_contents(), *request, dummy_prompt);

  EXPECT_FALSE(message_ui);
}
#elif BUILDFLAG(ENABLE_EXTENSIONS)
namespace {

constexpr char kExtensionOrigin[] =
    "chrome-extension://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

}  // namespace

// Fabricates a MIME handler OOPIF frame tree -- without loading a real
// extension -- to unit-test `GetEmbeddingOriginOverride()` in isolation.
class ChromePermissionsClientMimeHandlerTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void TearDown() override {
    web_contents()->RemoveUserData(
        extensions::mime_handler::MimeHandlerStreamManager::UserDataKey());
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Commits `url` on `host` and ensures the `MimeHandlerStreamManager` exists.
  content::RenderFrameHost* NavigateAndCommit(content::RenderFrameHost* host,
                                              const GURL& url) {
    content::RenderFrameHost* committed =
        content::NavigationSimulator::NavigateAndCommitFromDocument(url, host);
    extensions::mime_handler::MimeHandlerStreamManager::Create(web_contents());
    return committed;
  }

  content::RenderFrameHost* AppendChild(content::RenderFrameHost* parent,
                                        const std::string& name) {
    auto* tester = content::RenderFrameHostTester::For(parent);
    tester->InitializeRenderFrameIfNeeded();
    return tester->AppendChild(name);
  }
};

// The embedding origin must be keyed on the requesting frame's position in the
// MIME handler subtree, not on its origin. A frame inside the extension OOPIF
// subtree resolves to the extension origin; a same-origin frame outside the
// subtree (the outer page that embedded a same-origin PDF) does not -- so it
// cannot inherit the extension's permission attribution. Regression test for
// crbug.com/519078527.
TEST_F(ChromePermissionsClientMimeHandlerTest,
       OnlyRequestersInsideExtensionSubtreeGetExtensionOrigin) {
  // The sample stream's original URL; the embedder must commit to it for the
  // manager to recognize the extension host beneath it.
  const GURL original_url("https://original_url1");

  // embedder(origin A) -> extension host(chrome-extension) -> content(origin
  // A). The embedder is same-origin with the content frame and stands in for
  // the outer page that embedded the PDF.
  content::RenderFrameHost* embedder = AppendChild(main_rfh(), "embedder");
  embedder = NavigateAndCommit(embedder, original_url);

  content::RenderFrameHost* extension_host = AppendChild(embedder, "extension");
  content::OverrideLastCommittedOrigin(
      extension_host, url::Origin::Create(GURL(kExtensionOrigin)));

  content::RenderFrameHost* content_host =
      AppendChild(extension_host, "content");
  // In production the content frame commits to the original URL; fake that
  // committed origin here so the content frame shares the embedder/outer
  // origin -- the exploit precondition. (A real cross-origin navigation under
  // the chrome-extension:// parent isn't representable in this unit harness.)
  content::OverrideLastCommittedOrigin(content_host,
                                       url::Origin::Create(original_url));

  // Register the stream so the manager treats `extension_host` as the extension
  // OOPIF under `embedder` (mirrors what NavigateToExtensionUrl() does in
  // production).
  auto* manager =
      extensions::mime_handler::MimeHandlerStreamManager::FromWebContents(
          web_contents());
  ASSERT_TRUE(manager);
  manager->AddStreamContainer(
      embedder->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<testing::NiceMock<
          extensions::mime_handler::MockMimeHandlerStreamDelegate>>());
  manager->ClaimStreamInfoForTesting(embedder);
  manager->SetExtensionFrameTreeNodeIdForTesting(
      embedder, extension_host->GetFrameTreeNodeId());
  ASSERT_TRUE(manager->IsExtensionHost(extension_host));

  auto* client = ChromePermissionsClient::GetInstance();

  // The embedder (outer page) and the content frame committed to the same
  // origin; only their frame-tree position differs.
  ASSERT_EQ(embedder->GetLastCommittedOrigin(),
            content_host->GetLastCommittedOrigin());

  // Requester inside the subtree (the content frame) is attributed to the
  // extension.
  std::optional<GURL> inside = client->GetEmbeddingOriginOverride(
      content_host->GetLastCommittedOrigin().GetURL(), content_host);
  ASSERT_TRUE(inside.has_value());
  EXPECT_EQ(GURL(kExtensionOrigin), *inside);

  // Same-origin requester outside the subtree (the embedder/outer page) is not
  // overridden, so it keeps its own origin and cannot inherit the grant.
  std::optional<GURL> outside = client->GetEmbeddingOriginOverride(
      embedder->GetLastCommittedOrigin().GetURL(), embedder);
  EXPECT_FALSE(outside.has_value());
}
#endif  // BUILDFLAG(IS_ANDROID)
