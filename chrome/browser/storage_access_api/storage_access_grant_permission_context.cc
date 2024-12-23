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
#include "base/time/time.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context.h"
#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context_factory.h"
#include "chrome/browser/webid/federated_identity_permission_context.h"
#include "chrome/browser/webid/federated_identity_permission_context_factory.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/constants.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/runtime_feature_state/runtime_feature_state_document_data.h"
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
#include "third_party/blink/public/common/runtime_feature_state/runtime_feature_state_read_context.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"

namespace {

// This is mutable for testing purposes.
static int implicit_grant_limit = 0;

// How far back to look when requiring top-level user interaction on the
// requesting site for Storage Access API permission grants. If this value is an
// empty duration (e.g. "0s"), then no top-level user interaction is required.
constexpr base::TimeDelta kStorageAccessAPITopLevelUserInteractionBound =
    base::Days(30);

// Returns true if the request was answered by the user explicitly. Note that
// this is only called when persisting a permission grant.
bool IsUserDecidedPersistableOutcome(RequestOutcome outcome) {
  switch (outcome) {
    case RequestOutcome::kGrantedByFirstPartySet:
    case RequestOutcome::kGrantedByAllowance:
    case RequestOutcome::kDismissedByUser:
    case RequestOutcome::kReusedPreviousDecision:
    case RequestOutcome::kReusedImplicitGrant:
      return false;
    case RequestOutcome::kGrantedByUser:
    case RequestOutcome::kDeniedByUser:
      return true;

    case RequestOutcome::kDeniedByPrerequisites:
    case RequestOutcome::kDeniedByTopLevelInteractionHeuristic:
    case RequestOutcome::kAllowedByCookieSettings:
    case RequestOutcome::kDeniedByCookieSettings:
    case RequestOutcome::kAllowedBySameSite:
    case RequestOutcome::kDeniedAborted:
    case RequestOutcome::kAllowedByFedCM:
      NOTREACHED();
  }
}

// Returns true if the request outcome should be displayed in the omnibox.
bool ShouldDisplayOutcomeInOmnibox(RequestOutcome outcome) {
  switch (outcome) {
    case RequestOutcome::kGrantedByUser:
    case RequestOutcome::kDeniedByUser:
    case RequestOutcome::kDismissedByUser:
    case RequestOutcome::kReusedPreviousDecision:
      return true;
    case RequestOutcome::kGrantedByFirstPartySet:
    case RequestOutcome::kGrantedByAllowance:
    case RequestOutcome::kDeniedByTopLevelInteractionHeuristic:
    case RequestOutcome::kReusedImplicitGrant:
      return false;

    case RequestOutcome::kDeniedByPrerequisites:
    case RequestOutcome::kAllowedByCookieSettings:
    case RequestOutcome::kDeniedByCookieSettings:
    case RequestOutcome::kAllowedBySameSite:
    case RequestOutcome::kDeniedAborted:
    case RequestOutcome::kAllowedByFedCM:
      NOTREACHED();
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
  }
}

void RecordOutcomeSample(RequestOutcome outcome) {
  base::UmaHistogramEnumeration("API.StorageAccess.RequestOutcome", outcome);
}

content_settings::ContentSettingConstraints ComputeConstraints(
    RequestOutcome outcome,
    base::Time now) {
  content_settings::ContentSettingConstraints constraints(now);
  switch (outcome) {
    case RequestOutcome::kGrantedByFirstPartySet:
      constraints.set_lifetime(
          permissions::kStorageAccessAPIRelatedWebsiteSetsLifetime);
      constraints.set_decided_by_related_website_sets(true);
      return constraints;

    case RequestOutcome::kGrantedByAllowance:
      constraints.set_lifetime(
          permissions::kStorageAccessAPIImplicitPermissionLifetime);
      constraints.set_session_model(
          content_settings::mojom::SessionModel::USER_SESSION);
      return constraints;

    case RequestOutcome::kGrantedByUser:
    case RequestOutcome::kDeniedByUser:
      constraints.set_lifetime(
          permissions::kStorageAccessAPIExplicitPermissionLifetime);
      return constraints;

    case RequestOutcome::kDeniedByPrerequisites:
    case RequestOutcome::kDismissedByUser:
    case RequestOutcome::kReusedPreviousDecision:
    case RequestOutcome::kDeniedByTopLevelInteractionHeuristic:
    case RequestOutcome::kAllowedByCookieSettings:
    case RequestOutcome::kReusedImplicitGrant:
    case RequestOutcome::kDeniedByCookieSettings:
    case RequestOutcome::kAllowedBySameSite:
    case RequestOutcome::kDeniedAborted:
    case RequestOutcome::kAllowedByFedCM:
      NOTREACHED();
  }
}

bool ShouldPersistSetting(bool permission_allowed, RequestOutcome outcome) {
  // User responses to a prompt should be persisted to avoid user annoyance or
  // prompt spam.
  if (IsUserDecidedPersistableOutcome(outcome)) {
    return true;
  }
  // UA-generated denials are not persisted, since they can be re-derived easily
  // and don't have any user-facing concerns, so persistence just adds
  // complexity. UA-generated grants, however, should be persisted to ensure the
  // associated behavioral changes stick.
  return permission_allowed;
}

// Returns true if the user/field trials have enabled FedCM/SAA autogrants
// globally via the flag/Feature, or "locally" via the origin trial.
//
// Feature state overrides take precedence over origin trial state.
bool AreFedCmAutograntsEnabled(content::RenderFrameHost* rfh) {
  if (std::optional<bool> state = base::FeatureList::GetStateIfOverridden(
          blink::features::kFedCmWithStorageAccessAPI);
      state.has_value()) {
    return state.value();
  }
  content::RuntimeFeatureStateDocumentData* document_data =
      content::RuntimeFeatureStateDocumentData::GetForCurrentDocument(rfh);
  CHECK(document_data);

  return document_data->runtime_feature_state_read_context()
      .IsFedCmWithStorageAccessAPIEnabled();
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutograntViaFedCmOutcome {
  kAllowed,
  kDeniedByPermissionsPolicy,
  kDeniedByPermission,
  kDeniedByPreventSilentAccess,

  kMaxValue = kDeniedByPreventSilentAccess,
};

void RecordAutograntViaFedCmOutcomeSample(AutograntViaFedCmOutcome outcome) {
  base::UmaHistogramEnumeration("API.StorageAccess.AutograntViaFedCm", outcome);
}

FederatedIdentityPermissionContext* IsAutograntViaFedCmAllowed(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* rfh,
    const url::Origin& embedding_origin,
    const net::SchemefulSite& embedding_site,
    const net::SchemefulSite& requesting_site) {
  CHECK(browser_context);
  CHECK(base::FeatureList::IsEnabled(
      blink::features::kFedCmWithStorageAccessAPI));
  if (!rfh->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kIdentityCredentialsGet)) {
    RecordAutograntViaFedCmOutcomeSample(
        AutograntViaFedCmOutcome::kDeniedByPermissionsPolicy);
    return nullptr;
  }
  FederatedIdentityPermissionContext* fedcm_context =
      FederatedIdentityPermissionContextFactory::GetForProfile(browser_context);
  if (!fedcm_context || !fedcm_context->HasSharingPermission(
                            /*relying_party_embedder=*/embedding_site,
                            /*identity_provider=*/requesting_site)) {
    RecordAutograntViaFedCmOutcomeSample(
        AutograntViaFedCmOutcome::kDeniedByPermission);
    return nullptr;
  }

  if (FederatedIdentityAutoReauthnPermissionContext* reauth_context =
          FederatedIdentityAutoReauthnPermissionContextFactory::GetForProfile(
              browser_context);
      !reauth_context ||
      reauth_context->RequiresUserMediation(embedding_origin)) {
    RecordAutograntViaFedCmOutcomeSample(
        AutograntViaFedCmOutcome::kDeniedByPreventSilentAccess);
    return nullptr;
  }

  RecordAutograntViaFedCmOutcomeSample(AutograntViaFedCmOutcome::kAllowed);
  RecordOutcomeSample(RequestOutcome::kAllowedByFedCM);
  return fedcm_context;
}

}  // namespace

