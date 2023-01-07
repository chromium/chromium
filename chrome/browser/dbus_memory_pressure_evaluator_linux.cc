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
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace {

scoped_refptr<dbus::Bus> CreateBusOfType(dbus::Bus::BusType type) {
  dbus::Bus::Options options;
  options.bus_type = type;
  options.connection_type = dbus::Bus::PRIVATE;
  options.dbus_task_runner = dbus_thread_linux::GetTaskRunner();
  return base::MakeRefCounted<dbus::Bus>(options);
}

}  // namespace

const char DbusMemoryPressureEvaluatorLinux::kMethodNameHasOwner[] =
    "NameHasOwner";
const char DbusMemoryPressureEvaluatorLinux::kMethodListActivatableNames[] =
    "ListActivatableNames";

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

  if (!system_bus_)
    system_bus_ = CreateBusOfType(dbus::Bus::SYSTEM);

  CheckIfServiceIsAvailable(
      system_bus_, kLmmService,
      base::BindOnce(
          &DbusMemoryPressureEvaluatorLinux::CheckIfLmmIsAvailableResponse,
          weak_ptr_factory_.GetWeakPtr()));
}

void DbusMemoryPressureEvaluatorLinux::CheckIfLmmIsAvailableResponse(
    bool is_available) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_available) {
    VLOG(1) << "LMM is available, using " << kLmmInterface;

    object_proxy_ =
        system_bus_->GetObjectProxy(kLmmService, dbus::ObjectPath(kLmmObject));
    object_proxy_->ConnectToSignal(
        kLmmInterface, kLowMemoryWarningSignal,
        base::BindRepeating(
            &DbusMemoryPressureEvaluatorLinux::OnLowMemoryWarning,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&DbusMemoryPressureEvaluatorLinux::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    VLOG(1) << "LMM is not available, checking for portal";

    ResetBus(system_bus_);
    CheckIfPortalIsAvailable();
  }
}

void DbusMemoryPressureEvaluatorLinux::CheckIfPortalIsAvailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!session_bus_)
    session_bus_ = CreateBusOfType(dbus::Bus::SESSION);

  CheckIfServiceIsAvailable(
      session_bus_, kXdgPortalService,
      base::BindOnce(
          &DbusMemoryPressureEvaluatorLinux::CheckIfPortalIsAvailableResponse,
          weak_ptr_factory_.GetWeakPtr()));
}

void DbusMemoryPressureEvaluatorLinux::CheckIfPortalIsAvailableResponse(
    bool is_available) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_available) {
    VLOG(1) << "Portal is available, using "
            << kXdgPortalMemoryMonitorInterface;

    object_proxy_ = session_bus_->GetObjectProxy(
        kXdgPortalService, dbus::ObjectPath(kXdgPortalObject));
    object_proxy_->ConnectToSignal(
        kXdgPortalMemoryMonitorInterface, kLowMemoryWarningSignal,
        base::BindRepeating(
            &DbusMemoryPressureEvaluatorLinux::OnLowMemoryWarning,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&DbusMemoryPressureEvaluatorLinux::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    VLOG(1) << "No memory monitor found";

    ResetBus(session_bus_);
  }
}

void DbusMemoryPressureEvaluatorLinux::CheckIfServiceIsAvailable(
    scoped_refptr<dbus::Bus> bus,
    const std::string& service,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dbus::ObjectProxy* dbus_proxy =
      bus->GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));

  dbus::MethodCall method_call(DBUS_INTERFACE_DBUS, kMethodNameHasOwner);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(service);

  dbus_proxy->CallMethod(
      &method_call, DBUS_TIMEOUT_USE_DEFAULT,
      base::BindOnce(&DbusMemoryPressureEvaluatorLinux::OnNameHasOwnerResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(bus), service,
                     std::move(callback)));
}

