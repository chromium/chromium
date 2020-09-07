// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_encryption_keys_tab_helper.h"

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/common/sync_encryption_keys_extension.mojom.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_receiver_set.h"
#include "content/public/test/web_contents_tester.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::IsNull;
using testing::NotNull;

class SyncEncryptionKeysTabHelperTest : public ChromeRenderViewHostTestHarness {
 protected:
  SyncEncryptionKeysTabHelperTest() = default;

  ~SyncEncryptionKeysTabHelperTest() override = default;

  // content::RenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SyncEncryptionKeysTabHelper::CreateForWebContents(web_contents());
  }

  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents());
  }

  content::WebContentsReceiverSet* frame_receiver_set() {
    return content::WebContentsReceiverSet::GetForWebContents<
        chrome::mojom::SyncEncryptionKeysExtension>(web_contents());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncEncryptionKeysTabHelperTest);
};

TEST_F(SyncEncryptionKeysTabHelperTest, ShouldExposeMojoApiToAllowedOrigin) {
  ASSERT_THAT(frame_receiver_set(), IsNull());
  web_contents_tester()->NavigateAndCommit(GaiaUrls::GetInstance()->gaia_url());
  EXPECT_THAT(frame_receiver_set(), NotNull());
}

TEST_F(SyncEncryptionKeysTabHelperTest,
       ShouldNotExposeMojoApiToUnallowedOrigin) {
  web_contents_tester()->NavigateAndCommit(GURL("http://page.com"));
  EXPECT_THAT(frame_receiver_set(), IsNull());
}

TEST_F(SyncEncryptionKeysTabHelperTest, ShouldNotExposeMojoApiIfNavigatedAway) {
  web_contents_tester()->NavigateAndCommit(GaiaUrls::GetInstance()->gaia_url());
  ASSERT_THAT(frame_receiver_set(), NotNull());
  web_contents_tester()->NavigateAndCommit(GURL("http://page.com"));
  EXPECT_THAT(frame_receiver_set(), IsNull());
}

TEST_F(SyncEncryptionKeysTabHelperTest,
       ShouldNotExposeMojoApiIfNavigationFailed) {
  web_contents_tester()->NavigateAndFail(GaiaUrls::GetInstance()->gaia_url(),
                                         net::ERR_ABORTED);
  EXPECT_THAT(frame_receiver_set(), IsNull());
}

TEST_F(SyncEncryptionKeysTabHelperTest,
       ShouldNotExposeMojoApiIfNavigatedAwayToErrorPage) {
  web_contents_tester()->NavigateAndCommit(GaiaUrls::GetInstance()->gaia_url());
  ASSERT_THAT(frame_receiver_set(), NotNull());
  web_contents_tester()->NavigateAndFail(GURL("http://page.com"),
                                         net::ERR_ABORTED);
  EXPECT_THAT(frame_receiver_set(), IsNull());
}

}  // namespace
