// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/onboarding_manager.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/ui/views/dictation/onboarding_dialog_controller.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"

namespace dictation {

OnboardingManager::PendingSession::PendingSession(
    base::WeakPtr<tabs::TabInterface> tab,
    std::unique_ptr<Target> target,
    DictationSessionEntryPoint entry_point)
    : tab(std::move(tab)),
      target(std::move(target)),
      entry_point(entry_point) {}

OnboardingManager::PendingSession::PendingSession(PendingSession&&) = default;
OnboardingManager::PendingSession& OnboardingManager::PendingSession::operator=(
    PendingSession&&) = default;
OnboardingManager::PendingSession::~PendingSession() = default;

OnboardingManager::OnboardingManager(DictationKeyedService& service,
                                     PrefService& pref_service)
    : service_(service), pref_service_(pref_service) {}

OnboardingManager::~OnboardingManager() = default;

bool OnboardingManager::NeedsOnboarding() const {
  return !pref_service_->GetBoolean(prefs::kPrefDictationOnboardingCompleted);
}

void OnboardingManager::ShowOnboarding(tabs::TabInterface& tab,
                                       std::unique_ptr<Target> target,
                                       DictationSessionEntryPoint entry_point) {
  CHECK(NeedsOnboarding());

  // If an FRE dialog is already active on another tab, close it before opening
  // a new FRE dialog on the current tab.
  if (dialog_controller_) {
    dialog_controller_.reset();
    pending_session_.reset();
  }

  // TODO(bokan): I think we can extract this from the dialog_controller_ rather
  // than explicitly holding a weak ptr here.
  pending_session_.emplace(tab.GetWeakPtr(), std::move(target), entry_point);

  dialog_controller_ = std::make_unique<OnboardingDialogController>(tab);
  dialog_controller_->Show(
      base::BindOnce(&OnboardingManager::OnOnboardingCompleted,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&OnboardingManager::OnDialogClosed,
                     weak_ptr_factory_.GetWeakPtr()));

  if (!dialog_controller_->IsShowing()) {
    dialog_controller_.reset();
    pending_session_.reset();
    // TODO(b/527240600): Fails closed but this should report an error somehow.
  }
}

void OnboardingManager::OnOnboardingCompleted() {
  pref_service_->SetBoolean(prefs::kPrefDictationOnboardingCompleted, true);
}

void OnboardingManager::OnDialogClosed() {
  std::optional<PendingSession> pending = std::move(pending_session_);
  pending_session_.reset();
  dialog_controller_.reset();

  if (!NeedsOnboarding()) {
    if (pending && pending->tab && pending->target) {
      service_->DidCompleteOnboarding(*pending->tab, std::move(pending->target),
                                      pending->entry_point);
    }
  }
}

}  // namespace dictation
