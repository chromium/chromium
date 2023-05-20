// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_extensions_helper.h"

#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/process_map.h"
#endif

TEST(ChromeMetricsExtensionsHelperTest, Basic) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());
  // Owned by |profile_manager|.
  TestingProfile* profile =
      profile_manager.CreateTestingProfile("StabilityTestProfile");
  std::unique_ptr<content::MockRenderProcessHostFactory> rph_factory =
      std::make_unique<content::MockRenderProcessHostFactory>();
  scoped_refptr<content::SiteInstance> site_instance(
      content::SiteInstance::Create(profile));
  // Owned by rph_factory.
  content::RenderProcessHost* host =
      rph_factory->CreateRenderProcessHost(profile, site_instance.get());
  ChromeMetricsExtensionsHelper extensions_helper;

  // |host| is not an extensions host.
  EXPECT_FALSE(extensions_helper.IsExtensionProcess(host));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Tag |host| so that it's an extensions host.
  extensions::ProcessMap::Get(profile)->Insert("1", host->GetID());
  EXPECT_TRUE(extensions_helper.IsExtensionProcess(host));
#endif
  rph_factory.reset();
  profile_manager.DeleteAllTestingProfiles();
}
