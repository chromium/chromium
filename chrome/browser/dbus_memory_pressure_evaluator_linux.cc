// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dbus_memory_pressure_evaluator_linux.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/common/chrome_features.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/utils/check_for_service_and_start.h"
#include "components/dbus/utils/connect_to_signal.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

const char DbusMemoryPressureEvaluatorLinux::kLmmService[] =
    "org.freedesktop.LowMemoryMonitor";
const char DbusMemoryPressureEvaluatorLinux::kLmmObject[] =
    "/org/freedesktop/LowMemoryMonitor";
const char DbusMemoryPressureEvaluatorLinux::kLmmInterface[] =
    "org.freedesktop.LowMemoryMonitor";

const char DbusMemoryPressureEvaluatorLinux::kXdgPortalService[] =
    "org.freedesktop.portal.Desktop";
const char DbusMemoryPressureEvaluatorLinux::kXdgPortalObject[] =
    "/org/freedesktop/portal/desktop";
const char
    DbusMemoryPressureEvaluatorLinux::kXdgPortalMemoryMonitorInterface[] =
        "org.freedesktop.portal.MemoryMonitor";

const char DbusMemoryPressureEvaluatorLinux::kLowMemoryWarningSignal[] =
    "LowMemoryWarning";

// LMM emits signals every 15 seconds on pressure, so if we've been quiet for 20
// seconds, the pressure is likely cleared up.
const base::TimeDelta DbusMemoryPressureEvaluatorLinux::kResetVotePeriod =
    base::Seconds(20);

DbusMemoryPressureEvaluatorLinux::DbusMemoryPressureEvaluatorLinux(
    std::unique_ptr<memory_pressure::MemoryPressureVoter> voter)
    : DbusMemoryPressureEvaluatorLinux(std::move(voter), nullptr, nullptr) {
  // Only start the service checks in the public constructor, so the tests can
  // have time to set up mocks first when using the private constructor.
  CheckIfLmmIsAvailable();
}

DbusMemoryPressureEvaluatorLinux::DbusMemoryPressureEvaluatorLinux(
    std::unique_ptr<memory_pressure::MemoryPressureVoter> voter,
    scoped_refptr<dbus::Bus> system_bus,
    scoped_refptr<dbus::Bus> session_bus)
    : memory_pressure::SystemMemoryPressureEvaluator(std::move(voter)),
      system_bus_(system_bus),
      session_bus_(session_bus) {
  moderate_level_ = features::kLinuxLowMemoryMonitorModerateLevel.Get();
  critical_level_ = features::kLinuxLowMemoryMonitorCriticalLevel.Get();

  CHECK(critical_level_ > moderate_level_);
}

DbusMemoryPressureEvaluatorLinux::~DbusMemoryPressureEvaluatorLinux() {
  if (system_bus_) {
    system_bus_->ShutdownOnDBusThreadAndBlock();
    system_bus_.reset();
  }

  if (session_bus_) {
    session_bus_->ShutdownOnDBusThreadAndBlock();
    session_bus_.reset();
  }
}

void DbusMemoryPressureEvaluatorLinux::CheckIfLmmIsAvailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!system_bus_) {
    system_bus_ = dbus_thread_linux::GetSharedSystemBus();
  }

  dbus_utils::CheckForServiceAndStart(
      system_bus_, kLmmService,
      base::BindOnce(
          &DbusMemoryPressureEvaluatorLinux::CheckIfLmmIsAvailableResponse,
          weak_ptr_factory_.GetWeakPtr()));
}

