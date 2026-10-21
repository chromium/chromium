// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetManager.NativeInterfaceDelegate;
import org.chromium.ui.base.WindowAndroid;

/** Interface for native methods to interact with the tab bottom sheet. */
@NullMarked
public class TabBottomSheetNativeInterface implements NativeInterfaceDelegate {
    @JniType("::context_sharing::TabBottomSheetBridge")
    interface NativeTabBottomSheetBridge extends JniTypeToken {}

    private @Nullable Tab mTab;
    private @Nullable JniRawPtr<NativeTabBottomSheetBridge> mNativeTabBottomSheetBridge;

    /** Constructor. */
    @CalledByNative
    private TabBottomSheetNativeInterface(
            JniRawPtr<NativeTabBottomSheetBridge> nativeTabBottomSheetBridge, Tab tab) {
        mNativeTabBottomSheetBridge = nativeTabBottomSheetBridge;
        mTab = tab;
    }

    @CalledByNative
    private void destroy() {
        if (mNativeTabBottomSheetBridge != null) {
            mNativeTabBottomSheetBridge.release();
            mNativeTabBottomSheetBridge = null;
        }
        var tabBottomSheetManager = getTabBottomSheetManager(mTab);
        if (tabBottomSheetManager != null) {
            tabBottomSheetManager.detachNativeInterfaceDelegate(this);
        }
        mTab = null;
    }

    /**
     * Checks if a TabBottomSheetManager instance is initialized and bound to the tab's current
     * WindowAndroid.
     */
    @CalledByNative
    public boolean isManagerReady() {
        return getTabBottomSheetManager(mTab) != null;
    }

    // Native calls for glic.
    @CalledByNative
    public boolean show(CoBrowseViews coBrowseViews, boolean animate, boolean startsExpanded) {
        var tabBottomSheetManager = getTabBottomSheetManager(mTab);
        if (tabBottomSheetManager != null && coBrowseViews != null) {
            return tabBottomSheetManager.tryToShowBottomSheet(
                    this, coBrowseViews, animate, startsExpanded);
        }
        return false;
    }

    @CalledByNative
    public void close(boolean animate) {
        var tabBottomSheetManager = getTabBottomSheetManager(mTab);
        if (tabBottomSheetManager != null) {
            tabBottomSheetManager.tryToCloseBottomSheet(animate);
        }
    }

    @CalledByNativeForTesting
    public void suppressBottomSheetForTesting(boolean suppress) {
        var tabBottomSheetManager = getTabBottomSheetManager(mTab);
        if (tabBottomSheetManager != null) {
            tabBottomSheetManager.suppressBottomSheetForTesting(suppress); // IN-TEST
        }
    }

    private @Nullable TabBottomSheetManagerImpl getTabBottomSheetManager(@Nullable Tab tab) {
        if (tab == null) {
            return null;
        }
        return (TabBottomSheetManagerImpl)
                TabBottomSheetUtils.getManagerFromWindow(assumeNonNull(tab.getWindowAndroid()));
    }

    // Delegate methods.
    @Override
    public void onBottomSheetClosed() {
        if (mNativeTabBottomSheetBridge == null) return;
        TabBottomSheetNativeInterfaceJni.get().onClosed(mNativeTabBottomSheetBridge);
    }

    @Override
    public void onBottomSheetSuppressed() {
        if (mNativeTabBottomSheetBridge == null) return;
        TabBottomSheetNativeInterfaceJni.get().onSuppressed(mNativeTabBottomSheetBridge);
    }

    @Override
    public void onBottomSheetOpened(boolean isExpanded) {
        if (mNativeTabBottomSheetBridge == null) return;
        TabBottomSheetNativeInterfaceJni.get().onOpened(mNativeTabBottomSheetBridge, isExpanded);
    }

    @NativeMethods
    interface Natives {
        void onClosed(JniPtr<NativeTabBottomSheetBridge> self);

        void onSuppressed(JniPtr<NativeTabBottomSheetBridge> self);

        void onOpened(JniPtr<NativeTabBottomSheetBridge> self, boolean isExpanded);

        void onManagerInitialized(@JniType("ui::WindowAndroid*") WindowAndroid windowAndroid);
    }
}
