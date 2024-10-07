// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.APP_CONTEXT;

import android.app.Activity;
import android.app.ActivityOptions;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;

import dagger.Lazy;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.customtabs.CloseButtonVisibilityManager;
import org.chromium.chrome.browser.customtabs.CustomTabCompositorContentInitializer;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegateSupplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.components.browser_ui.share.ShareHelper;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.util.TokenHolder;
import org.chromium.url.GURL;

import javax.inject.Inject;
import javax.inject.Named;

/**
 * Works with the toolbar in a Custom Tab. Encapsulates interactions with Chrome's toolbar-related
 * classes such as {@link ToolbarManager} and {@link BrowserControlsVisibilityManager}.
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
    private final CustomTabActivityTabProvider mTabProvider;
    private final CustomTabsConnection mConnection;
    private final Activity mActivity;
    private final ActivityWindowAndroid mWindowAndroid;
    private final Context mAppContext;
    private final CustomTabActivityTabController mTabController;
    private final Lazy<BrowserControlsVisibilityManager> mBrowserControlsVisibilityManager;
    private final CustomTabActivityNavigationController mNavigationController;
    private final CloseButtonVisibilityManager mCloseButtonVisibilityManager;
    private final CustomTabBrowserControlsVisibilityDelegate mVisibilityDelegate;
    private final CustomTabToolbarColorController mToolbarColorController;

    @Nullable private ToolbarManager mToolbarManager;

    private int mControlsHidingToken = TokenHolder.INVALID_TOKEN;
    private boolean mInitializedToolbarWithNative;
    private PendingIntent.OnFinished mButtonClickOnFinishedForTesting;

    private static final String TAG = "CustomTabToolbarCoor";

    @Inject
    public CustomTabToolbarCoordinator(
            BrowserServicesIntentDataProvider intentDataProvider,
            CustomTabActivityTabProvider tabProvider,
            CustomTabsConnection connection,
            Activity activity,
            ActivityWindowAndroid windowAndroid,
            @Named(APP_CONTEXT) Context appContext,
            CustomTabActivityTabController tabController,
            Lazy<BrowserControlsVisibilityManager> controlsVisiblityManager,
            CustomTabActivityNavigationController navigationController,
            CloseButtonVisibilityManager closeButtonVisibilityManager,
            CustomTabBrowserControlsVisibilityDelegate visibilityDelegate,
            CustomTabCompositorContentInitializer compositorContentInitializer,
            CustomTabToolbarColorController toolbarColorController) {
        mIntentDataProvider = intentDataProvider;
        mTabProvider = tabProvider;
        mConnection = connection;
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mAppContext = appContext;
        mTabController = tabController;
        mBrowserControlsVisibilityManager = controlsVisiblityManager;
        mNavigationController = navigationController;
        mCloseButtonVisibilityManager = closeButtonVisibilityManager;
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
        mCloseButtonVisibilityManager.onToolbarInitialized(manager);

        manager.setShowTitle(
                mConnection.getTitleVisibilityState(mIntentDataProvider)
                        == CustomTabsIntent.SHOW_PAGE_TITLE);
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

    @VisibleForTesting
    void onCustomButtonClick(CustomButtonParams params) {
        Tab tab = mTabProvider.getTab();
        if (tab == null) return;

        // The share button from CCT should have custom actions, however if the
        // ShareDelegateSupplier is null, we should fallback to the default share action without
        // custom buttons.
        Supplier<ShareDelegate> supplier = ShareDelegateSupplier.from(mWindowAndroid);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SHARE_CUSTOM_ACTIONS_IN_CCT)
                && params.getType() == CustomButtonParams.ButtonType.CCT_SHARE_BUTTON
                && supplier != null
                && supplier.get() != null) {
            supplier.get()
                    .share(
                            tab,
                            /* shareDirectly= */ false,
                            ShareDelegate.ShareOrigin.CUSTOM_TAB_SHARE_BUTTON);
        } else if (params.getType() == CustomButtonParams.ButtonType.CCT_OPEN_IN_BROWSER_BUTTON) {
            if (mNavigationController.openCurrentUrlInBrowser()) {
                WebContents webContents = tab == null ? null : tab.getWebContents();
                mConnection.notifyOpenInBrowser(mIntentDataProvider.getSession(), webContents);
            }
        } else {
            sendButtonPendingIntentWithUrlAndTitle(params, tab.getOriginalUrl(), tab.getTitle());
        }

        RecordUserAction.record("CustomTabsCustomActionButtonClick");
        Resources resources = mActivity.getResources();
        if (mIntentDataProvider.shouldEnableEmbeddedMediaExperience()
                && TextUtils.equals(params.getDescription(), resources.getString(R.string.share))) {
            ShareHelper.recordShareSource(ShareHelper.ShareSourceAndroid.ANDROID_SHARE_SHEET);
            RecordUserAction.record("CustomTabsCustomActionButtonClick.DownloadsUI.Share");
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
            CustomButtonParams params, GURL url, String title) {
        Intent addedIntent = new Intent();
        addedIntent.setData(Uri.parse(url.getSpec()));
        addedIntent.putExtra(Intent.EXTRA_SUBJECT, title);
        try {
            ActivityOptions options = ActivityOptions.makeBasic();
            ApiCompatibilityUtils.setActivityOptionsBackgroundActivityStartMode(options);
            params.getPendingIntent()
                    .send(
                            mAppContext,
                            0,
                            addedIntent,
                            mButtonClickOnFinishedForTesting,
                            null,
                            null,
                            options.toBundle());
        } catch (PendingIntent.CanceledException e) {
            Log.e(TAG, "CanceledException while sending pending intent in custom tab");
        }
    }

    private void onCompositorContentInitialized(LayoutManagerImpl layoutDriver) {
        mToolbarManager.initializeWithNative(
                layoutDriver,
                /* stripLayoutHelperManager= */ null,
                /* openGridTabSwitcherHandler= */ null,
                /* bookmarkClickHandler= */ null,
                /* customTabsBackClickHandler= */ v -> onCloseButtonClick(),
                /* archivedTabCountSupplier= */ null);
        mInitializedToolbarWithNative = true;
    }

    private void onCloseButtonClick() {
        RecordUserAction.record("CustomTabs.CloseButtonClicked");
        if (mIntentDataProvider.shouldEnableEmbeddedMediaExperience()) {
            RecordUserAction.record("CustomTabs.CloseButtonClicked.DownloadsUI");
        }

        mNavigationController.navigateOnClose();
    }

    public void setBrowserControlsState(@BrowserControlsState int controlsState) {
        mVisibilityDelegate.setControlsState(controlsState);
        if (controlsState == BrowserControlsState.HIDDEN) {
            mControlsHidingToken =
                    mBrowserControlsVisibilityManager
                            .get()
                            .hideAndroidControlsAndClearOldToken(mControlsHidingToken);
        } else {
            mBrowserControlsVisibilityManager
                    .get()
                    .releaseAndroidControlsHidingToken(mControlsHidingToken);
        }
    }

    /** Shows toolbar temporarily, for a few seconds. */
    public void showToolbarTemporarily() {
        mBrowserControlsVisibilityManager
                .get()
                .getBrowserVisibilityDelegate()
                .showControlsTransient();
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
    public void setCustomButtonPendingIntentOnFinishedForTesting(
            PendingIntent.OnFinished onFinished) {
        mButtonClickOnFinishedForTesting = onFinished;
        ResettersForTesting.register(() -> mButtonClickOnFinishedForTesting = null);
    }
}
