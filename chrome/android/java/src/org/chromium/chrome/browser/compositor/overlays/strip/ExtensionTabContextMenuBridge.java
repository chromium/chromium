// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
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
    @JniType("::extensions::ExtensionTabContextMenuBridge")
    interface NativeExtensionTabContextMenuBridge extends JniTypeToken {}

    private @Nullable JniUniquePtr<NativeExtensionTabContextMenuBridge>
            mNativeExtensionTabContextMenuBridge;
    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    public static @Nullable ExtensionTabContextMenuBridge create(WebContents webContents) {
        JniUniquePtr<NativeExtensionTabContextMenuBridge> nativePtr =
                ExtensionTabContextMenuBridgeJni.get().init(webContents);
        if (nativePtr == null) {
            return null;
        }
        return new ExtensionTabContextMenuBridge(nativePtr);
    }

    private ExtensionTabContextMenuBridge(
            JniUniquePtr<NativeExtensionTabContextMenuBridge> nativePtr) {
        mNativeExtensionTabContextMenuBridge = nativePtr;
    }

    /** Returns the {@link ModelList} of extension menu items for this tab. */
    public ModelList getModelList() {
        if (mNativeExtensionTabContextMenuBridge == null) {
            return new ModelList();
        }
        MenuModelBridge menuModelBridge =
                ExtensionTabContextMenuBridgeJni.get()
                        .getMenuModelBridge(mNativeExtensionTabContextMenuBridge);
        return menuModelBridge != null ? menuModelBridge.populateModelList() : new ModelList();
    }

    @Override
    public void destroy() {
        if (mNativeExtensionTabContextMenuBridge != null) {
            mNativeExtensionTabContextMenuBridge.destroy();
            mNativeExtensionTabContextMenuBridge = null;
            LifetimeAssert.destroy(mLifetimeAssert);
        }
    }

    @NativeMethods
    interface Native {
        @Nullable JniUniquePtr<NativeExtensionTabContextMenuBridge> init(
                @JniType("content::WebContents*") WebContents webContents);

        @Nullable MenuModelBridge getMenuModelBridge(
                JniPtr<NativeExtensionTabContextMenuBridge> self);
    }
}
