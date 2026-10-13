// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_enterprise_policy_checker.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS) || \
    BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"
#endif

namespace actor {

namespace {

bool IsActuationEnabledByManagedPolicy(Profile& profile) {
  const features::GlicActorEnterprisePrefDefault default_pref =
      features::kGlicActorEnterprisePrefDefault.Get();
  if (default_pref ==
      features::GlicActorEnterprisePrefDefault::kForcedDisabled) {
    return false;
  }

  const auto capability_pref =
      glic::prefs::GetActuationOnWebCapability(profile.GetPrefs())
          .value_or(glic::prefs::GlicActuationOnWebPolicyState::kDisabled);
  return capability_pref ==
         glic::prefs::GlicActuationOnWebPolicyState::kEnabled;
}

}  // namespace

ActorEnterprisePolicyChecker::ActorEnterprisePolicyChecker(Profile* profile)
    : profile_(profile),
      url_blocklist_manager_(profile_->GetPrefs(),
                             glic::prefs::kGlicActuationOnWebBlockedForURLs,
                             glic::prefs::kGlicActuationOnWebAllowedForURLs) {
  CHECK(profile_);
}

ActorEnterprisePolicyChecker::~ActorEnterprisePolicyChecker() = default;

base::CallbackListSubscription
ActorEnterprisePolicyChecker::AddUrlListsUpdateObserver(
    base::RepeatingClosure callback) {
  return url_blocklist_manager_.AddObserver(std::move(callback));
}

EnterprisePolicyChecker::UrlBlockReason ActorEnterprisePolicyChecker::Evaluate(
    const GURL& url) const {
  const policy::URLBlocklist::URLBlocklistState state =
      url_blocklist_manager_.GetURLBlocklistState(url);
  if (state == policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST) {
    return UrlBlockReason::kExplicitlyBlocked;
  }
  if (state == policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST) {
    return UrlBlockReason::kExplicitlyAllowed;
  }

  if (!base::FeatureList::IsEnabled(
          features::kGlicActorPolicyControlExemption) &&
      !(glic::GlicEnabling::IsLikelyDogfoodClient() &&
        gaia::IsGoogleInternalAccountEmail(profile_->GetProfileUserName()))) {
    if (glic::GlicEnabling::IsBrowserManaged(profile_)) {
      if (!IsActuationEnabledByManagedPolicy(*profile_)) {
        return UrlBlockReason::kExplicitlyBlocked;
      }
    } else if (glic::GlicEnabling::IsEnterpriseAccount(profile_)) {
      if (features::kGlicActorEnterprisePrefDefault.Get() !=
          features::GlicActorEnterprisePrefDefault::kEnabledByDefault) {
        return UrlBlockReason::kExplicitlyBlocked;
      }
    }
  }

  return UrlBlockReason::kNotBlocked;
}

void ActorEnterprisePolicyChecker::ValidateContentSentToRenderer(
    content::RenderFrameHost* frame,
    const std::string& content,
    ContentValidationCallback callback) const {
  content::WebContents* web_contents =
      frame ? content::WebContents::FromRenderFrameHost(frame) : nullptr;
  if (!web_contents || !profile_) {
    std::move(callback).Run(ContentValidationReason::kAllowed);
    return;
  }

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS) || \
    BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
  enterprise_data_protection::PasteFromGeminiIfAllowedByPolicy(
      frame, content,
      base::BindOnce(
          [](ContentValidationCallback cb, bool allowed) {
            std::move(cb).Run(allowed ? ContentValidationReason::kAllowed
                                      : ContentValidationReason::kBlocked);
          },
          std::move(callback)));
#else
  std::move(callback).Run(ContentValidationReason::kAllowed);
#endif
}

}  // namespace actor
