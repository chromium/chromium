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
    private Delegate mDelegate;

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

    @Override
    public void setDelegate(Delegate delegate) {
        assert mDelegate == null;
        mDelegate = delegate;
    }

    @CalledByNative
    private String getNotificationTitle(int featureType) {
        return mDelegate.getNotificationTitle(featureType);
    }

    @CalledByNative
    private String getNotificationMessage(int featureType) {
        return mDelegate.getNotificationMessage(featureType);
    }

    @CalledByNative
    private void onNotificationClick(int featureType) {
        mDelegate.onNotificationClick(featureType);
    }
}
