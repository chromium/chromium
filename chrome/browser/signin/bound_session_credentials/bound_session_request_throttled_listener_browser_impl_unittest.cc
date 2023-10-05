// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_request_throttled_listener_browser_impl.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_param.h"
#include "chrome/common/bound_session_request_throttled_listener.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace {
class FakeBoundSessionCookieRefreshService
    : public BoundSessionCookieRefreshService {
 public:
  FakeBoundSessionCookieRefreshService() = default;

  void Initialize() override {}

  void RegisterNewBoundSession(
      const bound_session_credentials::BoundSessionParams& params) override {}

  void MaybeTerminateSession(const net::HttpResponseHeaders* headers) override {
  }

  chrome::mojom::BoundSessionThrottlerParamsPtr GetBoundSessionThrottlerParams()
      const override {
    return chrome::mojom::BoundSessionThrottlerParams::New();
  }

  void SetRendererBoundSessionThrottlerParamsUpdaterDelegate(
      RendererBoundSessionThrottlerParamsUpdaterDelegate renderer_updater)
      override {}

  void SetBoundSessionParamsUpdatedCallbackForTesting(
      base::RepeatingClosure updated_callback) override {}

  void OnRequestBlockedOnCookie(
      OnRequestBlockedOnCookieCallback resume_blocked_request) override {
    resume_blocked_request_ = std::move(resume_blocked_request);
  }

  void CreateRegistrationRequest(
      BoundSessionRegistrationFetcherParam registration_params) override {}

  base::WeakPtr<BoundSessionCookieRefreshService> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void SimulateUnblockRequest() { std::move(resume_blocked_request_).Run(); }

  bool IsRequestBlocked() { return !resume_blocked_request_.is_null(); }

 private:
  base::OnceClosure resume_blocked_request_;
  base::WeakPtrFactory<BoundSessionCookieRefreshService> weak_ptr_factory_{
      this};
};

TEST(BoundSessionRequestThrottledListenerBrowserImplTest, RefreshServiceAlive) {
  base::test::SingleThreadTaskEnvironment task_environment;
  FakeBoundSessionCookieRefreshService service;
  BoundSessionRequestThrottledListenerBrowserImpl listener(service);

  base::test::TestFuture<BoundSessionRequestThrottledListener::UnblockAction>
      future;
  listener.OnRequestBlockedOnCookie(future.GetCallback());

  EXPECT_TRUE(service.IsRequestBlocked());
  EXPECT_FALSE(future.IsReady());

  service.SimulateUnblockRequest();
  EXPECT_EQ(future.Get(),
            BoundSessionRequestThrottledListener::UnblockAction::kResume);
}

TEST(BoundSessionRequestThrottledListenerBrowserImplTest,
     RefreshServiceDestroyed) {
  base::test::SingleThreadTaskEnvironment task_environment;
  std::unique_ptr<FakeBoundSessionCookieRefreshService> service =
      std::make_unique<FakeBoundSessionCookieRefreshService>();
  BoundSessionRequestThrottledListenerBrowserImpl listener(*service);

  // Destory service.
  service.reset();
  base::test::TestFuture<BoundSessionRequestThrottledListener::UnblockAction>
      future_cancel;
  listener.OnRequestBlockedOnCookie(future_cancel.GetCallback());
  EXPECT_TRUE(future_cancel.IsReady());
  EXPECT_EQ(future_cancel.Get(),
            BoundSessionRequestThrottledListener::UnblockAction::kCancel);
}

}  // namespace
