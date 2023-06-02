// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_KERBEROS_IN_BROWSER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_KERBEROS_IN_BROWSER_ASH_H_

#include "chromeos/crosapi/mojom/kerberos_in_browser.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Implements the crosapi KerberosInBrowser interface. Lives in ash-chrome on
// the UI thread. Shows Kerberos UI in response to mojo IPCs from lacros-chrome.
class KerberosInBrowserAsh : public mojom::KerberosInBrowser {
 public:
  KerberosInBrowserAsh();
  KerberosInBrowserAsh(const KerberosInBrowserAsh&) = delete;
  KerberosInBrowserAsh& operator=(const KerberosInBrowserAsh&) = delete;
  ~KerberosInBrowserAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::KerberosInBrowser> receiver);

  // crosapi::mojom::KerberosInBrowser
  void ShowKerberosInBrowserDialog() override;

 private:
  mojo::ReceiverSet<mojom::KerberosInBrowser> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_KERBEROS_IN_BROWSER_ASH_H_
