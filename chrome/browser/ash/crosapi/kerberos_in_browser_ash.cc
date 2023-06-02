// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/kerberos_in_browser_ash.h"

#include "chrome/browser/ui/webui/ash/kerberos/kerberos_in_browser_dialog.h"

namespace crosapi {

KerberosInBrowserAsh::KerberosInBrowserAsh() = default;

KerberosInBrowserAsh::~KerberosInBrowserAsh() = default;

void KerberosInBrowserAsh::BindReceiver(
    mojo::PendingReceiver<mojom::KerberosInBrowser> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void KerberosInBrowserAsh::ShowKerberosInBrowserDialog() {
  ash::KerberosInBrowserDialog::Show();
}

}  // namespace crosapi
