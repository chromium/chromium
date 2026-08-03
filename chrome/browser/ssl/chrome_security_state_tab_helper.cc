// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"

#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/ssl/chrome_security_state_util.h"
#include "chrome/browser/ssl/known_interception_disclosure_infobar_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/secure_origin_allowlist.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/safe_browsing/buildflags.h"
#include "components/security_interstitials/core/pref_names.h"
#include "components/security_state/content/content_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
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
  return chrome_security_state::GetVisibleSecurityState(web_contents());
}

void ChromeSecurityStateTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsFormSubmission()) {
    return;
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
  MaybeShowKnownInterceptionDisclosureDialog(web_contents(), cert_status);
}

security_state::MaliciousContentStatus
ChromeSecurityStateTabHelper::GetMaliciousContentStatus() {
  return chrome_security_state::GetMaliciousContentStatus(web_contents());
}
