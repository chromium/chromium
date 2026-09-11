// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_SESSION_END_LISTENER_LINUX_H_
#define CHROME_BROWSER_LIFETIME_SESSION_END_LISTENER_LINUX_H_

#include <memory>
#include <string>

#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/dbus/utils/call_method.h"
#include "components/dbus/utils/connect_to_signal.h"
#include "dbus/bus.h"

// Listens for OS shutdown and desktop session end events on Linux via D-Bus.
//
// 1. On systemd-logind (system bus), requests a "delay" shutdown inhibitor and
//    listens for the PrepareForShutdown signal to trigger
//    chrome::SessionEnding() and flush state before the system powers off or
//    reboots.
// 2. On GNOME SessionManager (session bus), listens for the SessionOver signal
//    as a passive notification for desktop session end.
class SessionEndListenerLinux {
 public:
  // Creates and initializes the listener using shared system and session buses.
  // Returns nullptr if kLinuxLogindShutdownInhibitor is disabled or if no bus
  // is available.
  static std::unique_ptr<SessionEndListenerLinux> Create();

  // For testing with custom buses and a required callback instead of
  // terminating the process via chrome::SessionEnding().
  SessionEndListenerLinux(scoped_refptr<dbus::Bus> system_bus,
                          scoped_refptr<dbus::Bus> session_bus,
                          base::OnceClosure session_ending_cb);
  SessionEndListenerLinux(const SessionEndListenerLinux&) = delete;
  SessionEndListenerLinux& operator=(const SessionEndListenerLinux&) = delete;
  ~SessionEndListenerLinux();

  bool has_inhibitor_for_testing() const { return inhibitor_fd_.is_valid(); }
  void reset_inhibitor_for_testing() { inhibitor_fd_.reset(); }

 private:
  static const char kLogindServiceName[];
  static const char kLogindObjectPath[];
  static const char kLogindManagerInterface[];
  static const char kPrepareForShutdownSignal[];
  static const char kInhibitMethod[];

  static const char kGnomeSessionManagerServiceName[];
  static const char kGnomeSessionManagerObjectPath[];
  static const char kGnomeSessionManagerInterface[];
  static const char kSessionOverSignal[];

  void ConnectToLogind();
  void OnPrepareForShutdownConnected(const std::string& interface_name,
                                     const std::string& signal_name,
                                     bool success);
  void OnPrepareForShutdown(dbus_utils::ConnectToSignalResultSig<"b"> result);

  void RequestInhibitor();
  void OnInhibitResponse(dbus_utils::CallMethodResultSig<"h"> result);

  void ConnectToGnomeSessionManager();
  void OnSessionOverConnected(const std::string& interface_name,
                              const std::string& signal_name,
                              bool success);
  void OnSessionOver(dbus_utils::ConnectToSignalResultSig<""> result);

  void OnSessionEnding();

  const scoped_refptr<dbus::Bus> system_bus_;
  const scoped_refptr<dbus::Bus> session_bus_;
  const raw_ptr<dbus::ObjectProxy> logind_proxy_;
  const raw_ptr<dbus::ObjectProxy> gnome_session_proxy_;

  base::ScopedFD inhibitor_fd_;
  base::OnceClosure session_ending_cb_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<SessionEndListenerLinux> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_LIFETIME_SESSION_END_LISTENER_LINUX_H_
