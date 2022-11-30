// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/subresource_filter_history_observer.h"
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
  return base::Singleton<SubresourceFilterProfileContextFactory>::get();
}

SubresourceFilterProfileContextFactory::SubresourceFilterProfileContextFactory()
    : ProfileKeyedServiceFactory(
          "SubresourceFilterProfileContext",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
}

KeyedService* SubresourceFilterProfileContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  auto* subresource_filter_profile_context =
      new subresource_filter::SubresourceFilterProfileContext(
          HostContentSettingsMapFactory::GetForProfile(profile));

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
