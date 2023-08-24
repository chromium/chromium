// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.animation.Animator;
import android.content.Context;

import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.features.tasks.TasksView;

/**
 * A {@link Layout} that shows Start Surface home view.
 */
public class StartSurfaceHomeLayout extends Layout {
    private static final String TRACE_SHOW_START_SURFACE =
            "StartSurfaceHomeLayout.Show.StartSurface";
    private static final String TRACE_HIDE_START_SURFACE =
            "StartSurfaceHomeLayout.Hide.StartSurface";
    private static final String TRACE_DONE_SHOWING_START_SURFACE =
            "StartSurfaceHomeLayout.DoneShowing";
    private static final String TRACE_DONE_HIDING_START_SURFACE =
            "StartSurfaceHomeLayout.DoneHiding";

    private final StartSurface mStartSurface;

    private boolean mIsShown;
    private boolean mIsInitialized;
    private Animator mBackgroundTabAnimation;
    private SceneLayer mSceneLayer;

    /**
     * The {@link Layout} is not usable until sizeChanged is called. This is convenient this way so
     * we can pre-create the layout before the host is fully defined.
     *
     * @param context The current Android's context.
     * @param updateHost The parent {@link LayoutUpdateHost}.
     * @param renderHost The parent {@link LayoutRenderHost}.
     */
    public StartSurfaceHomeLayout(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, StartSurface startSurface) {
        super(context, updateHost, renderHost);
        mStartSurface = startSurface;
        mStartSurface.setOnTabSelectingListener(this::onTabSelecting);
    }

    @Override
    public void onFinishNativeInitialization() {
        if (mIsInitialized) return;
        mIsInitialized = true;
        ensureSceneLayerCreated();
        mStartSurface.initWithNative();
    }

    @Override
    protected void updateLayout(long time, long dt) {
        ensureSceneLayerCreated();
        super.updateLayout(time, dt);
    }

    @Override
    public void destroy() {}

    @Override
    public void show(long time, boolean animate) {
        try (TraceEvent e = TraceEvent.scoped(TRACE_SHOW_START_SURFACE)) {
            super.show(time, animate);

            // Lazy initialization if needed.
            mStartSurface.initialize();
            mStartSurface.show(animate);

            mIsShown = true;
            doneShowing();
        }
    }

    @Override
    public void startHiding(int nextTabId, boolean hintAtTabSelection) {
        try (TraceEvent e = TraceEvent.scoped(TRACE_HIDE_START_SURFACE)) {
            super.startHiding(nextTabId, hintAtTabSelection);
            mIsShown = false;
            mStartSurface.hide(false);
            doneHiding();
        }
    }

    @Override
    public void doneHiding() {
        try (TraceEvent e = TraceEvent.scoped(TRACE_DONE_HIDING_START_SURFACE)) {
            super.doneHiding();
        }
    }

    @Override
    public void doneShowing() {
        try (TraceEvent e = TraceEvent.scoped(TRACE_DONE_SHOWING_START_SURFACE)) {
            super.doneShowing();
        }
    }

    @Override
    public boolean onBackPressed() {
        return mStartSurface.onBackPressed();
    }

    @Override
    protected EventFilter getEventFilter() {
        return null;
    }

    @Override
    protected SceneLayer getSceneLayer() {
        return mSceneLayer;
    }

    @Override
    public void onTabCreated(long time, int id, int index, int sourceId, boolean newIsIncognito,
            boolean background, float originX, float originY) {
        super.onTabCreated(time, id, index, sourceId, newIsIncognito, background, originX, originY);
        if (!background || newIsIncognito || !mIsShown) {
            return;
        }
        TasksView startSurfaceView = mStartSurface.getPrimarySurfaceView();
        assert startSurfaceView != null;

        if (mBackgroundTabAnimation != null && mBackgroundTabAnimation.isStarted()) {
            mBackgroundTabAnimation.end();
        }
        float dpToPx = getContext().getResources().getDisplayMetrics().density;
        mBackgroundTabAnimation = BackgroundTabAnimation.create(this, startSurfaceView,
                originX * dpToPx, originY * dpToPx, getOrientation() == Orientation.PORTRAIT);
        mBackgroundTabAnimation.start();
    }

    public StartSurface getStartSurfaceForTesting() {
        return mStartSurface;
    }

    @Override
    public int getLayoutType() {
        return LayoutType.START_SURFACE;
    }

    private void ensureSceneLayerCreated() {
        if (mSceneLayer != null) return;
        mSceneLayer = new SceneLayer();
    }
}
