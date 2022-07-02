// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_destroyer.h"

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfileDestroyerTest : public testing::Test,
                             public testing::WithParamInterface<bool> {
 public:
  ProfileDestroyerTest() = default;
  ProfileDestroyerTest(const ProfileDestroyerTest&) = delete;
  ProfileDestroyerTest& operator=(const ProfileDestroyerTest&) = delete;

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  TestingProfile* original_profile() { return original_profile_; }
  TestingProfile* otr_profile() { return otr_profile_; }

  void CreateOriginalProfile() {
    original_profile_ = profile_manager_.CreateTestingProfile("foo");
    original_profile_->SetProfileDestructionObserver(
        base::BindOnce(&ProfileDestroyerTest::SetOriginalProfileDestroyed,
                       base::Unretained(this)));
    original_profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
        original_profile_, ProfileKeepAliveOrigin::kBrowserWindow);
  }

  void CreateOTRProfile() {
    Profile::OTRProfileID profile_id =
        is_primary_otr_ ? Profile::OTRProfileID::PrimaryID()
                        : Profile::OTRProfileID::CreateUniqueForTesting();
    TestingProfile::Builder builder;
    builder.SetPath(original_profile_->GetPath());
    otr_profile_ = builder.BuildOffTheRecord(original_profile_, profile_id);
    otr_profile_->SetProfileDestructionObserver(base::BindOnce(
        &ProfileDestroyerTest::SetOTRProfileDestroyed, base::Unretained(this)));
  }

  void SetOriginalProfileDestroyed() { original_profile_ = nullptr; }
  void SetOTRProfileDestroyed() { otr_profile_ = nullptr; }

  // Creates a render process host based on a new site instance given the
  // |profile| and mark it as used. Returns a reference to it.
  content::RenderProcessHost* CreatedRendererProcessHost(Profile* profile) {
    site_instances_.emplace_back(content::SiteInstance::Create(profile));

    content::RenderProcessHost* rph = site_instances_.back()->GetProcess();
    EXPECT_TRUE(rph);
    rph->SetIsUsed();
    return rph;
  }

  void StopKeepingAliveOriginalProfile() {
    original_profile_keep_alive_.reset();
  }

  // Destroying profile is still not universally supported. We need to disable
  // some tests, because it isn't possible to start destroying the profile.
  bool IsScopedProfileKeepAliveSupported() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
    return false;
#else
    return base::FeatureList::IsEnabled(
        features::kDestroyProfileOnBrowserClose);
#endif
  }

 protected:
  const bool is_primary_otr_ = GetParam();

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  content::RenderViewHostTestEnabler rvh_test_enabler_;

  raw_ptr<TestingProfile> original_profile_;
  raw_ptr<TestingProfile> otr_profile_;

  std::unique_ptr<ScopedProfileKeepAlive> original_profile_keep_alive_;
  std::vector<scoped_refptr<content::SiteInstance>> site_instances_;
};

TEST_P(ProfileDestroyerTest, DestroyOriginalProfileImmediately) {
  if (!IsScopedProfileKeepAliveSupported())
    return;
  CreateOriginalProfile();
  CreateOTRProfile();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(otr_profile());

  StopKeepingAliveOriginalProfile();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(otr_profile());

  // This doesn't really match real-world scenarios, because TestingProfile is
  // different from OffTheRecordProfileImpl. The real impl acquires a keepalive
  // on the parent profile, whereas OTR TestingProfile doesn't do that.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(original_profile());
  EXPECT_FALSE(otr_profile());
}

TEST_P(ProfileDestroyerTest, DestroyOriginalProfileDeferedByRenderProcessHost) {
  if (!IsScopedProfileKeepAliveSupported())
    return;
  CreateOriginalProfile();
  CreateOTRProfile();
  content::RenderProcessHost* render_process_host =
      CreatedRendererProcessHost(original_profile());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(otr_profile());

  // The original profile is not destroyed, because of the RenderProcessHost.
  StopKeepingAliveOriginalProfile();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(otr_profile());

  // Releasing the RenderProcessHost triggers the deletion of the Profile. It
  // happens in a posted task.
  render_process_host->Cleanup();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(otr_profile());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(original_profile());
  EXPECT_FALSE(otr_profile());
}

