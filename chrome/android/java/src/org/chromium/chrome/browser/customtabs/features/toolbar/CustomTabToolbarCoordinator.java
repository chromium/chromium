// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.APP_CONTEXT;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.Log;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.browserservices.BrowserServicesActivityTabController;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.customtabs.CustomButtonParams;
import org.chromium.chrome.browser.customtabs.CustomTabCompositorContentInitializer;
import org.chromium.chrome.browser.customtabs.CustomTabUmaRecorder;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.content_public.common.BrowserControlsState;
import org.chromium.ui.util.TokenHolder;

import javax.inject.Inject;
import javax.inject.Named;

import dagger.Lazy;

/**
 * Works with the toolbar in a Custom Tab. Encapsulates interactions with Chrome's toolbar-related
 * classes such as {@link ToolbarManager} and {@link FullscreenManager}.
 *
 * TODO(pshmakov):
 * 1. Reduce the coupling between Custom Tab toolbar and Chrome's common code. In particular,
 * ToolbarLayout has Custom Tab specific methods that throw unless we're in a Custom Tab - we need a
 * better design.
 * 2. Make toolbar lazy. E.g. in Trusted Web Activities we always start without toolbar - delay
 * executing any initialization code and creating {@link ToolbarManager} until the toolbar needs
 * to appear.
 * 3. Refactor to MVC.
 */
@ActivityScope
public class CustomTabToolbarCoordinator {
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final @Nullable CustomTabUmaRecorder mUmaRecorder;
    private final CustomTabActivityTabProvider mTabProvider;
    private final CustomTabsConnection mConnection;
    private final ChromeActivity mActivity;
    private final Context mAppContext;
    private final BrowserServicesActivityTabController mTabController;
    private final Lazy<ChromeFullscreenManager> mFullscreenManager;
    private final CustomTabActivityNavigationController mNavigationController;
    private final CustomTabBrowserControlsVisibilityDelegate mVisibilityDelegate;
    private final CustomTabToolbarColorController mToolbarColorController;

    @Nullable
    private ToolbarManager mToolbarManager;

    private int mControlsHidingToken = TokenHolder.INVALID_TOKEN;
    private boolean mInitializedToolbarWithNative;
    private PendingIntent.OnFinished mCustomButtonClickOnFinished;

    private static final String TAG = "CustomTabToolbarCoor";

    @Inject
    public CustomTabToolbarCoordinator(BrowserServicesIntentDataProvider intentDataProvider,
            @Nullable CustomTabUmaRecorder umaRecorder, CustomTabActivityTabProvider tabProvider,
            CustomTabsConnection connection, ChromeActivity activity,
            @Named(APP_CONTEXT) Context appContext,
            BrowserServicesActivityTabController tabController,
            Lazy<ChromeFullscreenManager> fullscreenManager,
            CustomTabActivityNavigationController navigationController,
            CustomTabBrowserControlsVisibilityDelegate visibilityDelegate,
            CustomTabCompositorContentInitializer compositorContentInitializer,
            CustomTabToolbarColorController toolbarColorController) {
        mIntentDataProvider = intentDataProvider;
        mUmaRecorder = umaRecorder;
        mTabProvider = tabProvider;
        mConnection = connection;
        mActivity = activity;
        mAppContext = appContext;
        mTabController = tabController;
        mFullscreenManager = fullscreenManager;
        mNavigationController = navigationController;
        mVisibilityDelegate = visibilityDelegate;
        mToolbarColorController = toolbarColorController;

        compositorContentInitializer.addCallback(this::onCompositorContentInitialized);
    }

    /**
     * Notifies the navigation controller that the ToolbarManager has been created and is ready for
     * use. ToolbarManager isn't passed directly to the constructor because it's not guaranteed to
     * be initialized yet.
     */
    public void onToolbarInitialized(ToolbarManager manager) {
        assert manager != null : "Toolbar manager not initialized";
        mToolbarManager = manager;
        mToolbarColorController.onToolbarInitialized(manager);

        manager.setCloseButtonDrawable(mIntentDataProvider.getCloseButtonDrawable());
        manager.setShowTitle(
                mIntentDataProvider.getTitleVisibilityState() == CustomTabsIntent.SHOW_PAGE_TITLE);
        if (mConnection.shouldHideDomainForSession(mIntentDataProvider.getSession())) {
            manager.setUrlBarHidden(true);
        }
        if (mIntentDataProvider.isMediaViewer()) {
            manager.setToolbarShadowVisibility(View.GONE);
        }
        showCustomButtonsOnToolbar();
    }

