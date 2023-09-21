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
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/site_for_cookies.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"

using content_settings::URLToSchemefulSitePattern;

namespace {

// `kPermissionStorageAccessAPI` enables StorageAccessAPIwithPrompts
// (https://chromestatus.com/feature/5085655327047680). StorageAccessAPI is
// considered enabled when either feature is enabled (by different field trial
// studies).
bool StorageAccessAPIEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kStorageAccessAPI) ||
         base::FeatureList::IsEnabled(
             permissions::features::kPermissionStorageAccessAPI);
}

// `kPermissionStorageAccessAPI` enables StorageAccessAPIwithPrompts
// (https://chromestatus.com/feature/5085655327047680), which should not
// auto-deny if FPS is irrelevant.
bool ShouldAutoDenyOutsideFPS() {
  return blink::features::kStorageAccessAPIAutoDenyOutsideFPS.Get() &&
         !base::FeatureList::IsEnabled(
             permissions::features::kPermissionStorageAccessAPI);
}

bool NeedsFirstPartySetMetadata() {
  return base::FeatureList::IsEnabled(features::kFirstPartySets) &&
         (blink::features::kStorageAccessAPIAutoGrantInFPS.Get() ||
          ShouldAutoDenyOutsideFPS());
}

// Returns true if the request wasn't answered by the user explicitly.
bool IsImplicitOutcome(RequestOutcome outcome) {
  switch (outcome) {
    case RequestOutcome::kAllowedByCookieSettings:
    case RequestOutcome::kAllowedBySameSite:
    case RequestOutcome::kDeniedByCookieSettings:
    case RequestOutcome::kDeniedByFirstPartySet:
    case RequestOutcome::kDeniedByPrerequisites:
    case RequestOutcome::kDeniedByTopLevelInteractionHeuristic:
    case RequestOutcome::kDismissedByUser:
    case RequestOutcome::kGrantedByAllowance:
    case RequestOutcome::kGrantedByFirstPartySet:
    case RequestOutcome::kReusedImplicitGrant:
    case RequestOutcome::kReusedPreviousDecision:
    case RequestOutcome::kDeniedAborted:
      return true;
    case RequestOutcome::kDeniedByUser:
    case RequestOutcome::kGrantedByUser:
      return false;
  }
}

// Returns true if the request outcome should be displayed in the omnibox.
bool ShouldDisplayOutcomeInOmnibox(RequestOutcome outcome) {
  switch (outcome) {
    case RequestOutcome::kDeniedByUser:
    case RequestOutcome::kDismissedByUser:
    case RequestOutcome::kGrantedByUser:
    case RequestOutcome::kReusedPreviousDecision:
      return true;
    case RequestOutcome::kAllowedByCookieSettings:
    case RequestOutcome::kAllowedBySameSite:
    case RequestOutcome::kDeniedByCookieSettings:
    case RequestOutcome::kDeniedByFirstPartySet:
    case RequestOutcome::kDeniedByTopLevelInteractionHeuristic:
    case RequestOutcome::kGrantedByAllowance:
    case RequestOutcome::kGrantedByFirstPartySet:
    case RequestOutcome::kReusedImplicitGrant:
    case RequestOutcome::kDeniedByPrerequisites:
    case RequestOutcome::kDeniedAborted:
      return false;
  }
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
    RequestOutcome outcome) {
  content_settings::ContentSettingConstraints constraints;
  switch (outcome) {
    case RequestOutcome::kGrantedByFirstPartySet:
      constraints.set_lifetime(
          blink::features::kStorageAccessAPIImplicitPermissionLifetime.Get());
      constraints.set_session_model(
          content_settings::SessionModel::NonRestorableUserSession);
      return constraints;
    case RequestOutcome::kGrantedByAllowance:
      constraints.set_lifetime(
          blink::features::kStorageAccessAPIImplicitPermissionLifetime.Get());
      constraints.set_session_model(
          content_settings::SessionModel::UserSession);
      return constraints;
    case RequestOutcome::kDismissedByUser:
    case RequestOutcome::kDeniedByFirstPartySet:
    case RequestOutcome::kDeniedByPrerequisites:
    case RequestOutcome::kReusedPreviousDecision:
    case RequestOutcome::kReusedImplicitGrant:
    case RequestOutcome::kDeniedByTopLevelInteractionHeuristic:
    case RequestOutcome::kAllowedByCookieSettings:
    case RequestOutcome::kDeniedByCookieSettings:
    case RequestOutcome::kAllowedBySameSite:
    case RequestOutcome::kDeniedAborted:
      NOTREACHED_NORETURN();
    case RequestOutcome::kGrantedByUser:
    case RequestOutcome::kDeniedByUser:
      constraints.set_lifetime(
          blink::features::kStorageAccessAPIExplicitPermissionLifetime.Get());
      constraints.set_session_model(content_settings::SessionModel::Durable);
      return constraints;
  }
}

