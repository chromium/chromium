// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import android.view.View;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewDemoService;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewDemoServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabViewProvider;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.paintpreview.player.PlayerManager;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.widget.Toast;
import org.chromium.url.GURL;

/**
 * Responsible for displaying the Paint Preview demo. When displaying, the Paint Preview will
 * overlay the associated {@link Tab}'s content view.
 */
public class PaintPreviewDemoManager implements TabViewProvider {
    private Tab mTab;
    private PaintPreviewDemoService mPaintPreviewDemoService;
    private PlayerManager mPlayerManager;

    /**
     * This is called on all tab creations. We don't initialize the PaintPreviewDemoService here to
     * reduce overhead.
     */
    public PaintPreviewDemoManager(Tab tab) {
        mTab = tab;
    }

    public void showPaintPreviewDemo() {
        if (isShowingPaintPreviewDemo()) return;

        if (mPaintPreviewDemoService == null) {
            mPaintPreviewDemoService = PaintPreviewDemoServiceFactory.getServiceInstance();
        }
        mPaintPreviewDemoService.capturePaintPreview(mTab.getWebContents(), mTab.getId(),
                success
                -> PostTask.runOrPostTask(
                        UiThreadTaskTraits.USER_VISIBLE, () -> onCapturedPaintPreview(success)));
    }

    private void onCapturedPaintPreview(boolean success) {
        if (success) {
            mPlayerManager = new PlayerManager(mTab.getUrl(), mTab.getContext(),
                    mPaintPreviewDemoService, String.valueOf(mTab.getId()),
                    PaintPreviewDemoManager.this::onLinkClicked,
                    PaintPreviewDemoManager.this::removePaintPreviewDemo,
                    PaintPreviewDemoManager.this::addPlayerView, null, null,
                    ChromeColors.getPrimaryBackgroundColor(mTab.getContext().getResources(), false),
                    (status) -> {
                        Toast.makeText(mTab.getContext(),
                                     R.string.paint_preview_demo_playback_failure,
                                     Toast.LENGTH_LONG)
                                .show();
                        removePaintPreviewDemo();
                    },
                    /*ignoreInitialScrollOffset=*/false);
        }
        int toastStringRes = success ? R.string.paint_preview_demo_capture_success
                                     : R.string.paint_preview_demo_capture_failure;
        Toast.makeText(mTab.getContext(), toastStringRes, Toast.LENGTH_LONG).show();
    }

    private void addPlayerView() {
        mTab.getTabViewManager().addTabViewProvider(this);
    }

    void removePaintPreviewDemo() {
        PaintPreviewCompositorUtils.stopWarmCompositor();
        if (mTab == null || mPlayerManager == null) {
            return;
        }

        mTab.getTabViewManager().removeTabViewProvider(this);
        mPaintPreviewDemoService.cleanUpForTabId(mTab.getId());
        mPlayerManager.destroy();
        mPlayerManager = null;
    }

    boolean isShowingPaintPreviewDemo() {
        return mPlayerManager != null && mTab.getTabViewManager().isShowing(this);
    }

    private void onLinkClicked(GURL url) {
        if (mTab == null || !url.isValid() || url.isEmpty()) return;

        mTab.loadUrl(new LoadUrlParams(url.getSpec()));
    }

    public void destroy() {
        if (mPaintPreviewDemoService != null) {
            mPaintPreviewDemoService.cleanUpForTabId(mTab.getId());
        }
    }

    @Override
    public int getTabViewProviderType() {
        return Type.PAINT_PREVIEW;
    }

    @Override
    public View getView() {
        return mPlayerManager == null ? null : mPlayerManager.getView();
    }

    @Override
    public void onShown() {
        Toast.makeText(mTab.getContext(), R.string.paint_preview_demo_playback_start,
                     Toast.LENGTH_LONG)
                .show();
    }

    @Override
    public void onHidden() {
        Toast.makeText(
                     mTab.getContext(), R.string.paint_preview_demo_playback_end, Toast.LENGTH_LONG)
                .show();
    }
}
