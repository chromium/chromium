// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_destroyer.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"

class ProfileDestroyerTest : public BrowserWithTestWindowTest,
                             public testing::WithParamInterface<bool> {
 public:
  ProfileDestroyerTest() : is_primary_otr_(GetParam()) {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    GetOriginalProfile()->SetProfileDestructionObserver(
        base::BindOnce(&ProfileDestroyerTest::SetOriginalProfileDestroyed,
                       base::Unretained(this)));
  }

  TestingProfile* GetOriginalProfile() { return GetProfile(); }

  TestingProfile* GetOffTheRecordProfile() {
    if (!otr_profile_) {
      TestingProfile::Builder builder;
      Profile::OTRProfileID profile_id =
          is_primary_otr_ ? Profile::OTRProfileID::PrimaryID()
                          : Profile::OTRProfileID("Test::ProfileDestroyer");
      otr_profile_ =
          builder.BuildOffTheRecord(GetOriginalProfile(), profile_id);
      otr_profile_->SetProfileDestructionObserver(
          base::BindOnce(&ProfileDestroyerTest::SetOTRProfileDestroyed,
                         base::Unretained(this)));
    }
    return otr_profile_;
  }

  void SetOriginalProfileDestroyed() { original_profile_destroyed_ = true; }
  void SetOTRProfileDestroyed() { otr_profile_destroyed_ = true; }

  bool IsOriginalProfileDestroyed() { return original_profile_destroyed_; }
  bool IsOTRProfileDestroyed() { return otr_profile_destroyed_; }

  // Creates a render process host based on a new site instance given the
  // |profile| and mark it as used.
  std::unique_ptr<content::RenderProcessHost> CreatedRendererProcessHost(
      Profile* profile) {
    site_instances_.emplace_back(content::SiteInstance::Create(profile));

    std::unique_ptr<content::RenderProcessHost> render_process_host;
    render_process_host.reset(site_instances_.back()->GetProcess());
    render_process_host->SetIsUsed();
    EXPECT_NE(render_process_host.get(), nullptr);

    return render_process_host;
  }

 protected:
  bool is_primary_otr_;
  bool original_profile_destroyed_{false};
  bool otr_profile_destroyed_{false};
  TestingProfile* otr_profile_{nullptr};

  std::vector<scoped_refptr<content::SiteInstance>> site_instances_;

  DISALLOW_COPY_AND_ASSIGN(ProfileDestroyerTest);
};

// Expect immediate OTR profile destruction when no pending renderer
// process host exists.
TEST_P(ProfileDestroyerTest, ImmediateOTRProfileDestruction) {
  TestingProfile* otr_profile = GetOffTheRecordProfile();

  // Destroying the regular browser does not result in destruction of regular
  // profile and hence should not destroy the OTR profile.
  set_browser(nullptr);
  EXPECT_FALSE(IsOriginalProfileDestroyed());
  EXPECT_FALSE(IsOTRProfileDestroyed());

  // Ask for destruction of OTR profile, and expect immediate destruction.
  ProfileDestroyer::DestroyProfileWhenAppropriate(otr_profile);
  EXPECT_TRUE(IsOTRProfileDestroyed());
}

// Expect pending renderer process hosts delay OTR profile destruction.
TEST_P(ProfileDestroyerTest, DelayedOTRProfileDestruction) {
  TestingProfile* otr_profile = GetOffTheRecordProfile();

  // Create two render process hosts.
  std::unique_ptr<content::RenderProcessHost> render_process_host1 =
      CreatedRendererProcessHost(otr_profile);
  std::unique_ptr<content::RenderProcessHost> render_process_host2 =
      CreatedRendererProcessHost(otr_profile);

  // Ask for destruction of OTR profile, but expect it to be delayed.
  ProfileDestroyer::DestroyProfileWhenAppropriate(otr_profile);
  EXPECT_FALSE(IsOTRProfileDestroyed());

  // Destroy the first pending render process host, and expect it not to destroy
  // the OTR profile.
  render_process_host1.release()->Cleanup();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsOTRProfileDestroyed());

  // Destroy the other renderer process, and expect destruction of OTR
  // profile.
  render_process_host2.release()->Cleanup();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsOTRProfileDestroyed());
}

INSTANTIATE_TEST_SUITE_P(AllOTRProfileTypes,
                         ProfileDestroyerTest,
                         /*is_primary_otr=*/testing::Bool());
