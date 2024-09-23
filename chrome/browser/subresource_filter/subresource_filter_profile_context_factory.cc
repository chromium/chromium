// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/subresource_filter_history_observer.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"

// static
subresource_filter::SubresourceFilterProfileContext*
SubresourceFilterProfileContextFactory::GetForProfile(Profile* profile) {
  return static_cast<subresource_filter::SubresourceFilterProfileContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true /* create */));
}

// static
SubresourceFilterProfileContextFactory*
SubresourceFilterProfileContextFactory::GetInstance() {
  static base::NoDestructor<SubresourceFilterProfileContextFactory> instance;
  return instance.get();
}

SubresourceFilterProfileContextFactory::SubresourceFilterProfileContextFactory()
    : ProfileKeyedServiceFactory(
          "SubresourceFilterProfileContext",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
}

KeyedService* SubresourceFilterProfileContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  auto* subresource_filter_profile_context =
      new subresource_filter::SubresourceFilterProfileContext(
          HostContentSettingsMapFactory::GetForProfile(profile),
          CookieSettingsFactory::GetForProfile(profile));

  // Create and attach a SubresourceFilterHistoryObserver instance if possible.
  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  if (history_service) {
    subresource_filter_profile_context->SetEmbedderData(
        std::make_unique<SubresourceFilterHistoryObserver>(
            subresource_filter_profile_context->settings_manager(),
            history_service));
  }

  return subresource_filter_profile_context;
}
