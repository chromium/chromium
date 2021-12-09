// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feature_guide.notifications;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

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
        return null;
    }

    @CalledByNative
    private String getNotificationMessage(@FeatureType int featureType) {
        return null;
    }

    @CalledByNative
    private void onNotificationClick(@FeatureType int featureType) {}
}
