// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/top_level_storage_access_api/top_level_storage_access_permission_context.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chrome/browser/bad_message.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/constants.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/permission_prompt_decision.h"
#include "components/permissions/permission_request_data.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-shared.h"

namespace {

void RecordOutcomeSample(TopLevelStorageAccessRequestOutcome outcome) {
  base::UmaHistogramEnumeration("API.TopLevelStorageAccess.RequestOutcome",
                                outcome);
}

bool IsNonAllowSetting(const ContentSettingPatternSource& setting) {
  return setting.GetContentSetting() != CONTENT_SETTING_ALLOW;
}

}  // namespace

TopLevelStorageAccessPermissionContext::TopLevelStorageAccessPermissionContext(
    content::BrowserContext* browser_context)
    : ContentSettingPermissionContextBase(
          browser_context,
          ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
          network::mojom::PermissionsPolicyFeature::kStorageAccessAPI) {}

TopLevelStorageAccessPermissionContext::
    ~TopLevelStorageAccessPermissionContext() = default;

void TopLevelStorageAccessPermissionContext::DecidePermissionForTesting(
    std::unique_ptr<permissions::PermissionRequestData> request_data,
    permissions::BrowserPermissionCallback callback) {
  DecidePermission(std::move(request_data), std::move(callback));
}

void TopLevelStorageAccessPermissionContext::DecidePermission(
    std::unique_ptr<permissions::PermissionRequestData> request_data,
    permissions::BrowserPermissionCallback callback) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M162);

  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      request_data->id.global_render_frame_host_id());
  CHECK(rfh);
  if (!rfh->IsInPrimaryMainFrame()) {
    rfh->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kError,
                             "requestStorageAccessFor: Only supported in "
                             "primary top-level browsing contexts.");
    RecordOutcomeSample(
        TopLevelStorageAccessRequestOutcome::kDeniedByPrerequisites);
    std::move(callback).Run(content::PermissionResult(
        blink::mojom::PermissionStatus::DENIED,
        content::PermissionStatusSource::UNSPECIFIED));
    return;
  }

  net::SchemefulSite embedding_site(request_data->embedding_origin);
  net::SchemefulSite requesting_site(request_data->requesting_origin);

  if (requesting_site == embedding_site) {
    // Well-behaved renderers don't send same-site permissions requests, since
    // there is no privacy boundary within a site. This must be a compromised
    // renderer.
    bad_message::ReceivedBadMessage(
        rfh->GetProcess(), bad_message::BadMessageReason::
                               TLSAPC_INVALID_PERMISSION_REQUEST_CONTEXT);
    std::move(callback).Run(content::PermissionResult(
        blink::mojom::PermissionStatus::DENIED,
        content::PermissionStatusSource::UNSPECIFIED));
    return;
  }

  if (!request_data->user_gesture || !rfh->HasTransientUserActivation() ||
      !request_data->requesting_origin.is_valid() ||
      !request_data->embedding_origin.is_valid()) {
    if (!request_data->user_gesture || !rfh->HasTransientUserActivation()) {
      rfh->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kError,
          "requestStorageAccessFor: Must be handling a user gesture to use.");
    }
    RecordOutcomeSample(
        TopLevelStorageAccessRequestOutcome::kDeniedByPrerequisites);
    std::move(callback).Run(content::PermissionResult(
        blink::mojom::PermissionStatus::DENIED,
        content::PermissionStatusSource::UNSPECIFIED));
    return;
  }

  NotifyPermissionSetInternal(
      *request_data, std::move(callback),
      /*persist=*/false, PermissionDecision::kDeny,
      TopLevelStorageAccessRequestOutcome::kDeniedByFirstPartySet);
}

