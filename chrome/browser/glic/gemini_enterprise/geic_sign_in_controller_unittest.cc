// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_sign_in_controller.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace glic {
namespace {

class GeicSignInControllerTest : public ChromeRenderViewHostTestHarness {
 protected:
  const url::Origin guest_origin_ =
      url::Origin::Create(GURL("https://vertexaisearch.cloud.google"));
};

TEST_F(GeicSignInControllerTest, GetForProfileReturnsProfileScopedInstance) {
  EXPECT_EQ(GeicSignInController::GetIfExists(profile()), nullptr);
  GeicSignInController* controller =
      GeicSignInController::GetForProfile(profile());
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(GeicSignInController::GetIfExists(profile()), controller);
  EXPECT_EQ(GeicSignInController::GetForProfile(profile()), controller);
}

TEST_F(GeicSignInControllerTest,
       NotifiesAllRegisteredCallbacksOnCompletionWithCsesIdxAlreadySet) {
  GeicSignInController* controller =
      GeicSignInController::GetForProfile(profile());
  const GURL auth_url(
      "https://auth.cloud.google/select-session?continueUrl="
      "https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel");
  controller->OnSignInRequired(auth_url);
  EXPECT_EQ(controller->last_intercepted_signin_url_for_testing(), auth_url);

  int first_instance_notified = 0;
  int second_instance_notified = 0;
  GURL resolved_url_in_callback;

  base::CallbackListSubscription sub =
      controller->RegisterSignInCompletedCallback(
          base::BindLambdaForTesting([&]() {
            ++first_instance_notified;
            resolved_url_in_callback = controller->ResolveGuestUrl(
                GURL("https://vertexaisearch.cloud.google/side-panel"));
          }));
  // A second window clicking Sign in while observing the same sign-in tab must
  // not overwrite the first window's completion callback.
  controller->ObserveSignInContentsForTesting(
      web_contents(), guest_origin_,
      base::BindLambdaForTesting([&]() { ++second_instance_notified; }));

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(),
      GURL("https://vertexaisearch.cloud.google/side-panel?csesidx=11"));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(first_instance_notified, 1);
  EXPECT_EQ(second_instance_notified, 1);
  EXPECT_EQ(resolved_url_in_callback,
            GURL("https://vertexaisearch.cloud.google/side-panel?csesidx=11"));
  EXPECT_TRUE(controller->last_intercepted_signin_url_for_testing().is_empty());

  // A subsequent sign-in also notifies the persistent subscription until `sub`
  // is destroyed.
  controller->ObserveSignInContentsForTesting(web_contents(), guest_origin_,
                                              base::DoNothing());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(),
      GURL("https://vertexaisearch.cloud.google/side-panel?csesidx=12"));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(first_instance_notified, 2);
  EXPECT_EQ(resolved_url_in_callback,
            GURL("https://vertexaisearch.cloud.google/side-panel?csesidx=12"));
}

TEST_F(
    GeicSignInControllerTest,
    ObserverIgnoresNonHttpSchemesWrongOriginsSubframesErrorsAndEmptyCsesIdx) {
  GeicSignInController* controller =
      GeicSignInController::GetForProfile(profile());
  int completed_count = 0;
  controller->ObserveSignInContentsForTesting(
      web_contents(), guest_origin_,
      base::BindLambdaForTesting([&]() { ++completed_count; }));

  // 1. Wrong origin with valid `csesidx`: ignored.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://auth.cloud.google/side-panel?csesidx=7"));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(completed_count, 0);

  // 2. Non-HTTP/HTTPS (`blob:`) URL with matching inner origin and `csesidx`:
  // ignored.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(),
      GURL("blob:https://vertexaisearch.cloud.google/uuid?csesidx=7"));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(completed_count, 0);

  // 3. Matching guest origin without `csesidx` or with empty `csesidx=`:
  // ignored.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(),
      GURL("https://vertexaisearch.cloud.google/side-panel?configId=1"));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(),
      GURL("https://vertexaisearch.cloud.google/side-panel?csesidx="));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(completed_count, 0);

  // 4. Subframe navigation to matching guest origin with `csesidx`: ignored.
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://vertexaisearch.cloud.google/side-panel?csesidx=7"),
      subframe);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(completed_count, 0);

  // 5. Primary main-frame error page on matching guest origin with `csesidx`:
  // ignored.
  content::NavigationSimulator::NavigateAndFailFromBrowser(
      web_contents(),
      GURL("https://vertexaisearch.cloud.google/side-panel?csesidx=7"),
      net::ERR_CONNECTION_RESET);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(completed_count, 0);

  // 6. Valid primary main-frame commit on matching guest origin with non-empty
  // `csesidx`: completes sign-in and resolves `csesidx` on guest URLs.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://vertexaisearch.cloud.google/side-panel?"
                           "configId=cfg-1&csesidx=42"));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(completed_count, 1);
  EXPECT_EQ(controller->ResolveGuestUrl(
                GURL("https://vertexaisearch.cloud.google/side-panel?"
                     "configId=cfg-1")),
            GURL("https://vertexaisearch.cloud.google/side-panel?"
                 "configId=cfg-1&csesidx=42"));

  // Subsequent `OnSignInRequired()` clears the resolved `csesidx`.
  controller->OnSignInRequired(
      GURL("https://auth.cloud.google/select-session"));
  EXPECT_EQ(controller->ResolveGuestUrl(
                GURL("https://vertexaisearch.cloud.google/side-panel?"
                     "configId=cfg-1")),
            GURL("https://vertexaisearch.cloud.google/side-panel?"
                 "configId=cfg-1"));
}

TEST_F(GeicSignInControllerTest,
       WebContentsDestroyedDoesNotInvokeCompletionCallback) {
  GeicSignInController* controller =
      GeicSignInController::GetForProfile(profile());
  std::unique_ptr<content::WebContents> signin_contents =
      CreateTestWebContents();
  int completed_count = 0;
  controller->ObserveSignInContentsForTesting(
      signin_contents.get(), guest_origin_,
      base::BindLambdaForTesting([&]() { ++completed_count; }));

  signin_contents.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(completed_count, 0);
}

}  // namespace
}  // namespace glic
