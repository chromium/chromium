// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.listmenu.MenuModelBridge;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/**
 * Bridge that provides extension context menu items for tabs on Android. Note that this is included
 * whether or not extensions are enabled on the build; the native bridge becomes a no-op in cases
 * that extensions aren't supported.
 */
@NullMarked
@JNINamespace("extensions")
public class ExtensionTabContextMenuBridge implements Destroyable {
    private long mNativeExtensionTabContextMenuBridge;
    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    public static @Nullable ExtensionTabContextMenuBridge create(WebContents webContents) {
        long nativePtr = ExtensionTabContextMenuBridgeJni.get().init(webContents);
        if (nativePtr == 0) {
            return null;
        }
        return new ExtensionTabContextMenuBridge(nativePtr);
    }

    private ExtensionTabContextMenuBridge(long nativePtr) {
        mNativeExtensionTabContextMenuBridge = nativePtr;
    }

    /** Returns the {@link ModelList} of extension menu items for this tab. */
    public ModelList getModelList() {
        if (mNativeExtensionTabContextMenuBridge == 0) {
            return new ModelList();
        }
        MenuModelBridge menuModelBridge =
                ExtensionTabContextMenuBridgeJni.get()
                        .getMenuModelBridge(mNativeExtensionTabContextMenuBridge);
        return menuModelBridge != null ? menuModelBridge.populateModelList() : new ModelList();
    }

    @Override
    public void destroy() {
        if (mNativeExtensionTabContextMenuBridge != 0) {
            ExtensionTabContextMenuBridgeJni.get().destroy(mNativeExtensionTabContextMenuBridge);
            mNativeExtensionTabContextMenuBridge = 0;
            LifetimeAssert.destroy(mLifetimeAssert);
        }
    }

    @NativeMethods
    public interface Native {
        long init(@JniType("content::WebContents*") WebContents webContents);

        @Nullable MenuModelBridge getMenuModelBridge(long nativeExtensionTabContextMenuBridge);

        void destroy(long nativeExtensionTabContextMenuBridge);
    }
}
