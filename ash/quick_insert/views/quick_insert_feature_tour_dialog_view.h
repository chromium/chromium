// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_FEATURE_TOUR_DIALOG_VIEW_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_FEATURE_TOUR_DIALOG_VIEW_H_

#include "ash/ash_export.h"
#include "ash/style/system_dialog_delegate_view.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/metadata/view_factory.h"

namespace ui {
class Accelerator;
}

namespace views {
class StyledLabel;
class Link;
}  // namespace views

namespace ash {

class ASH_EXPORT QuickInsertFeatureTourDialogView
    : public SystemDialogDelegateView {
  METADATA_HEADER(QuickInsertFeatureTourDialogView, SystemDialogDelegateView)

 public:
  enum class EditorStatus {
    kEligible,
    kNotEligible,
  };

  explicit QuickInsertFeatureTourDialogView(
      EditorStatus editor_status,
      base::RepeatingClosure learn_more_callback,
      base::OnceClosure completion_callback);
  ~QuickInsertFeatureTourDialogView() override;

  // SystemDialogDelegateView:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // Returns the link to learn more.
  const views::Link* learn_more_link_for_testing() const;
  // Returns the button to complete the tour.
  const views::Button* complete_button_for_testing() const;
  // Returns the button to close the tour.
  const views::Button* close_button_for_testing() const;

 private:
  void CloseWidget();

  raw_ptr<views::StyledLabel> body_text_for_testing_ = nullptr;
  raw_ptr<views::Button> close_button_for_testing_ = nullptr;

  base::WeakPtrFactory<QuickInsertFeatureTourDialogView> weak_ptr_factory_{
      this};
};

BEGIN_VIEW_BUILDER(ASH_EXPORT,
                   QuickInsertFeatureTourDialogView,
                   SystemDialogDelegateView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::QuickInsertFeatureTourDialogView)

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_FEATURE_TOUR_DIALOG_VIEW_H_
