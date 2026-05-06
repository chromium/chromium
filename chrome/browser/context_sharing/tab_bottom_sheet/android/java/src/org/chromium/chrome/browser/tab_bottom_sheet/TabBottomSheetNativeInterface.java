// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetManager.NativeInterfaceDelegate;

/** Interface for native methods to interact with the tab bottom sheet. */
@NullMarked
public class TabBottomSheetNativeInterface implements NativeInterfaceDelegate {

    private final Tab mTab;
    private long mNativeTabBottomSheetBridge;

    /** Constructor. */
    @CalledByNative
    private TabBottomSheetNativeInterface(long nativeTabBottomSheetBridge, Tab tab) {
        mNativeTabBottomSheetBridge = nativeTabBottomSheetBridge;
        mTab = tab;
    }

    @CalledByNative
    private void destroy() {
        mNativeTabBottomSheetBridge = 0;
        var tabBottomSheetManager = getTabBottomSheetManager(mTab);
        if (tabBottomSheetManager != null) {
            tabBottomSheetManager.detachNativeInterfaceDelegate(this);
        }
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
        if (mNativeTabBottomSheetBridge == 0) return;
        TabBottomSheetNativeInterfaceJni.get().onClosed(mNativeTabBottomSheetBridge);
    }

    @Override
    public void onBottomSheetSuppressed() {
        if (mNativeTabBottomSheetBridge == 0) return;
        TabBottomSheetNativeInterfaceJni.get().onSuppressed(mNativeTabBottomSheetBridge);
    }

    @Override
    public void onBottomSheetOpened(boolean isExpanded) {
        if (mNativeTabBottomSheetBridge == 0) return;
        TabBottomSheetNativeInterfaceJni.get().onOpened(mNativeTabBottomSheetBridge, isExpanded);
    }

    @NativeMethods
    interface Natives {
        void onClosed(long nativeTabBottomSheetBridge);

        void onSuppressed(long nativeTabBottomSheetBridge);

        void onOpened(long nativeTabBottomSheetBridge, boolean isExpanded);
    }
}
