// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/dbus/ash_dbus_services.h"

#include "ash/constants/ash_features.h"
#include "ash/dbus/display_service_provider.h"
#include "ash/dbus/gesture_properties_service_provider.h"
#include "ash/dbus/liveness_service_provider.h"
#include "ash/dbus/privacy_screen_service_provider.h"
#include "ash/dbus/url_handler_service_provider.h"
#include "ash/dbus/user_authentication_service_provider.h"
#include "base/feature_list.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

AshDBusServices::AshDBusServices(dbus::Bus* system_bus) {
  display_service_ =
      CrosDBusService::Create(system_bus, chromeos::kDisplayServiceName,
                              dbus::ObjectPath(chromeos::kDisplayServicePath),
                              CrosDBusService::CreateServiceProviderList(
                                  std::make_unique<DisplayServiceProvider>()));
  if (base::FeatureList::IsEnabled(features::kGesturePropertiesDBusService)) {
    gesture_properties_service_ = CrosDBusService::Create(
        system_bus, chromeos::kGesturePropertiesServiceName,
        dbus::ObjectPath(chromeos::kGesturePropertiesServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<GesturePropertiesServiceProvider>()));
  }
  liveness_service_ =
      CrosDBusService::Create(system_bus, chromeos::kLivenessServiceName,
                              dbus::ObjectPath(chromeos::kLivenessServicePath),
                              CrosDBusService::CreateServiceProviderList(
                                  std::make_unique<LivenessServiceProvider>()));
  privacy_screen_service_ = CrosDBusService::Create(
      system_bus, privacy_screen::kPrivacyScreenServiceName,
      dbus::ObjectPath(privacy_screen::kPrivacyScreenServicePath),
      CrosDBusService::CreateServiceProviderList(
          std::make_unique<PrivacyScreenServiceProvider>()));
  url_handler_service_ = CrosDBusService::Create(
      system_bus, chromeos::kUrlHandlerServiceName,
      dbus::ObjectPath(chromeos::kUrlHandlerServicePath),
      CrosDBusService::CreateServiceProviderList(
          std::make_unique<UrlHandlerServiceProvider>()));
  user_authentication_service_ = CrosDBusService::Create(
      system_bus, chromeos::kUserAuthenticationServiceName,
      dbus::ObjectPath(chromeos::kUserAuthenticationServicePath),
      CrosDBusService::CreateServiceProviderList(
          std::make_unique<UserAuthenticationServiceProvider>()));
}

AshDBusServices::~AshDBusServices() {
  display_service_.reset();
  gesture_properties_service_.reset();
  liveness_service_.reset();
  privacy_screen_service_.reset();
  url_handler_service_.reset();
  user_authentication_service_.reset();
}

}  // namespace ash