void DbusMemoryPressureEvaluatorLinux::OnNameHasOwnerResponse(
    scoped_refptr<dbus::Bus> bus,
    const std::string& service,
    base::OnceCallback<void(bool)> callback,
    dbus::Response* response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool is_running = false;

  if (response) {
    dbus::MessageReader reader(response);
    bool owned = false;

    if (!reader.PopBool(&owned)) {
      LOG(ERROR) << "Failed to read " << kMethodNameHasOwner << " response";
    } else if (owned) {
      is_running = true;
    }
  } else {
    LOG(ERROR) << "Failed to call " << kMethodNameHasOwner;
  }

  if (is_running) {
    std::move(callback).Run(true);
  } else {
    dbus::ObjectProxy* dbus_proxy = bus->GetObjectProxy(
        DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));

    dbus::MethodCall method_call(DBUS_INTERFACE_DBUS,
                                 kMethodListActivatableNames);
    dbus_proxy->CallMethod(
        &method_call, DBUS_TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &DbusMemoryPressureEvaluatorLinux::OnListActivatableNamesResponse,
            weak_ptr_factory_.GetWeakPtr(), std::move(service),
            std::move(callback)));
  }
}

void DbusMemoryPressureEvaluatorLinux::OnListActivatableNamesResponse(
    const std::string& service,
    base::OnceCallback<void(bool)> callback,
    dbus::Response* response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool is_activatable = false;

  if (response) {
    dbus::MessageReader reader(response);
    std::vector<std::string> names;
    if (!reader.PopArrayOfStrings(&names)) {
      LOG(ERROR) << "Failed to read " << kMethodListActivatableNames
                 << " response";
    } else if (base::Contains(names, service)) {
      is_activatable = true;
    }
  } else {
    LOG(ERROR) << "Failed to call " << kMethodListActivatableNames;
  }

  std::move(callback).Run(is_activatable);
}

void DbusMemoryPressureEvaluatorLinux::ResetBus(scoped_refptr<dbus::Bus>& bus) {
  if (!bus)
    return;

  bus->GetDBusTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&dbus::Bus::ShutdownAndBlock, bus));
  bus.reset();
}

void DbusMemoryPressureEvaluatorLinux::OnSignalConnected(
    const std::string& interface,
    const std::string& signal,
    bool connected) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!connected) {
    LOG(WARNING) << "Failed to connect to " << interface << '.' << signal;

    ResetBus(system_bus_);
    ResetBus(session_bus_);
  }
}

void DbusMemoryPressureEvaluatorLinux::OnLowMemoryWarning(
    dbus::Signal* signal) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dbus::MessageReader reader(signal);
  uint8_t lmm_level;
  if (!reader.PopByte(&lmm_level)) {
    LOG(WARNING) << "Failed to parse low memory level";
    return;
  }

  // static_cast is needed as lmm_level is a uint8_t, which is often an alias to
  // char, meaning that sending it to the output stream would just print the
  // character representation rather than the numeric representation.
  VLOG(1) << "Monitor sent memory pressure level: "
          << static_cast<int>(lmm_level);

  base::MemoryPressureListener::MemoryPressureLevel new_level =
      LmmToBasePressureLevel(lmm_level);

  VLOG(1) << "MemoryPressureLevel: " << new_level;
  UpdateLevel(new_level);
}

base::MemoryPressureListener::MemoryPressureLevel
DbusMemoryPressureEvaluatorLinux::LmmToBasePressureLevel(uint8_t lmm_level) {
  if (lmm_level >= critical_level_)
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
  if (lmm_level >= moderate_level_)
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
  return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
}

void DbusMemoryPressureEvaluatorLinux::UpdateLevel(
    base::MemoryPressureListener::MemoryPressureLevel new_level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  reset_vote_timer_.Stop();

  SetCurrentVote(new_level);
  switch (new_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      // By convention no notifications are sent when returning to NONE level.
      SendCurrentVote(false);
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      SendCurrentVote(true);

      reset_vote_timer_.Start(
          FROM_HERE, kResetVotePeriod,
          base::BindOnce(
              &DbusMemoryPressureEvaluatorLinux::UpdateLevel,
              weak_ptr_factory_.GetWeakPtr(),
              base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE));
      break;
  }
}
