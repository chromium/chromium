// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/kaleidoscope/kaleidoscope_tab_helper.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/media/kaleidoscope/constants.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom.h"

namespace {

const url::Origin& KaleidoscopeOrigin() {
  static base::NoDestructor<url::Origin> origin(
      url::Origin::Create(GURL(kKaleidoscopeUntrustedContentUIURL)));
  return *origin;
}

const char kKaleidoscopeNavigationHistogramName[] =
    "Media.Kaleidoscope.Navigation";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class KaleidoscopeNavigation {
  kNormal = 0,
  kMaxValue = kNormal,
};

}  // namespace

KaleidoscopeTabHelper::KaleidoscopeTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

KaleidoscopeTabHelper::~KaleidoscopeTabHelper() = default;

void KaleidoscopeTabHelper::ReadyToCommitNavigation(
    content::NavigationHandle* handle) {
  if (!handle->IsInMainFrame() || handle->IsSameDocument() ||
      handle->IsErrorPage()) {
    return;
  }

  if (!handle->GetInitiatorOrigin() ||
      !handle->GetInitiatorOrigin()->IsSameOriginWith(KaleidoscopeOrigin())) {
    return;
  }

  mojo::AssociatedRemote<blink::mojom::AutoplayConfigurationClient> client;
  handle->GetRenderFrameHost()->GetRemoteAssociatedInterfaces()->GetInterface(
      &client);
  client->AddAutoplayFlags(url::Origin::Create(handle->GetURL()),
                           blink::mojom::kAutoplayFlagUserException);

  base::UmaHistogramEnumeration(kKaleidoscopeNavigationHistogramName,
                                KaleidoscopeNavigation::kNormal);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(KaleidoscopeTabHelper)
