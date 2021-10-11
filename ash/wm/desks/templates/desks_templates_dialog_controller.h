// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_DIALOG_CONTROLLER_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_DIALOG_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/scoped_observation.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace aura {
class Window;
}

namespace ash {

// DesksTemplatesDialogController controls when to show the various confirmation
// dialogs for modifying desk templates.
class ASH_EXPORT DesksTemplatesDialogController : public views::WidgetObserver {
 public:
  // The types of dialogs that desks templates can show. Each one has slight
  // variations in the UI.
  enum class DialogType {
    // Shown when a user tries to create a new desk template from the current
    // desk, and there are unsupported apps (i.e. crostini).
    kUnsupported,
    // TODO(sammiequon): Add a description for when this dialog gets shown.
    kReplace,
    // Shown when a user tries to delete a template using the delete button.
    kDelete,
  };

  DesksTemplatesDialogController();
  DesksTemplatesDialogController(const DesksTemplatesDialogController&) =
      delete;
  DesksTemplatesDialogController& operator=(
      const DesksTemplatesDialogController&) = delete;
  ~DesksTemplatesDialogController() override;

  // Convenience function to get the controller instance, which is created and
  // owned by OverviewSession.
  static DesksTemplatesDialogController* Get();

  const views::Widget* dialog_widget() const { return dialog_widget_; }

  // Shows the dialog according to `type`.
  // TODO(sammiequon): This should also take a vector of images for
  // `kUnsupported`, which should be empty for other types.
  void Show(DialogType type, aura::Window* root_window);

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  // Pointer to the widget (if any) that contains the current dialog.
  views::Widget* dialog_widget_ = nullptr;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      dialog_widget_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_DIALOG_CONTROLLER_H_
