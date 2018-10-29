// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.ComponentName;
import android.content.Intent;
import android.provider.Browser;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.widget.ControlContainer;
import org.chromium.content_public.browser.ScreenOrientationProvider;
import org.chromium.content_public.browser.WebContentsObserver;

/**
 * An Activity used to display fullscreen WebContents.
 *
 * This Activity used to be called FullscreenWebContentsActivity and extended FullScreenActivity.
 * When FullScreenActivity was renamed to SingleTabActivity, this was changed to FullscreenActivity.
 */
public class FullscreenActivity extends SingleTabActivity {
    private static final String TAG = "FullscreenActivity";

    private static final SparseArray<Tab> sTabsToSteal = new SparseArray<>();

    private WebContentsObserver mWebContentsObserver;

    @Override
    protected Tab createTab() {
        assert getIntent().hasExtra(IntentHandler.EXTRA_TAB_ID);

        final Tab tab = getTabToSteal(IntentUtils.safeGetIntExtra(
                getIntent(), IntentHandler.EXTRA_TAB_ID, Tab.INVALID_TAB_ID));

        FullscreenOptions options = IntentUtils.safeGetParcelableExtra(
                getIntent(), IntentHandler.EXTRA_FULLSCREEN_OPTIONS);

        tab.reparent(this, createTabDelegateFactory());

        tab.getFullscreenManager().setTab(tab);
        tab.enterFullscreenMode(options);

        mWebContentsObserver = new WebContentsObserver(tab.getWebContents()) {
            @Override
            public void didFinishNavigation(String url, boolean isInMainFrame, boolean isErrorPage,
                    boolean hasCommitted, boolean isSameDocument, boolean isFragmentNavigation,
                    boolean isRendererInitiated, boolean isDownload, Integer pageTransition,
                    int errorCode, String errorDescription, int httpStatusCode) {
                if (hasCommitted && isInMainFrame) {
                    // Notify the renderer to permanently hide the top controls since they do
                    // not apply to fullscreen content views.
                    tab.updateBrowserControlsState(tab.getBrowserControlsStateConstraints(), true);
                }
            }
        };
        return tab;
    }

    @Override
    public void finishNativeInitialization() {
        ControlContainer controlContainer = (ControlContainer) findViewById(R.id.control_container);
        initializeCompositorContent(new LayoutManager(getCompositorViewHolder()),
                (View) controlContainer, (ViewGroup) findViewById(android.R.id.content),
                controlContainer);

        if (getFullscreenManager() != null) getFullscreenManager().setTab(getActivityTab());
        super.finishNativeInitialization();
    }

    @Override
    protected void initializeToolbar() {}

    @Override
    protected int getControlContainerLayoutId() {
        // TODO(peconn): Determine if there's something more suitable to use here.
        return R.layout.fullscreen_control_container;
    }

    @Override
    public int getControlContainerHeightResource() {
        return R.dimen.fullscreen_activity_control_container_height;
    }

    @Override
    protected ChromeFullscreenManager createFullscreenManager() {
        // Create a Fullscreen manager that won't change the Tab's fullscreen state when the
        // Activity ends - we handle leaving fullscreen ourselves.
        return new ChromeFullscreenManager(
                this, ChromeFullscreenManager.ControlsPosition.NONE, false);
    }

    @Override
    public boolean supportsFullscreenActivity() {
        return true;
    }

    public static void enterFullscreenMode(final Tab tab, FullscreenOptions options) {
        if (tab.getFullscreenManager() == null) {
            Log.w(TAG, "Cannot toggle fullscreen, manager is null.");
            return;
        }

        if (tab.getFullscreenManager().getTab() == tab) {
            tab.getFullscreenManager().setTab(null);
        }

        launchFullscreenActivityThenStealTab(tab, options);
    }

    public static void exitFullscreenMode(final Tab tab) {
        if (tab.getFullscreenManager() == null) {
            Log.w(TAG, "Cannot toggle fullscreen, manager is null.");
            return;
        }

        if (tab.getFullscreenManager().getTab() == tab) {
            tab.getFullscreenManager().setTab(null);
        }

        reparentTabToOriginalOwner(tab);
    }

    private static void reparentTabToOriginalOwner(final Tab tab) {
        ChromeActivity activity = tab.getActivity();

        // On Android O, if you return to a portrait Activity from one locked in landscape, the
        // Activity gets config changes signalling it has been changed to landscape and back again.
        // I believe this is a bug. Since the FullscreenActivity may have had its orientation locked
        // to landscape for the video, we unlock it so it doesn't trigger an erroneous config change
        // in the receiving Activity.
        ScreenOrientationProvider.unlockOrientation(activity.getWindowAndroid());

        // If reparenting is triggered by the back button, this has already been called. If not we
        // must call it to restore everything to a good state before sending the Tab back.
        activity.exitFullscreenIfShowing();

        Intent intent = new Intent();
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, activity.getPackageName());
        // By default Intents from Chrome open in the current tab. We add this extra to prevent
        // clobbering the top tab.
        intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);

        // Send back to the Activity it came from.
        ComponentName parent = IntentUtils.safeGetParcelableExtra(
                activity.getIntent(), IntentHandler.EXTRA_PARENT_COMPONENT);
        if (parent != null) {
            intent.setComponent(parent);
        } else {
            Log.d(TAG, "Cannot return fullscreen tab to parent Activity.");
            // Tab.detachAndStartReparenting will give the intent a default component if it
            // has none.
        }

        // TODO(peconn): Can we not put this in onStop?
        ChromeActivity tabActivity = tab.getActivity();
        if (tabActivity instanceof FullscreenActivity) {
            FullscreenActivity fullscreenActivity = (FullscreenActivity) tabActivity;
            if (fullscreenActivity.mWebContentsObserver != null) {
                fullscreenActivity.mWebContentsObserver.destroy();
                fullscreenActivity.mWebContentsObserver = null;
            }
        }

        tab.detachAndStartReparenting(intent, null, () -> {
            // The Tab's FullscreenManager changes when it is moved.
            tab.getFullscreenManager().setTab(tab);

            // TODO(peconn): Will this not already happen?
            tab.exitFullscreenMode();
        });
    }

    private static void launchFullscreenActivityThenStealTab(Tab tab, FullscreenOptions options) {
        ChromeActivity activity = tab.getActivity();

        sTabsToSteal.put(tab.getId(), tab);

        Intent intent = new Intent();
        intent.setClass(activity, FullscreenActivity.class);
        intent.putExtra(IntentHandler.EXTRA_TAB_ID, tab.getId());
        intent.putExtra(IntentHandler.EXTRA_FULLSCREEN_OPTIONS, options);
        intent.putExtra(IntentHandler.EXTRA_PARENT_COMPONENT, activity.getComponentName());
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, activity.getPackageName());
        // In multiwindow mode we want both activities to be able to launch independent
        // FullscreenActivity's.
        intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);

        activity.startActivity(intent);
    }

    public static boolean shouldUseFullscreenActivity(Tab tab) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.FULLSCREEN_ACTIVITY)) return false;

        ChromeActivity activity = tab.getActivity();
        if (!activity.supportsFullscreenActivity()) return false;

        // FullscreenActivity transitions involve Intent-ing to a new Activity. If the current
        // Activity is not in the foreground we don't want to do this (as it would re-launch
        // Chrome).
        return ApplicationStatus.getStateForActivity(activity) == ActivityState.RESUMED;
    }

    private static Tab getTabToSteal(int id) {
        Tab tab = sTabsToSteal.get(id);
        assert tab != null;

        sTabsToSteal.remove(id);
        return tab;
    }
}
