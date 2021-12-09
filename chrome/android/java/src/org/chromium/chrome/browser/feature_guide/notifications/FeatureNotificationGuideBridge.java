// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feature_guide.notifications;

import androidx.annotation.StringRes;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.R;

/**
 * Contains JNI methods needed by the feature notification guide.
 */
@JNINamespace("feature_guide")
public final class FeatureNotificationGuideBridge implements FeatureNotificationGuideService {
    private long mNativeFeatureNotificationGuideBridge;

    @CalledByNative
    private static FeatureNotificationGuideBridge create(long nativePtr) {
        return new FeatureNotificationGuideBridge(nativePtr);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeFeatureNotificationGuideBridge = 0;
    }

    private FeatureNotificationGuideBridge(long nativePtr) {
        mNativeFeatureNotificationGuideBridge = nativePtr;
    }

    @CalledByNative
    private String getNotificationTitle(@FeatureType int featureType) {
        return ContextUtils.getApplicationContext().getResources().getString(
                R.string.feature_notification_guide_notification_title);
    }

    @CalledByNative
    private String getNotificationMessage(@FeatureType int featureType) {
        @StringRes
        int stringId = 0;
        switch (featureType) {
            case FeatureType.DEFAULT_BROWSER:
                stringId = R.string.feature_notification_guide_notification_message_default_browser;
                break;
            case FeatureType.SIGN_IN:
                stringId = R.string.feature_notification_guide_notification_message_sign_in;
                break;
            case FeatureType.INCOGNITO_TAB:
                stringId = R.string.feature_notification_guide_notification_message_incognito_tab;
                break;
            case FeatureType.NTP_SUGGESTION_CARD:
                stringId =
                        R.string.feature_notification_guide_notification_message_ntp_suggestion_card;
                break;
            case FeatureType.VOICE_SEARCH:
                stringId = R.string.feature_notification_guide_notification_message_voice_search;
                break;
            default:
                assert false : "Found unknown feature type " + featureType;
                break;
        }
        return ContextUtils.getApplicationContext().getResources().getString(stringId);
    }

    @CalledByNative
    private void onNotificationClick(@FeatureType int featureType) {}
}
