// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"

#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lookalikes/safety_tip_web_contents_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ssl/https_only_mode_tab_helper.h"
#include "chrome/browser/ssl/known_interception_disclosure_infobar_delegate.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/secure_origin_allowlist.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/security_interstitials/core/pref_names.h"
#include "components/security_state/content/content_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/policy/networking/policy_cert_service.h"
#include "chrome/browser/policy/networking/policy_cert_service_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#endif

using password_manager::metrics_util::PasswordType;
using safe_browsing::SafeBrowsingUIManager;
using UsesEmbedderInformation = SecurityStateTabHelper::UsesEmbedderInformation;

void ChromeSecurityStateTabHelper::CreateForWebContents(
    content::WebContents* contents) {
  DCHECK(contents);
  SecurityStateTabHelper* helper = FromWebContents(contents);
  if (!helper) {
    helper = new ChromeSecurityStateTabHelper(contents);
    contents->SetUserData(UserDataKey(), base::WrapUnique(helper));
  }
  CHECK(helper->uses_embedder_information())
      << "Do not create a SecurityStateTabHelper in chrome/!";
}

ChromeSecurityStateTabHelper::ChromeSecurityStateTabHelper(
    content::WebContents* web_contents)
    : SecurityStateTabHelper(web_contents, UsesEmbedderInformation(true)),
      content::WebContentsObserver(web_contents) {}

ChromeSecurityStateTabHelper::~ChromeSecurityStateTabHelper() = default;

std::unique_ptr<security_state::VisibleSecurityState>
ChromeSecurityStateTabHelper::GetVisibleSecurityState() {
  auto state = SecurityStateTabHelper::GetVisibleSecurityState();

  // Malware status might already be known even if connection security
  // information is still being initialized, thus no need to check for that.
  state->malicious_content_status = GetMaliciousContentStatus();

  SafetyTipWebContentsObserver* safety_tip_web_contents_observer =
      SafetyTipWebContentsObserver::FromWebContents(web_contents());
  state->safety_tip_info =
      safety_tip_web_contents_observer
          ? safety_tip_web_contents_observer
                ->GetSafetyTipInfoForVisibleNavigation()
          : security_state::SafetyTipInfo(
                {security_state::SafetyTipStatus::kUnknown, GURL()});

  // If both the mixed form warnings are not disabled by policy we don't degrade
  // the lock icon for sites with mixed forms present.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (profile &&
      profile->GetPrefs()->GetBoolean(prefs::kMixedFormsWarningsEnabled)) {
    state->should_treat_displayed_mixed_forms_as_secure = true;
  }

  // TODO(crbug.com/40248833): Track upgrade/fallback state per-navigation.
  // Currently HTTPS Upgrades state is tracked via a TabHelper attached to the
  // current WebContents (i.e., per tab), which can cause this state to "leak"
  // across multiple different navigations, potentially causing the wrong
  // security state to be computed.
  auto* https_only_mode_tab_helper =
      HttpsOnlyModeTabHelper::FromWebContents(web_contents());
  if (https_only_mode_tab_helper &&
      (https_only_mode_tab_helper->is_navigation_upgraded() ||
       https_only_mode_tab_helper->is_navigation_fallback())) {
    state->is_https_only_mode_upgraded = true;
  }

  return state;
}

void ChromeSecurityStateTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsFormSubmission()) {
    return;
  }
  UMA_HISTOGRAM_ENUMERATION("Security.SecurityLevel.FormSubmission",
                            GetSecurityLevel(),
                            security_state::SECURITY_LEVEL_COUNT);
  if (navigation_handle->IsInMainFrame() &&
      !security_state::IsSchemeCryptographic(GetVisibleSecurityState()->url)) {
    UMA_HISTOGRAM_ENUMERATION(
        "Security.SecurityLevel.InsecureMainFrameFormSubmission",
        GetSecurityLevel(), security_state::SECURITY_LEVEL_COUNT);
  }

  if (navigation_handle->GetURL().SchemeIs(url::kHttpsScheme)) {
    ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
    CHECK(ukm_recorder);
    ukm::SourceId source_id = ukm::ConvertToSourceId(
        navigation_handle->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
    ukm::builders::OmniboxSecurityIndicator_FormSubmission(source_id)
        .SetSubmitted(true)
        .Record(ukm_recorder);
  }
}

void ChromeSecurityStateTabHelper::PrimaryPageChanged(content::Page& page) {
  net::CertStatus cert_status = GetVisibleSecurityState()->cert_status;
  if (net::IsCertStatusError(cert_status) &&
      !page.GetMainDocument().IsErrorDocument()) {
    // Record each time a user visits a site after having clicked through a
    // certificate warning interstitial. This is used as a baseline for
    // interstitial.ssl.did_user_revoke_decision2 in order to determine how
    // many times the re-enable warnings button is clicked, as a fraction of
    // the number of times it was available.
    UMA_HISTOGRAM_BOOLEAN("interstitial.ssl.visited_site_after_warning", true);
  }

  MaybeShowKnownInterceptionDisclosureDialog(web_contents(), cert_status);
}

