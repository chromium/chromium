// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_TAB_DATA_H_
#define CHROME_BROWSER_GLIC_GLIC_TAB_DATA_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/glic.mojom.h"
#include "content/public/browser/web_contents.h"

namespace glic {
struct FocusedTabCandidate {
  FocusedTabCandidate(
      content::WebContents* const web_contents,
      glic::mojom::InvalidCandidateError invalid_candidate_error);
  ~FocusedTabCandidate();
  FocusedTabCandidate(const FocusedTabCandidate& other);
  FocusedTabCandidate(FocusedTabCandidate&& other) noexcept;
  FocusedTabCandidate& operator=(const FocusedTabCandidate& other);
  FocusedTabCandidate& operator=(FocusedTabCandidate&& other) noexcept;
  bool operator==(const FocusedTabCandidate& other) const;

  base::WeakPtr<content::WebContents> focused_tab_candidate_contents = nullptr;
  glic::mojom::InvalidCandidateError invalid_candidate_error;
};

struct FocusedTabData {
  FocusedTabData(
      content::WebContents* const web_contents,
      std::optional<glic::mojom::InvalidCandidateError> invalid_candidate_error,
      std::optional<glic::mojom::NoCandidateTabError> no_candidate_tab_error);
  ~FocusedTabData();
  FocusedTabData(const FocusedTabData& other);
  FocusedTabData(FocusedTabData&& other) noexcept;
  FocusedTabData& operator=(const FocusedTabData& other);
  FocusedTabData& operator=(FocusedTabData&& other) noexcept;
  bool operator==(const FocusedTabData& other) const;

  base::WeakPtr<content::WebContents> focused_tab_contents = nullptr;
  std::optional<FocusedTabCandidate> focused_tab_candidate;
  std::optional<glic::mojom::NoCandidateTabError> no_candidate_tab_error;
};

// Populates and returns a TabDataPtr from a given WebContents, or null if
// web_contents is null.
glic::mojom::TabDataPtr CreateTabData(content::WebContents* web_contents);

// Populates and returns a FocusedTabDataPtr from a given FocusedTabData.
glic::mojom::FocusedTabDataPtr CreateFocusedTabData(
    FocusedTabData focused_tab_data);
}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_TAB_DATA_H_
