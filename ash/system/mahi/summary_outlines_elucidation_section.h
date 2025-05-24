// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_SUMMARY_OUTLINES_ELUCIDATION_SECTION_H_
#define ASH_SYSTEM_MAHI_SUMMARY_OUTLINES_ELUCIDATION_SECTION_H_

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

// The view containing the summary/outlines/elucidation section within the Mahi
// panel.
class ASH_EXPORT SummaryOutlinesElucidationSection
    : public views::BoxLayoutView,
      public MahiUiController::Delegate {
  METADATA_HEADER(SummaryOutlinesElucidationSection, views::BoxLayoutView)

 public:
  explicit SummaryOutlinesElucidationSection(MahiUiController* ui_controller);
  SummaryOutlinesElucidationSection(const SummaryOutlinesElucidationSection&) =
      delete;
  SummaryOutlinesElucidationSection& operator=(
      const SummaryOutlinesElucidationSection&) = delete;
  ~SummaryOutlinesElucidationSection() override;

 private:
  enum class ContentType {
    kSummaryAndOutline,
    kElucidation,
  };
  // MahiUiController::Delegate:
  views::View* GetView() override;
  bool GetViewVisibility(VisibilityState state) const override;
  void OnUpdated(const MahiUiUpdate& update) override;

  // views::BoxLayoutView:
  void AddedToWidget() override;

  void HandleOutlinesLoaded(const std::vector<chromeos::MahiOutline>& outlines);

  void HandleSummaryOrElucidationLoaded(const std::u16string& result_text);

  // Requests (summary and outlines) or elucidation  from `MahiManager` for the
  // currently active content and starts playing the loading animations.
  void LoadContentForDisplay(ContentType content_type);

  // `ui_controller_` will outlive `this`.
  const raw_ptr<MahiUiController> ui_controller_;

  // Owned by the views hierarchy.
  raw_ptr<views::AnimatedImageView>
      summary_or_elucidation_loading_animated_image_ = nullptr;
  raw_ptr<views::AnimatedImageView> outlines_loading_animated_image_ = nullptr;
  // Label that indicates whether the result is a summary or an elucidation.
  raw_ptr<views::Label> indicator_label_ = nullptr;
  // Label that hosts the summary or elucidation result.
  raw_ptr<views::Label> summary_or_elucidation_label_ = nullptr;
  raw_ptr<views::View> outlines_container_ = nullptr;

  base::Time start_loading_time_;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT,
                   SummaryOutlinesElucidationSection,
                   views::BoxLayoutView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::SummaryOutlinesElucidationSection)

#endif  // ASH_SYSTEM_MAHI_SUMMARY_OUTLINES_ELUCIDATION_SECTION_H_
