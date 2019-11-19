// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.native_page;

import android.view.View;
import android.widget.FrameLayout.LayoutParams;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBrowserControlsState;
import org.chromium.chrome.browser.ui.styles.ChromeColors;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.BrowserControlsState;

/**
 * A basic implementation of a white {@link NativePage} that docks below the toolbar.
 */
public abstract class BasicNativePage
        extends EmptyTabObserver implements NativePage, ChromeFullscreenManager.FullscreenListener {
    private final NativePageHost mHost;
    private final ChromeFullscreenManager mFullscreenManager;
    private final int mBackgroundColor;

    private String mUrl;

    public BasicNativePage(ChromeActivity activity, NativePageHost host) {
        initialize(activity, host);
        mHost = host;
        mBackgroundColor = ChromeColors.getPrimaryBackgroundColor(activity.getResources(), false);

        mFullscreenManager = activity.getFullscreenManager();
        mFullscreenManager.addListener(this);

        updateMargins();

        if (host.getActiveTab() != null) host.getActiveTab().addObserver(this);
    }

    /**
     * Subclasses shall implement this method to initialize the UI that they hold.
     */
    protected abstract void initialize(ChromeActivity activity, NativePageHost host);

    @Override
    public void onBrowserControlsConstraintsUpdated(
            Tab tab, @BrowserControlsState int constraints) {
        updateMargins();
    }

    @Override
    public abstract View getView();

    @Override
    public String getUrl() {
        return mUrl;
    }

    @Override
    public int getBackgroundColor() {
        return mBackgroundColor;
    }

    @Override
    public boolean needsToolbarShadow() {
        return true;
    }

    @Override
    public void updateForUrl(String url) {
        mUrl = url;
    }

    @Override
    public void destroy() {
        if (mHost.getActiveTab() == null) return;
        mHost.getActiveTab().removeObserver(this);
        mFullscreenManager.removeListener(this);
    }

    @Override
    public void onContentOffsetChanged(int offset) {}

    @Override
    public void onControlsOffsetChanged(int topOffset, int bottomOffset, boolean needsAnimate) {}

    @Override
    public void onToggleOverlayVideoMode(boolean enabled) {}

    @Override
    public void onBottomControlsHeightChanged(int bottomControlsHeight) {
        updateMargins();
    }

    /**
     * Tells the native page framework that the url should be changed.
     * @param url The URL of the page.
     * @param replaceLastUrl Whether the last navigation entry should be replaced with the new URL.
     */
    public void onStateChange(String url, boolean replaceLastUrl) {
        if (url.equals(mUrl)) return;
        LoadUrlParams params = new LoadUrlParams(url);
        params.setShouldReplaceCurrentEntry(replaceLastUrl);
        mHost.loadUrl(params, /* incognito = */ false);
    }

    /**
     * Updates the top margin depending on whether the browser controls are shown or hidden.
     */
    private void updateMargins() {
        final @BrowserControlsState int constraints = mHost.getActiveTab() != null
                ? TabBrowserControlsState.getConstraints(mHost.getActiveTab())
                : BrowserControlsState.SHOWN;
        int topMargin = mFullscreenManager.getTopControlsHeight();
        int bottomMargin = mFullscreenManager.getBottomControlsHeight();
        if (constraints == BrowserControlsState.HIDDEN) {
            topMargin = 0;
            bottomMargin = 0;
        }
        LayoutParams layoutParams =
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        layoutParams.setMargins(0, topMargin, 0, bottomMargin);
        getView().setLayoutParams(layoutParams);
    }
}
