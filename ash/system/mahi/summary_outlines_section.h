// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_SUMMARY_OUTLINES_SECTION_H_
#define ASH_SYSTEM_MAHI_SUMMARY_OUTLINES_SECTION_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

// The view containing the summary and outlines section within the Mahi panel.
class ASH_EXPORT SummaryOutlinesSection : public views::BoxLayoutView {
  METADATA_HEADER(SummaryOutlinesSection, views::BoxLayoutView)

 public:
  SummaryOutlinesSection();
  SummaryOutlinesSection(const SummaryOutlinesSection&) = delete;
  SummaryOutlinesSection& operator=(const SummaryOutlinesSection&) = delete;
  ~SummaryOutlinesSection() override;

 private:
  base::WeakPtrFactory<SummaryOutlinesSection> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAHI_SUMMARY_OUTLINES_SECTION_H_
