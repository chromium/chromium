// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_metadata_classifier.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace indigo {

IndigoMetadataClassifier::IndigoMetadataClassifier(
    IndigoService* indigo_service,
    base::RepeatingClosure on_result_updated)
    : indigo_service_(indigo_service),
      on_result_updated_(std::move(on_result_updated)) {}

IndigoMetadataClassifier::~IndigoMetadataClassifier() = default;

void IndigoMetadataClassifier::OnNavigationCommitted(
    content::WebContents* web_contents,
    bool is_same_document) {
  const bool preserve_remote =
      is_same_document && web_contents_ == web_contents;
  CancelPendingClassification();
  if (!preserve_remote) {
    metadata_remote_.reset();
  }
  web_contents_ = web_contents;
  state_ = State::kUnknown;
  document_onload_completed_ = false;
  retry_completed_ = false;

  if (!base::FeatureList::IsEnabled(
          features::kIndigoMetadataKeywordHeuristic)) {
    return;
  }

  if (!web_contents_ || !indigo_service_ ||
      (indigo_service_->IsConfigLoaded() &&
       !indigo_service_->IsOriginAllowed(
           url::Origin::Create(web_contents_->GetLastCommittedURL())))) {
    state_ = State::kNoMatch;
    return;
  }

  state_ = State::kPending;
  if (is_same_document) {
    retry_timer_.Start(
        FROM_HERE,
        features::kIndigoMetadataKeywordHeuristicSameDocumentNavigationDelay
            .Get(),
        this, &IndigoMetadataClassifier::OnRetryTimeout);
  }

  max_wait_timer_.Start(
      FROM_HERE, features::kIndigoMetadataKeywordHeuristicMaxWaitTime.Get(),
      this, &IndigoMetadataClassifier::OnMaxWaitTimeout);
}

void IndigoMetadataClassifier::OnDOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  if (state_ != State::kPending || !web_contents_ ||
      render_frame_host != web_contents_->GetPrimaryMainFrame()) {
    return;
  }

  retry_timer_.Start(
      FROM_HERE, features::kIndigoMetadataKeywordHeuristicPostDclDelay.Get(),
      this, &IndigoMetadataClassifier::OnRetryTimeout);
  TriggerClassification(/*is_final_check=*/false);
}

void IndigoMetadataClassifier::OnDocumentOnLoadCompletedInPrimaryMainFrame() {
  if (state_ != State::kPending) {
    return;
  }
  document_onload_completed_ = true;
  TriggerClassification(/*is_final_check=*/retry_completed_);
}

void IndigoMetadataClassifier::Reset() {
  CancelPendingClassification();
  metadata_remote_.reset();
  web_contents_ = nullptr;
  state_ = State::kUnknown;
  document_onload_completed_ = false;
  retry_completed_ = false;
}

void IndigoMetadataClassifier::CancelPendingClassification() {
  if (state_ == State::kPending) {
    state_ = State::kUnknown;
  }
  retry_timer_.Stop();
  max_wait_timer_.Stop();
  pending_request_.Cancel();
}

void IndigoMetadataClassifier::OnRetryTimeout() {
  retry_completed_ = true;
  TriggerClassification(/*is_final_check=*/document_onload_completed_);
}

void IndigoMetadataClassifier::OnMaxWaitTimeout() {
  TriggerClassification(/*is_final_check=*/true);
}

void IndigoMetadataClassifier::TriggerClassification(bool is_final_check) {
  if (state_ != State::kPending) {
    return;
  }

  if (!web_contents_ || !indigo_service_) {
    LockInResult(false, /*record_uma=*/false);
    return;
  }

  if (!indigo_service_->IsConfigLoaded()) {
    if (!max_wait_timer_.IsRunning()) {
      LockInResult(false, /*record_uma=*/false);
    }
    return;
  }

  const GURL& url = web_contents_->GetLastCommittedURL();
  if (!indigo_service_->IsOriginAllowed(url::Origin::Create(url))) {
    LockInResult(false, /*record_uma=*/false);
    return;
  }

  content::RenderFrameHost* rfh = web_contents_->GetPrimaryMainFrame();
  if (!rfh || !rfh->IsRenderFrameLive()) {
    LockInResult(false, /*record_uma=*/false);
    return;
  }

  if (!metadata_remote_.is_bound()) {
    rfh->GetRemoteInterfaces()->GetInterface(
        metadata_remote_.BindNewPipeAndPassReceiver());
    metadata_remote_.reset_on_disconnect();
  }

  pending_request_.Reset(
      base::BindOnce(&IndigoMetadataClassifier::OnProductClassified,
                     base::Unretained(this), is_final_check));
  metadata_remote_->ClassifyProductDetails(
      indigo_service_->GetAllowedKeywords(),
      indigo_service_->GetBlockedKeywords(), pending_request_.callback());
}

void IndigoMetadataClassifier::OnProductClassified(
    bool is_final_check,
    blink::mojom::ProductClassificationResultPtr result) {
  if (result) {
    LockInResult(
        result->allowed_keyword_found && !result->blocked_keyword_found,
        /*record_uma=*/true);
  } else if (is_final_check) {
    LockInResult(false, /*record_uma=*/true);
  }
}

void IndigoMetadataClassifier::LockInResult(bool matches, bool record_uma) {
  state_ = matches ? State::kMatch : State::kNoMatch;
  CancelPendingClassification();
  if (record_uma) {
    base::UmaHistogramBoolean("Indigo.Discovery.MetadataKeywordHeuristic",
                              matches);
  }
  on_result_updated_.Run();
}

}  // namespace indigo
