// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_MODEL_PICKER_SEARCH_RESULTS_SECTION_H_
#define ASH_PICKER_MODEL_PICKER_SEARCH_RESULTS_SECTION_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/picker/picker_search_result.h"
#include "base/containers/span.h"

namespace ash {

enum class ASH_EXPORT PickerSectionType {
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
class ASH_EXPORT PickerSearchResultsSection {
 public:
  explicit PickerSearchResultsSection(PickerSectionType type,
                                      std::vector<PickerSearchResult> results,
                                      bool has_more_results);
  PickerSearchResultsSection(const PickerSearchResultsSection& other);
  PickerSearchResultsSection& operator=(
      const PickerSearchResultsSection& other);
  PickerSearchResultsSection(PickerSearchResultsSection&& other);
  PickerSearchResultsSection& operator=(PickerSearchResultsSection&& other);
  ~PickerSearchResultsSection();

  PickerSectionType type() const;

  base::span<const PickerSearchResult> results() const;

  bool has_more_results() const;

 private:
  PickerSectionType type_;
  std::vector<PickerSearchResult> results_;
  bool has_more_results_;
};

}  // namespace ash

#endif  // ASH_PICKER_MODEL_PICKER_SEARCH_RESULTS_SECTION_H_
