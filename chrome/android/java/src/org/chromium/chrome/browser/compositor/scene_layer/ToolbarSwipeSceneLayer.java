// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.scene_layer;

import android.content.Context;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;

/** A SceneLayer that displays one or two tab content layers for toolbar swipe functionality. */
@JNINamespace("android")
public class ToolbarSwipeSceneLayer extends SceneLayer {
    private final Context mContext;
    private long mNativePtr;

    public ToolbarSwipeSceneLayer(Context context, TabContentManager tabContentManager) {
        mContext = context;
        ToolbarSwipeSceneLayerJni.get().setTabContentManager(mNativePtr, this, tabContentManager);
    }

    public void update(LayoutTab tab, boolean isLeftTab, int backgroundColor) {
        if (tab == null) return;

        final float dpToPx = mContext.getResources().getDisplayMetrics().density;

        ToolbarSwipeSceneLayerJni.get().updateLayer(mNativePtr, this, tab.get(LayoutTab.TAB_ID),
                isLeftTab, tab.get(LayoutTab.CAN_USE_LIVE_TEXTURE), backgroundColor,
                tab.get(LayoutTab.X) * dpToPx, tab.get(LayoutTab.Y) * dpToPx);
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = ToolbarSwipeSceneLayerJni.get().init(this);
        }
        assert mNativePtr != 0;
    }

    @NativeMethods
    interface Natives {
        long init(ToolbarSwipeSceneLayer caller);
        void setTabContentManager(long nativeToolbarSwipeSceneLayer, ToolbarSwipeSceneLayer caller,
                TabContentManager tabContentManager);
        void updateLayer(long nativeToolbarSwipeSceneLayer, ToolbarSwipeSceneLayer caller, int id,
                boolean leftTab, boolean canUseLiveLayer, int defaultBackgroundColor, float x,
                float y);
    }
}