TEST_P(ProfileDestroyerTest,
       DestroyOriginalProfileDeferedByOffTheRecordRenderProcessHost) {
  if (!IsScopedProfileKeepAliveSupported())
    return;
  CreateOriginalProfile();
  CreateOTRProfile();
  content::RenderProcessHost* render_process_host =
      CreatedRendererProcessHost(otr_profile());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(otr_profile());

  // The original profile is not destroyed, because of the RenderProcessHost.
  StopKeepingAliveOriginalProfile();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(otr_profile());

  // Releasing the RenderProcessHost triggers the deletion of the Profile. It
  // happens in a posted task.
  render_process_host->Cleanup();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(otr_profile());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(original_profile());
  EXPECT_FALSE(otr_profile());
}

TEST_P(ProfileDestroyerTest,
       DetroyBothProfileDeferedByMultipleRenderProcessHost) {
  if (!IsScopedProfileKeepAliveSupported())
    return;
  CreateOriginalProfile();
  CreateOTRProfile();
  content::RenderProcessHost* rph_otr_profile =
      CreatedRendererProcessHost(otr_profile());
  content::RenderProcessHost* rph_original_profile =
      CreatedRendererProcessHost(original_profile());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(otr_profile());

  // No profile are destroyed, because of the RenderProcessHosts.
  StopKeepingAliveOriginalProfile();
  ProfileDestroyer::DestroyProfileWhenAppropriate(otr_profile());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(otr_profile());

  // Release the first process. It causes the associated profile to be released.
  // This happens in a posted task.
  rph_otr_profile->Cleanup();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(otr_profile());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(original_profile());
  EXPECT_FALSE(otr_profile());

  // Release the second process. It causes the associated profile to be
  // released. This happens in a posted task.
  rph_original_profile->Cleanup();
  EXPECT_TRUE(original_profile());
  EXPECT_FALSE(otr_profile());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(original_profile());
  EXPECT_FALSE(otr_profile());
}

// Expect immediate OTR profile destruction when no pending renderer
// process host exists.
TEST_P(ProfileDestroyerTest, ImmediateOTRProfileDestruction) {
  CreateOriginalProfile();
  CreateOTRProfile();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(otr_profile());

  // Ask for destruction of OTR profile, and expect immediate destruction.
  ProfileDestroyer::DestroyProfileWhenAppropriate(otr_profile());
  EXPECT_FALSE(otr_profile());
}

// Expect pending renderer process hosts delay OTR profile destruction.
TEST_P(ProfileDestroyerTest, DelayedOTRProfileDestruction) {
  CreateOriginalProfile();
  CreateOTRProfile();

  // Create two render process hosts.
  content::RenderProcessHost* render_process_host1 =
      CreatedRendererProcessHost(otr_profile());
  content::RenderProcessHost* render_process_host2 =
      CreatedRendererProcessHost(otr_profile());

  // Ask for destruction of OTR profile, but expect it to be delayed.
  ProfileDestroyer::DestroyProfileWhenAppropriate(otr_profile());
  EXPECT_TRUE(otr_profile());

  // Destroy the first pending render process host, and expect it not to destroy
  // the OTR profile.
  render_process_host1->Cleanup();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(otr_profile());

  // Destroy the other renderer process, and expect destruction of OTR
  // profile.
  render_process_host2->Cleanup();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(otr_profile());
}

// Regression test for:
// https://crbug.com/1337388#c11
TEST_P(ProfileDestroyerTest,
       DestructionRequestedTwiceWhileDelayedOriginalProfile) {
  if (!IsScopedProfileKeepAliveSupported())
    return;
  CreateOriginalProfile();

  content::RenderProcessHost* render_process_host =
      CreatedRendererProcessHost(original_profile());
  StopKeepingAliveOriginalProfile();

  EXPECT_TRUE(original_profile());
  ProfileDestroyer::DestroyProfileWhenAppropriate(original_profile());
  EXPECT_TRUE(original_profile());
  ProfileDestroyer::DestroyProfileWhenAppropriate(original_profile());
  EXPECT_TRUE(original_profile());

  render_process_host->Cleanup();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(original_profile());
}

// Regression test for:
// https://crbug.com/1337388#c11
TEST_P(ProfileDestroyerTest, DestructionRequestedTwiceWhileDelayedOTRProfile) {
  CreateOriginalProfile();
  CreateOTRProfile();

  content::RenderProcessHost* render_process_host =
      CreatedRendererProcessHost(otr_profile());

  ProfileDestroyer::DestroyProfileWhenAppropriate(otr_profile());
  EXPECT_TRUE(otr_profile());
  ProfileDestroyer::DestroyProfileWhenAppropriate(otr_profile());
  EXPECT_TRUE(otr_profile());

  render_process_host->Cleanup();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(otr_profile());
}

INSTANTIATE_TEST_SUITE_P(AllOTRProfileTypes,
                         ProfileDestroyerTest,
                         /*is_primary_otr=*/testing::Bool());
