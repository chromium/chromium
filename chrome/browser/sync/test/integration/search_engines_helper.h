// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SEARCH_ENGINES_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SEARCH_ENGINES_HELPER_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"

class TemplateURL;

using GUIDToTURLMap = std::map<std::string, const TemplateURL*>;

namespace search_engines_helper {

// Used to access the search engines within a particular sync profile.
TemplateURLService* GetServiceForBrowserContext(int profile_index);

// Used to access the search engines within the verifier sync profile.
TemplateURLService* GetVerifierService();

// Compared a single TemplateURLService for a given profile to the verifier.
// Retrns true iff their user-visible fields match.
bool ServiceMatchesVerifier(int profile_index);

// Returns true iff all TemplateURLServices match with the verifier.
bool AllServicesMatch();
bool AllServicesMatch(std::ostream* os);

// Builder class that by default infers all fields from |keyword| and allows
// overriding those default values.
class TemplateURLBuilder {
 public:
  explicit TemplateURLBuilder(const std::string& keyword);
  ~TemplateURLBuilder();

  TemplateURLData* data() { return &data_; }
  std::unique_ptr<TemplateURL> Build();

 private:
  TemplateURLData data_;
};

// Add a search engine based on a keyword to the service at index
// |profile_index| and the verifier if it is used.
void AddSearchEngine(int profile_index, const std::string& keyword);

// Retrieves a search engine from the service at index |profile_index| with
// original keyword |keyword| and changes its user-visible fields. Does the same
// to the verifier, if it is used.
void EditSearchEngine(int profile_index,
                      const std::string& keyword,
                      const std::u16string& short_name,
                      const std::string& new_keyword,
                      const std::string& url);

// Deletes a search engine from the service at index |profile_index| with
// |keyword|.
void DeleteSearchEngine(int profile_index, const std::string& keyword);

// Changes the search engine with |keyword| to be the new default for
// |profile_index|. Does the same to the verifier, if it is used.
void ChangeDefaultSearchProvider(int profile_index, const std::string& keyword);

// Returns true if the profile at |profile_index| has a search engine matching
// |keyword|.
bool HasSearchEngine(int profile_index, const std::string& keyword);

// Returns the keyword for the default search engine at |profile_index|.
std::string GetDefaultSearchEngineKeyword(int profile_index);

// Checker that blocks until all services have the same search engine data.
class SearchEnginesMatchChecker : public StatusChangeChecker,
                                  public TemplateURLServiceObserver {
 public:
  SearchEnginesMatchChecker();
  ~SearchEnginesMatchChecker() override;

  // StatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // TemplateURLServiceObserver overrides.
  void OnTemplateURLServiceChanged() override;

 private:
  base::ScopedMultiSourceObservation<TemplateURLService,
                                     TemplateURLServiceObserver>
      observations_{this};
};

// Checker that blocks until |profile_index| has a search engine matching the
// search engine generated with |keyword|.
class HasSearchEngineChecker : public StatusChangeChecker,
                               public TemplateURLServiceObserver {
 public:
  HasSearchEngineChecker(int profile_index, const std::string& keyword);
  ~HasSearchEngineChecker() override;

  // StatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // TemplateURLServiceObserver overrides.
  void OnTemplateURLServiceChanged() override;

 private:
  const raw_ptr<TemplateURLService> service_;
  const std::u16string keyword_;
  base::ScopedMultiSourceObservation<TemplateURLService,
                                     TemplateURLServiceObserver>
      observations_{this};
};

}  // namespace search_engines_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SEARCH_ENGINES_HELPER_H_
