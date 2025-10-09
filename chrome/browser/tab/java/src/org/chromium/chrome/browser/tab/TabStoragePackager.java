// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.nio.ByteBuffer;

/** Saves Java-accessible tab data for use in C++. */
@JNINamespace("tabs")
@NullMarked
public class TabStoragePackager {
    private final long mNativeTabStoragePackager;

    private TabStoragePackager(long nativeTabStoragePackager) {
        mNativeTabStoragePackager = nativeTabStoragePackager;
    }

    @CalledByNative
    private static TabStoragePackager create(long nativeTabStoragePackager) {
        return new TabStoragePackager(nativeTabStoragePackager);
    }

    @CalledByNative
    public void packageTab(@JniType("const TabAndroid*") Tab tab) {
        WebContentsState state = TabStateExtractor.getWebContentsState(tab);
        TabStoragePackagerJni.get()
                .consolidatePackageData(
                        mNativeTabStoragePackager,
                        tab.getTimestampMillis(),
                        state == null ? null : state.buffer(),
                        assumeNonNull(TabAssociatedApp.getAppId(tab)),
                        tab.getThemeColor(),
                        tab.getLastNavigationCommittedTimestampMillis(),
                        tab.getTabHasSensitiveContent(),
                        tab);
    }

    @NativeMethods
    interface Natives {
        void consolidatePackageData(
                long nativeTabStoragePackagerAndroid,
                long timestampMillis,
                @Nullable ByteBuffer webContentsStateBuffer,
                @Nullable @JniType("std::string") String openerAppId,
                int themeColor,
                long lastNavigationCommittedTimestampMillis,
                boolean tabHasSensitiveContent,
                @JniType("TabAndroid*") Tab tab);
    }
}
