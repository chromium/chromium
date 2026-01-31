// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/search_permissions/search_permissions_service.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_uma_util.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

// Default implementation of SearchEngineDelegate that is used for production
// code.
class SearchEngineDelegateImpl
    : public SearchPermissionsService::SearchEngineDelegate {
 public:
  explicit SearchEngineDelegateImpl(Profile* profile)
      : profile_(profile),
        template_url_service_(
            TemplateURLServiceFactory::GetForProfile(profile_)) {}

  url::Origin GetDSEOrigin() override {
    if (template_url_service_) {
      return template_url_service_->GetDefaultSearchProviderOrigin();
    }

    return url::Origin();
  }

 private:
  raw_ptr<Profile> profile_;

  // Will be null in unittests.
  raw_ptr<TemplateURLService> template_url_service_;
};

}  // namespace

// static
SearchPermissionsService*
SearchPermissionsService::Factory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<SearchPermissionsService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
SearchPermissionsService::Factory*
SearchPermissionsService::Factory::GetInstance() {
  return base::Singleton<SearchPermissionsService::Factory>::get();
}

SearchPermissionsService::Factory::Factory()
    : ProfileKeyedServiceFactory(
          "SearchPermissionsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

SearchPermissionsService::Factory::~Factory() = default;

bool SearchPermissionsService::Factory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

std::unique_ptr<KeyedService>
SearchPermissionsService::Factory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<SearchPermissionsService>(
      Profile::FromBrowserContext(context));
}

SearchPermissionsService::SearchPermissionsService(Profile* profile) {
  // This class should never be constructed in incognito.
  DCHECK(!profile->IsOffTheRecord());

  delegate_ = std::make_unique<SearchEngineDelegateImpl>(profile);

  RecordEffectiveDSEOriginPermissions(profile);
}

bool SearchPermissionsService::IsDseOrigin(const url::Origin& origin) {
  return origin.scheme() == url::kHttpsScheme &&
         origin.IsSameOriginWith(delegate_->GetDSEOrigin());
}

void SearchPermissionsService::Shutdown() {
  delegate_.reset();
}

SearchPermissionsService::~SearchPermissionsService() = default;

void SearchPermissionsService::SetSearchEngineDelegateForTest(
    std::unique_ptr<SearchEngineDelegate> delegate) {
  delegate_ = std::move(delegate);
}

void SearchPermissionsService::RecordEffectiveDSEOriginPermissions(
    Profile* profile) {
  GURL dse_origin = delegate_->GetDSEOrigin().GetURL();
  if (!dse_origin.is_valid())
    return;

  HostContentSettingsMap* hcsm =
      HostContentSettingsMapFactory::GetForProfile(profile);

  permissions::PermissionUmaUtil::RecordDSEEffectiveSetting(
      ContentSettingsType::NOTIFICATIONS,
      hcsm->GetUserModifiableContentSetting(
          dse_origin, dse_origin, ContentSettingsType::NOTIFICATIONS));

  permissions::PermissionUmaUtil::RecordDSEEffectiveSetting(
      ContentSettingsType::GEOLOCATION,
      hcsm->GetUserModifiableContentSetting(dse_origin, dse_origin,
                                            ContentSettingsType::GEOLOCATION));
}
