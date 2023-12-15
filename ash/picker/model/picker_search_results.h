// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_MODEL_PICKER_SEARCH_RESULTS_H_
#define ASH_PICKER_MODEL_PICKER_SEARCH_RESULTS_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/containers/span.h"

namespace ash {

// The search results for a particular Picker query.
class ASH_EXPORT PickerSearchResults {
 public:
  // Search results are divided into different sections.
  class Section {
   public:
    explicit Section(const std::u16string& heading,
                     base::span<const std::u16string> results);
    Section(const Section& other);
    Section& operator=(const Section& other);
    ~Section();

   private:
    std::u16string heading_;

    // TODO(b/310088338): Support result types beyond just literal text.
    std::vector<std::u16string> results_;
  };

  explicit PickerSearchResults(base::span<const Section> sections);
  PickerSearchResults(const PickerSearchResults& other);
  PickerSearchResults& operator=(const PickerSearchResults& other);
  ~PickerSearchResults();

  base::span<const Section> sections() const;

 private:
  // Sections ordered by relevance.
  std::vector<Section> sections_;
};

}  // namespace ash

#endif  // ASH_PICKER_MODEL_PICKER_SEARCH_RESULTS_H_
