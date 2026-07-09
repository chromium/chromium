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

bool OnboardingManager::ShowOnboardingIfNeeded(tabs::TabInterface& tab,
                                               const TargetId& target_id) {
  if (pref_service_->GetBoolean(prefs::kPrefDictationOnboardingCompleted)) {
    return false;
  }

  // TODO(b/525857719): Handle the case where the FRE is triggered from a second
  // tab while a first tab already has an active FRE.
  if (dialog_controller_) {
    return true;
  }

  // TODO(bokan): I think we can extract this from the dialog_controller_ rather
  // than explicitly holding a weak ptr here.
  pending_tab_ = tab.GetWeakPtr();
  pending_target_id_ = target_id;

  dialog_controller_ = std::make_unique<OnboardingDialogController>(tab);
  dialog_controller_->Show(
      base::BindOnce(&OnboardingManager::OnOnboardingCompleted,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&OnboardingManager::OnDialogClosed,
                     weak_ptr_factory_.GetWeakPtr()));

  if (!dialog_controller_->IsShowing()) {
    dialog_controller_.reset();
    pending_tab_.reset();
    pending_target_id_.reset();
    // TODO(b/527240600): Fails closed but this should report an error somehow.
  }

  return true;
}

void OnboardingManager::OnOnboardingCompleted() {
  pref_service_->SetBoolean(prefs::kPrefDictationOnboardingCompleted, true);
  if (pending_tab_) {
    CHECK(pending_target_id_);
    service_->StartSession(*pending_tab_, *pending_target_id_);
  }
  pending_tab_.reset();
  pending_target_id_.reset();
}

void OnboardingManager::OnDialogClosed() {
  dialog_controller_.reset();
}

}  // namespace dictation
