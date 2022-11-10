// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_manager_lacros.h"

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/browser_cleanup_handler.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/extension_cleanup_handler.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

namespace {

// Must kept in sync with the CleanupHandler variant in
// tools/metrics/histograms/metadata/enterprise/histograms.xml
constexpr char kLacrosBrowserCleanupHandlerHistogramName[] = "LacrosBrowser";
constexpr char kLacrosExtensionCleanupHandlerHistogramName[] =
    "LacrosExtension";

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
  cleanup_handlers_.insert({kLacrosBrowserCleanupHandlerHistogramName,
                            std::make_unique<BrowserCleanupHandler>()});
  cleanup_handlers_.insert({kLacrosExtensionCleanupHandlerHistogramName,
                            std::make_unique<ExtensionCleanupHandler>()});
}

void CleanupManagerLacros::OnLacrosCleanupTriggered(
    OnLacrosCleanupTriggeredCallback callback) {
  Cleanup(std::move(callback));
}

}  // namespace chromeos
