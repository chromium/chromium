// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_FULLSCREEN_CONTROLLER_CLIENT_LACROS_H_
#define CHROME_BROWSER_LACROS_FULLSCREEN_CONTROLLER_CLIENT_LACROS_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/fullscreen_controller.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
class KeepFullscreenForUrlChecker;
}  // namespace chromeos

namespace content {
class WebContents;
}  // namespace content

// The lacros-chrome implementation of the full screen controller client crosapi
// interface. Receives and processes requests from ash-chrome.
class FullscreenControllerClientLacros
    : public crosapi::mojom::FullscreenControllerClient {
 public:
  FullscreenControllerClientLacros();
  FullscreenControllerClientLacros(const FullscreenControllerClientLacros&) =
      delete;
  FullscreenControllerClientLacros& operator=(
      const FullscreenControllerClientLacros&) = delete;
  ~FullscreenControllerClientLacros() override;

 private:
  // crosapi::mojom::FullscreenControllerClient:
  void ShouldExitFullscreenBeforeLock(
      base::OnceCallback<void(bool)> callback) override;

  content::WebContents* GetActiveAppWindowWebContents();

  std::unique_ptr<chromeos::KeepFullscreenForUrlChecker>
      keep_fullscreen_checker_;

  // Endpoint to communicate with Ash.
  mojo::Receiver<crosapi::mojom::FullscreenControllerClient> receiver_{this};

  base::WeakPtrFactory<FullscreenControllerClientLacros> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_LACROS_FULLSCREEN_CONTROLLER_CLIENT_LACROS_H_
