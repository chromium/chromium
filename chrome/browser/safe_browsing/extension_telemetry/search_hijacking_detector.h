// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_SEARCH_HIJACKING_DETECTOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_SEARCH_HIJACKING_DETECTOR_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

class PrefService;
class TemplateURLService;
struct AutocompleteMatch;

namespace safe_browsing {

class ExtensionTelemetryReportRequest_SearchHijackingSignal;

// A class that implements a heuristic to detect potential victims of
// search hijacking extensions. It works by checking for a discrepancy
// between the number of omnibox searches and the number of search results
// page (SERP) landings.
class SearchHijackingDetector {
 public:
  explicit SearchHijackingDetector(PrefService* pref_service,
                                   TemplateURLService* template_url_service);
  ~SearchHijackingDetector();

  SearchHijackingDetector(const SearchHijackingDetector&) = delete;
  SearchHijackingDetector& operator=(const SearchHijackingDetector&) = delete;

  // Records an omnibox search event associated with
  // the default search engine (DSE)
  void OnOmniboxSearch(const AutocompleteMatch& match);

  // Records a DSE SERP landing event.
  void OnDseSerpLoaded();

  // This method is meant to be called periodically. It checks to see if
  // enough time has elapsed since the last check. Then it checks for a
  // heuristics match and saves the match data in prefs.
  void MaybeCheckForHeuristicMatch();

  // Returns a search hijacking signal object if one is stored in prefs.
  std::unique_ptr<ExtensionTelemetryReportRequest_SearchHijackingSignal>
  GetSignalForReport();

  // Clears all data related to search hijacking detection from prefs.
  void ClearAllDataFromPrefs();

  // A struct to hold the counts of omnibox searches and SERP landings.
  struct EventCounts {
    int omnibox_searches = 0;
    int serp_landings = 0;
  };

  // Returns the current counts of omnibox searches and SERP landings.
  EventCounts GetCurrentEventCountsForTesting();

  // Returns the last time the heuristic check was performed.
  base::Time GetLastHeuristicCheckTimeForTesting();

  // The minimum interval between heuristic checks.
  static constexpr base::TimeDelta kDefaultHeuristicCheckInterval =
      base::Hours(8);

  // Sets the interval between heuristic checks.
  void SetHeuristicCheckInterval(base::TimeDelta interval);

  // Returns the interval between heuristic checks.
  base::TimeDelta GetHeuristicCheckInterval() const;

  // Default heuristic threshold.
  static constexpr int kDefaultHeuristicThreshold = 2;

  // Sets the threshold for the heuristic. Triggers a heuristic match if:
  // (omnibox_search_count - serp_landing_count >= threshold)
  void SetHeuristicThreshold(int threshold);

  // Returns the threshold for the heuristic.
  int GetHeuristicThreshold() const;

 private:
  // Used to store search hijacking data in prefs.
  raw_ptr<PrefService> pref_service_;

  // Used to check for the default search provider.
  raw_ptr<TemplateURLService> template_url_service_;

  // The interval between heuristic checks.
  base::TimeDelta heuristic_check_interval_ = kDefaultHeuristicCheckInterval;

  // The threshold for the difference between omnibox searches and SERP
  // landings that triggers the heuristic.
  int heuristic_threshold_ = kDefaultHeuristicThreshold;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_SEARCH_HIJACKING_DETECTOR_H_
