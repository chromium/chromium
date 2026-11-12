// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.util.ArrayMap;
import android.util.DisplayMetrics;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.background_tabs.OffscreenRenderingManager;
import org.chromium.chrome.browser.compositor.CompositorViewHolderSupplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.Map;

/**
 * Creates offscreen background tabs, configures their offscreen viewport dimensions, and waits for
 * initial {@code about:blank} page load completion.
 */
@NullMarked
final class OffscreenTabProvisioner {
    private static final String TAG = "ActorBackgroundMgr";

    // Map of triggering message ID to pending tab undergoing initial about:blank load.
    private final Map<String, Tab> mPendingActuationTabs = new ArrayMap<>();

    void setupBackgroundTab(
            Profile profile, String glicTriggerMessageId, Callback<@Nullable Tab> callback) {
        ThreadUtils.assertOnUiThread();
        Log.d(TAG, "Provisioning offscreen tab for message: %s", glicTriggerMessageId);
        Tab tab = createOffscreenTab(profile);
        mPendingActuationTabs.put(glicTriggerMessageId, tab);

        loadBlankThenCallback(
                tab,
                (preparedTab) -> {
                    if (!mPendingActuationTabs.containsKey(glicTriggerMessageId)) {
                        return;
                    }
                    callback.onResult(preparedTab);
                });
    }

    /** Transfers ownership of the provisioned tab to the caller once session setup succeeds. */
    void releasePendingTab(String glicTriggerMessageId) {
        mPendingActuationTabs.remove(glicTriggerMessageId);
    }

    /** Stops offscreen rendering and destroys a pending tab when setup is cancelled or fails. */
    boolean cancelPendingTab(String glicTriggerMessageId) {
        Tab pendingTab = mPendingActuationTabs.remove(glicTriggerMessageId);
        if (pendingTab != null) {
            OffscreenRenderingManager.getInstance().stopOffscreenRendering(pendingTab);
            pendingTab.destroy();
            return true;
        }
        return false;
    }

    void destroyPendingTabs() {
        for (Tab pendingTab : mPendingActuationTabs.values()) {
            OffscreenRenderingManager.getInstance().stopOffscreenRendering(pendingTab);
            pendingTab.destroy();
        }
        mPendingActuationTabs.clear();
    }

    Tab createOffscreenTab(Profile profile) {
        WindowAndroid window = OffscreenRenderingManager.getInstance().getOffscreenWindow();
        // TODO(crbug.com/548875143): Persist TabState to disk whenever the background tab's
        // URL/navigation updates so that on-disk state remains continuously accurate.
        Tab tab =
                TabBuilder.createLiveTab(profile, true)
                        .setWindow(window)
                        .setLaunchType(TabLaunchType.FROM_CHROME_UI)
                        .setDelegateFactory(new ActorTabDelegateFactory())
                        .build();
        startOffscreenRendering(tab);
        return tab;
    }

    void startOffscreenRendering(Tab tab) {
        View compositorView =
                CompositorViewHolderSupplier.getValueOrNullFrom(tab.getWindowAndroid());

        int width;
        int height;
        if (compositorView != null
                && compositorView.getWidth() > 0
                && compositorView.getHeight() > 0) {
            width = compositorView.getWidth();
            height = compositorView.getHeight();
        } else {
            // Fallback to display metrics might behave incorrectly for floating window or
            // split screen mode, but is sufficient for this use case.
            DisplayMetrics displayMetrics =
                    ContextUtils.getApplicationContext().getResources().getDisplayMetrics();
            width = displayMetrics.widthPixels;
            height = displayMetrics.heightPixels;
        }

        OffscreenRenderingManager.getInstance().startOffscreenRendering(tab, width, height);
    }

    void loadBlankThenCallback(Tab tab, Callback<@Nullable Tab> callback) {
        ThreadUtils.assertOnUiThread();
        TabObserver observer =
                new TabObserver() {
                    private boolean mInitialLoadFinished;

                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        Log.d(TAG, "Actor: Offscreen page load finished: %s", url.getSpec());
                        if (!mInitialLoadFinished) {
                            mInitialLoadFinished = true;
                            callback.onResult(tab);
                            tab.removeObserver(this);
                        }
                    }

                    @Override
                    public void onPageLoadFailed(Tab tab, int errorCode) {
                        Log.d(TAG, "Actor: Offscreen page load failed, error: %d", errorCode);
                        if (!mInitialLoadFinished) {
                            mInitialLoadFinished = true;
                            callback.onResult(null);
                            tab.removeObserver(this);
                        }
                    }

                    @Override
                    public void onCrash(Tab tab) {
                        Log.d(TAG, "Actor: Offscreen tab crashed");
                        if (!mInitialLoadFinished) {
                            mInitialLoadFinished = true;
                            callback.onResult(null);
                            tab.removeObserver(this);
                        }
                    }

                    @Override
                    public void onDestroyed(Tab tab) {
                        if (!mInitialLoadFinished) {
                            mInitialLoadFinished = true;
                            callback.onResult(null);
                        }
                        tab.removeObserver(this);
                    }
                };
        tab.addObserver(observer);
        tab.loadUrl(new LoadUrlParams("about:blank"));
    }
}
