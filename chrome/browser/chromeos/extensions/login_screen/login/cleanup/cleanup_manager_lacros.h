// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_CLEANUP_MANAGER_LACROS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_CLEANUP_MANAGER_LACROS_H_

#include "base/functional/callback.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_manager.h"
#include "chromeos/crosapi/mojom/login.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class BrowserContext;
}

namespace chromeos {

// A KeyedService which manages the cleanup handlers in Lacros.
class CleanupManagerLacros
    : public CleanupManager,
      public crosapi::mojom::LacrosCleanupTriggeredObserver,
      public KeyedService {
 public:
  explicit CleanupManagerLacros(content::BrowserContext* browser_context);

  CleanupManagerLacros(const CleanupManagerLacros&) = delete;
  CleanupManagerLacros& operator=(const CleanupManagerLacros&) = delete;

  ~CleanupManagerLacros() override;

  // crosapi::mojom::LacrosTriggeredObserver:
  void OnLacrosCleanupTriggered(
      OnLacrosCleanupTriggeredCallback callback) override;

 private:
  void InitializeCleanupHandlers() override;

  mojo::Receiver<crosapi::mojom::LacrosCleanupTriggeredObserver> receiver_{
      this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_CLEANUP_MANAGER_LACROS_H_
