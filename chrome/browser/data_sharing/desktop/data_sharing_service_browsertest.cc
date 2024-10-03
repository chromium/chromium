// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/public/data_sharing_service.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/data_sharing/desktop/data_sharing_sdk_delegate_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/data_sharing/public/features.h"
#include "content/public/test/browser_test.h"

class DataSharingServiceBrowserTest : public InProcessBrowserTest {
 public:
  DataSharingServiceBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {data_sharing::features::kDataSharingFeature}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DataSharingServiceBrowserTest, ReadGroup) {
// In branded build, tests are skipped since it fetches data from the server.
// In non-branded build, fake data are returned from a dummy implementation.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  GTEST_SKIP() << "N/A for Google Chrome Branding Build";
#else
  base::RunLoop run_loop;
  auto* service = data_sharing::DataSharingServiceFactory::GetForProfile(
      browser()->profile());
  service->ReadGroup(
      data_sharing::GroupId("12345"),
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const data_sharing::DataSharingService::GroupDataOrFailureOutcome&
                 result) {
            EXPECT_EQ(data_sharing::GroupId("12345"),
                      result->group_token.group_id);
            EXPECT_EQ("GROUP_NAME", result->display_name);
            EXPECT_EQ(1u, result->members.size());
            data_sharing::GroupMember member = result->members[0];
            EXPECT_EQ("GAIA_ID", member.gaia_id);
            EXPECT_EQ("MEMBER_NAME", member.display_name);
            EXPECT_EQ("test@gmail.com", member.email);
            EXPECT_EQ(data_sharing::MemberRole::kMember, member.role);
            EXPECT_EQ(GURL("http://example.com"), member.avatar_url);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

IN_PROC_BROWSER_TEST_F(DataSharingServiceBrowserTest, DeleteGroup) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  GTEST_SKIP() << "N/A for Google Chrome Branding Build";
#else
  base::RunLoop run_loop;
  auto* service = data_sharing::DataSharingServiceFactory::GetForProfile(
      browser()->profile());
  service->DeleteGroup(
      data_sharing::GroupId("12345"),
      base::BindOnce(
          [](base::RunLoop* run_loop,
             data_sharing::DataSharingService::PeopleGroupActionOutcome
                 result) {
            EXPECT_EQ(result, data_sharing::DataSharingService::
                                  PeopleGroupActionOutcome::kPersistentFailure);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}