    /**
     * Configures the custom button on toolbar. Does nothing if invalid data is provided by clients.
     */
    private void showCustomButtonsOnToolbar() {
        for (CustomButtonParams params : mIntentDataProvider.getCustomButtonsOnToolbar()) {
            View.OnClickListener onClickListener = v -> onCustomButtonClick(params);
            mToolbarManager.addCustomActionButton(
                    params.getIcon(mActivity), params.getDescription(), onClickListener);
        }
    }

    private void onCustomButtonClick(CustomButtonParams params) {
        Tab tab = mTabProvider.getTab();
        if (tab == null) return;

        sendButtonPendingIntentWithUrlAndTitle(params, tab.getUrl(), tab.getTitle());

        if (mUmaRecorder != null) {
            mUmaRecorder.recordCustomButtonClick(mActivity.getResources(), params);
        }
    }

    /**
     * Sends the pending intent for the custom button on the toolbar with the given {@code params},
     *         with the given {@code url} as data.
     * @param params The parameters for the custom button.
     * @param url The URL to attach as additional data to the {@link PendingIntent}.
     * @param title The title to attach as additional data to the {@link PendingIntent}.
     */
    private void sendButtonPendingIntentWithUrlAndTitle(
            CustomButtonParams params, String url, String title) {
        Intent addedIntent = new Intent();
        addedIntent.setData(Uri.parse(url));
        addedIntent.putExtra(Intent.EXTRA_SUBJECT, title);
        try {
            params.getPendingIntent().send(
                    mAppContext, 0, addedIntent, mCustomButtonClickOnFinished, null);
        } catch (PendingIntent.CanceledException e) {
            Log.e(TAG, "CanceledException while sending pending intent in custom tab");
        }
    }

    private void onCompositorContentInitialized(LayoutManager layoutDriver) {
        mToolbarManager.initializeWithNative(mTabController.getTabModelSelector(),
                mFullscreenManager.get().getBrowserVisibilityDelegate(), null, layoutDriver, null,
                null, null, v -> onCloseButtonClick());
        mInitializedToolbarWithNative = true;
    }

    private void onCloseButtonClick() {
        if (mUmaRecorder != null) {
            mUmaRecorder.recordCloseButtonClick();
        }
        mNavigationController.navigateOnClose();
    }

    public void setBrowserControlsState(@BrowserControlsState int controlsState) {
        mVisibilityDelegate.setControlsState(controlsState);
        if (controlsState == BrowserControlsState.HIDDEN) {
            mControlsHidingToken = mFullscreenManager.get()
                    .hideAndroidControlsAndClearOldToken(mControlsHidingToken);
        } else {
            mFullscreenManager.get().releaseAndroidControlsHidingToken(mControlsHidingToken);
        }
    }

    /**
     * Shows toolbar temporarily, for a few seconds.
     */
    public void showToolbarTemporarily() {
        mFullscreenManager.get().getBrowserVisibilityDelegate().showControlsTransient();
    }

    /**
     * Updates a custom button with a new icon and description. Provided {@link CustomButtonParams}
     * must have the updated data.
     * Returns whether has succeeded.
     */
    public boolean updateCustomButton(CustomButtonParams params) {
        if (!params.doesIconFitToolbar(mActivity)) {
            return false;
        }
        int index = mIntentDataProvider.getCustomToolbarButtonIndexForId(params.getId());
        if (index == -1) {
            assert false;
            return false;
        }
        if (mToolbarManager == null) {
            return false;
        }

        mToolbarManager.updateCustomActionButton(
                index, params.getIcon(mActivity), params.getDescription());
        return true;
    }

    /**
     * Returns whether the toolbar has been fully initialized
     * ({@link ToolbarManager#initializeWithNative}).
     */
    public boolean toolbarIsInitialized() {
        return mInitializedToolbarWithNative;
    }

    /**
     * Set the callback object for the {@link PendingIntent} which is sent by the custom buttons.
     */
    @VisibleForTesting
    public void setCustomButtonPendingIntentOnFinishedForTesting(
            PendingIntent.OnFinished onFinished) {
        mCustomButtonClickOnFinished = onFinished;
    }
}
