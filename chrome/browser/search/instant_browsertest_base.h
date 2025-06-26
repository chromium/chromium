// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_INSTANT_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_SEARCH_INSTANT_BROWSERTEST_BASE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_clock.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"

// This class provides an extension on top of InProcessBrowserTest, and
// adds some utility methods which can be useful for various unit tests for
// Embedded Search / Instant implementation classes.
class InstantBrowserTestBase : public InProcessBrowserTest {
 protected:
  InstantBrowserTestBase();
  ~InstantBrowserTestBase() override;

  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  ntp_tiles::MostVisitedSites* most_visited_sites();

  // Adds and sets the default search provider using the base_url.
  // The base_url should have the http[s]:// prefix and a trailing / after the
  // TLD.
  // It will always use an instant-enabled configuration using a
  // search_terms_replacement_key.
  void SetUserSelectedDefaultSearchProvider(const std::string& base_url);

  raw_ptr<InstantService, DanglingUntriaged> instant_service_;
  raw_ptr<TemplateURLService, DanglingUntriaged> template_url_service_;
  raw_ptr<base::SimpleTestClock, DanglingUntriaged> clock_;

 private:
  Profile* CreateProfile(const std::string& profile_name);
};

#endif  // CHROME_BROWSER_SEARCH_INSTANT_BROWSERTEST_BASE_H_
