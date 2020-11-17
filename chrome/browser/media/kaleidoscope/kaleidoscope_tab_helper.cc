// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/kaleidoscope/kaleidoscope_tab_helper.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/media/kaleidoscope/constants.h"
#include "content/public/browser/navigation_handle.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom.h"

namespace {

const url::Origin& KaleidoscopeOrigin() {
  static base::NoDestructor<url::Origin> origin(
      url::Origin::Create(GURL(kKaleidoscopeUIURL)));
  return *origin;
}

const url::Origin& KaleidoscopeUntrustedOrigin() {
  static base::NoDestructor<url::Origin> origin(
      url::Origin::Create(GURL(kKaleidoscopeUntrustedContentUIURL)));
  return *origin;
}

bool IsOpenedFromKaleidoscope(content::NavigationHandle* handle) {
  return (handle->GetInitiatorOrigin() &&
          handle->GetInitiatorOrigin()->IsSameOriginWith(
              KaleidoscopeUntrustedOrigin()));
}

bool ShouldAllowAutoplay(content::NavigationHandle* handle) {
  // If the initiating origin is Kaleidoscope then we should allow autoplay.
  if (IsOpenedFromKaleidoscope(handle))
    return true;

  // If the tab is Kaleidoscope then we should allow autoplay.
  auto parent_origin =
      url::Origin::Create(handle->GetWebContents()->GetLastCommittedURL());
  if (parent_origin.IsSameOriginWith(KaleidoscopeOrigin())) {
    return true;
  }

  return false;
}

}  // namespace

const char KaleidoscopeTabHelper::kKaleidoscopeNavigationHistogramName[] =
    "Media.Kaleidoscope.Navigation";

KaleidoscopeTabHelper::KaleidoscopeTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

KaleidoscopeTabHelper::~KaleidoscopeTabHelper() = default;

void KaleidoscopeTabHelper::ReadyToCommitNavigation(
    content::NavigationHandle* handle) {
  if (handle->IsSameDocument() || handle->IsErrorPage())
    return;

  RecordMetricsOnNavigation(handle);
  SetAutoplayOnNavigation(handle);

  if (IsOpenedFromKaleidoscope(handle)) {
    is_kaleidoscope_derived_ = true;
    return;
  }

  auto current_origin =
      url::Origin::Create(handle->GetWebContents()->GetLastCommittedURL());
  auto new_origin = url::Origin::Create(handle->GetURL());
  if (!current_origin.IsSameOriginWith(new_origin)) {
    is_kaleidoscope_derived_ = false;
  }
}

void KaleidoscopeTabHelper::RecordMetricsOnNavigation(
    content::NavigationHandle* handle) {
  // Only record metrics if this page was opened by Kaleidoscope.
  if (IsOpenedFromKaleidoscope(handle)) {
    base::UmaHistogramEnumeration(kKaleidoscopeNavigationHistogramName,
                                  KaleidoscopeNavigation::kNormal);

    ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
    if (!ukm_recorder)
      return;

    ukm::builders::Media_Kaleidoscope_Navigation(
        handle->GetNextPageUkmSourceId())
        .SetWasFromKaleidoscope(true)
        .Record(ukm_recorder);
  }
}

void KaleidoscopeTabHelper::SetAutoplayOnNavigation(
    content::NavigationHandle* handle) {
  if (!ShouldAllowAutoplay(handle))
    return;

  mojo::AssociatedRemote<blink::mojom::AutoplayConfigurationClient> client;
  handle->GetRenderFrameHost()->GetRemoteAssociatedInterfaces()->GetInterface(
      &client);
  client->AddAutoplayFlags(url::Origin::Create(handle->GetURL()),
                           blink::mojom::kAutoplayFlagUserException);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(KaleidoscopeTabHelper)
