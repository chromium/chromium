// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/onboarding_manager.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/dictation/onboarding_dialog_controller.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"

namespace dictation {

OnboardingManager::OnboardingManager(DictationKeyedService& service,
                                     PrefService& pref_service)
    : service_(service), pref_service_(pref_service) {}

OnboardingManager::~OnboardingManager() = default;

bool OnboardingManager::ShowOnboardingIfNeeded(
    BrowserWindowInterface& window,
    const TargetId& target_id,
    const std::string& selected_text) {
  if (pref_service_->GetBoolean(prefs::kPrefDictationOnboardingCompleted)) {
    return false;
  }

  // TODO(b/525853741): Passed in argument should be a TabInterface.
  tabs::TabInterface* active_tab = window.GetActiveTabInterface();
  if (!active_tab) {
    return true;
  }

  // TODO(b/525857719): Handle the case where the FRE is triggered from a second
  // tab while a first tab already has an active FRE.
  if (dialog_controller_) {
    return true;
  }

  pending_window_ = window.GetWeakPtr();
  pending_target_id_ = target_id;
  pending_selected_text_ = selected_text;

  dialog_controller_ =
      std::make_unique<OnboardingDialogController>(*active_tab);
  dialog_controller_->Show(
      base::BindOnce(&OnboardingManager::OnOnboardingCompleted,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&OnboardingManager::OnDialogClosed,
                     weak_ptr_factory_.GetWeakPtr()));

  if (!dialog_controller_->IsShowing()) {
    dialog_controller_.reset();
    pending_window_.reset();
    pending_target_id_.reset();
    pending_selected_text_.clear();
    // TODO(b/527240600): Fails closed but this should report an error somehow.
  }

  return true;
}

void OnboardingManager::OnOnboardingCompleted() {
  pref_service_->SetBoolean(prefs::kPrefDictationOnboardingCompleted, true);
  if (pending_window_) {
    CHECK(pending_target_id_);
    service_->StartSession(*pending_window_, *pending_target_id_,
                           pending_selected_text_);
  }
  pending_window_.reset();
  pending_target_id_.reset();
  pending_selected_text_.clear();
}

void OnboardingManager::OnDialogClosed() {
  dialog_controller_.reset();
}

}  // namespace dictation