void DbusMemoryPressureEvaluatorLinux::CheckIfLmmIsAvailableResponse(
    std::optional<bool> is_available) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_available.value_or(false)) {
    VLOG(1) << "LMM is available, using " << kLmmInterface;

    object_proxy_ =
        system_bus_->GetObjectProxy(kLmmService, dbus::ObjectPath(kLmmObject));
    dbus_utils::ConnectToSignal<"y">(
        object_proxy_, kLmmInterface, kLowMemoryWarningSignal,
        base::BindRepeating(
            &DbusMemoryPressureEvaluatorLinux::OnLowMemoryWarning,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&DbusMemoryPressureEvaluatorLinux::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    VLOG(1) << "LMM is not available, checking for portal";

    system_bus_.reset();
    CheckIfPortalIsAvailable();
  }
}

void DbusMemoryPressureEvaluatorLinux::CheckIfPortalIsAvailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!session_bus_) {
    session_bus_ = dbus_thread_linux::GetSharedSessionBus();
  }

  dbus_utils::CheckForServiceAndStart(
      session_bus_, kXdgPortalService,
      base::BindOnce(
          &DbusMemoryPressureEvaluatorLinux::CheckIfPortalIsAvailableResponse,
          weak_ptr_factory_.GetWeakPtr()));
}

void DbusMemoryPressureEvaluatorLinux::CheckIfPortalIsAvailableResponse(
    std::optional<bool> is_available) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_available.value_or(false)) {
    VLOG(1) << "Portal is available, using "
            << kXdgPortalMemoryMonitorInterface;

    object_proxy_ = session_bus_->GetObjectProxy(
        kXdgPortalService, dbus::ObjectPath(kXdgPortalObject));
    dbus_utils::ConnectToSignal<"y">(
        object_proxy_, kXdgPortalMemoryMonitorInterface,
        kLowMemoryWarningSignal,
        base::BindRepeating(
            &DbusMemoryPressureEvaluatorLinux::OnLowMemoryWarning,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&DbusMemoryPressureEvaluatorLinux::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    VLOG(1) << "No memory monitor found";

    session_bus_.reset();
  }
}

void DbusMemoryPressureEvaluatorLinux::OnSignalConnected(
    const std::string& interface,
    const std::string& signal,
    bool connected) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!connected) {
    LOG(WARNING) << "Failed to connect to " << interface << '.' << signal;

    system_bus_.reset();
    session_bus_.reset();
  }
}

void DbusMemoryPressureEvaluatorLinux::OnLowMemoryWarning(
    dbus_utils::ConnectToSignalResultSig<"y"> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    LOG(WARNING) << "Failed to parse low memory level";
    return;
  }
  auto [lmm_level] = *result;

  // static_cast is needed as lmm_level is a uint8_t, which is often an alias to
  // char, meaning that sending it to the output stream would just print the
  // character representation rather than the numeric representation.
  VLOG(1) << "Monitor sent memory pressure level: "
          << static_cast<int>(lmm_level);

  base::MemoryPressureLevel new_level = LmmToBasePressureLevel(lmm_level);

  VLOG(1) << "MemoryPressureLevel: " << new_level;
  UpdateLevel(new_level);
}

base::MemoryPressureLevel
DbusMemoryPressureEvaluatorLinux::LmmToBasePressureLevel(uint8_t lmm_level) {
  if (lmm_level >= critical_level_) {
    return base::MEMORY_PRESSURE_LEVEL_CRITICAL;
  }
  if (lmm_level >= moderate_level_) {
    return base::MEMORY_PRESSURE_LEVEL_MODERATE;
  }
  return base::MEMORY_PRESSURE_LEVEL_NONE;
}

void DbusMemoryPressureEvaluatorLinux::UpdateLevel(
    base::MemoryPressureLevel new_level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  reset_vote_timer_.Stop();

  base::MemoryPressureLevel old_vote = current_vote();

  SetCurrentVote(new_level);
  switch (new_level) {
    case base::MEMORY_PRESSURE_LEVEL_NONE:
      // Only notify when transitioning to no pressure.
      SendCurrentVote(old_vote != base::MEMORY_PRESSURE_LEVEL_NONE);
      break;
    case base::MEMORY_PRESSURE_LEVEL_MODERATE:
    case base::MEMORY_PRESSURE_LEVEL_CRITICAL:
      SendCurrentVote(true);

      reset_vote_timer_.Start(
          FROM_HERE, kResetVotePeriod,
          base::BindOnce(&DbusMemoryPressureEvaluatorLinux::UpdateLevel,
                         weak_ptr_factory_.GetWeakPtr(),
                         base::MEMORY_PRESSURE_LEVEL_NONE));
      break;
  }
}
