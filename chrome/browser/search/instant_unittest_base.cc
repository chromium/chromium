// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_unittest_base.h"

#include <string>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/search/search.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"

InstantUnitTestBase::InstantUnitTestBase() {
}

InstantUnitTestBase::~InstantUnitTestBase() {
}

void InstantUnitTestBase::SetUp() {
  BrowserWithTestWindowTest::SetUp();

  clock_ = new base::SimpleTestClock();

  template_url_service_ = TemplateURLServiceFactory::GetForProfile(profile());
  search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service_);

  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  instant_service_ = InstantServiceFactory::GetForProfile(profile());
}

void InstantUnitTestBase::TearDown() {
  delete clock_;
  clock_ = nullptr;

  BrowserWithTestWindowTest::TearDown();
}

ntp_tiles::MostVisitedSites* InstantUnitTestBase::most_visited_sites() {
  return instant_service_->most_visited_sites_.get();
}

void InstantUnitTestBase::SetUserSelectedDefaultSearchProvider(
    const std::string& base_url) {
  TemplateURLData data;
  data.SetShortName(base::UTF8ToUTF16(base_url));
  data.SetKeyword(base::UTF8ToUTF16(base_url));
  data.SetURL(base_url + "url?bar={searchTerms}");
  data.new_tab_url = base_url + "newtab";
  data.alternate_urls.push_back(base_url + "alt#quux={searchTerms}");

  TemplateURL* template_url =
      template_url_service_->Add(std::make_unique<TemplateURL>(data));
  template_url_service_->SetUserSelectedDefaultSearchProvider(template_url);
}

TestingProfile* InstantUnitTestBase::CreateProfile(
    const std::string& profile_name) {
  TestingProfile* profile =
      BrowserWithTestWindowTest::CreateProfile(profile_name);
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile,
      base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
  return profile;
}