// static
int StorageAccessGrantPermissionContext::GetImplicitGrantLimitForTesting() {
  return implicit_grant_limit;
}

// static
void StorageAccessGrantPermissionContext::SetImplicitGrantLimitForTesting(
    int limit) {
  implicit_grant_limit = limit;
}

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

  const url::Origin embedding_origin =
      url::Origin::Create(request_data.embedding_origin);

  // Return early without prompting users if cookie access is already allowed.
  // This does not take previously granted SAA permission into account.
  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  net::CookieSettingOverrides overrides = rfh->GetCookieSettingOverrides();
  if (overrides.Has(net::CookieSettingOverride::kStorageAccessGrantEligible) ||
      overrides.Has(
          net::CookieSettingOverride::kStorageAccessGrantEligibleViaHeader)) {
    RecordOutcomeSample(RequestOutcome::kDeniedAborted);
    // The caller already has the `kStorageAccessGrantEligible` or
    // `kStorageAccessGrantEligibleViaHeader` override, which is impossible
    // since those overrides should be used solely by the network service. This
    // suggests the renderer is misbehaving.
    mojo::ReportBadMessage(
        "requestStorageAccess: inconsistent state. Requested permission for a "
        "frame that already claims to have permission.");
    std::move(callback).Run(CONTENT_SETTING_BLOCK);
    return;
  }
  if (cookie_settings->IsFullCookieAccessAllowed(request_data.requesting_origin,
                                                 net::SiteForCookies(),
                                                 embedding_origin, overrides)) {
    RecordOutcomeSample(RequestOutcome::kAllowedByCookieSettings);
    std::move(callback).Run(CONTENT_SETTING_ALLOW);
    return;
  }

  const net::SchemefulSite requesting_site(request_data.requesting_origin);
  const net::SchemefulSite embedding_site(embedding_origin);

  // Return early without prompting users if the requesting frame is same-site
  // with the top-level frame.
  if (requesting_site == embedding_site) {
    RecordOutcomeSample(RequestOutcome::kAllowedBySameSite);
    std::move(callback).Run(CONTENT_SETTING_ALLOW);
    return;
  }

  {
    // Normally a previous prompt rejection would already be filtered before
    // reaching `StorageAccessGrantPermissionContext::DecidePermission`, but the
    // requirement not to surface the user's denial back to the caller means
    // this code is reachable even after permission has been blocked.
    // Accordingly, check the default implementation, and if a denial has been
    // persisted, respect that decision.
    ContentSetting existing_setting =
        PermissionContextBase::GetPermissionStatusInternal(
            rfh, request_data.requesting_origin, request_data.embedding_origin);
    // ALLOW grants are handled by PermissionContextBase so they never reach
    // this point.
    CHECK_NE(existing_setting, CONTENT_SETTING_ALLOW);
    if (existing_setting == CONTENT_SETTING_BLOCK) {
      NotifyPermissionSetInternal(
          request_data.id, request_data.requesting_origin,
          request_data.embedding_origin, std::move(callback),
          /*persist=*/false, existing_setting,
          RequestOutcome::kReusedPreviousDecision);
      return;
    }
    CHECK_EQ(existing_setting, CONTENT_SETTING_ASK);
  }

  // FedCM grants (and the appropriate permissions policy) may allow the call to
  // auto-resolve (without granting a new permission).
  if (AreFedCmAutograntsEnabled(rfh)) {
    if (FederatedIdentityPermissionContext* fedcm_context =
            IsAutograntViaFedCmAllowed(browser_context(), rfh, embedding_origin,
                                       embedding_site, requesting_site);
        fedcm_context) {
      fedcm_context->MarkStorageAccessEligible(
          /*relying_party_embedder=*/embedding_site,
          /*identity_provider=*/requesting_site,
          base::BindOnce(std::move(callback), CONTENT_SETTING_ALLOW));
      return;
    }
  }

  if (!request_data.user_gesture) {
    rfh->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "requestStorageAccess: Must be handling a user gesture to use.");
    RecordOutcomeSample(RequestOutcome::kDeniedByPrerequisites);
    std::move(callback).Run(CONTENT_SETTING_BLOCK);
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
  if (metadata.AreSitesInSameFirstPartySet()) {
    switch (metadata.top_frame_entry()->site_type()) {
      case net::SiteType::kPrimary:
      case net::SiteType::kAssociated:
        // Since the sites are in the same First-Party Set, risk of abuse due
        // to allowing access is considered to be low.
        NotifyPermissionSetInternal(
            request_data.id, request_data.requesting_origin,
            request_data.embedding_origin, std::move(callback),
            /*persist=*/true, CONTENT_SETTING_ALLOW,
            RequestOutcome::kGrantedByFirstPartySet);
        return;
      case net::SiteType::kService:
        break;
    }
  }

  // Get all of our implicit grants and see which ones apply to our
  // |requesting_origin|.
  if (implicit_grant_limit > 0) {
    HostContentSettingsMap* settings_map =
        HostContentSettingsMapFactory::GetForProfile(browser_context());
    CHECK(settings_map);
    ContentSettingsForOneType implicit_grants =
        settings_map->GetSettingsForOneType(
            ContentSettingsType::STORAGE_ACCESS,
            content_settings::mojom::SessionModel::USER_SESSION);

    const int existing_implicit_grants = base::ranges::count_if(
        implicit_grants, [&request_data](const auto& entry) {
          return entry.primary_pattern.Matches(request_data.requesting_origin);
        });

    // If we have fewer grants than our limit, we can just set an implicit grant
    // now and skip prompting the user.
    if (existing_implicit_grants < implicit_grant_limit) {
      NotifyPermissionSetInternal(
          request_data.id, request_data.requesting_origin,
          request_data.embedding_origin, std::move(callback),
          /*persist=*/true, CONTENT_SETTING_ALLOW,
          RequestOutcome::kGrantedByAllowance);
      return;
    }
  }

  // We haven't found a reason to auto-grant permission, but before we prompt
  // there's one more hurdle: the user must have interacted with the requesting
  // site in a top-level context recently.
  DIPSService* dips_service = DIPSService::Get(browser_context());
  if (!dips_service ||
      kStorageAccessAPITopLevelUserInteractionBound == base::TimeDelta()) {
    // If we don't have access to this kind of historical info or the time bound
    // is empty, we waive the requirement, and show the prompt.
    PermissionContextBase::DecidePermission(std::move(request_data),
                                            std::move(callback));
    return;
  }

  GURL site(request_data.requesting_origin);
  dips_service->DidSiteHaveInteractionSince(
      site, base::Time::Now() - kStorageAccessAPITopLevelUserInteractionBound,
      base::BindOnce(&StorageAccessGrantPermissionContext::
                         OnCheckedUserInteractionHeuristic,
                     weak_factory_.GetWeakPtr(), std::move(request_data),
                     std::move(callback)));
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

    if (info.metadata.decided_by_related_website_sets()) {
      outcome = RequestOutcome::kReusedImplicitGrant;
    } else {
      switch (info.metadata.session_model()) {
        case content_settings::mojom::SessionModel::NON_RESTORABLE_USER_SESSION:
        case content_settings::mojom::SessionModel::USER_SESSION:
          outcome = RequestOutcome::kReusedImplicitGrant;
          break;
        case content_settings::mojom::SessionModel::DURABLE:
        case content_settings::mojom::SessionModel::ONE_TIME:
          break;
      }
    }
  }
  NotifyPermissionSetInternal(
      id, requesting_origin, embedding_origin, std::move(callback),
      persist && ShouldPersistSetting(content_setting == CONTENT_SETTING_ALLOW,
                                      outcome),
      content_setting, outcome);
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

  if (!persist) {
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
  CHECK(IsUserDecidedPersistableOutcome(outcome) || permission_allowed);
  if (permission_allowed) {
    base::UmaHistogramBoolean("API.StorageAccess.GrantIsImplicit",
                              !IsUserDecidedPersistableOutcome(outcome));
  }
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context());
  CHECK(settings_map);
  CHECK(persist);

  settings_map->SetContentSettingDefaultScope(
      requesting_origin, embedding_origin, ContentSettingsType::STORAGE_ACCESS,
      content_setting, ComputeConstraints(outcome, settings_map->Now()));

  ContentSettingsForOneType grants =
      settings_map->GetSettingsForOneType(ContentSettingsType::STORAGE_ACCESS);

  // TODO(crbug.com/40638427): Ensure that this update of settings doesn't
  // cause a double update with
  // ProfileNetworkContextService::OnContentSettingChanged.

  // We only want to signal the renderer process once the default storage
  // partition has updated and ack'd the update. This prevents a race where
  // the renderer could initiate a network request based on the response to this
  // request before the access grants have updated in the network service.
  browser_context()
      ->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->SetContentSettings(
          ContentSettingsType::STORAGE_ACCESS, grants,
          base::BindOnce(std::move(callback), content_setting));
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
  NOTREACHED();
}
