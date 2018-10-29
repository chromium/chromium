// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/use_counter/ukm_features.h"

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"

using WebFeature = blink::mojom::WebFeature;

// UKM-based UseCounter features (WebFeature) should be defined in
// opt_in_features list.
bool IsAllowedUkmFeature(blink::mojom::WebFeature feature) {
  static base::NoDestructor<base::flat_set<WebFeature>> opt_in_features(
      base::flat_set<WebFeature>({
          WebFeature::kNavigatorVibrate, WebFeature::kNavigatorVibrateSubFrame,
          WebFeature::kTouchEventPreventedNoTouchAction,
          WebFeature::kTouchEventPreventedForcedDocumentPassiveNoTouchAction,
          // kDataUriHasOctothorpe may not be recorded correctly for iframes.
          // See https://crbug.com/796173 for details.
          WebFeature::kDataUriHasOctothorpe,
          WebFeature::kApplicationCacheManifestSelectInsecureOrigin,
          WebFeature::kApplicationCacheManifestSelectSecureOrigin,
          WebFeature::kMixedContentAudio, WebFeature::kMixedContentImage,
          WebFeature::kMixedContentVideo, WebFeature::kMixedContentPlugin,
          WebFeature::kOpenerNavigationWithoutGesture,
          WebFeature::kUsbRequestDevice, WebFeature::kXMLHttpRequestSynchronous,
          WebFeature::kPaymentHandler,
          WebFeature::kPaymentRequestShowWithoutGesture,
          WebFeature::kHTMLImports, WebFeature::kHTMLImportsHasStyleSheets,
          WebFeature::kElementCreateShadowRoot,
          WebFeature::kDocumentRegisterElement,
          WebFeature::kCredentialManagerCreatePublicKeyCredential,
          WebFeature::kCredentialManagerGetPublicKeyCredential,
          WebFeature::kCredentialManagerMakePublicKeyCredentialSuccess,
          WebFeature::kCredentialManagerGetPublicKeyCredentialSuccess,
          WebFeature::kV8AudioContext_Constructor,
          WebFeature::kElementAttachShadow,
          WebFeature::kElementAttachShadowOpen,
          WebFeature::kElementAttachShadowClosed,
          WebFeature::kCustomElementRegistryDefine,
          WebFeature::kTextToSpeech_Speak,
          WebFeature::kTextToSpeech_SpeakDisallowedByAutoplay,
          WebFeature::kCSSEnvironmentVariable,
          WebFeature::kCSSEnvironmentVariable_SafeAreaInsetTop,
          WebFeature::kCSSEnvironmentVariable_SafeAreaInsetLeft,
          WebFeature::kCSSEnvironmentVariable_SafeAreaInsetRight,
          WebFeature::kCSSEnvironmentVariable_SafeAreaInsetBottom,
          WebFeature::kMediaControlsDisplayCutoutGesture,
          WebFeature::kPolymerV1Detected, WebFeature::kPolymerV2Detected,
          WebFeature::kFullscreenSecureOrigin,
          WebFeature::kFullscreenInsecureOrigin,
          WebFeature::kPrefixedVideoEnterFullscreen,
          WebFeature::kPrefixedVideoExitFullscreen,
          WebFeature::kPrefixedVideoEnterFullScreen,
          WebFeature::kPrefixedVideoExitFullScreen,
          WebFeature::kDocumentLevelPassiveDefaultEventListenerPreventedWheel,
          WebFeature::kDocumentDomainBlockedCrossOriginAccess,
          WebFeature::kDocumentDomainEnabledCrossOriginAccess,
          WebFeature::kSuppressHistoryEntryWithoutUserGesture,
          WebFeature::kCursorImageGT32x32, WebFeature::kCursorImageLE32x32,
      }));
  return opt_in_features->count(feature);
}
