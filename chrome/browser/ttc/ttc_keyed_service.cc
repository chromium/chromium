// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/ttc_keyed_service.h"

#include "chrome/browser/ttc/session_controller_impl.h"
#include "chrome/browser/ttc/ttc_keyed_service_factory.h"

namespace ttc {

// static
TtcKeyedService* TtcKeyedService::Get(content::BrowserContext* context) {
  return TtcKeyedServiceFactory::GetTtcKeyedService(context);
}

TtcKeyedService::TtcKeyedService(Profile* profile) : profile_(profile) {}

TtcKeyedService::~TtcKeyedService() = default;

void TtcKeyedService::Shutdown() {
  EndSession();
}

void TtcKeyedService::StartSession() {
  CHECK(!session_controller_);
  session_controller_ = std::make_unique<SessionControllerImpl>(*this);
}

void TtcKeyedService::EndSession() {
  session_controller_.reset();
}

}  // namespace ttc
