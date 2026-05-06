// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_keyed_service.h"

#include "base/feature_list.h"
#include "chrome/browser/dictation/dictation_keyed_service_factory.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/session_controller.h"
#include "chrome/browser/dictation/session_ui_impl.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

namespace dictation {

// static
DictationKeyedService* DictationKeyedService::Get(
    content::BrowserContext* context) {
  return DictationKeyedServiceFactory::GetDictationKeyedService(context);
}

DictationKeyedService::SessionState::SessionState(
    SessionControllerDelegate& delegate,
    base::WeakPtr<BrowserWindowInterface> window)
    : controller_(delegate), window_(window) {}

DictationKeyedService::SessionState::~SessionState() = default;

DictationKeyedService::DictationKeyedService(Profile* profile)
    : profile_(profile) {
  CHECK(base::FeatureList::IsEnabled(kDictation));
}

DictationKeyedService::~DictationKeyedService() = default;

void DictationKeyedService::Shutdown() {
  EndSession();
}

std::unique_ptr<StreamProvider> DictationKeyedService::CreateStreamProvider(
    SessionController& controller) const {
  return nullptr;
}

std::unique_ptr<SessionUi> DictationKeyedService::CreateUi(
    SessionController& controller) const {
  CHECK(session_);
  if (!session_->window_) {
    return nullptr;
  }

  return std::make_unique<SessionUiImpl>(*session_->window_, controller);
}

void DictationKeyedService::StartSession(BrowserWindowInterface& window,
                                         Target* target) {
  CHECK(!session_);

  session_.emplace(*this, window.GetWeakPtr());

  session_->controller_.Initialize();

  if (target) {
    session_->controller_.StartDictationStream(*target);
  }
}

void DictationKeyedService::EndSession() {
  session_.reset();
}

}  // namespace dictation
