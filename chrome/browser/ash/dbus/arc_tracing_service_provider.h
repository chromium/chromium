// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_ARC_TRACING_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_ARC_TRACING_SERVICE_PROVIDER_H_

#include <deque>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"
#include "mojo/public/cpp/system/handle.h"

namespace dbus {
class MethodCall;
}  // namespace dbus

namespace arc {
class OverviewTracingHandler;
struct OverviewTracingResult;
}  // namespace arc

namespace ash {

// This class exports a D-Bus method that libvda will call to establish a
// mojo pipe to the VideoAcceleratorFactory interface.
class ArcTracingServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  ArcTracingServiceProvider();

  ArcTracingServiceProvider(const ArcTracingServiceProvider&) = delete;
  ArcTracingServiceProvider& operator=(const ArcTracingServiceProvider&) =
      delete;

  ~ArcTracingServiceProvider() override;

  void set_trace_outdir_for_testing(const base::FilePath& trace_outdir) {
    trace_outdir_ = trace_outdir;
  }

  // CrosDBusService::ServiceProviderInterface:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Adds a message to the circular log buffer, possibly removing the oldest
  // entry.
  void AddStatusMessage(std::string_view status);

  // Called from ExportedObject when a handler is exported as a D-Bus
  // method or failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  void OnTraceEnd(std::unique_ptr<arc::OverviewTracingResult> result);

  void StartTrace(dbus::MethodCall* method_call,
                  dbus::ExportedObject::ResponseSender response_sender);

  // Supplies a new tracing handler for a trace that is just about to begin.
  // Virtual so that tests can supply a testable subclass.
  virtual std::unique_ptr<arc::OverviewTracingHandler> NewHandler();

  // Responds with (Gets) the messages in the circular log buffer, oldest first.
  void GetStatus(dbus::MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender response_sender);

  // This is only present if a trace is running.
  std::unique_ptr<arc::OverviewTracingHandler> handler_;

  // The last few status messages.
  std::deque<std::string> msgs_;

  // Where finished traces are saved.
  base::FilePath trace_outdir_{"/tmp"};

  // Keep this last so that all weak pointers will be invalidated at the
  // beginning of destruction.
  base::WeakPtrFactory<ArcTracingServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_ARC_TRACING_SERVICE_PROVIDER_H_
