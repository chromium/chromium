// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/session_end_listener_linux.h"

#include <tuple>
#include <utility>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/common/chrome_features.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/version_info/version_info.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

const char SessionEndListenerLinux::kLogindServiceName[] =
    "org.freedesktop.login1";
const char SessionEndListenerLinux::kLogindObjectPath[] =
    "/org/freedesktop/login1";
const char SessionEndListenerLinux::kLogindManagerInterface[] =
    "org.freedesktop.login1.Manager";
const char SessionEndListenerLinux::kPrepareForShutdownSignal[] =
    "PrepareForShutdown";
const char SessionEndListenerLinux::kInhibitMethod[] = "Inhibit";

const char SessionEndListenerLinux::kGnomeSessionManagerServiceName[] =
    "org.gnome.SessionManager";
const char SessionEndListenerLinux::kGnomeSessionManagerObjectPath[] =
    "/org/gnome/SessionManager";
const char SessionEndListenerLinux::kGnomeSessionManagerInterface[] =
    "org.gnome.SessionManager";
const char SessionEndListenerLinux::kSessionOverSignal[] = "SessionOver";

// static
std::unique_ptr<SessionEndListenerLinux> SessionEndListenerLinux::Create() {
  if (!base::FeatureList::IsEnabled(features::kLinuxLogindShutdownInhibitor)) {
    return nullptr;
  }
  auto system_bus = dbus_thread_linux::GetSharedSystemBus();
  auto session_bus = dbus_thread_linux::GetSharedSessionBus();
  if (!system_bus && !session_bus) {
    return nullptr;
  }
  return std::make_unique<SessionEndListenerLinux>(
      std::move(system_bus), std::move(session_bus),
      base::BindOnce(&chrome::SessionEnding));
}

SessionEndListenerLinux::SessionEndListenerLinux(
    scoped_refptr<dbus::Bus> system_bus,
    scoped_refptr<dbus::Bus> session_bus,
    base::OnceClosure session_ending_cb)
    : system_bus_(std::move(system_bus)),
      session_bus_(std::move(session_bus)),
      logind_proxy_(system_bus_ ? system_bus_->GetObjectProxy(
                                      kLogindServiceName,
                                      dbus::ObjectPath(kLogindObjectPath))
                                : nullptr),
      gnome_session_proxy_(
          session_bus_ ? session_bus_->GetObjectProxy(
                             kGnomeSessionManagerServiceName,
                             dbus::ObjectPath(kGnomeSessionManagerObjectPath))
                       : nullptr),
      session_ending_cb_(std::move(session_ending_cb)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ConnectToLogind();
  ConnectToGnomeSessionManager();
}

SessionEndListenerLinux::~SessionEndListenerLinux() = default;

void SessionEndListenerLinux::ConnectToLogind() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!logind_proxy_) {
    return;
  }

  dbus_utils::ConnectToSignal<"b">(
      logind_proxy_, kLogindManagerInterface, kPrepareForShutdownSignal,
      base::BindRepeating(&SessionEndListenerLinux::OnPrepareForShutdown,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&SessionEndListenerLinux::OnPrepareForShutdownConnected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SessionEndListenerLinux::OnPrepareForShutdownConnected(
    const std::string& interface_name,
    const std::string& signal_name,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    VLOG(1) << "Failed to connect to " << interface_name << "." << signal_name;
    return;
  }
  RequestInhibitor();
}

void SessionEndListenerLinux::OnPrepareForShutdown(
    dbus_utils::ConnectToSignalResultSig<"b"> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.has_value()) {
    return;
  }
  bool is_preparing = std::get<0>(result.value());
  if (is_preparing) {
    OnSessionEnding();
  } else if (!inhibitor_fd_.is_valid()) {
    CHECK_IS_TEST();
    // Re-acquire the inhibitor if an uninhibited cancellation event is
    // received in tests where session_ending_cb_ did not terminate the
    // process.
    RequestInhibitor();
  }
}

void SessionEndListenerLinux::RequestInhibitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!logind_proxy_) {
    return;
  }

  dbus_utils::CallMethod<"ssss", "h">(
      logind_proxy_, kLogindManagerInterface, kInhibitMethod,
      base::BindOnce(&SessionEndListenerLinux::OnInhibitResponse,
                     weak_ptr_factory_.GetWeakPtr()),
      "shutdown", std::string(version_info::GetProductName()),
      "Saving browser state", "delay");
}

void SessionEndListenerLinux::OnInhibitResponse(
    dbus_utils::CallMethodResultSig<"h"> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.has_value()) {
    VLOG(1) << "Failed to acquire logind shutdown delay inhibitor";
    return;
  }
  inhibitor_fd_ = std::move(std::get<0>(result.value()));
}

void SessionEndListenerLinux::ConnectToGnomeSessionManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!gnome_session_proxy_) {
    return;
  }

  dbus_utils::ConnectToSignal<"">(
      gnome_session_proxy_, kGnomeSessionManagerInterface, kSessionOverSignal,
      base::BindRepeating(&SessionEndListenerLinux::OnSessionOver,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&SessionEndListenerLinux::OnSessionOverConnected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SessionEndListenerLinux::OnSessionOverConnected(
    const std::string& interface_name,
    const std::string& signal_name,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    VLOG(1) << "Failed to connect to " << interface_name << "." << signal_name;
  }
}

void SessionEndListenerLinux::OnSessionOver(
    dbus_utils::ConnectToSignalResultSig<""> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.has_value()) {
    return;
  }
  OnSessionEnding();
}

void SessionEndListenerLinux::OnSessionEnding() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (session_ending_cb_) {
    std::move(session_ending_cb_).Run();
  }
}
