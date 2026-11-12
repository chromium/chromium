// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

@JNINamespace("variations::android")
@NullMarked
public final class InitializeFeatureList {

    private InitializeFeatureList() {}

    public static void setupBackgroundThreadPoolFieldTrial() {
        if (CommandLine.getInstance().hasSwitch(BaseSwitches.BACKGROUND_THREAD_POOL_FIELD_TRIAL)) {
            return;
        }
        if (!ChromeFeatureList.sBackgroundThreadPoolFieldTrial.isEnabled()) {
            return;
        }
        int configValue = ChromeFeatureList.sBackgroundThreadPoolFieldTrialConfig.getValue();
        if (configValue > 0) {
            CommandLine.getInstance()
                    .appendSwitchWithValue(
                            BaseSwitches.BACKGROUND_THREAD_POOL_FIELD_TRIAL,
                            String.valueOf(configValue));
        }
    }

    /**
     * Creates the ThreadPool, BrowserTaskExecutor, and Local State, but not the FeatureList. Native
     * finishes initialization during content main.
     */
    public static void initializeLocalState() {
        setupBackgroundThreadPoolFieldTrial();
        InitializeFeatureListJni.get().initializeLocalState();
    }

    /** Creates the ThreadPool, BrowserTaskExecutor, Local State, and the FeatureList. */
    public static void initializeFeatureList() {
        setupBackgroundThreadPoolFieldTrial();
        InitializeFeatureListJni.get().initializeFeatureList();
    }

    @NativeMethods
    interface Natives {
        void initializeLocalState();

        void initializeFeatureList();
    }
}
