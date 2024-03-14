// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_SUMMARY_OUTLINES_SECTION_H_
#define ASH_SYSTEM_MAHI_SUMMARY_OUTLINES_SECTION_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/metadata/view_factory.h"

namespace views {
class AnimatedImageView;
class Label;
}  // namespace views

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
  // Requests summary and outlines data from `MahiManager` for the currently
  // active content and starts playing the loading animations.
  void LoadSummaryAndOutlines();

  // Callback provided to the `MahiManager` which runs when the summary is
  // available.
  void OnSummaryLoaded(std::u16string summary,
                       chromeos::MahiResponseStatus status);

  // Callback provided to the `MahiManager` which runs when all outlines are
  // available.
  void OnOutlinesLoaded(std::vector<chromeos::MahiOutline> outlines,
                        chromeos::MahiResponseStatus status);

  raw_ptr<views::AnimatedImageView> summary_loading_animated_image_ = nullptr;
  raw_ptr<views::AnimatedImageView> outlines_loading_animated_image_ = nullptr;
  raw_ptr<views::Label> summary_label_ = nullptr;

  base::WeakPtrFactory<SummaryOutlinesSection> weak_ptr_factory_{this};
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, SummaryOutlinesSection, views::BoxLayoutView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::SummaryOutlinesSection)

#endif  // ASH_SYSTEM_MAHI_SUMMARY_OUTLINES_SECTION_H_
