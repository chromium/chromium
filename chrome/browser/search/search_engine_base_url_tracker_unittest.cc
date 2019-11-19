// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search_engine_base_url_tracker.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/search/instant_unittest_base.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "url/gurl.h"

using SearchEngineBaseURLTrackerTest = InstantUnitTestBase;

TEST_F(SearchEngineBaseURLTrackerTest, DispatchDefaultSearchProviderChanged) {
  base::MockCallback<SearchEngineBaseURLTracker::BaseURLChangedCallback>
      callback;
  SearchEngineBaseURLTracker tracker(
      template_url_service_, std::make_unique<UIThreadSearchTermsData>(),
      callback.Get());

  // Changing the search provider should invoke the callback.
  EXPECT_CALL(
      callback,
      Run(SearchEngineBaseURLTracker::ChangeReason::DEFAULT_SEARCH_PROVIDER));
  SetUserSelectedDefaultSearchProvider("https://bar.com/");
}
