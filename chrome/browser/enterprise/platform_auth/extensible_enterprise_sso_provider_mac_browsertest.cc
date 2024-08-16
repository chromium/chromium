// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_provider_mac.h"

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;

namespace enterprise_auth {

class ExtensibleEnterpriseSSOTest : public InProcessBrowserTest {};

IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSSOTest, UnsupportedCalledTwice) {
  ExtensibleEnterpriseSSOProvider provider;
  {
    base::RunLoop run_loop;
    base::MockCallback<PlatformAuthProviderManager::GetDataCallback> mock;
    EXPECT_CALL(mock, Run(_)).WillOnce([&run_loop](net::HttpRequestHeaders) {
      run_loop.Quit();
    });

    provider.GetData(GURL(), mock.Get());
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    base::MockCallback<PlatformAuthProviderManager::GetDataCallback> mock;
    EXPECT_CALL(mock, Run(_)).WillOnce([&run_loop](net::HttpRequestHeaders) {
      run_loop.Quit();
    });

    provider.GetData(GURL(), mock.Get());
    run_loop.Run();
  }
}

}  // namespace enterprise_auth
