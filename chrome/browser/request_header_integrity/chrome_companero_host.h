// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REQUEST_HEADER_INTEGRITY_CHROME_COMPANERO_HOST_H_
#define CHROME_BROWSER_REQUEST_HEADER_INTEGRITY_CHROME_COMPANERO_HOST_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/request_header_integrity/chrome_companero.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace request_header_integrity {

// Browser-process host for Request Header Integrity.
// Owned by GlobalFeatures on BrowserProcess. Manages background library
// loading and serves token requests from sandboxed child renderers over Mojo.
class ChromeCompaneroHost : public mojom::ChromeCompanero {
 public:
  ChromeCompaneroHost();
  ChromeCompaneroHost(const ChromeCompaneroHost&) = delete;
  ChromeCompaneroHost& operator=(const ChromeCompaneroHost&) = delete;
  ~ChromeCompaneroHost() override;

  // Binds an incoming Mojo receiver from a child process.
  void BindReceiver(mojo::PendingReceiver<mojom::ChromeCompanero> receiver);

  // mojom::ChromeCompanero implementation:
  void GetHeaderNameAndValue(GetHeaderNameAndValueCallback callback) override;

 private:
  // Receivers connected from child processes.
  mojo::ReceiverSet<mojom::ChromeCompanero> receivers_;

  base::WeakPtrFactory<ChromeCompaneroHost> weak_factory_{this};
};

}  // namespace request_header_integrity

#endif  // CHROME_BROWSER_REQUEST_HEADER_INTEGRITY_CHROME_COMPANERO_HOST_H_
