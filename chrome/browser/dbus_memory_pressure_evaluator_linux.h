// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DBUS_MEMORY_PRESSURE_EVALUATOR_LINUX_H_
#define CHROME_BROWSER_DBUS_MEMORY_PRESSURE_EVALUATOR_LINUX_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/memory_pressure/system_memory_pressure_evaluator.h"
#include "dbus/bus.h"

namespace dbus {
class Response;
class Signal;
}  // namespace dbus

namespace memory_pressure {
class MemoryPressureVoter;
}  // namespace memory_pressure

// A memory pressure evaluator that uses the low-memory-monitor service
// (abbreviated in the code as "LMM") to monitor the memory pressure. If the
// service is not available, it can use the XDG memory monitor portal as a
// fallback (which itself is a thin wrapper over LMM).
//
// The LMM API is described here:
// https://hadess.pages.freedesktop.org/low-memory-monitor/
// and the portal API wrapper is here:
// https://flatpak.github.io/xdg-desktop-portal/portal-docs.html#gdbus-org.freedesktop.portal.MemoryMonitor
class DbusMemoryPressureEvaluatorLinux
    : public memory_pressure::SystemMemoryPressureEvaluator {
 public:
  explicit DbusMemoryPressureEvaluatorLinux(
      std::unique_ptr<memory_pressure::MemoryPressureVoter> voter);
  ~DbusMemoryPressureEvaluatorLinux() override;

  DbusMemoryPressureEvaluatorLinux(const DbusMemoryPressureEvaluatorLinux&) =
      delete;
  DbusMemoryPressureEvaluatorLinux& operator=(
      const DbusMemoryPressureEvaluatorLinux&) = delete;

 private:
  friend class DbusMemoryPressureEvaluatorLinuxTest;
  friend class DbusMemoryPressureEvaluatorLinuxSignalConnectionTest;

  // Constants for D-Bus services, object paths, methods, and signals. In-class
  // so they can be shared with the tests.
  static const char kMethodNameHasOwner[];
  static const char kMethodListActivatableNames[];

  static const char kLmmService[];
  static const char kLmmObject[];
  static const char kLmmInterface[];

  static const char kXdgPortalService[];
  static const char kXdgPortalObject[];
  static const char kXdgPortalMemoryMonitorInterface[];

  static const char kLowMemoryWarningSignal[];

  static const base::TimeDelta kResetVotePeriod;

  // The public constructor just delegates to this private one, but it's
  // separated so that the test cases can pass in the mock bus instances.
  DbusMemoryPressureEvaluatorLinux(
      std::unique_ptr<memory_pressure::MemoryPressureVoter> voter,
      scoped_refptr<dbus::Bus> system_bus,
      scoped_refptr<dbus::Bus> session_bus);

  // Checks if LMM itself is available, setting up the memory pressure signal
  // handler if so. Otherwise, checks if the portal is available instead.
  void CheckIfLmmIsAvailable();
  // Handles the availability response from above.
  void CheckIfLmmIsAvailableResponse(bool is_available);

  // Checks if the portal service is available, setting up the memory pressure
  // signal handler if so.
  void CheckIfPortalIsAvailable();
  // Handles the availability response from above.
  void CheckIfPortalIsAvailableResponse(bool is_available);

  // Checks if the given service is available, calling callback(true) if so or
  // callback(false) otherwise.
  void CheckIfServiceIsAvailable(scoped_refptr<dbus::Bus> bus,
                                 const std::string& service,
                                 base::OnceCallback<void(bool)> callback);

  void OnNameHasOwnerResponse(scoped_refptr<dbus::Bus> bus,
                              const std::string& service,
                              base::OnceCallback<void(bool)> callback,
                              dbus::Response* response);
  void OnListActivatableNamesResponse(const std::string& service,
                                      base::OnceCallback<void(bool)> callback,
                                      dbus::Response* response);

  // Shuts down the given bus on the D-Bus thread and clears the pointer.
  void ResetBus(scoped_refptr<dbus::Bus>& bus);

  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool connected);

  void OnLowMemoryWarning(dbus::Signal* signal);

  // Converts a pressure level from LMM to base's memory pressure constants.
  base::MemoryPressureListener::MemoryPressureLevel LmmToBasePressureLevel(
      uint8_t lmm_level);

  void UpdateLevel(base::MemoryPressureListener::MemoryPressureLevel new_level);

  scoped_refptr<dbus::Bus> system_bus_;
  scoped_refptr<dbus::Bus> session_bus_;
  raw_ptr<dbus::ObjectProxy> object_proxy_ = nullptr;

  // The values used to determine how to translate LMM memory pressure levels to
  // Chrome's are stored here, gathered from feature params.
  uint8_t moderate_level_;
  uint8_t critical_level_;

  // LMM never emits signals once the memory pressure has ended, so we need to
  // estimate when that is the case by checking when the monitor has gone silent
  // for a while.
  base::OneShotTimer reset_vote_timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DbusMemoryPressureEvaluatorLinux> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_DBUS_MEMORY_PRESSURE_EVALUATOR_LINUX_H_
