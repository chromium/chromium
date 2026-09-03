// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REQUEST_HEADER_INTEGRITY_CHROME_COMPANERO_HOST_H_
#define CHROME_BROWSER_REQUEST_HEADER_INTEGRITY_CHROME_COMPANERO_HOST_H_

#include "base/threading/sequence_bound.h"
#include "chrome/common/request_header_integrity/chrome_companero.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace request_header_integrity {

// Browser-process host for Request Header Integrity.
//
// Owned by GlobalFeatures on the browser UI thread. Serves header integrity
// tokens to child processes (such as Renderers) over Mojo.
//
// Native dynamic library loading (libchromecompaneros) and C function execution
// are offloaded to base::ThreadPool via a SequenceBound Backend object to avoid
// blocking the browser UI thread and to guarantee shutdown safety.
class ChromeCompaneroHost {
 public:
  ChromeCompaneroHost();
  ChromeCompaneroHost(const ChromeCompaneroHost&) = delete;
  ChromeCompaneroHost& operator=(const ChromeCompaneroHost&) = delete;
  ~ChromeCompaneroHost();

  // Binds an incoming Mojo receiver from a child process (typically via
  // BrowserInterfaceBroker).
  void BindReceiver(mojo::PendingReceiver<mojom::ChromeCompanero> receiver);

  // Asynchronously generates or fetches a header name and token pair on the
  // thread pool, returning nullptr on error or if the feature is disabled.
  using GetHeaderNameAndValueCallback =
      mojom::ChromeCompanero::GetHeaderNameAndValueCallback;
  void GetHeaderNameAndValue(GetHeaderNameAndValueCallback callback);

 private:
  friend class ChromeCompaneroHostTest;

  class Backend;

  // Backend worker living on a sequenced ThreadPool runner.
  base::SequenceBound<Backend> backend_;
};

}  // namespace request_header_integrity

#endif  // CHROME_BROWSER_REQUEST_HEADER_INTEGRITY_CHROME_COMPANERO_HOST_H_
