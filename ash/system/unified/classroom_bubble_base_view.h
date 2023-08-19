// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_BASE_VIEW_H_
#define ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_BASE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/unified/glanceable_tray_child_bubble.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view_observer.h"

class GURL;

namespace views {
class Combobox;
class Label;
}

namespace ui {
class ComboboxModel;
}

namespace ash {

class GlanceablesListFooterView;
class GlanceablesProgressBarView;
struct GlanceablesClassroomAssignment;

class ASH_EXPORT ClassroomBubbleBaseView : public GlanceableTrayChildBubble,
                                           public views::ViewObserver {
 public:
  METADATA_HEADER(ClassroomBubbleBaseView);

  // TODO(b:283370907): Add classroom glanceable contents.
  ClassroomBubbleBaseView(DetailedViewDelegate* delegate,
                          std::unique_ptr<ui::ComboboxModel> combobox_model);
  ClassroomBubbleBaseView(const ClassroomBubbleBaseView&) = delete;
  ClassroomBubbleBaseView& operator=(const ClassroomBubbleBaseView&) = delete;
  ~ClassroomBubbleBaseView() override;

  // views::ViewObserver:
  void OnViewFocused(views::View* view) override;

 protected:
  // Handles press on the "See all" button in `GlanceablesListFooterView`. Opens
  // classroom web UI based on the selected menu option.
  virtual void OnSeeAllPressed() = 0;

  // Called from the bubble view implementation before it requests new list of
  // assignments. Updates base bubble UI to indicate the content is being
  // updated.
  void AboutToRequestAssignments();

  // Handles received assignments by rendering them in `list_container_view_`.
  void OnGetAssignments(
      const std::u16string& list_name,
      bool success,
      std::vector<std::unique_ptr<GlanceablesClassroomAssignment>> assignments);

  // Opens classroom url.
  void OpenUrl(const GURL& url) const;

  // Announces text describing the assignment list state through a screen
  // reader, using `combo_box_view_` view accessibility helper.
  void AnnounceListStateOnComboBoxAccessibility();

  // Total number of assignments in the selected assignment list.
  size_t total_assignments_ = 0u;

  // Owned by views hierarchy.
  raw_ptr<views::FlexLayoutView, ExperimentalAsh> header_view_ = nullptr;
  raw_ptr<views::Combobox, ExperimentalAsh> combo_box_view_ = nullptr;
  raw_ptr<views::View, ExperimentalAsh> list_container_view_ = nullptr;
  raw_ptr<GlanceablesListFooterView, ExperimentalAsh> list_footer_view_ =
      nullptr;
  raw_ptr<GlanceablesProgressBarView, ExperimentalAsh> progress_bar_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> empty_list_label_ = nullptr;

  base::ScopedObservation<views::View, views::ViewObserver>
      combobox_view_observation_{this};

  base::WeakPtrFactory<ClassroomBubbleBaseView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_BASE_VIEW_H_
