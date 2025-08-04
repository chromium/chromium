// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_UI_THREAD_SEARCH_TERMS_DATA_ANDROID_H_
#define CHROME_BROWSER_SEARCH_ENGINES_UI_THREAD_SEARCH_TERMS_DATA_ANDROID_H_

#include <optional>
#include <string>

// Additional data needed by TemplateURLRef::ReplaceSearchTerms on Android.
struct SearchTermsDataAndroid {
  static std::u16string& GetRlzParameterValue();
  static std::string& GetSearchClient();
  static std::optional<std::string>& GetCustomTabSearchClient();
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_UI_THREAD_SEARCH_TERMS_DATA_ANDROID_H_
