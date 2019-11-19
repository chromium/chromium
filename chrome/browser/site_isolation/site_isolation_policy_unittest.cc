// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_isolation/site_isolation_policy.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class SiteIsolationPolicyTest : public testing::Test {
 public:
  SiteIsolationPolicyTest() : manager_(TestingBrowserProcess::GetGlobal()) {}

 protected:
  void SetUp() override { ASSERT_TRUE(manager_.SetUp()); }

  TestingProfileManager* manager() { return &manager_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager manager_;

  DISALLOW_COPY_AND_ASSIGN(SiteIsolationPolicyTest);
};

// Helper class that enables site isolation for password sites.
class PasswordSiteIsolationPolicyTest : public SiteIsolationPolicyTest {
 public:
  PasswordSiteIsolationPolicyTest() {}

 protected:
  void SetUp() override {
    feature_list_.InitWithFeatures({features::kSiteIsolationForPasswordSites},
                                   {features::kSitePerProcess});
    SiteIsolationPolicyTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(PasswordSiteIsolationPolicyTest);
};

// Verifies that SiteIsolationPolicy::ApplyPersistedIsolatedOrigins applies
// stored isolated origins correctly when using site isolation for password
// sites.
TEST_F(PasswordSiteIsolationPolicyTest, ApplyPersistedIsolatedOrigins) {
  EXPECT_TRUE(SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled());
  TestingProfile* profile = manager()->CreateTestingProfile("Test");

  // Add foo.com and bar.com to stored isolated origins.
  {
    ListPrefUpdate update(profile->GetPrefs(),
                          prefs::kUserTriggeredIsolatedOrigins);
    base::ListValue* list = update.Get();
    list->Append("http://foo.com");
    list->Append("https://bar.com");
  }

  // New SiteInstances for foo.com and bar.com shouldn't require a dedicated
  // process to start with.  An exception is if this test runs with a
  // command-line --site-per-process flag (which might be the case on some
  // bots).  This will override the feature configuration in this test and make
  // all sites isolated.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess)) {
    scoped_refptr<content::SiteInstance> foo_instance =
        content::SiteInstance::CreateForURL(profile, GURL("http://foo.com/1"));
    EXPECT_FALSE(foo_instance->RequiresDedicatedProcess());

    scoped_refptr<content::SiteInstance> bar_instance =
        content::SiteInstance::CreateForURL(profile,
                                            GURL("https://baz.bar.com/2"));
    EXPECT_FALSE(bar_instance->RequiresDedicatedProcess());
  }

  // Apply isolated origins and ensure that they take effect for SiteInstances
  // in new BrowsingInstances.
  base::HistogramTester histograms;
  SiteIsolationPolicy::ApplyPersistedIsolatedOrigins(profile);
  histograms.ExpectUniqueSample(
      "SiteIsolation.SavedUserTriggeredIsolatedOrigins.Size", 2, 1);
  {
    scoped_refptr<content::SiteInstance> foo_instance =
        content::SiteInstance::CreateForURL(profile, GURL("http://foo.com/1"));
    EXPECT_TRUE(foo_instance->RequiresDedicatedProcess());

    scoped_refptr<content::SiteInstance> bar_instance =
        content::SiteInstance::CreateForURL(profile,
                                            GURL("https://baz.bar.com/2"));
    EXPECT_TRUE(bar_instance->RequiresDedicatedProcess());
  }
}

// Helper class that disables strict site isolation as well as site isolation
// for password sites.
class NoPasswordSiteIsolationPolicyTest : public SiteIsolationPolicyTest {
 public:
  NoPasswordSiteIsolationPolicyTest() {}

 protected:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {},
        {features::kSiteIsolationForPasswordSites, features::kSitePerProcess});
    SiteIsolationPolicyTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(NoPasswordSiteIsolationPolicyTest);
};

// Verifies that SiteIsolationPolicy::ApplyPersistedIsolatedOrigins ignores
// stored isolated origins when site isolation for password sites is off.
TEST_F(NoPasswordSiteIsolationPolicyTest,
       PersistedIsolatedOriginsIgnoredWithoutPasswordIsolation) {
  // Running this test with a command-line --site-per-process flag (which might
  // be the case on some bots) doesn't make sense, as that will make all sites
  // isolated, overriding the feature configuration in this test.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess))
    return;

  EXPECT_FALSE(SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled());
  TestingProfile* profile = manager()->CreateTestingProfile("Test");

  // Add foo.com to stored isolated origins.
  {
    ListPrefUpdate update(profile->GetPrefs(),
                          prefs::kUserTriggeredIsolatedOrigins);
    base::ListValue* list = update.Get();
    list->Append("http://foo.com");
  }

  // Applying saved isolated origins should have no effect, since site
  // isolation for password sites is off.
  SiteIsolationPolicy::ApplyPersistedIsolatedOrigins(profile);
  scoped_refptr<content::SiteInstance> foo_instance =
      content::SiteInstance::CreateForURL(profile, GURL("http://foo.com/"));
  EXPECT_FALSE(foo_instance->RequiresDedicatedProcess());
}
