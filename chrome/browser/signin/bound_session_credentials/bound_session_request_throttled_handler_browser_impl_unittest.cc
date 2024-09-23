// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_request_throttled_handler_browser_impl.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_param.h"
#include "chrome/browser/signin/bound_session_credentials/fake_bound_session_cookie_refresh_service.h"
#include "chrome/common/bound_session_request_throttled_handler.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace {

TEST(BoundSessionRequestThrottledHandlerBrowserImplTest, RefreshServiceAlive) {
  base::test::SingleThreadTaskEnvironment task_environment;
  FakeBoundSessionCookieRefreshService service;
  BoundSessionRequestThrottledHandlerBrowserImpl handler(service);

  base::test::TestFuture<BoundSessionRequestThrottledHandler::UnblockAction,
                         chrome::mojom::ResumeBlockedRequestsTrigger>
      future;
  handler.HandleRequestBlockedOnCookie(GURL(), future.GetCallback());

  EXPECT_TRUE(service.IsRequestBlocked());
  EXPECT_FALSE(future.IsReady());

  const auto kResumeTrigger =
      chrome::mojom::ResumeBlockedRequestsTrigger::kCookieAlreadyFresh;
  service.SimulateUnblockRequest(kResumeTrigger);
  EXPECT_THAT(future.Get(),
              testing::FieldsAre(
                  BoundSessionRequestThrottledHandler::UnblockAction::kResume,
                  kResumeTrigger));
}

TEST(BoundSessionRequestThrottledHandlerBrowserImplTest,
     RefreshServiceDestroyed) {
  base::test::SingleThreadTaskEnvironment task_environment;
  std::unique_ptr<FakeBoundSessionCookieRefreshService> service =
      std::make_unique<FakeBoundSessionCookieRefreshService>();
  BoundSessionRequestThrottledHandlerBrowserImpl handler(*service);

  // Destory service.
  service.reset();
  base::test::TestFuture<BoundSessionRequestThrottledHandler::UnblockAction,
                         chrome::mojom::ResumeBlockedRequestsTrigger>
      future_cancel;
  handler.HandleRequestBlockedOnCookie(GURL(), future_cancel.GetCallback());
  EXPECT_TRUE(future_cancel.IsReady());
  EXPECT_THAT(future_cancel.Get(),
              testing::FieldsAre(
                  BoundSessionRequestThrottledHandler::UnblockAction::kCancel,
                  chrome::mojom::ResumeBlockedRequestsTrigger::
                      kShutdownOrSessionTermination));
}

}  // namespace
