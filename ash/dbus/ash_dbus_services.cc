// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/dbus/ash_dbus_services.h"

#include "ash/dbus/display_service_provider.h"
#include "ash/dbus/gesture_properties_service_provider.h"
#include "ash/dbus/liveness_service_provider.h"
#include "ash/dbus/url_handler_service_provider.h"
#include "base/feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

AshDBusServices::AshDBusServices(dbus::Bus* system_bus) {
  display_service_ = chromeos::CrosDBusService::Create(
      system_bus, chromeos::kDisplayServiceName,
      dbus::ObjectPath(chromeos::kDisplayServicePath),
      chromeos::CrosDBusService::CreateServiceProviderList(
          std::make_unique<DisplayServiceProvider>()));
  if (base::FeatureList::IsEnabled(
          chromeos::features::kGesturePropertiesDBusService)) {
    gesture_properties_service_ = chromeos::CrosDBusService::Create(
        system_bus, chromeos::kGesturePropertiesServiceName,
        dbus::ObjectPath(chromeos::kGesturePropertiesServicePath),
        chromeos::CrosDBusService::CreateServiceProviderList(
            std::make_unique<GesturePropertiesServiceProvider>()));
  }
  liveness_service_ = chromeos::CrosDBusService::Create(
      system_bus, chromeos::kLivenessServiceName,
      dbus::ObjectPath(chromeos::kLivenessServicePath),
      chromeos::CrosDBusService::CreateServiceProviderList(
          std::make_unique<LivenessServiceProvider>()));
  url_handler_service_ = chromeos::CrosDBusService::Create(
      system_bus, chromeos::kUrlHandlerServiceName,
      dbus::ObjectPath(chromeos::kUrlHandlerServicePath),
      chromeos::CrosDBusService::CreateServiceProviderList(
          std::make_unique<UrlHandlerServiceProvider>()));
}

AshDBusServices::~AshDBusServices() {
  display_service_.reset();
  gesture_properties_service_.reset();
  liveness_service_.reset();
  url_handler_service_.reset();
}

}  // namespace ash
