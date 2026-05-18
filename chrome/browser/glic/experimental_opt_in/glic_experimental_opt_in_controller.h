// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/widget/widget.h"

class Profile;

namespace content {
class WebContents;
}

namespace views {
class Widget;
}

namespace glic {

class GlicExperimentalOptInDialogView;

// Controller for the experimental triggering opt-in flow.
class GlicExperimentalOptInController {
 public:
  explicit GlicExperimentalOptInController(Profile* profile);
  GlicExperimentalOptInController(const GlicExperimentalOptInController&) =
      delete;
  GlicExperimentalOptInController& operator=(
      const GlicExperimentalOptInController&) = delete;
  ~GlicExperimentalOptInController();

  // Shows the opt-in dialog. Returns the widget, or null if the dialog could
  // not be shown. `callback` is fired on dialog close, with true if the opt in
  // was accepted, and false otherwise. If the dialog could not be shown,
  // callback is fired immediately with true if the dialog was not needed
  // because the user is already opted in, and false otherwise.
  // If a dialog is already showing, the existing dialog is returned, and
  // `callback` will fire on closing of the existing dialog.
  views::Widget* ShowDialog(content::WebContents* web_contents,
                            base::OnceCallback<void(bool)> callback);
  void CloseDialog(bool accepted);

  GlicExperimentalOptInDialogView* GetDialogViewForTesting() {
    return dialog_view_.get();
  }

 private:
  void CloseWidget(views::Widget::ClosedReason reason);

  raw_ptr<Profile> profile_;
  std::unique_ptr<GlicExperimentalOptInDialogView> dialog_view_;
  std::unique_ptr<views::Widget> dialog_widget_;

  std::vector<base::OnceCallback<void(bool)>> callbacks_;

  base::WeakPtrFactory<GlicExperimentalOptInController> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_CONTROLLER_H_
