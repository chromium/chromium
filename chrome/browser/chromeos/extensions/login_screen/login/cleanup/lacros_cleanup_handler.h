// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_LACROS_CLEANUP_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_LACROS_CLEANUP_HANDLER_H_

#include <optional>
#include <set>

#include "base/barrier_closure.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_handler.h"
#include "chromeos/crosapi/mojom/login.mojom.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {

// A cleanup handler which notifies `CleanupManagerLacros` and waits for its
// reply.
class LacrosCleanupHandler : public CleanupHandler {
 public:
  LacrosCleanupHandler();
  ~LacrosCleanupHandler() override;

  // CleanupHandler:
  void Cleanup(CleanupHandlerCallback callback) override;

  void SetCleanupTriggeredObserversForTesting(
      mojo::RemoteSet<crosapi::mojom::LacrosCleanupTriggeredObserver>*
          observers);

 private:
  void OnDisconnect(mojo::RemoteSetElementId id);

  void OnObserverDone(mojo::RemoteSetElementId id,
                      const std::optional<std::string>& error);

  void OnAllObserversDone();

  mojo::RemoteSet<crosapi::mojom::LacrosCleanupTriggeredObserver>*
  GetCleanupTriggeredObservers();

  std::set<mojo::RemoteSetElementId> pending_observers_;
  raw_ptr<mojo::RemoteSet<crosapi::mojom::LacrosCleanupTriggeredObserver>>
      observers_for_testing_ = nullptr;
  std::vector<std::string> errors_;
  base::RepeatingClosure barrier_closure_;
  bool has_set_disconnect_handlers_ = false;
  CleanupHandlerCallback callback_;
};

}  // namespace chromeos

#endif  //  CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_LACROS_CLEANUP_HANDLER_H_