bool ShouldPersistSetting(bool permission_allowed,
                          RequestOutcome outcome,
                          bool persist) {
  // Regardless of how the result was obtained, the permissions code determined
  // the result should not be persisted; respect that determination.
  if (!persist) {
    return false;
  }
  // Explicit responses to a prompt should be persisted to avoid user annoyance
  // or prompt spam.
  if (!IsImplicitOutcome(outcome)) {
    return true;
  }
  // Implicit denials are not persisted, since they can be re-derived easily and
  // don't have any user-facing concerns, so persistence just adds complexity.
  // Grants, however, should be persisted to ensure the associated behavioral
  // changes stick.
  return permission_allowed;
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
    permissions::PermissionRequestData request_data,
    permissions::BrowserPermissionCallback callback) {
  DecidePermission(std::move(request_data), std::move(callback));
}

void StorageAccessGrantPermissionContext::DecidePermission(
    permissions::PermissionRequestData request_data,
    permissions::BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(request_data.requesting_origin.is_valid());
  CHECK(request_data.embedding_origin.is_valid());

  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      request_data.id.global_render_frame_host_id());
  CHECK(rfh);

  if (rfh->GetLastCommittedOrigin().opaque() || rfh->IsCredentialless() ||
      rfh->IsNestedWithinFencedFrame() ||
      rfh->IsSandboxed(
          network::mojom::WebSandboxFlags::kStorageAccessByUserActivation)) {
    // No need to log anything here, since well-behaved renderers have already
    // done these checks and have logged to the console. This block is to handle
    // compromised renderers.
    RecordOutcomeSample(RequestOutcome::kDeniedByPrerequisites);
    mojo::ReportBadMessage(
        "requestStorageAccess: Must not be called by a fenced frame, iframe "
        "with an opaque origin, credentialless iframe, or sandboxed iframe");
    std::move(callback).Run(CONTENT_SETTING_BLOCK);
    return;
  }

  // Return early without letting SAA override any explicit user settings to
  // block 3p cookies.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context());
  CHECK(settings_map);
  ContentSetting setting = settings_map->GetContentSetting(
      request_data.requesting_origin, request_data.embedding_origin,
      ContentSettingsType::COOKIES);
  if (setting == CONTENT_SETTING_BLOCK) {
    RecordOutcomeSample(RequestOutcome::kDeniedByCookieSettings);
    std::move(callback).Run(CONTENT_SETTING_BLOCK);
    return;
  }

  // Return early without prompting users if cookie access is already allowed.
  // This does not take previously granted SAA permission into account.
  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  net::CookieSettingOverrides overrides = rfh->GetCookieSettingOverrides();
  overrides.Remove(net::CookieSettingOverride::kStorageAccessGrantEligible);
  if (cookie_settings->IsFullCookieAccessAllowed(
          request_data.requesting_origin, net::SiteForCookies(),
          url::Origin::Create(request_data.embedding_origin), overrides)) {
    RecordOutcomeSample(RequestOutcome::kAllowedByCookieSettings);
    std::move(callback).Run(CONTENT_SETTING_ALLOW);
    return;
  }

  net::SchemefulSite requesting_site(request_data.requesting_origin);
  net::SchemefulSite embedding_site(request_data.embedding_origin);

  // Return early without prompting users if the requesting frame is same-site
  // with the top-level frame.
  if (requesting_site == embedding_site) {
    RecordOutcomeSample(RequestOutcome::kAllowedBySameSite);
    std::move(callback).Run(CONTENT_SETTING_ALLOW);
    return;
  }

  if (!request_data.user_gesture || !StorageAccessAPIEnabled()) {
    if (!request_data.user_gesture) {
      rfh->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kError,
          "requestStorageAccess: Must be handling a user gesture to use.");
    }
    RecordOutcomeSample(RequestOutcome::kDeniedByPrerequisites);
    std::move(callback).Run(CONTENT_SETTING_BLOCK);
    return;
  }

  if (!NeedsFirstPartySetMetadata()) {
    // First-Party Sets is disabled, or Auto-grants and auto-denials are both
    // disabled, so don't bother getting First-Party Sets data.
    UseImplicitGrantOrPrompt(std::move(request_data), std::move(callback));
    return;
  }

  first_party_sets::FirstPartySetsPolicyServiceFactory::GetForBrowserContext(
      browser_context())
      ->ComputeFirstPartySetMetadata(
          requesting_site, &embedding_site,
          base::BindOnce(&StorageAccessGrantPermissionContext::
                             CheckForAutoGrantOrAutoDenial,
                         weak_factory_.GetWeakPtr(), std::move(request_data),
                         std::move(callback)));
}

