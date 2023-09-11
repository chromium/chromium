// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_CORE_COMPANION_URL_BUILDER_H_
#define CHROME_BROWSER_COMPANION_CORE_COMPANION_URL_BUILDER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "url/gurl.h"

class PrefService;
namespace companion {
class SigninDelegate;

// Utility to build URL for the search companion request. The URL contains
// various query parameters needed at the server side such as main page URL,
// origin, promo state etc. The params are packed into a single protobuf for
// schema consistency.
class CompanionUrlBuilder {
 public:
  CompanionUrlBuilder(PrefService* pref_service,
                      SigninDelegate* signin_delegate);
  CompanionUrlBuilder(const CompanionUrlBuilder&) = delete;
  CompanionUrlBuilder& operator=(const CompanionUrlBuilder&) = delete;
  ~CompanionUrlBuilder();

  // Returns the companion URL that will be loaded in the side panel with the
  // query parameter set to the protobuf representation of the `page_url` and
  // associated state. Invokes `BuildCompanionUrlParamProto` internally to
  // create the proto.
  GURL BuildCompanionURL(const GURL& page_url);
  GURL BuildCompanionURL(
      const GURL& page_url,
      const std::string& text_query,
      std::unique_ptr<base::Time> text_query_start_time = nullptr);

  // Returns the `base_url` with the query parameters set to the protobuf
  // representation of the `page_url` and associated state.
  GURL AppendCompanionParamsToURL(
      const GURL& base_url,
      const GURL& page_url,
      const std::string& text_query,
      std::unique_ptr<base::Time> text_query_start_time = nullptr);

  // Returns the protobuf representation of the `page_url` and
  // associated state. Used to notify the companion page both during initial
  // load and subsequent state updates.
  std::string BuildCompanionUrlParamProto(
      const GURL& page_url,
      std::unique_ptr<base::Time> text_query_start_time = nullptr);

 private:
  raw_ptr<PrefService> pref_service_;
  raw_ptr<SigninDelegate> signin_delegate_;
};

}  // namespace companion

#endif  // CHROME_BROWSER_COMPANION_CORE_COMPANION_URL_BUILDER_H_
