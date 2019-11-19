// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_TEST_UTIL_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_TEST_UTIL_H_

#include <memory>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service_observer.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"

class KeywordWebDataService;
class TemplateURLService;
class TestingProfile;

// Sets the managed preferences for the default search provider.
// enabled arg enables/disables use of managed engine by DefaultSearchManager.
void SetManagedDefaultSearchPreferences(const TemplateURLData& managed_data,
                                        bool enabled,
                                        TestingProfile* profile);

// Removes all the managed preferences for the default search provider.
void RemoveManagedDefaultSearchPreferences(TestingProfile* profile);

class TemplateURLServiceTestUtil : public TemplateURLServiceObserver {
 public:
  TemplateURLServiceTestUtil();
  ~TemplateURLServiceTestUtil() override;

  // TemplateURLServiceObserver implemementation.
  void OnTemplateURLServiceChanged() override;

  // Gets the observer count.
  int GetObserverCount();

  // Sets the observer count to 0.
  void ResetObserverCount();

  // Makes sure the load was successful and sent the correct notification.
  void VerifyLoad();

  // Makes the model believe it has been loaded (without actually doing the
  // load). Since this avoids setting the built-in keyword version, the next
  // load will do a merge from prepopulated data.
  void ChangeModelToLoadState();

  // Deletes the current model (and doesn't create a new one).
  void ClearModel();

  // Creates a new TemplateURLService.
  void ResetModel(bool verify_load);

  // Returns the search term from the last invocation of
  // TemplateURLService::SetKeywordSearchTermsForURL and clears the search term.
  base::string16 GetAndClearSearchTerm();

  // Adds extension controlled TemplateURL to the model and overrides default
  // search pref in an extension controlled preferences, if extension wants to
  // be default.
  TemplateURL* AddExtensionControlledTURL(
      std::unique_ptr<TemplateURL> extension_turl);

  // Removes a TemplateURL controlled by |extension_id| from the model, and,
  // if necessary, from the extension-controlled default search preference.
  // This TemplateURL must exist.
  void RemoveExtensionControlledTURL(const std::string& extension_id);

  KeywordWebDataService* web_data_service() { return web_data_service_.get(); }
  TemplateURLService* model() { return model_.get(); }
  TestingProfile* profile() { return profile_.get(); }

 private:
  std::unique_ptr<TestingProfile> profile_;
  base::ScopedTempDir temp_dir_;
  int changed_count_;
  base::string16 search_term_;
  scoped_refptr<KeywordWebDataService> web_data_service_;
  std::unique_ptr<TemplateURLService> model_;
  data_decoder::test::InProcessDataDecoder data_decoder_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLServiceTestUtil);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_TEST_UTIL_H_
