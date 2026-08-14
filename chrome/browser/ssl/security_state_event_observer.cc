// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/security_state_event_observer.h"

#include "chrome/browser/ssl/chrome_security_state_util.h"
#include "chrome/browser/ssl/known_interception_disclosure_infobar_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

SecurityStateEventObserver::SecurityStateEventObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

SecurityStateEventObserver::~SecurityStateEventObserver() = default;

void SecurityStateEventObserver::DidStartNavigation(
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

void SecurityStateEventObserver::PrimaryPageChanged(content::Page& page) {
  net::CertStatus cert_status =
      chrome_security_state::GetVisibleSecurityState(web_contents())
          ->cert_status;
  MaybeShowKnownInterceptionDisclosureDialog(web_contents(), cert_status);
}
