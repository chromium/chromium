// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import androidx.annotation.AnyThread;
import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Helper class that provides a test or production instance for {@link ParentAuthDelegate}. */
@NullMarked
public class ParentAuthDelegateProvider {
    private static @Nullable ParentAuthDelegate sInstance;
    private static @Nullable ParentAuthDelegate sTestingInstance;

    /**
     * Sets the test instance. Can be called multiple times to change the instance
     * during testing.
     */
    @VisibleForTesting
    @AnyThread
    public static void setInstanceForTests(ParentAuthDelegate parentAuthDelegate) {
        // TODO(b/243916194): Change to the recommended alternative for deprecated method.
        ThreadUtils.runOnUiThread(
                () -> {
                    sTestingInstance = parentAuthDelegate;
                });
    }

    /** Returns singleton instance. */
    @MainThread
    public static @Nullable ParentAuthDelegate getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sTestingInstance != null) {
            return sTestingInstance;
        }
        if (sInstance == null) {
            sInstance = ServiceLoaderUtil.maybeCreate(ParentAuthDelegate.class);
        }
        return sInstance;
    }
}
