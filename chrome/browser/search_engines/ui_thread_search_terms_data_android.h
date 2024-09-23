// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_UI_THREAD_SEARCH_TERMS_DATA_ANDROID_H_
#define CHROME_BROWSER_SEARCH_ENGINES_UI_THREAD_SEARCH_TERMS_DATA_ANDROID_H_

#include <optional>
#include <string>

#include "base/lazy_instance.h"

// Additional data needed by TemplateURLRef::ReplaceSearchTerms on Android.
struct SearchTermsDataAndroid {
  static base::LazyInstance<std::u16string>::Leaky rlz_parameter_value_;
  static base::LazyInstance<std::string>::Leaky search_client_;
  static base::LazyInstance<std::optional<std::string>>::Leaky
      custom_tab_search_client_;
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_UI_THREAD_SEARCH_TERMS_DATA_ANDROID_H_
