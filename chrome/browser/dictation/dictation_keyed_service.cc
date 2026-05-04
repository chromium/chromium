// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_keyed_service.h"

#include "chrome/browser/dictation/dictation_keyed_service_factory.h"
#include "chrome/browser/dictation/session_controller.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/profiles/profile.h"

namespace dictation {

// static
DictationKeyedService* DictationKeyedService::Get(
    content::BrowserContext* context) {
  return DictationKeyedServiceFactory::GetDictationKeyedService(context);
}

DictationKeyedService::DictationKeyedService(Profile* profile)
    : profile_(profile) {}

DictationKeyedService::~DictationKeyedService() = default;

void DictationKeyedService::Shutdown() {
  EndSession();
}

std::unique_ptr<StreamProvider> DictationKeyedService::CreateStreamProvider(
    SessionController& controller) const {
  return nullptr;
}

std::unique_ptr<Ui> DictationKeyedService::CreateUi(
    SessionController& controller) const {
  return nullptr;
}

void DictationKeyedService::StartSession(Target* target) {
  CHECK(!session_controller_);

  session_controller_ = std::make_unique<SessionController>(*this);

  if (target) {
    session_controller_->StartDictationStream(*target);
  }
}

void DictationKeyedService::EndSession() {
  session_controller_.reset();
}

}  // namespace dictation
