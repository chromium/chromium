// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <optional>

#include "base/memory/memory_pressure_listener.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/preloading/autocomplete_dictionary_preload_service.h"
#include "chrome/browser/preloading/autocomplete_dictionary_preload_service_factory.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_browser_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace {

constexpr char kOmniboxSuggestPrefetchQuery[] = "porgs";

}  // namespace

// Test suite to check the AutocompleteDictionaryPreload feature.
class AutocompleteDictionaryPreloadBrowserTest
    : public SearchPrefetchBaseBrowserTest {
 public:
  AutocompleteDictionaryPreloadBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {kAutocompleteDictionaryPreload,
         {{"autocomplete_preloaded_dictionary_timeout", "10ms"}}}};
    feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
  }

 protected:
  bool HasPreloadedSharedDictionaryInfo() {
    base::test::TestFuture<bool> future;
    browser()
        ->profile()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->HasPreloadedSharedDictionaryInfoForTesting(future.GetCallback());
    return future.Get();
  }

  void SendMemoryPressureToNetworkService() {
    content::GetNetworkService()->OnMemoryPressure(
        base::MEMORY_PRESSURE_LEVEL_CRITICAL);
    // To make sure that OnMemoryPressure has been received by the network
    // service, send a GetNetworkList IPC and wait for the result.
    base::RunLoop run_loop;
    content::GetNetworkService()->GetNetworkList(
        net::INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES,
        base::BindLambdaForTesting(
            [&](const std::optional<net::NetworkInterfaceList>&
                    interface_list) { run_loop.Quit(); }));
    run_loop.Run();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AutocompleteDictionaryPreloadBrowserTest,
                       PreloadDictionaryAndDiscard) {
  auto* dictionary_preload_service =
      AutocompleteDictionaryPreloadServiceFactory::GetForProfile(
          browser()->profile());
  std::string search_terms = kOmniboxSuggestPrefetchQuery;
  AutocompleteMatch autocomplete_match =
      CreateSearchSuggestionMatch(search_terms, search_terms, false);
  AutocompleteResult autocomplete_result;
  autocomplete_result.AppendMatches({autocomplete_match});
  dictionary_preload_service->MaybePreload(autocomplete_result);
  EXPECT_TRUE(HasPreloadedSharedDictionaryInfo());
  WaitForDuration(base::Milliseconds(11));
  EXPECT_FALSE(HasPreloadedSharedDictionaryInfo());
}

IN_PROC_BROWSER_TEST_F(AutocompleteDictionaryPreloadBrowserTest,
                       NonHttpFamilyAreIgnored) {
  auto* dictionary_preload_service =
      AutocompleteDictionaryPreloadServiceFactory::GetForProfile(
          browser()->profile());
  std::string search_terms = kOmniboxSuggestPrefetchQuery;
  AutocompleteMatch autocomplete_match =
      CreateSearchSuggestionMatch(search_terms, search_terms, false);
  autocomplete_match.destination_url = GURL("chrome://blank");
  AutocompleteResult autocomplete_result;
  autocomplete_result.AppendMatches({autocomplete_match});
  dictionary_preload_service->MaybePreload(autocomplete_result);
  EXPECT_FALSE(HasPreloadedSharedDictionaryInfo());
}

IN_PROC_BROWSER_TEST_F(AutocompleteDictionaryPreloadBrowserTest,
                       DoNotPreloadDictionayUnderMemoryPressure) {
  auto* dictionary_preload_service =
      AutocompleteDictionaryPreloadServiceFactory::GetForProfile(
          browser()->profile());
  std::string search_terms = kOmniboxSuggestPrefetchQuery;
  AutocompleteMatch autocomplete_match =
      CreateSearchSuggestionMatch(search_terms, search_terms, false);
  AutocompleteResult autocomplete_result;
  autocomplete_result.AppendMatches({autocomplete_match});
  SendMemoryPressureToNetworkService();
  dictionary_preload_service->MaybePreload(autocomplete_result);
  EXPECT_FALSE(HasPreloadedSharedDictionaryInfo());
}

IN_PROC_BROWSER_TEST_F(AutocompleteDictionaryPreloadBrowserTest,
                       PreloadedDictionayDiscardedByMemoryPressure) {
  auto* dictionary_preload_service =
      AutocompleteDictionaryPreloadServiceFactory::GetForProfile(
          browser()->profile());
  std::string search_terms = kOmniboxSuggestPrefetchQuery;
  AutocompleteMatch autocomplete_match =
      CreateSearchSuggestionMatch(search_terms, search_terms, false);
  AutocompleteResult autocomplete_result;
  autocomplete_result.AppendMatches({autocomplete_match});
  dictionary_preload_service->MaybePreload(autocomplete_result);
  EXPECT_TRUE(HasPreloadedSharedDictionaryInfo());
  SendMemoryPressureToNetworkService();
  EXPECT_FALSE(HasPreloadedSharedDictionaryInfo());
}
