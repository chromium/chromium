// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_EMBEDDED_ACCESSIBILITY_HELPER_CLIENT_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_EMBEDDED_ACCESSIBILITY_HELPER_CLIENT_ASH_H_

#include "chromeos/crosapi/mojom/embedded_accessibility_helper.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

class EmbeddedAccessibilityHelperClientAsh
    : public crosapi::mojom::EmbeddedAccessibilityHelperClientFactory,
      public crosapi::mojom::EmbeddedAccessibilityHelperClient {
 public:
  EmbeddedAccessibilityHelperClientAsh();
  EmbeddedAccessibilityHelperClientAsh(
      const EmbeddedAccessibilityHelperClientAsh&) = delete;
  EmbeddedAccessibilityHelperClientAsh& operator=(
      const EmbeddedAccessibilityHelperClientAsh&) = delete;
  ~EmbeddedAccessibilityHelperClientAsh() override;

  // crosapi::mojom::EmbeddedAccessibilityHelperClientFactory:
  void BindEmbeddedAccessibilityHelperClient(
      mojo::PendingReceiver<crosapi::mojom::EmbeddedAccessibilityHelperClient>
          embeded_ax_helper_client) override;
  void BindEmbeddedAccessibilityHelper(
      mojo::PendingRemote<crosapi::mojom::EmbeddedAccessibilityHelper>
          embedded_ax_helper) override;

  // crosapi::mojom::EmbeddedAccessibilityHelperClient:
  void SpeakSelectedText() override;
  void FocusChanged(const gfx::Rect& focus_bounds_in_screen) override;

  void ClipboardCopyInActiveGoogleDoc(const std::string& url);

  void BindEmbeddedAccessibilityHelperClientFactoryReceiver(
      mojo::PendingReceiver<
          crosapi::mojom::EmbeddedAccessibilityHelperClientFactory> receiver);

 private:
  mojo::RemoteSet<crosapi::mojom::EmbeddedAccessibilityHelper>
      embedded_ax_helper_remotes_;
  mojo::ReceiverSet<crosapi::mojom::EmbeddedAccessibilityHelperClientFactory>
      embedded_ax_helper_factory_receivers_;
  mojo::ReceiverSet<crosapi::mojom::EmbeddedAccessibilityHelperClient>
      embedded_ax_helper_receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_EMBEDDED_ACCESSIBILITY_HELPER_CLIENT_ASH_H_
