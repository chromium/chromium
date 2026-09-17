// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REQUEST_HEADER_INTEGRITY_CHROME_COMPANERO_HOST_H_
#define CHROME_BROWSER_REQUEST_HEADER_INTEGRITY_CHROME_COMPANERO_HOST_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/task_traits.h"
#include "base/threading/sequence_bound.h"
#include "chrome/common/request_header_integrity/chrome_companero.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace request_header_integrity {

// Carrier for the "TokenTaskPriority" param below. Enabled by default;
// disabling it simply pins the priority to the compiled-in default.
BASE_DECLARE_FEATURE(kRequestHeaderIntegrityTokenPriority);

// base::ThreadPool priority of the sequence that loads libchromecompaneros and
// generates integrity tokens. Configured via the "TokenTaskPriority" param,
// whose accepted values are the base::TaskPriorityToString() spellings:
// "BEST_EFFORT", "USER_VISIBLE" (default) and "USER_BLOCKING".
//
// This work always runs on a base::ThreadPool sequence, never on the UI or IO
// thread, and nothing on a request path ever blocks on it. The priority
// therefore only controls how soon the sequence is scheduled, not who runs it.
BASE_DECLARE_FEATURE_PARAM(base::TaskPriority, kTokenTaskPriority);

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
