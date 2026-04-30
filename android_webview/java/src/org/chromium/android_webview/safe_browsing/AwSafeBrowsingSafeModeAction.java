// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.safe_browsing;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.SafeModeAction;
import org.chromium.android_webview.common.SafeModeActionIds;
import org.chromium.android_webview.common.SafeModeController;
import org.chromium.build.annotations.NullMarked;

/**
 * A {@link SafeModeAction} to disable safe browsing.
 *
 * <p>This action itself is a NOOP. The actual work is done in 2 places:
 * AwUrlCheckerDelegateImpl.ShouldSkipRequestCheck skips safe browsing checks for URL loads.
 * AwContentsStatics.initSafeBrowsing skips GMSCore communication in safe browsing initialization.
 */
@JNINamespace("android_webview")
@Lifetime.Singleton
@NullMarked
public class AwSafeBrowsingSafeModeAction extends SafeModeAction {
    // This ID should not be changed or reused.
    private static final String ID = SafeModeActionIds.DISABLE_AW_SAFE_BROWSING;

    @Override
    public String getId() {
        return ID;
    }

    @CalledByNative
    public static boolean isSafeBrowsingDisabled() {
        return SafeModeController.getInstance()
                .isActionEnabled(SafeModeActionIds.DISABLE_AW_SAFE_BROWSING);
    }
}
