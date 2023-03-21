// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_grant_permission_context.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "net/base/features.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/same_party_context.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

using content_settings::URLToSchemefulSitePattern;

namespace {

constexpr base::TimeDelta kImplicitGrantDuration = base::Hours(24);
constexpr base::TimeDelta kExplicitGrantDuration = base::Days(30);

// Returns true iff the request was answered implicitly (assuming it met some
// other baseline prerequisites).
bool IsImplicitOutcome(RequestOutcome outcome) {
  return outcome == RequestOutcome::kGrantedByAllowance ||
         outcome == RequestOutcome::kGrantedByFirstPartySet ||
         outcome == RequestOutcome::kDeniedByFirstPartySet;
}

// Converts a ContentSetting to the corresponding RequestOutcome. This assumes
// that the request was not answered implicitly; i.e., that a prompt was shown
// to the user (at some point - not necessarily for this request).
RequestOutcome RequestOutcomeFromPrompt(ContentSetting content_setting,
                                        bool persist) {
  switch (content_setting) {
    case CONTENT_SETTING_DEFAULT:
      return RequestOutcome::kDismissedByUser;
    case CONTENT_SETTING_ALLOW:
      return persist ? RequestOutcome::kGrantedByUser
                     : RequestOutcome::kReusedPreviousDecision;
    case CONTENT_SETTING_BLOCK:
      return persist ? RequestOutcome::kDeniedByUser
                     : RequestOutcome::kReusedPreviousDecision;
    default:
      NOTREACHED();
      return RequestOutcome::kDeniedByUser;
  }
}

void RecordOutcomeSample(RequestOutcome outcome) {
  base::UmaHistogramEnumeration("API.StorageAccess.RequestOutcome", outcome);
}

content_settings::ContentSettingConstraints ComputeConstraints(
    RequestOutcome outcome,
    bool implicit_result) {
  if (!implicit_result) {
    return {content_settings::GetConstraintExpiration(kExplicitGrantDuration),
            content_settings::SessionModel::Durable};
  }
  if (outcome == RequestOutcome::kGrantedByFirstPartySet) {
    return {content_settings::GetConstraintExpiration(kImplicitGrantDuration),
            content_settings::SessionModel::NonRestorableUserSession};
  }
  return {content_settings::GetConstraintExpiration(kImplicitGrantDuration),
          content_settings::SessionModel::UserSession};
}

}  // namespace

StorageAccessGrantPermissionContext::StorageAccessGrantPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::STORAGE_ACCESS,
          blink::mojom::PermissionsPolicyFeature::kStorageAccessAPI) {}

StorageAccessGrantPermissionContext::~StorageAccessGrantPermissionContext() =
    default;

void StorageAccessGrantPermissionContext::DecidePermissionForTesting(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    permissions::BrowserPermissionCallback callback) {
  DecidePermission(id, requesting_origin, embedding_origin, user_gesture,
                   std::move(callback));
}

void StorageAccessGrantPermissionContext::DecidePermission(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    permissions::BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!user_gesture ||
      !base::FeatureList::IsEnabled(blink::features::kStorageAccessAPI) ||
      !requesting_origin.is_valid() || !embedding_origin.is_valid()) {
    RecordOutcomeSample(RequestOutcome::kDeniedByPrerequisites);
    std::move(callback).Run(CONTENT_SETTING_BLOCK);
    return;
  }

  if (!base::FeatureList::IsEnabled(features::kFirstPartySets) ||
      (!blink::features::kStorageAccessAPIAutoGrantInFPS.Get() &&
       !blink::features::kStorageAccessAPIAutoDenyOutsideFPS.Get())) {
    // First-Party Sets is disabled, or Auto-grants and auto-denials are both
    // disabled, so don't bother getting First-Party Sets data.
    UseImplicitGrantOrPrompt(id, requesting_origin, embedding_origin,
                             user_gesture, std::move(callback));
    return;
  }

  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(id.global_render_frame_host_id());
  DCHECK(rfh);

  net::SchemefulSite embedding_site(embedding_origin);

  first_party_sets::FirstPartySetsPolicyServiceFactory::GetForBrowserContext(
      browser_context())
      ->ComputeFirstPartySetMetadata(
          net::SchemefulSite(requesting_origin), &embedding_site,
          /*party_context=*/{},
          base::BindOnce(&StorageAccessGrantPermissionContext::
                             CheckForAutoGrantOrAutoDenial,
                         weak_factory_.GetWeakPtr(), id, requesting_origin,
                         embedding_origin, user_gesture, std::move(callback)));
}

void StorageAccessGrantPermissionContext::CheckForAutoGrantOrAutoDenial(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    permissions::BrowserPermissionCallback callback,
    net::FirstPartySetMetadata metadata) {
  // We should only run this method if something might need the FPS metadata.
  DCHECK(blink::features::kStorageAccessAPIAutoGrantInFPS.Get() ||
         blink::features::kStorageAccessAPIAutoDenyOutsideFPS.Get());

  if (metadata.AreSitesInSameFirstPartySet()) {
    if (blink::features::kStorageAccessAPIAutoGrantInFPS.Get()) {
      // Service domains are not allowed to request storage access on behalf
      // of other domains, even in the same First-Party Set.
      if (metadata.top_frame_entry()->site_type() == net::SiteType::kService) {
        NotifyPermissionSetInternal(id, requesting_origin, embedding_origin,
                                    std::move(callback),
                                    /*persist=*/true, CONTENT_SETTING_BLOCK,
                                    RequestOutcome::kDeniedByPrerequisites);
        return;
      }
      // Since the sites are in the same First-Party Set, risk of abuse due to
      // allowing access is considered to be low.
      NotifyPermissionSetInternal(id, requesting_origin, embedding_origin,
                                  std::move(callback),
                                  /*persist=*/true, CONTENT_SETTING_ALLOW,
                                  RequestOutcome::kGrantedByFirstPartySet);
      return;
    }
    // Not autogranting; fall back to implicit grants or prompt.
  } else {
    if (blink::features::kStorageAccessAPIAutoDenyOutsideFPS.Get()) {
      NotifyPermissionSetInternal(id, requesting_origin, embedding_origin,
                                  std::move(callback),
                                  /*persist=*/true, CONTENT_SETTING_BLOCK,
                                  RequestOutcome::kDeniedByFirstPartySet);
      return;
    }
    // Not autodenying; fall back to implicit grants or prompt.
  }

  return UseImplicitGrantOrPrompt(id, requesting_origin, embedding_origin,
                                  user_gesture, std::move(callback));
}

