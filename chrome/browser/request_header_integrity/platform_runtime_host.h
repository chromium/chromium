// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REQUEST_HEADER_INTEGRITY_PLATFORM_RUNTIME_HOST_H_
#define CHROME_BROWSER_REQUEST_HEADER_INTEGRITY_PLATFORM_RUNTIME_HOST_H_

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "chrome/common/request_header_integrity/platform_runtime.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/backoff_entry.h"
#include "net/http/http_request_headers.h"

namespace request_header_integrity {

// Owns the browser side of Platform Runtime header distribution.
//
// The native library runs only in a sandboxed utility process. This class is
// the only thing that talks to that process. It periodically asks the service
// for a fresh set of headers, and publishes it to this process and to every
// renderer.
//
// Lives on the UI thread.
class PlatformRuntimeHost {
 public:
  PlatformRuntimeHost();

  PlatformRuntimeHost(const PlatformRuntimeHost&) = delete;
  PlatformRuntimeHost& operator=(const PlatformRuntimeHost&) = delete;

  ~PlatformRuntimeHost();

  // Called by the component installer once the library is installed or
  // updated.
  void OnComponentReady(const base::FilePath& library_path);

  // The most recent output of the library. Empty until the first successful
  // refresh.
  const net::HttpRequestHeaders& headers() const;

  // Notifies `callback` whenever headers() changes. Renderer plumbing uses
  // this to rebroadcast the new value.
  base::CallbackListSubscription RegisterHeadersChangedCallback(
      base::RepeatingClosure callback);

  void SetHeadersForTesting(net::HttpRequestHeaders headers);

  // Replaces process launching with `launcher`, which receives the pending
  // receiver that would otherwise have gone to a real service process. Pass a
  // null callback to restore normal behaviour.
  using ServiceLauncher = base::RepeatingCallback<void(
      mojo::PendingReceiver<mojom::PlatformRuntimeService>)>;
  void SetServiceLauncherForTesting(ServiceLauncher launcher);

 private:
  // Asks the service for a fresh set of headers, launching it if needed.
  void Refresh() VALID_CONTEXT_REQUIRED(sequence_checker_);

  void OnHeadersReceived(base::expected<net::HttpRequestHeaders,
                                        mojom::PlatformRuntimeStatus> result)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // The service accepted the call but never answered.
  void OnRequestTimedOut() VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Common recovery for a service that crashed or hung: tear it down and
  // schedule a relaunch under `launch_backoff_`.
  void HandleServiceFailure() VALID_CONTEXT_REQUIRED(sequence_checker_);

  void Publish(net::HttpRequestHeaders headers)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Empty until the component is installed and verified.
  base::FilePath library_path_ GUARDED_BY_CONTEXT(sequence_checker_);

  net::HttpRequestHeaders headers_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::RepeatingClosureList headers_changed_callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  ServiceLauncher service_launcher_ GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::Remote<mojom::PlatformRuntimeService> service_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Remote<mojom::PlatformRuntime> runtime_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Schedules the next Refresh() after each completed reply.
  base::RetainingOneShotTimer refresh_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Deadline after which an in-flight ProcessHeaders() call is abandoned.
  base::RetainingOneShotTimer request_timeout_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Throttles relaunch attempts after the service fails or crashes, so a
  // library that always fails to load does not turn into a process-spawn
  // loop.
  net::BackoffEntry launch_backoff_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::OneShotTimer retry_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace request_header_integrity

#endif  // CHROME_BROWSER_REQUEST_HEADER_INTEGRITY_PLATFORM_RUNTIME_HOST_H_
