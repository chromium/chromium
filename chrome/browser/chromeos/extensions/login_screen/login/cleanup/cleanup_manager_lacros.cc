// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_manager_lacros.h"

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/browsing_data_cleanup_handler.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

namespace {

// Must kept in sync with the CleanupHandler variant in
// tools/metrics/histograms/metadata/enterprise/histograms.xml
constexpr char kLacrosBrowsingDataCleanupHandlerHistogramName[] =
    "LacrosBrowsingData";

}  // namespace

CleanupManagerLacros::CleanupManagerLacros(
    content::BrowserContext* browser_context) {
  chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
  DCHECK(lacros_service);
  if (LacrosService::Get()->IsAvailable<crosapi::mojom::Login>()) {
    lacros_service->GetRemote<crosapi::mojom::Login>()
        ->AddLacrosCleanupTriggeredObserver(
            receiver_.BindNewPipeAndPassRemoteWithVersion());
  }
}

CleanupManagerLacros::~CleanupManagerLacros() = default;

void CleanupManagerLacros::InitializeCleanupHandlers() {
  // TODO(jityao, b:217155485): Add ExtensionCleanupHandler.
  cleanup_handlers_.insert({kLacrosBrowsingDataCleanupHandlerHistogramName,
                            std::make_unique<BrowsingDataCleanupHandler>()});
}

void CleanupManagerLacros::OnLacrosCleanupTriggered(
    OnLacrosCleanupTriggeredCallback callback) {
  Cleanup(std::move(callback));
}

}  // namespace chromeos
