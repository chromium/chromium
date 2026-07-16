// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/password_manager/remote_actor_credential_sharing_policy.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

class MockBadMessageHelper {
 public:
  MockBadMessageHelper() {
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &MockBadMessageHelper::OnBadMessage, base::Unretained(this)));
  }
  ~MockBadMessageHelper() {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  MOCK_METHOD(void, OnBadMessage, (const std::string& error));
};

class RemoteActorCredentialSharingImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ~RemoteActorCredentialSharingImplTest() override = default;

 protected:
  void NavigateAndCommit(const GURL& url) {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                               url);
    if (main_rfh()) {
      main_rfh()->GetProcess()->Init();
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_{features::kRemoteActorCredentialSharing};
};

// Verify that when the feature flag kRemoteActorCredentialSharing is enabled,
// binding the receiver succeeds on the whitelisted origins, and authentication
// request works.
TEST_F(RemoteActorCredentialSharingImplTest,
       FeatureEnabledWhitelistedOriginSucceeds) {
  NavigateAndCommit(GURL("https://gemini.google.com"));
  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  RemoteActorCredentialSharingImpl::CreateForCurrentDocument(main_rfh());

  RemoteActorCredentialSharingImpl* impl =
      RemoteActorCredentialSharingImpl::GetForCurrentDocument(main_rfh());
  ASSERT_NE(impl, nullptr);

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
  impl->Bind(remote.BindNewEndpointAndPassDedicatedReceiver());

  // Call RequestAgentAuthentication (expected false).
  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> result;
  remote->RequestAgentAuthentication(
      "123456789", "google.com", "actor_id", result.GetCallback());
  EXPECT_FALSE(result.Get());
}

// Verify that when the feature flag kRemoteActorCredentialSharing is enabled,
// binding the receiver succeeds on the sandbox whitelisted origin.
TEST_F(RemoteActorCredentialSharingImplTest,
       FeatureEnabledSandboxOriginSucceeds) {
  NavigateAndCommit(GURL("https://gemini-preprod.corp.google.com"));
  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  RemoteActorCredentialSharingImpl::CreateForCurrentDocument(main_rfh());

  RemoteActorCredentialSharingImpl* impl =
      RemoteActorCredentialSharingImpl::GetForCurrentDocument(main_rfh());
  ASSERT_NE(impl, nullptr);

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
  impl->Bind(remote.BindNewEndpointAndPassDedicatedReceiver());

  // Call RequestAgentAuthentication (expected false).
  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> result;
  remote->RequestAgentAuthentication(
      "123456789", "google.com", "actor_id", result.GetCallback());
  EXPECT_FALSE(result.Get());
}

// Verify behavior with empty arguments.
TEST_F(RemoteActorCredentialSharingImplTest, RequestWithEmptyArguments) {
  NavigateAndCommit(GURL("https://gemini.google.com"));
  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  RemoteActorCredentialSharingImpl::CreateForCurrentDocument(main_rfh());

  RemoteActorCredentialSharingImpl* impl =
      RemoteActorCredentialSharingImpl::GetForCurrentDocument(main_rfh());
  ASSERT_NE(impl, nullptr);

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
  impl->Bind(remote.BindNewEndpointAndPassDedicatedReceiver());

  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> result;
  remote->RequestAgentAuthentication(
      "", "", "", result.GetCallback());
  EXPECT_FALSE(result.Get());
}