void StorageAccessGrantPermissionContext::CheckForAutoGrantOrAutoDenial(
    permissions::PermissionRequestData request_data,
    permissions::BrowserPermissionCallback callback,
    net::FirstPartySetMetadata metadata) {
  // We should only run this method if something might need the FPS metadata.
  CHECK(blink::features::kStorageAccessAPIAutoGrantInFPS.Get() ||
        ShouldAutoDenyOutsideFPS());

  if (metadata.AreSitesInSameFirstPartySet()) {
    if (blink::features::kStorageAccessAPIAutoGrantInFPS.Get()) {
      // Service domains are not allowed to request storage access on behalf
      // of other domains, even in the same First-Party Set.
      if (metadata.top_frame_entry()->site_type() == net::SiteType::kService) {
        NotifyPermissionSetInternal(
            request_data.id, request_data.requesting_origin,
            request_data.embedding_origin, std::move(callback),
            /*persist=*/false, CONTENT_SETTING_BLOCK,
            RequestOutcome::kDeniedByPrerequisites);
        return;
      }
      // Since the sites are in the same First-Party Set, risk of abuse due to
      // allowing access is considered to be low.
      NotifyPermissionSetInternal(
          request_data.id, request_data.requesting_origin,
          request_data.embedding_origin, std::move(callback),
          /*persist=*/true, CONTENT_SETTING_ALLOW,
          RequestOutcome::kGrantedByFirstPartySet);
      return;
    }
    // Not autogranting; fall back to implicit grants or prompt.
  } else {
    if (ShouldAutoDenyOutsideFPS()) {
      NotifyPermissionSetInternal(
          request_data.id, request_data.requesting_origin,
          request_data.embedding_origin, std::move(callback),
          /*persist=*/true, CONTENT_SETTING_BLOCK,
          RequestOutcome::kDeniedByFirstPartySet);
      return;
    }
    // Not autodenying; fall back to implicit grants or prompt.
  }

  return UseImplicitGrantOrPrompt(std::move(request_data), std::move(callback));
}

