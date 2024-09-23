// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_SUMMARY_OUTLINES_SECTION_H_
#define ASH_SYSTEM_MAHI_SUMMARY_OUTLINES_SECTION_H_

#include <optional>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/mahi/mahi_ui_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
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
class ASH_EXPORT SummaryOutlinesSection : public views::BoxLayoutView,
                                          public MahiUiController::Delegate {
  METADATA_HEADER(SummaryOutlinesSection, views::BoxLayoutView)

 public:
  explicit SummaryOutlinesSection(MahiUiController* ui_controller);
  SummaryOutlinesSection(const SummaryOutlinesSection&) = delete;
  SummaryOutlinesSection& operator=(const SummaryOutlinesSection&) = delete;
  ~SummaryOutlinesSection() override;

 private:
  // MahiUiController::Delegate:
  views::View* GetView() override;
  bool GetViewVisibility(VisibilityState state) const override;
  void OnUpdated(const MahiUiUpdate& update) override;

  // views::BoxLayoutView:
  void AddedToWidget() override;

  void HandleOutlinesLoaded(const std::vector<chromeos::MahiOutline>& outlines);

  void HandleSummaryLoaded(const std::u16string& summary_text);

  // Requests summary and outlines data from `MahiManager` for the currently
  // active content and starts playing the loading animations.
  void LoadSummaryAndOutlines();

  // `ui_controller_` will outlive `this`.
  const raw_ptr<MahiUiController> ui_controller_;

  // Owned by the views hierarchy.
  raw_ptr<views::AnimatedImageView> summary_loading_animated_image_ = nullptr;
  raw_ptr<views::AnimatedImageView> outlines_loading_animated_image_ = nullptr;
  raw_ptr<views::Label> summary_label_ = nullptr;
  raw_ptr<views::View> outlines_container_ = nullptr;

  base::Time summary_start_loading_time_;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, SummaryOutlinesSection, views::BoxLayoutView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::SummaryOutlinesSection)

#endif  // ASH_SYSTEM_MAHI_SUMMARY_OUTLINES_SECTION_H_
