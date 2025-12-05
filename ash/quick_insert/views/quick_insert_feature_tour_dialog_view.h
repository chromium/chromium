// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_FEATURE_TOUR_DIALOG_VIEW_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_FEATURE_TOUR_DIALOG_VIEW_H_

#include "ash/ash_export.h"
#include "ash/style/system_dialog_delegate_view.h"
#include "base/functional/callback_forward.h"
#include "ui/views/metadata/view_factory.h"

namespace ui {
class Accelerator;
}

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
      base::OnceClosure learn_more_callback,
      base::OnceClosure completion_callback);
  ~QuickInsertFeatureTourDialogView() override;

  // SystemDialogDelegateView:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // Returns the button to learn more.
  const views::Button* learn_more_button_for_testing() const;
  // Returns the button to complete the tour.
  const views::Button* complete_button_for_testing() const;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT,
                   QuickInsertFeatureTourDialogView,
                   SystemDialogDelegateView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::QuickInsertFeatureTourDialogView)

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_FEATURE_TOUR_DIALOG_VIEW_H_
