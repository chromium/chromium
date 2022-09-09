// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_CHROME_AUTOCOMPLETE_SCHEME_CLASSIFIER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_CHROME_AUTOCOMPLETE_SCHEME_CLASSIFIER_H_

#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"

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
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_CHROME_AUTOCOMPLETE_SCHEME_CLASSIFIER_H_
