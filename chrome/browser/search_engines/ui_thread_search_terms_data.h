// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_UI_THREAD_SEARCH_TERMS_DATA_H_
#define CHROME_BROWSER_SEARCH_ENGINES_UI_THREAD_SEARCH_TERMS_DATA_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/search_engines/search_terms_data.h"

// Implementation of SearchTermsData that is only usable on the UI thread.
class UIThreadSearchTermsData : public SearchTermsData {
 public:
  UIThreadSearchTermsData();

  std::string GoogleBaseURLValue() const override;
  std::string GetApplicationLocale() const override;
  base::string16 GetRlzParameterValue(bool from_app_list) const override;
  std::string GetSearchClient() const override;
  std::string GetSuggestClient() const override;
  std::string GetSuggestRequestIdentifier() const override;
  std::string GoogleImageSearchSource() const override;

#if defined(OS_ANDROID)
  std::string GetYandexReferralID() const override;
  std::string GetMailRUReferralID() const override;
#endif

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UIThreadSearchTermsData);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_UI_THREAD_SEARCH_TERMS_DATA_H_
