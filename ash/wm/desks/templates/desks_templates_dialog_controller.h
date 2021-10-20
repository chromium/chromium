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

class DesksTemplatesDialog;

// DesksTemplatesDialogController controls when to show the various confirmation
// dialogs for modifying desk templates.
class ASH_EXPORT DesksTemplatesDialogController : public views::WidgetObserver {
 public:
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

  // Shows the dialog. The dialog will look slightly different depending on the
  // type. The `template_name` is used for the replace and delete dialogs, which
  // show the name of the template which will be replaced and deleted in the
  // dialog description.
  void ShowUnsupportedAppsDialog(aura::Window* root_window);
  void ShowReplaceDialog(aura::Window* root_window,
                         const std::u16string& template_name);
  void ShowDeleteDialog(aura::Window* root_window,
                        const std::u16string& template_name);

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  // Creates and shows the dialog on `root_window`.
  void CreateDialogWidget(std::unique_ptr<DesksTemplatesDialog> dialog,
                          aura::Window* root_window);

  // Pointer to the widget (if any) that contains the current dialog.
  views::Widget* dialog_widget_ = nullptr;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      dialog_widget_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_DIALOG_CONTROLLER_H_
