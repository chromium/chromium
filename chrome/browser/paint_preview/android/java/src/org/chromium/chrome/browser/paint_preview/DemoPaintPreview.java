// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.paintpreview.player.PlayerManager;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.widget.Toast;
import org.chromium.url.GURL;

/**
 * Responsible for displaying the Paint Preview demo. When displaying, the Paint Preview will
 * overlay the associated {@link Tab}'s content view.
 */
public class DemoPaintPreview implements PlayerManager.Listener {
    private Tab mTab;
    private TabbedPaintPreview mTabbedPaintPreview;
    private DemoPaintPreviewTabObserver mTabObserver;

    public static void showForTab(Tab tab) {
        if (tab == null) return;

        new DemoPaintPreview(tab).show();
    }

    private DemoPaintPreview(Tab tab) {
        mTab = tab;
        mTabbedPaintPreview = TabbedPaintPreview.get(mTab);
        mTabObserver = new DemoPaintPreviewTabObserver();
        mTab.addObserver(mTabObserver);
    }

    private void show() {
        PaintPreviewCompositorUtils.warmupCompositor();
        mTabbedPaintPreview.capture(
                success ->
                        PostTask.runOrPostTask(
                                TaskTraits.UI_USER_VISIBLE, () -> onCapturedPaintPreview(success)));
    }

    private void onCapturedPaintPreview(boolean captureSuccess) {
        boolean shown = false;
        if (captureSuccess) shown = mTabbedPaintPreview.maybeShow(this);
        int toastStringRes =
                shown
                        ? R.string.paint_preview_demo_capture_success
                        : R.string.paint_preview_demo_capture_failure;
        Toast.makeText(mTab.getContext(), toastStringRes, Toast.LENGTH_LONG).show();
        if (!captureSuccess || !shown) {
            PaintPreviewCompositorUtils.stopWarmCompositor();
            destroy();
        }
    }

    private void removePaintPreviewDemo() {
        if (mTab == null) return;

        mTabbedPaintPreview.remove(false);
        destroy();
    }

    private void destroy() {
        mTab.removeObserver(mTabObserver);
        mTab = null;
        mTabbedPaintPreview = null;
    }

    @Override
    public void onCompositorError(int status) {
        Toast.makeText(
                        mTab.getContext(),
                        R.string.paint_preview_demo_playback_failure,
                        Toast.LENGTH_LONG)
                .show();
        removePaintPreviewDemo();
    }

    @Override
    public void onViewReady() {
        Toast.makeText(
                        mTab.getContext(),
                        R.string.paint_preview_demo_playback_start,
                        Toast.LENGTH_LONG)
                .show();
    }

    @Override
    public void onFirstPaint() {}

    @Override
    public void onUserInteraction() {}

    @Override
    public void onUserFrustration() {}

    @Override
    public void onPullToRefresh() {
        removePaintPreviewDemo();
    }

    @Override
    public void onLinkClick(GURL url) {
        if (mTab == null || !url.isValid() || url.isEmpty()) return;

        mTab.loadUrl(new LoadUrlParams(url.getSpec()));
        removePaintPreviewDemo();
    }

    @Override
    public boolean isAccessibilityEnabled() {
        return ChromeAccessibilityUtil.get().isAccessibilityEnabled();
    }

    @Override
    public void onAccessibilityNotSupported() {
        if (isAccessibilityEnabled()) {
            Toast.makeText(
                            mTab.getContext(),
                            R.string.paint_preview_demo_no_accessibility,
                            Toast.LENGTH_LONG)
                    .show();
        }
    }

    private class DemoPaintPreviewTabObserver extends EmptyTabObserver {
        @Override
        public void onDidStartNavigationInPrimaryMainFrame(
                Tab tab, NavigationHandle navigationHandle) {
            if (!mTabbedPaintPreview.isAttached()) return;
            removePaintPreviewDemo();
        }
    }
}
