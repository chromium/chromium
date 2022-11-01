// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_PROFILER_STATUS_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_PROFILER_STATUS_SERVICE_PROVIDER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace dbus {
class MethodCall;
}

namespace ash {

// A dbus interface that lets tast tests check that the stack profiler is
// working.
class ProfilerStatusServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  // Normally, these would go into
  // third_party/cros_system_api/dbus/service_constants.h to be shared with
  // ChromeOS. But since this interface is only called from Go code, we won't
  // use the C-style declarations in ChromeOS-land and it's easier to just
  // define the constants here.
  static constexpr char kServiceName[] = "org.chromium.ProfilerStatusService";
  static constexpr char kServicePath[] = "/org/chromium/ProfilerStatusService";
  static constexpr char kGetSuccessfullyCollectedCountsMethod[] =
      "GetSuccessfullyCollectedCounts";

  ProfilerStatusServiceProvider();
  ProfilerStatusServiceProvider(const ProfilerStatusServiceProvider&) = delete;
  ProfilerStatusServiceProvider& operator=(
      const ProfilerStatusServiceProvider&) = delete;
  ~ProfilerStatusServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Called from ExportedObject when a handler is exported as a D-Bus method or
  // failed to be exported.
  static void OnExported(const std::string& interface_name,
                         const std::string& method_name,
                         bool success);
  static void GetSuccessfullyCollectedCounts(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_PROFILER_STATUS_SERVICE_PROVIDER_H_
