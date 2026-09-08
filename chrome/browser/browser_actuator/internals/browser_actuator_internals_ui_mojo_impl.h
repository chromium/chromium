// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_ACTUATOR_INTERNALS_BROWSER_ACTUATOR_INTERNALS_UI_MOJO_IMPL_H_
#define CHROME_BROWSER_BROWSER_ACTUATOR_INTERNALS_BROWSER_ACTUATOR_INTERNALS_UI_MOJO_IMPL_H_

#include "chrome/browser/browser_actuator/internals/browser_actuator_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace browser_actuator {

// Mojo implementation of
// browser_actuator_internals::mojom::BrowserActuatorInternalsUI.
class BrowserActuatorInternalsUIMojoImpl
    : public browser_actuator_internals::mojom::BrowserActuatorInternalsUI {
 public:
  BrowserActuatorInternalsUIMojoImpl(
      mojo::PendingReceiver<
          browser_actuator_internals::mojom::BrowserActuatorInternalsUI>
          receiver,
      mojo::PendingRemote<
          browser_actuator_internals::mojom::BrowserActuatorInternalsPage>
          page);
  BrowserActuatorInternalsUIMojoImpl(
      const BrowserActuatorInternalsUIMojoImpl&) = delete;
  BrowserActuatorInternalsUIMojoImpl& operator=(
      const BrowserActuatorInternalsUIMojoImpl&) = delete;
  ~BrowserActuatorInternalsUIMojoImpl() override;

 private:
  mojo::Receiver<browser_actuator_internals::mojom::BrowserActuatorInternalsUI>
      receiver_;
  mojo::Remote<browser_actuator_internals::mojom::BrowserActuatorInternalsPage>
      page_;
};

}  // namespace browser_actuator

#endif  // CHROME_BROWSER_BROWSER_ACTUATOR_INTERNALS_BROWSER_ACTUATOR_INTERNALS_UI_MOJO_IMPL_H_