ContentSetting
TopLevelStorageAccessPermissionContext::GetContentSettingStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  if (render_frame_host && !render_frame_host->IsInPrimaryMainFrame()) {
    // Note that fenced frames and other main but non-outermost frames are
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

  ContentSetting setting =
      ContentSettingPermissionContextBase::GetContentSettingStatusInternal(
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
    const permissions::PermissionRequestData& request_data,
    permissions::BrowserPermissionCallback callback,
    bool persist,
    const content::PermissionResult* permission_result,
    const permissions::PermissionPromptDecision& decision) {
  CHECK(decision.overall_decision != PermissionDecision::kAllowThisTime);
  CHECK(decision.is_final);
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M162);

  if (decision.overall_decision == PermissionDecision::kDeny) {
    CHECK(!persist);
  }

  NotifyPermissionSetInternal(
      request_data, std::move(callback), persist, decision.overall_decision,
      decision.overall_decision == PermissionDecision::kAllow
          ? TopLevelStorageAccessRequestOutcome::kGrantedByFirstPartySet
          : TopLevelStorageAccessRequestOutcome::kDeniedByFirstPartySet);
}

void TopLevelStorageAccessPermissionContext::NotifyPermissionSetInternal(
    const permissions::PermissionRequestData& request_data,
    permissions::BrowserPermissionCallback callback,
    bool persist,
    PermissionDecision decision,
    TopLevelStorageAccessRequestOutcome outcome) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M162);

  RecordOutcomeSample(outcome);

  UpdateTabContext(request_data, decision == PermissionDecision::kAllow);

  if (!persist) {
    auto status = blink::mojom::PermissionStatus::ASK;
    if (decision == PermissionDecision::kAllow ||
        decision == PermissionDecision::kDeny) {
      status = decision == PermissionDecision::kAllow
                   ? blink::mojom::PermissionStatus::GRANTED
                   : blink::mojom::PermissionStatus::DENIED;
    }
    std::move(callback).Run(content::PermissionResult(
        status, content::PermissionStatusSource::UNSPECIFIED));
    return;
  }

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context());
  CHECK(settings_map);
  CHECK(persist);
  // This permission type doesn't support user prompts, so any denials are
  // user-agent-generated. Machine-generated denials are not persisted.
  CHECK_EQ(decision, PermissionDecision::kAllow);

  content_settings::ContentSettingConstraints constraints;
  constraints.set_lifetime(
      permissions::kStorageAccessAPIRelatedWebsiteSetsLifetime);
  constraints.set_decided_by_related_website_sets(true);

  settings_map->SetContentSettingDefaultScope(
      request_data.requesting_origin, request_data.embedding_origin,
      ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS, CONTENT_SETTING_ALLOW,
      constraints);

  // Because this is a superset of the regular storage access permission, we
  // also store that one.
  settings_map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(request_data.requesting_origin),
      ContentSettingsPattern::FromURLToSchemefulSitePattern(
          request_data.embedding_origin),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW, constraints);

  ContentSettingsForOneType storage_access_grants =
      settings_map->GetSettingsForOneType(ContentSettingsType::STORAGE_ACCESS);
  // The network service only cares about "granted" settings, so we don't bother
  // to send any others.
  std::erase_if(storage_access_grants, IsNonAllowSetting);

  // TODO(crbug.com/40638427): Ensure that this update of settings doesn't
  // cause a double update with
  // ProfileNetworkContextService::OnContentSettingChanged.

  // We only want to signal the renderer process once the default storage
  // partition has updated and ack'd the update. This prevents a race where
  // the renderer could initiate a network request based on the response to this
  // request before the access grants have updated in the network service.
  auto* cookie_manager = browser_context()
                             ->GetDefaultStoragePartition()
                             ->GetCookieManagerForBrowserProcess();
  cookie_manager->SetContentSettings(
      ContentSettingsType::STORAGE_ACCESS, storage_access_grants,
      base::BindOnce(std::move(callback),
                     content::PermissionResult(
                         blink::mojom::PermissionStatus::GRANTED,
                         content::PermissionStatusSource::UNSPECIFIED)));
}

void TopLevelStorageAccessPermissionContext::UpdateSetting(
    const permissions::PermissionRequestData& request_data,
    const PermissionSetting& setting,
    bool is_one_time) {
  // We need to notify the network service of content setting updates before we
  // run our callback. As a result we do our updates when we're notified of a
  // permission being set and should not be called here.
  NOTREACHED();
}