void StorageAccessGrantPermissionContext::UseImplicitGrantOrPrompt(
    permissions::PermissionRequestData request_data,
    permissions::BrowserPermissionCallback callback) {
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context());
  CHECK(settings_map);

  // Normally a previous prompt rejection would already be filtered, but the
  // requirement not to surface the user's denial back to the caller means this
  // path can be reached on subsequent requests. Accordingly, check the default
  // implementation, and if a denial has been persisted, respect that decision.
  content::RenderFrameHost* const rfh = content::RenderFrameHost::FromID(
      request_data.id.global_render_frame_host_id());

  if (!rfh) {
    // After async steps, the RenderFrameHost is not guaranteed to still be
    // alive.
    RecordOutcomeSample(RequestOutcome::kDeniedAborted);
    std::move(callback).Run(CONTENT_SETTING_BLOCK);
    return;
  }

  ContentSetting existing_setting =
      PermissionContextBase::GetPermissionStatusInternal(
          rfh, request_data.requesting_origin, request_data.embedding_origin);
  // ALLOW grants are handled by PermissionContextBase so they never reach this
  // point. StorageAccessGrantPermissionContext::GetPermissionStatusInternal
  // rewrites BLOCK to ASK, so we need to handle BLOCK here explicitly.
  CHECK_NE(existing_setting, CONTENT_SETTING_ALLOW);
  if (existing_setting == CONTENT_SETTING_BLOCK) {
    NotifyPermissionSetInternal(request_data.id, request_data.requesting_origin,
                                request_data.embedding_origin,
                                std::move(callback),
                                /*persist=*/false, existing_setting,
                                RequestOutcome::kReusedPreviousDecision);
    return;
  }

  // Get all of our implicit grants and see which ones apply to our
  // |requesting_origin|.
  ContentSettingsForOneType implicit_grants =
      settings_map->GetSettingsForOneType(
          ContentSettingsType::STORAGE_ACCESS,
          content_settings::SessionModel::UserSession);

  const int existing_implicit_grants = base::ranges::count_if(
      implicit_grants, [&request_data](const auto& entry) {
        return entry.primary_pattern.Matches(request_data.requesting_origin);
      });

  // If we have fewer grants than our limit, we can just set an implicit grant
  // now and skip prompting the user.
  if (existing_implicit_grants <
      blink::features::kStorageAccessAPIImplicitGrantLimit.Get()) {
    NotifyPermissionSetInternal(request_data.id, request_data.requesting_origin,
                                request_data.embedding_origin,
                                std::move(callback),
                                /*persist=*/true, CONTENT_SETTING_ALLOW,
                                RequestOutcome::kGrantedByAllowance);
    return;
  }

  // We haven't found a reason to auto-grant permission, but before we prompt
  // there's one more hurdle: the user must have interacted with the requesting
  // site in a top-level context recently.
  DIPSService* dips_service = DIPSService::Get(browser_context());
  const base::TimeDelta bound =
      blink::features::kStorageAccessAPITopLevelUserInteractionBound.Get();
  if (bound != base::TimeDelta() && dips_service) {
    GURL site = request_data.requesting_origin;
    dips_service->DidSiteHaveInteractionSince(
        site, base::Time::Now() - bound,
        base::BindOnce(&StorageAccessGrantPermissionContext::
                           OnCheckedUserInteractionHeuristic,
                       weak_factory_.GetWeakPtr(), std::move(request_data),
                       std::move(callback)));
    return;
  }

  // If we don't have access to this kind of historical info or the time bound
  // is empty, we waive the requirement, and show the prompt.
  PermissionContextBase::DecidePermission(std::move(request_data),
                                          std::move(callback));
}

void StorageAccessGrantPermissionContext::OnCheckedUserInteractionHeuristic(
    permissions::PermissionRequestData request_data,
    permissions::BrowserPermissionCallback callback,
    bool had_top_level_user_interaction) {
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      request_data.id.global_render_frame_host_id());

  if (!rfh) {
    // After async steps, the RenderFrameHost is not guaranteed to still be
    // alive.
    RecordOutcomeSample(RequestOutcome::kDeniedAborted);
    std::move(callback).Run(CONTENT_SETTING_BLOCK);
    return;
  }

  if (!had_top_level_user_interaction) {
    rfh->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "requestStorageAccess: Request denied because the embedded site has "
        "never been interacted with as a top-level context");
    NotifyPermissionSetInternal(
        request_data.id, request_data.requesting_origin,
        request_data.embedding_origin, std::move(callback),
        /*persist=*/false, CONTENT_SETTING_BLOCK,
        RequestOutcome::kDeniedByTopLevelInteractionHeuristic);
    return;
  }

  // PermissionContextBase::DecidePermission requires that the RenderFrameHost
  // is still alive.
  CHECK(rfh);
  // Show prompt.
  PermissionContextBase::DecidePermission(std::move(request_data),
                                          std::move(callback));
}