// Verify behavior with extremely long strings.
TEST_F(RemoteActorCredentialSharingImplTest,
       RequestWithExtremelyLongArguments) {
  NavigateAndCommit(GURL("https://gemini.google.com"));
  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  RemoteActorCredentialSharingImpl::CreateForCurrentDocument(main_rfh());

  RemoteActorCredentialSharingImpl* impl =
      RemoteActorCredentialSharingImpl::GetForCurrentDocument(main_rfh());
  ASSERT_NE(impl, nullptr);

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
  impl->Bind(remote.BindNewEndpointAndPassDedicatedReceiver());

  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  std::string long_string(1000000, 'a');

  MockBadMessageHelper bad_message_helper;
  base::RunLoop run_loop;
  EXPECT_CALL(bad_message_helper,
              OnBadMessage(testing::HasSubstr(
                  "RemoteActorCredentialSharing: Argument length limit exceeded")))
      .WillOnce([&run_loop](const std::string&) { run_loop.Quit(); });

  remote->RequestAgentAuthentication(
      long_string, "google.com", long_string, base::DoNothing());
  run_loop.Run();
}

// Verify behavior with special/null characters.
TEST_F(RemoteActorCredentialSharingImplTest,
       RequestWithSpecialCharacters) {
  NavigateAndCommit(GURL("https://gemini.google.com"));
  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  RemoteActorCredentialSharingImpl::CreateForCurrentDocument(main_rfh());

  RemoteActorCredentialSharingImpl* impl =
      RemoteActorCredentialSharingImpl::GetForCurrentDocument(main_rfh());
  ASSERT_NE(impl, nullptr);

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
  impl->Bind(remote.BindNewEndpointAndPassDedicatedReceiver());

  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> result;
  remote->RequestAgentAuthentication(
      "gaia\0id", "google.com", "actor\nhack", result.GetCallback());
  EXPECT_FALSE(result.Get());
}

// Verify that calling RequestAgentAuthentication from a subframe
// triggers a bad message.
TEST_F(RemoteActorCredentialSharingImplTest,
       SubframeRequestTriggersBadMessage) {
  NavigateAndCommit(GURL("https://gemini.google.com"));
  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();

  content::RenderFrameHost* subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://gemini.google.com"),
          content::RenderFrameHostTester::For(main_rfh())
              ->AppendChild("subframe"));

  ASSERT_NE(subframe, nullptr);
  subframe->GetProcess()->Init();

  RemoteActorCredentialSharingImpl::CreateForCurrentDocument(subframe);
  RemoteActorCredentialSharingImpl* impl =
      RemoteActorCredentialSharingImpl::GetForCurrentDocument(subframe);
  ASSERT_NE(impl, nullptr);

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
  impl->Bind(remote.BindNewEndpointAndPassDedicatedReceiver());

  MockBadMessageHelper bad_message_helper;
  base::RunLoop run_loop;
  EXPECT_CALL(bad_message_helper,
              OnBadMessage(testing::HasSubstr(
                  "RemoteActorCredentialSharing: Request from subframe")))
      .WillOnce([&run_loop](const std::string&) { run_loop.Quit(); });

  remote->RequestAgentAuthentication(
      "123456789", "google.com", "actor_id", base::DoNothing());
  run_loop.Run();
}

// Verify that calling RequestAgentAuthentication without user activation
// reports a bad message.
TEST_F(RemoteActorCredentialSharingImplTest,
       RequestWithoutUserGestureReportsBadMessage) {
  NavigateAndCommit(GURL("https://gemini.google.com"));
  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  RemoteActorCredentialSharingImpl::CreateForCurrentDocument(main_rfh());

  RemoteActorCredentialSharingImpl* impl =
      RemoteActorCredentialSharingImpl::GetForCurrentDocument(main_rfh());
  ASSERT_NE(impl, nullptr);

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
  impl->Bind(remote.BindNewEndpointAndPassDedicatedReceiver());

  MockBadMessageHelper bad_message_helper;
  base::RunLoop run_loop;
  EXPECT_CALL(bad_message_helper,
              OnBadMessage(testing::HasSubstr(
                  "RemoteActorCredentialSharing: Request without user gesture")))
      .WillOnce([&run_loop](const std::string&) { run_loop.Quit(); });

  remote->RequestAgentAuthentication(
      "123456789", "google.com", "actor_id", base::DoNothing());
  run_loop.Run();
}

}  // namespace password_manager
