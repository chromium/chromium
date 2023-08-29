// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/top_level_storage_access_api/top_level_storage_access_permission_context.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"

namespace {

constexpr base::TimeDelta kGrantDuration = base::Hours(24);

void RecordOutcomeSample(TopLevelStorageAccessRequestOutcome outcome) {
  base::UmaHistogramEnumeration("API.TopLevelStorageAccess.RequestOutcome",
                                outcome);
}

}  // namespace

TopLevelStorageAccessPermissionContext::TopLevelStorageAccessPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
          blink::mojom::PermissionsPolicyFeature::kStorageAccessAPI) {}

TopLevelStorageAccessPermissionContext::
    ~TopLevelStorageAccessPermissionContext() = default;

void TopLevelStorageAccessPermissionContext::DecidePermissionForTesting(
    permissions::PermissionRequestData request_data,
    permissions::BrowserPermissionCallback callback) {
  DecidePermission(std::move(request_data), std::move(callback));
}

void TopLevelStorageAccessPermissionContext::DecidePermission(
    permissions::PermissionRequestData request_data,
    permissions::BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      request_data.id.global_render_frame_host_id());
  CHECK(rfh);
  if (!request_data.user_gesture ||
      !base::FeatureList::IsEnabled(
          blink::features::kStorageAccessAPIForOriginExtension) ||
      !request_data.requesting_origin.is_valid() ||
      !request_data.embedding_origin.is_valid()) {
    if (!request_data.user_gesture) {
      rfh->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kError,
          "requestStorageAccessFor: Must be handling a user gesture to use.");
    }
    RecordOutcomeSample(
        TopLevelStorageAccessRequestOutcome::kDeniedByPrerequisites);
    std::move(callback).Run(CONTENT_SETTING_BLOCK);
    return;
  }

  if (!base::FeatureList::IsEnabled(features::kFirstPartySets)) {
    // First-Party Sets is disabled, so reject the request.
    RecordOutcomeSample(
        TopLevelStorageAccessRequestOutcome::kDeniedByPrerequisites);
    std::move(callback).Run(CONTENT_SETTING_BLOCK);
    return;
  }

  net::SchemefulSite embedding_site(request_data.embedding_origin);
  net::SchemefulSite requesting_site(request_data.requesting_origin);

  first_party_sets::FirstPartySetsPolicyServiceFactory::GetForBrowserContext(
      browser_context())
      ->ComputeFirstPartySetMetadata(
          requesting_site, &embedding_site,
          base::BindOnce(&TopLevelStorageAccessPermissionContext::
                             CheckForAutoGrantOrAutoDenial,
                         weak_factory_.GetWeakPtr(), std::move(request_data),
                         std::move(callback)));
}

void TopLevelStorageAccessPermissionContext::CheckForAutoGrantOrAutoDenial(
    permissions::PermissionRequestData request_data,
    permissions::BrowserPermissionCallback callback,
    net::FirstPartySetMetadata metadata) {
  if (metadata.AreSitesInSameFirstPartySet()) {
    // Service domains are not allowed to request storage access on behalf
    // of other domains, even in the same First-Party Set.
    if (metadata.top_frame_entry()->site_type() == net::SiteType::kService) {
      NotifyPermissionSetInternal(
          request_data.id, request_data.requesting_origin,
          request_data.embedding_origin, std::move(callback),
          /*persist=*/true, CONTENT_SETTING_BLOCK,
          TopLevelStorageAccessRequestOutcome::kDeniedByPrerequisites);
      return;
    }
    // Since the sites are in the same First-Party Set, risk of abuse due to
    // allowing access is considered to be low.
    NotifyPermissionSetInternal(
        request_data.id, request_data.requesting_origin,
        request_data.embedding_origin, std::move(callback),
        /*persist=*/true, CONTENT_SETTING_ALLOW,
        TopLevelStorageAccessRequestOutcome::kGrantedByFirstPartySet);
    return;
  }
  NotifyPermissionSetInternal(
      request_data.id, request_data.requesting_origin,
      request_data.embedding_origin, std::move(callback),
      /*persist=*/true, CONTENT_SETTING_BLOCK,
      TopLevelStorageAccessRequestOutcome::kDeniedByFirstPartySet);
}

