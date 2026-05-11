// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_controller.h"

#include <memory>

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_dialog_view.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"

namespace glic {

GlicExperimentalOptInController::GlicExperimentalOptInController(
    Profile* profile)
    : profile_(profile) {}

GlicExperimentalOptInController::~GlicExperimentalOptInController() = default;

views::Widget* GlicExperimentalOptInController::ShowDialog(
    content::WebContents* web_contents) {
  if (dialog_widget_) {
    dialog_widget_->Show();
    return dialog_widget_.get();
  }

  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab_interface) {
    return nullptr;
  }

  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_);
  if (!service || service->enabling().GetRequiredExperimentalOptIn() ==
                      RequiredExperimentalOptIn::kNotNeeded) {
    return nullptr;
  }

  dialog_view_ = std::make_unique<GlicExperimentalOptInDialogView>(profile_);
  dialog_view_->SetOwnershipOfNewWidget(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  dialog_widget_ = tab_interface->GetTabFeatures()
                       ->tab_dialog_manager()
                       ->CreateAndShowDialog(
                           dialog_view_.get(),
                           std::make_unique<tabs::TabDialogManager::Params>());

  dialog_widget_->MakeCloseSynchronous(
      base::BindOnce(&GlicExperimentalOptInController::CloseWidget,
                     weak_ptr_factory_.GetWeakPtr()));

  return dialog_widget_.get();
}

void GlicExperimentalOptInController::CloseDialog() {
  CloseWidget(views::Widget::ClosedReason::kUnspecified);
}

void GlicExperimentalOptInController::CloseWidget(
    views::Widget::ClosedReason reason) {
  dialog_widget_.reset();
  dialog_view_.reset();
}

}  // namespace glic
