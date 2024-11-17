// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_external_protocol_handler.h"

#include <string_view>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/display/display.h"

namespace guest_os {

GuestOsUrlHandler::GuestOsUrlHandler(std::string_view name,
                                     const HandlerCallback handler)
    : name_(name), handler_(handler) {}

GuestOsUrlHandler::GuestOsUrlHandler(const GuestOsUrlHandler& other) = default;

GuestOsUrlHandler::~GuestOsUrlHandler() = default;

void GuestOsUrlHandler::Handle(Profile* profile, const GURL& url) {
  handler_.Run(profile, url);
}

std::optional<GuestOsUrlHandler> GuestOsUrlHandler::GetForUrl(Profile* profile,
                                                              const GURL& url) {
  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);
  if (!registry_service) {
    // GuestOsRegistryService does not exist for incognito or guest profiles, so
    // don't try and use it.
    return std::nullopt;
  }

  return registry_service->GetHandler(url);
}

}  // namespace guest_os
