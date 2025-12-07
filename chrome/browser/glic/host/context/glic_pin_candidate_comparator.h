// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PIN_CANDIDATE_COMPARATOR_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PIN_CANDIDATE_COMPARATOR_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/types/optional_ref.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace content {

class WebContents;

}  // namespace content

namespace glic {

struct SearchResult {
  bool matches_prefix = false;
  bool matches_word_prefix = false;
  bool contained = false;
};

// This class is meant to be used in conjunction with std::sort to put the pin
// candidates in a good order following the priorities listed in
// chrome/browser/resources/tab_search/search.ts.
class GlicPinCandidateComparator {
 public:
  explicit GlicPinCandidateComparator(
      const base::optional_ref<const std::string> query);
  ~GlicPinCandidateComparator();

  GlicPinCandidateComparator(const GlicPinCandidateComparator&) = delete;
  GlicPinCandidateComparator& operator=(const GlicPinCandidateComparator&) =
      delete;

  bool operator()(content::WebContents* a, content::WebContents* b);

 private:
  const SearchResult GetSearchResults(std::u16string_view title);

  std::u16string query_;
  absl::flat_hash_map<std::u16string, SearchResult> search_results_cache_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PIN_CANDIDATE_COMPARATOR_H_
