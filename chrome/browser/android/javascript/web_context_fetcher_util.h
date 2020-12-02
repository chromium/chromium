// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_JAVASCRIPT_WEB_CONTEXT_FETCHER_UTIL_H_
#define CHROME_BROWSER_ANDROID_JAVASCRIPT_WEB_CONTEXT_FETCHER_UTIL_H_

#include <string>
#include "base/macros.h"

// Util class for functions related to web context fetching.
class WebContextFetcherUtil {
 public:
  // The JS execution function returns the JSON object as a quoted string
  // literal. Remove the surrounding quotes and the internal escaping, to
  // convert it into a JSON object that can be parsed. E.g.:
  // "{\"foo\":\"bar\"}" --> {"foo":"bar"}
  static std::string ConvertJavascriptOutputToValidJson(std::string& json);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(WebContextFetcherUtil);
};

#endif  // CHROME_BROWSER_ANDROID_JAVASCRIPT_WEB_CONTEXT_FETCHER_UTIL_H_
