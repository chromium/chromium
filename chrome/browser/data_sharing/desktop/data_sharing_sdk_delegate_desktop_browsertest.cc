// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/desktop/data_sharing_sdk_delegate_desktop.h"

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace data_sharing {

namespace {

class DataSharingSDKDelegateDesktopBrowserTest : public InProcessBrowserTest {
 public:
  DataSharingSDKDelegateDesktopBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kDataSharingFeature);
  }
  ~DataSharingSDKDelegateDesktopBrowserTest() override = default;

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(460041655): Remove this once the underlying issue is fixed.
#if BUILDFLAG(IS_CHROMEOS) && defined(ADDRESS_SANITIZER)
#define MAYBE_ReadGroupLoadsWebContents DISABLED_ReadGroupLoadsWebContents
#else
#define MAYBE_ReadGroupLoadsWebContents ReadGroupLoadsWebContents
#endif
IN_PROC_BROWSER_TEST_F(DataSharingSDKDelegateDesktopBrowserTest,
                       MAYBE_ReadGroupLoadsWebContents) {
  DataSharingService* service =
      DataSharingServiceFactory::GetForProfile(browser()->profile());
  ASSERT_FALSE(service->IsEmptyService());
  DataSharingSDKDelegateDesktop* delegate =
      static_cast<DataSharingSDKDelegateDesktop*>(service->GetSDKDelegate());
  ASSERT_EQ(delegate->web_contents_for_testing(), nullptr);

  base::RunLoop run_loop;
  service->ReadGroupDeprecated(
      GroupId("group_id"),
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const DataSharingService::GroupDataOrFailureOutcome& result) {
            run_loop->Quit();
          },
          &run_loop));

  content::WebContents* web_contents = delegate->web_contents_for_testing();
  ASSERT_NE(web_contents, nullptr);
  content::WaitForLoadStop(web_contents);
  EXPECT_EQ(web_contents->GetURL(),
            GURL(chrome::kChromeUIUntrustedDataSharingAPIURL));
}

}  // namespace

}  // namespace data_sharing
