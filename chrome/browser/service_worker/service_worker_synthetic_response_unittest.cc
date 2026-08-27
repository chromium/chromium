// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/service_worker/service_worker_synthetic_response.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chrome_service_worker {

using ServiceWorkerSyntheticResponseTest = ChromeRenderViewHostTestHarness;

TEST_F(ServiceWorkerSyntheticResponseTest,
       IsServiceWorkerSyntheticResponseAllowed) {
  // Update the default search engine.
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(),
      base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);
  TemplateURLData data;
  data.SetShortName(u"example.com");
  data.SetURL("https://example.com/test?q={searchTerms}");
  data.new_tab_url = chrome::kChromeUINewTabURL;
  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

  EXPECT_FALSE(IsServiceWorkerSyntheticResponseAllowed(
      profile(), GURL("https://foo.com/test")));
  EXPECT_FALSE(IsServiceWorkerSyntheticResponseAllowed(
      profile(), GURL("https://example.com/")));
  EXPECT_FALSE(IsServiceWorkerSyntheticResponseAllowed(
      profile(), GURL("https://example.com/test")));
  EXPECT_FALSE(IsServiceWorkerSyntheticResponseAllowed(
      profile(), GURL("https://example.com/test?q=")));
  EXPECT_TRUE(IsServiceWorkerSyntheticResponseAllowed(
      profile(), GURL("https://example.com/test?q=test")));
}

TEST_F(ServiceWorkerSyntheticResponseTest,
       IsServiceWorkerSyntheticResponseAllowedForAlternateUrls) {
  // Update the default search engine with an alternate URL on a different
  // origin.
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(),
      base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);
  TemplateURLData data;
  data.SetShortName(u"example.com");
  data.SetURL("https://example.com/test?q={searchTerms}");
  data.alternate_urls.push_back("https://other.test/{searchTerms}");
  data.new_tab_url = chrome::kChromeUINewTabURL;
  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

  // The synthetic response should only be allowed for navigations to the
  // default search provider's own origin, even when an alternate URL on a
  // different origin matches.
  EXPECT_TRUE(IsServiceWorkerSyntheticResponseAllowed(
      profile(), GURL("https://example.com/test?q=test")));
  EXPECT_FALSE(IsServiceWorkerSyntheticResponseAllowed(
      profile(), GURL("https://other.test/page")));
  EXPECT_FALSE(IsServiceWorkerSyntheticResponseAllowed(
      profile(), GURL("http://example.com/test?q=test")));
}

}  // namespace chrome_service_worker