void StorageAccessGrantPermissionContext::UseImplicitGrantOrPrompt(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    permissions::BrowserPermissionCallback callback) {
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context());
  DCHECK(settings_map);

  // Normally a previous prompt rejection would already be filtered, but the
  // requirement not to surface the user's denial back to the caller means this
  // path can be reached on subsequent requests. Accordingly, check the default
  // implementation, and if a denial has been persisted, respect that decision.
  content::RenderFrameHost* const rfh =
      content::RenderFrameHost::FromID(id.global_render_frame_host_id());
  ContentSetting existing_setting =
      PermissionContextBase::GetPermissionStatusInternal(rfh, requesting_origin,
                                                         embedding_origin);
  if (existing_setting == CONTENT_SETTING_BLOCK) {
    NotifyPermissionSetInternal(id, requesting_origin, embedding_origin,
                                std::move(callback),
                                /*persist=*/true, CONTENT_SETTING_BLOCK,
                                RequestOutcome::kReusedPreviousDecision);
    return;
  }

  // Get all of our implicit grants and see which ones apply to our
  // |requesting_origin|.
  ContentSettingsForOneType implicit_grants;
  settings_map->GetSettingsForOneType(
      ContentSettingsType::STORAGE_ACCESS, &implicit_grants,
      content_settings::SessionModel::UserSession);

  const int existing_implicit_grants = base::ranges::count_if(
      implicit_grants, [requesting_origin](const auto& entry) {
        return entry.primary_pattern.Matches(requesting_origin);
      });

  // If we have fewer grants than our limit, we can just set an implicit grant
  // now and skip prompting the user.
  if (existing_implicit_grants <
      blink::features::kStorageAccessAPIImplicitGrantLimit.Get()) {
    NotifyPermissionSetInternal(id, requesting_origin, embedding_origin,
                                std::move(callback),
                                /*persist=*/true, CONTENT_SETTING_ALLOW,
                                RequestOutcome::kGrantedByAllowance);
    return;
  }

  // Show prompt.
  PermissionContextBase::DecidePermission(id, requesting_origin,
                                          embedding_origin, user_gesture,
                                          std::move(callback));
}

ContentSetting StorageAccessGrantPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  if (!base::FeatureList::IsEnabled(blink::features::kStorageAccessAPI)) {
    return CONTENT_SETTING_BLOCK;
  }

  ContentSetting setting = PermissionContextBase::GetPermissionStatusInternal(
      render_frame_host, requesting_origin, embedding_origin);

  // The spec calls for avoiding exposure of rejections to prevent any attempt
  // at retaliating against users who would reject a prompt.
  if (setting == CONTENT_SETTING_BLOCK) {
    return CONTENT_SETTING_ASK;
  }
  return setting;
}

void StorageAccessGrantPermissionContext::NotifyPermissionSet(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    permissions::BrowserPermissionCallback callback,
    bool persist,
    ContentSetting content_setting,
    bool is_one_time,
    bool is_final_decision) {
  DCHECK(!is_one_time);
  DCHECK(is_final_decision);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  NotifyPermissionSetInternal(
      id, requesting_origin, embedding_origin, std::move(callback), persist,
      content_setting, RequestOutcomeFromPrompt(content_setting, persist));
}

void StorageAccessGrantPermissionContext::NotifyPermissionSetInternal(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    permissions::BrowserPermissionCallback callback,
    bool persist,
    ContentSetting content_setting,
    RequestOutcome outcome) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(blink::features::kStorageAccessAPI)) {
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

  // Our failure cases are tracked by the prompt outcomes in the
  // `Permissions.Action.StorageAccess` histogram. We'll only log when a grant
  // is actually generated.
  bool implicit_result = IsImplicitOutcome(outcome);
  base::UmaHistogramBoolean("API.StorageAccess.GrantIsImplicit",
                            implicit_result);

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context());
  DCHECK(settings_map);
  DCHECK(persist);

  // This permission was allowed so store it either ephemerally or more
  // permanently depending on if the allow came from a prompt or automatic
  // grant.
  settings_map->SetContentSettingCustomScope(
      URLToSchemefulSitePattern(requesting_origin),
      URLToSchemefulSitePattern(embedding_origin),
      ContentSettingsType::STORAGE_ACCESS, content_setting,
      ComputeConstraints(outcome, implicit_result));

  ContentSettingsForOneType grants;
  settings_map->GetSettingsForOneType(ContentSettingsType::STORAGE_ACCESS,
                                      &grants);

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
      ->SetStorageAccessGrantSettings(
          grants, base::BindOnce(std::move(callback), content_setting));
}

void StorageAccessGrantPermissionContext::UpdateContentSetting(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    ContentSetting content_setting,
    bool is_one_time) {
  DCHECK(!is_one_time);
  // We need to notify the network service of content setting updates before we
  // run our callback. As a result we do our updates when we're notified of a
  // permission being set and should not be called here.
  NOTREACHED();
}
