// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/unexportable_keys/features.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "net/base/features.h"
#include "net/device_bound_sessions/session_key.h"
#include "net/device_bound_sessions/test_support.h"
#include "net/test/embedded_test_server/http_response.h"

using net::device_bound_sessions::SessionKey;

namespace {

class DeviceBoundSessionAccessObserver : public content::WebContentsObserver {
 public:
  DeviceBoundSessionAccessObserver(
      content::WebContents* web_contents,
      base::RepeatingCallback<void(const SessionKey&)> on_access_callback)
      : WebContentsObserver(web_contents),
        on_access_callback_(std::move(on_access_callback)) {}

  void OnDeviceBoundSessionAccessed(content::NavigationHandle* navigation,
                                    const SessionKey& session) override {
    on_access_callback_.Run(session);
  }
  void OnDeviceBoundSessionAccessed(content::RenderFrameHost* rfh,
                                    const SessionKey& session) override {
    on_access_callback_.Run(session);
  }

 private:
  base::RepeatingCallback<void(const SessionKey&)> on_access_callback_;
};

class DeviceBoundSessionBrowserTest : public InProcessBrowserTest {
 public:
  DeviceBoundSessionBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {net::features::kDeviceBoundSessions,
         unexportable_keys::
             kEnableBoundSessionCredentialsSoftwareKeysForManualTesting},
        {});

    EXPECT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->RegisterRequestHandler(
        net::device_bound_sessions::GetTestRequestHandler(
            embedded_test_server()->base_url()));
    embedded_test_server()->StartAcceptingConnections();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest,
                       AccessCalledOnRegistration) {
  base::test::TestFuture<SessionKey> future;
  DeviceBoundSessionAccessObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      future.GetRepeatingCallback<const SessionKey&>());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/dbsc_required")));

  SessionKey session = future.Take();
  EXPECT_EQ(session.site,
            net::SchemefulSite(embedded_test_server()->base_url()));
  EXPECT_EQ(session.id, SessionKey::Id("session_id"));
}

}  // namespace
