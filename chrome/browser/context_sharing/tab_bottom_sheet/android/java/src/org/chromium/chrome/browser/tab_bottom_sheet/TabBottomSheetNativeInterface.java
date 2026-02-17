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
import org.chromium.content_public.browser.WebContents;

/** Interface for native methods to interact with the tab bottom sheet. */
@NullMarked
public class TabBottomSheetNativeInterface implements NativeInterfaceDelegate {

    private final long mNativePtr;
    private final Tab mTab;

    /** Constructor. */
    @CalledByNative
    private TabBottomSheetNativeInterface(long nativePtr, Tab tab) {
        mNativePtr = nativePtr;
        mTab = tab;
    }

    @CalledByNative
    private void destroy() {
        TabBottomSheetManager tabBottomSheetManager = getTabBottomSheetManager(mTab);
        if (tabBottomSheetManager != null) {
            tabBottomSheetManager.detachNativeInterfaceDelegate(this);
        }
    }

    // Native calls for glic.
    @CalledByNative
    public boolean show() {
        TabBottomSheetManager tabBottomSheetManager = getTabBottomSheetManager(mTab);
        if (tabBottomSheetManager != null) {
            return tabBottomSheetManager.tryToShowBottomSheet(
                    /* nativeInterfaceDelegate= */ this,
                    /* shouldShowToolbar= */ false,
                    /* shouldShowFusebox= */ true);
        }
        return false;
    }

    @CalledByNative
    public void close() {
        TabBottomSheetManager tabBottomSheetManager = getTabBottomSheetManager(mTab);
        if (tabBottomSheetManager != null) {
            tabBottomSheetManager.tryToCloseBottomSheet();
        }
    }

    @CalledByNative
    public boolean setWebContents(WebContents webContents) {
        TabBottomSheetManager tabBottomSheetManager = getTabBottomSheetManager(mTab);
        if (tabBottomSheetManager != null) {
            return tabBottomSheetManager.setWebContents(webContents);
        }
        return false;
    }

    public @Nullable WebContents getWebContents() {
        TabBottomSheetManager tabBottomSheetManager = getTabBottomSheetManager(mTab);
        return tabBottomSheetManager != null ? tabBottomSheetManager.getWebContents() : null;
    }

    private @Nullable TabBottomSheetManager getTabBottomSheetManager(Tab tab) {
        return TabBottomSheetUtils.getManagerFromWindow(assumeNonNull(tab.getWindowAndroid()));
    }

    // Delegate methods.
    @Override
    public long getRequestId() {
        return mNativePtr;
    }

    @Override
    public void onBottomSheetClosed() {
        TabBottomSheetNativeInterfaceJni.get().onClose(mNativePtr);
    }

    @NativeMethods
    interface Natives {
        void onClose(long nativeGlicSidePanelCoordinatorAndroid);
    }
}
