// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_CHROME_AUTOCOMPLETE_SCHEME_CLASSIFIER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_CHROME_AUTOCOMPLETE_SCHEME_CLASSIFIER_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"

#if BUILDFLAG(IS_LINUX)
#include "base/memory/scoped_refptr.h"

class XdgSchemeHandlerCache;
#endif

class Profile;

// The subclass to provide chrome-specific scheme handling.
class ChromeAutocompleteSchemeClassifier : public AutocompleteSchemeClassifier {
 public:
  explicit ChromeAutocompleteSchemeClassifier(Profile* profile);

  ChromeAutocompleteSchemeClassifier(
      const ChromeAutocompleteSchemeClassifier&) = delete;
  ChromeAutocompleteSchemeClassifier& operator=(
      const ChromeAutocompleteSchemeClassifier&) = delete;

  ~ChromeAutocompleteSchemeClassifier() override;

  // AutocompleteInputSchemeChecker:
  metrics::OmniboxInputType GetInputTypeForScheme(
      const std::string& scheme) const override;

 private:
  raw_ptr<Profile> profile_;

#if BUILDFLAG(IS_LINUX)
  // Null when features::kXdgSchemeHandlerDetection is disabled.
  scoped_refptr<XdgSchemeHandlerCache> xdg_scheme_handler_cache_;
#endif
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_CHROME_AUTOCOMPLETE_SCHEME_CLASSIFIER_H_
