// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRACING_WINDOWS_SYSTEM_TRACING_CLIENT_WIN_H_
#define CHROME_BROWSER_TRACING_WINDOWS_SYSTEM_TRACING_CLIENT_WIN_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/process/process_handle.h"
#include "base/win/windows_types.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/tracing/public/mojom/traced_process.mojom-forward.h"

// A client to the ETW tracing service. See
// //chrome/windows_services/elevated_tracing_service for the service itself.
class WindowsSystemTracingClient {
 public:
  static std::unique_ptr<WindowsSystemTracingClient> Create(const CLSID& clsid,
                                                            const IID& iid);

  WindowsSystemTracingClient(const WindowsSystemTracingClient&);
  WindowsSystemTracingClient& operator=(const WindowsSystemTracingClient&);
  virtual ~WindowsSystemTracingClient() = default;

  using OnRemoteProcess = base::OnceCallback<void(
      base::ProcessId pid,
      mojo::PendingRemote<tracing::mojom::TracedProcess> remote_process)>;

  // Starts the service asynchronously, running `on_remote_process` with a
  // PendingRemote associated with the service's TracedProcess. The callback
  // will not be run following destruction of the instance.
  virtual void Start(OnRemoteProcess on_remote_process) = 0;

 protected:
  WindowsSystemTracingClient() = default;
};

#endif  // CHROME_BROWSER_TRACING_WINDOWS_SYSTEM_TRACING_CLIENT_WIN_H_