ContentSetting StorageAccessGrantPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  if (!StorageAccessAPIEnabled()) {
    return CONTENT_SETTING_BLOCK;
  }

  // Permission query from top-level frame should be "granted" by default.
  if (render_frame_host && render_frame_host->IsInPrimaryMainFrame()) {
    return CONTENT_SETTING_ALLOW;
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
  CHECK(!is_one_time);
  CHECK(is_final_decision);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  RequestOutcome outcome = RequestOutcomeFromPrompt(content_setting, persist);
  if (outcome == RequestOutcome::kReusedPreviousDecision) {
    // This could be an implicit, e.g. FPS or allowance based permission. Check
    // if the exception has an ephemeral session model.
    content_settings::SettingInfo info;
    HostContentSettingsMapFactory::GetForProfile(browser_context())
        ->GetContentSetting(requesting_origin, embedding_origin,
                            ContentSettingsType::STORAGE_ACCESS, &info);

    switch (info.metadata.session_model()) {
      case content_settings::SessionModel::NonRestorableUserSession:
      case content_settings::SessionModel::UserSession:
        outcome = RequestOutcome::kReusedImplicitGrant;
        break;
      case content_settings::SessionModel::Durable:
      case content_settings::SessionModel::OneTime:
        break;
    }
  }
  NotifyPermissionSetInternal(id, requesting_origin, embedding_origin,
                              std::move(callback), persist, content_setting,
                              outcome);
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

  if (!StorageAccessAPIEnabled()) {
    return;
  }

  RecordOutcomeSample(outcome);

  const bool permission_allowed = (content_setting == CONTENT_SETTING_ALLOW);
  UpdateTabContext(id, requesting_origin, permission_allowed);

  if (ShouldDisplayOutcomeInOmnibox(outcome)) {
    auto* content_settings =
        content_settings::PageSpecificContentSettings::GetForFrame(
            id.global_render_frame_host_id());
    if (content_settings) {
      content_settings->OnTwoSitePermissionChanged(
          ContentSettingsType::STORAGE_ACCESS,
          net::SchemefulSite(requesting_origin), content_setting);
    }
  }

  if (!ShouldPersistSetting(permission_allowed, outcome, persist)) {
    if (content_setting == CONTENT_SETTING_DEFAULT) {
      content_setting = CONTENT_SETTING_ASK;
    }

    std::move(callback).Run(content_setting);
    return;
  }

  // Our failure cases are tracked by the prompt outcomes in the
  // `Permissions.Action.StorageAccess` histogram. Because implicitly denied
  // results return early, in practice this means that an implicit result at
  // this point means a grant was generated.
  CHECK(!IsImplicitOutcome(outcome) || permission_allowed);
  if (permission_allowed) {
    base::UmaHistogramBoolean("API.StorageAccess.GrantIsImplicit",
                              IsImplicitOutcome(outcome));
  }
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context());
  CHECK(settings_map);
  CHECK(persist);

  settings_map->SetContentSettingDefaultScope(
      requesting_origin, embedding_origin, ContentSettingsType::STORAGE_ACCESS,
      content_setting, ComputeConstraints(outcome));

  ContentSettingsForOneType grants =
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
      ->SetStorageAccessGrantSettings(
          grants, base::BindOnce(std::move(callback), content_setting));
}

void StorageAccessGrantPermissionContext::UpdateContentSetting(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    ContentSetting content_setting,
    bool is_one_time) {
  CHECK(!is_one_time);
  // We need to notify the network service of content setting updates before we
  // run our callback. As a result we do our updates when we're notified of a
  // permission being set and should not be called here.
  NOTREACHED_NORETURN();
}
