// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_MODEL_QUICK_INSERT_SEARCH_RESULTS_SECTION_H_
#define ASH_QUICK_INSERT_MODEL_QUICK_INSERT_SEARCH_RESULTS_SECTION_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "base/containers/span.h"

namespace ash {

enum class ASH_EXPORT QuickInsertSectionType {
  kNone,
  kClipboard,
  kExamples,
  kLinks,
  kLocalFiles,
  kDriveFiles,
  kContentEditor,
  kMaxValue = kContentEditor,
};

// Search results are divided into different sections.
class ASH_EXPORT QuickInsertSearchResultsSection {
 public:
  explicit QuickInsertSearchResultsSection(
      QuickInsertSectionType type,
      std::vector<QuickInsertSearchResult> results,
      bool has_more_results);
  QuickInsertSearchResultsSection(const QuickInsertSearchResultsSection& other);
  QuickInsertSearchResultsSection& operator=(
      const QuickInsertSearchResultsSection& other);
  QuickInsertSearchResultsSection(QuickInsertSearchResultsSection&& other);
  QuickInsertSearchResultsSection& operator=(
      QuickInsertSearchResultsSection&& other);
  ~QuickInsertSearchResultsSection();

  QuickInsertSectionType type() const;

  base::span<const QuickInsertSearchResult> results() const;

  bool has_more_results() const;

 private:
  QuickInsertSectionType type_;
  std::vector<QuickInsertSearchResult> results_;
  bool has_more_results_;
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_MODEL_QUICK_INSERT_SEARCH_RESULTS_SECTION_H_