ContentSetting
TopLevelStorageAccessPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  if (!base::FeatureList::IsEnabled(
          blink::features::kStorageAccessAPIForOriginExtension)) {
    return CONTENT_SETTING_BLOCK;
  }

  if (render_frame_host && !render_frame_host->IsInPrimaryMainFrame()) {
    // Note that portal and other main but non-outermost frames are
    // currently disallowed from queries by the PermissionService. This check
    // ensures that we do not assume that behavior, however.
    net::SchemefulSite top_level_site(
        render_frame_host->GetOutermostMainFrame()->GetLastCommittedURL());
    net::SchemefulSite current_site(render_frame_host->GetLastCommittedURL());
    if (top_level_site != current_site) {
      // Cross-site frames cannot receive real answers.
      return CONTENT_SETTING_ASK;
    }
  }

  ContentSetting setting = PermissionContextBase::GetPermissionStatusInternal(
      render_frame_host, requesting_origin, embedding_origin);

  // Although the current implementation does not persist rejected permissions,
  // the spec calls for avoiding exposure of rejections to prevent any attempt
  // at retaliating against users who would reject a prompt.
  if (setting == CONTENT_SETTING_BLOCK) {
    return CONTENT_SETTING_ASK;
  }
  return setting;
}

void TopLevelStorageAccessPermissionContext::NotifyPermissionSet(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    permissions::BrowserPermissionCallback callback,
    bool persist,
    ContentSetting content_setting,
    bool is_one_time,
    bool is_final_decision) {
  CHECK(!is_one_time);
  CHECK(is_final_decision);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  NotifyPermissionSetInternal(
      id, requesting_origin, embedding_origin, std::move(callback), persist,
      content_setting,
      content_setting == CONTENT_SETTING_ALLOW
          ? TopLevelStorageAccessRequestOutcome::kGrantedByFirstPartySet
          : TopLevelStorageAccessRequestOutcome::kDeniedByFirstPartySet);
}

void TopLevelStorageAccessPermissionContext::NotifyPermissionSetInternal(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    permissions::BrowserPermissionCallback callback,
    bool persist,
    ContentSetting content_setting,
    TopLevelStorageAccessRequestOutcome outcome) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(
          blink::features::kStorageAccessAPIForOriginExtension)) {
    return;
  }

  RecordOutcomeSample(outcome);

  const bool permission_allowed = (content_setting == CONTENT_SETTING_ALLOW);
  UpdateTabContext(id, requesting_origin, permission_allowed);

  if (!permission_allowed || !persist) {
    if (content_setting == CONTENT_SETTING_DEFAULT) {
      content_setting = CONTENT_SETTING_ASK;
    }

    std::move(callback).Run(content_setting);
    return;
  }

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context());
  CHECK(settings_map);
  CHECK(persist);

  // This permission was allowed, so store it. Because this is a superset of the
  // regular storage access permission, we also store that one.
  const net::SchemefulSite embedding_site(embedding_origin);
  const GURL embedding_site_as_url = embedding_site.GetURL();
  ContentSettingsPattern secondary_site_pattern =
      ContentSettingsPattern::CreateBuilder()
          ->WithScheme(embedding_site_as_url.scheme())
          ->WithDomainWildcard()
          ->WithHost(embedding_site_as_url.host())
          ->WithPathWildcard()
          ->WithPortWildcard()
          ->Build();

  content_settings::ContentSettingConstraints constraints;
  constraints.set_lifetime(kGrantDuration);
  constraints.set_session_model(
      content_settings::SessionModel::NonRestorableUserSession);

  settings_map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(requesting_origin),
      secondary_site_pattern, ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
      content_setting, constraints);

  settings_map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(requesting_origin),
      secondary_site_pattern, ContentSettingsType::STORAGE_ACCESS,
      content_setting, constraints);

  ContentSettingsForOneType top_level_grants =
      settings_map->GetSettingsForOneType(
          ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS);
  ContentSettingsForOneType storage_access_grants =
      settings_map->GetSettingsForOneType(ContentSettingsType::STORAGE_ACCESS);

  // TODO(https://crbug.com/989663): Ensure that this update of settings doesn't
  // cause a double update with
  // ProfileNetworkContextService::OnContentSettingChanged.

  // We only want to signal the renderer process once the default storage
  // partition has updated and ack'd the update. This prevents a race where
  // the renderer could initiate a network request based on the response to this
  // request before the access grants have updated in the network service.
  browser_context()
      ->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->SetAllStorageAccessSettings(
          storage_access_grants, top_level_grants,
          base::BindOnce(std::move(callback), content_setting));
}

void TopLevelStorageAccessPermissionContext::UpdateContentSetting(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    ContentSetting content_setting,
    bool is_one_time) {
  CHECK(!is_one_time);
  // We need to notify the network service of content setting updates before we
  // run our callback. As a result we do our updates when we're notified of a
  // permission being set and should not be called here.
  NOTREACHED();
}
