// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feature_guide.notifications;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/** Contains JNI methods needed by the feature notification guide. */
@JNINamespace("feature_guide")
public final class FeatureNotificationGuideBridge extends FeatureNotificationGuideService {
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
        return FeatureNotificationUtils.getNotificationTitle(featureType);
    }

    @CalledByNative
    private String getNotificationMessage(@FeatureType int featureType) {
        return FeatureNotificationUtils.getNotificationMessage(featureType);
    }

    @CalledByNative
    private void onNotificationClick(@FeatureType int featureType) {
        FeatureNotificationGuideService.getDelegate().launchActivityToShowIph(featureType);
    }

    @CalledByNative
    private void closeNotification(String notificationGuid) {
        FeatureNotificationUtils.closeNotification(notificationGuid);
    }

    @CalledByNative
    private boolean shouldSkipFeature(@FeatureType int featureType) {
        return FeatureNotificationUtils.shouldSkipFeature(featureType);
    }

    @CalledByNative
    private String getNotificationParamGuidForFeature(@FeatureType int featureType) {
        return FeatureNotificationUtils.getNotificationParamGuidForFeature(featureType);
    }
}