bool ChromeSecurityStateTabHelper::UsedPolicyInstalledCertificate() const {
#if BUILDFLAG(IS_CHROMEOS)
  policy::PolicyCertService* service =
      policy::PolicyCertServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  if (service && service->UsedPolicyCertificates()) {
    return true;
  }
#endif
  return false;
}

security_state::MaliciousContentStatus
ChromeSecurityStateTabHelper::GetMaliciousContentStatus() const {
  using enum safe_browsing::SBThreatType;

  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  if (!entry) {
    return security_state::MALICIOUS_CONTENT_STATUS_NONE;
  }
  safe_browsing::SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  if (!sb_service) {
    return security_state::MALICIOUS_CONTENT_STATUS_NONE;
  }
  scoped_refptr<SafeBrowsingUIManager> sb_ui_manager = sb_service->ui_manager();
  safe_browsing::SBThreatType threat_type;
  if (sb_ui_manager->IsUrlAllowlistedOrPendingForWebContents(
          entry->GetURL(), entry, web_contents(), false, &threat_type)) {
    switch (threat_type) {
      case SB_THREAT_TYPE_UNUSED:
      case SB_THREAT_TYPE_SAFE:
      case SB_THREAT_TYPE_URL_PHISHING:
      case SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING:
        return security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING;
      case SB_THREAT_TYPE_URL_MALWARE:
        return security_state::MALICIOUS_CONTENT_STATUS_MALWARE;
      case SB_THREAT_TYPE_URL_UNWANTED:
        return security_state::MALICIOUS_CONTENT_STATUS_UNWANTED_SOFTWARE;
      case SB_THREAT_TYPE_SAVED_PASSWORD_REUSE:
#if BUILDFLAG(FULL_SAFE_BROWSING)
        return security_state::MALICIOUS_CONTENT_STATUS_SAVED_PASSWORD_REUSE;
#endif
      case SB_THREAT_TYPE_SIGNED_IN_SYNC_PASSWORD_REUSE:
#if BUILDFLAG(FULL_SAFE_BROWSING)
        if (safe_browsing::ChromePasswordProtectionService::
                ShouldShowPasswordReusePageInfoBubble(
                    web_contents(), PasswordType::PRIMARY_ACCOUNT_PASSWORD)) {
          return security_state::
              MALICIOUS_CONTENT_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE;
        }
#endif
        [[fallthrough]];
      case SB_THREAT_TYPE_SIGNED_IN_NON_SYNC_PASSWORD_REUSE:
#if BUILDFLAG(FULL_SAFE_BROWSING)
        if (safe_browsing::ChromePasswordProtectionService::
                ShouldShowPasswordReusePageInfoBubble(
                    web_contents(), PasswordType::OTHER_GAIA_PASSWORD)) {
          return security_state::
              MALICIOUS_CONTENT_STATUS_SIGNED_IN_NON_SYNC_PASSWORD_REUSE;
        }
#endif
        [[fallthrough]];
      case SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE:
#if BUILDFLAG(FULL_SAFE_BROWSING)
        if (safe_browsing::ChromePasswordProtectionService::
                ShouldShowPasswordReusePageInfoBubble(
                    web_contents(), PasswordType::ENTERPRISE_PASSWORD)) {
          return security_state::
              MALICIOUS_CONTENT_STATUS_ENTERPRISE_PASSWORD_REUSE;
        }
#endif
        // If user has already changed password or FULL_SAFE_BROWSING isn't
        // enabled, returns the regular social engineering content status.
        return security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING;
      case SB_THREAT_TYPE_BILLING:
        return security_state::MALICIOUS_CONTENT_STATUS_BILLING;
      case SB_THREAT_TYPE_MANAGED_POLICY_BLOCK:
        return security_state::MALICIOUS_CONTENT_STATUS_MANAGED_POLICY_BLOCK;
      case SB_THREAT_TYPE_MANAGED_POLICY_WARN:
        return security_state::MALICIOUS_CONTENT_STATUS_MANAGED_POLICY_WARN;
      case DEPRECATED_SB_THREAT_TYPE_URL_PASSWORD_PROTECTION_PHISHING:
      case DEPRECATED_SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE:
      case SB_THREAT_TYPE_URL_BINARY_MALWARE:
      case SB_THREAT_TYPE_EXTENSION:
      case SB_THREAT_TYPE_API_ABUSE:
      case SB_THREAT_TYPE_SUBRESOURCE_FILTER:
      case SB_THREAT_TYPE_CSD_ALLOWLIST:
      case SB_THREAT_TYPE_AD_SAMPLE:
      case SB_THREAT_TYPE_BLOCKED_AD_POPUP:
      case SB_THREAT_TYPE_BLOCKED_AD_REDIRECT:
      case SB_THREAT_TYPE_SUSPICIOUS_SITE:
      case SB_THREAT_TYPE_APK_DOWNLOAD:
      case SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST:
        // These threat types are not currently associated with
        // interstitials, and thus resources with these threat types are
        // not ever whitelisted or pending whitelisting.
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
  return security_state::MALICIOUS_CONTENT_STATUS_NONE;
}
