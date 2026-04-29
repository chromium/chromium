// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_controller.h"

#include <memory>

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_dialog_view.h"
#include "chrome/browser/profiles/profile.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"

namespace glic {

GlicExperimentalOptInController::GlicExperimentalOptInController(
    Profile* profile)
    : profile_(profile) {}

GlicExperimentalOptInController::~GlicExperimentalOptInController() {
  CloseDialog();
}

views::Widget* GlicExperimentalOptInController::ShowDialog(
    content::WebContents* web_contents) {
  views::View* existing_view = view_tracker_.view();
  if (existing_view) {
    existing_view->GetWidget()->Show();
    return existing_view->GetWidget();
  }

  auto view = std::make_unique<GlicExperimentalOptInDialogView>(profile_);
  view_tracker_.SetView(view->GetContentsView());

  return constrained_window::ShowWebModalDialogViews(view.release(),
                                                     web_contents);
}

void GlicExperimentalOptInController::CloseDialog() {
  views::View* view = view_tracker_.view();
  if (view) {
    view->GetWidget()->Close();
  }
}

}  // namespace glic
