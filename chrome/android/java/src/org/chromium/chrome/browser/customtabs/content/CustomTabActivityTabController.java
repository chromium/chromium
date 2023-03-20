// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.SAVED_INSTANCE_SUPPLIER;

import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.Window;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ServiceTabLauncher;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingDelegateFactory;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory;
import org.chromium.chrome.browser.customtabs.CustomTabIncognitoManager;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabNavigationEventObserver;
import org.chromium.chrome.browser.customtabs.CustomTabObserver;
import org.chromium.chrome.browser.customtabs.CustomTabTabPersistencePolicy;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.FirstMeaningfulPaintObserver;
import org.chromium.chrome.browser.customtabs.PageLoadMetricsObserver;
import org.chromium.chrome.browser.customtabs.ReparentingTaskProvider;
import org.chromium.chrome.browser.customtabs.features.TabInteractionRecorder;
import org.chromium.chrome.browser.customtabs.features.sessionrestore.SessionRestoreManager;
import org.chromium.chrome.browser.customtabs.features.sessionrestore.SessionRestoreMessageController;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.RedirectHandlerTabHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.AsyncTabParams;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelInitializer;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import javax.inject.Inject;
import javax.inject.Named;

import dagger.Lazy;

/**
 * Creates a new Tab or retrieves an existing Tab for the CustomTabActivity, and initializes it.
 */
@ActivityScope
public class CustomTabActivityTabController implements InflationObserver {
    // For CustomTabs.WebContentsStateOnLaunch, see histograms.xml. Append only.
    @IntDef({WebContentsState.NO_WEBCONTENTS, WebContentsState.PRERENDERED_WEBCONTENTS,
            WebContentsState.SPARE_WEBCONTENTS, WebContentsState.TRANSFERRED_WEBCONTENTS})
    @Retention(RetentionPolicy.SOURCE)
    private @interface WebContentsState {
        int NO_WEBCONTENTS = 0;

        int PRERENDERED_WEBCONTENTS = 1;
        int SPARE_WEBCONTENTS = 2;
        int TRANSFERRED_WEBCONTENTS = 3;
        int NUM_ENTRIES = 4;
    }

    private final Lazy<CustomTabDelegateFactory> mCustomTabDelegateFactory;
    private final AppCompatActivity mActivity;
    private final CustomTabsConnection mConnection;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final TabObserverRegistrar mTabObserverRegistrar;
    private final Lazy<CompositorViewHolder> mCompositorViewHolder;
    private final WarmupManager mWarmupManager;
    private final CustomTabTabPersistencePolicy mTabPersistencePolicy;
    private final CustomTabActivityTabFactory mTabFactory;
    private final Lazy<CustomTabObserver> mCustomTabObserver;
    private final WebContentsFactory mWebContentsFactory;
    private final CustomTabNavigationEventObserver mTabNavigationEventObserver;
    private final ActivityTabProvider mActivityTabProvider;
    private final CustomTabActivityTabProvider mTabProvider;
    private final ReparentingTaskProvider mReparentingTaskProvider;
    private final Lazy<CustomTabIncognitoManager> mCustomTabIncognitoManager;
    private final Lazy<AsyncTabParamsManager> mAsyncTabParamsManager;
    private final Supplier<Bundle> mSavedInstanceStateSupplier;
    private final ActivityWindowAndroid mWindowAndroid;
    private final TabModelInitializer mTabModelInitializer;
    private final SessionRestoreMessageController mRestoreMsgController;

    @Nullable
    private final CustomTabsSessionToken mSession;
    private final Intent mIntent;

    @Nullable
    private RealtimeEngagementSignalObserver mRealtimeEngagementSignalObserver;

    @Inject
    public CustomTabActivityTabController(AppCompatActivity activity,
            Lazy<CustomTabDelegateFactory> customTabDelegateFactory,
            CustomTabsConnection connection, BrowserServicesIntentDataProvider intentDataProvider,
            ActivityTabProvider activityTabProvider, TabObserverRegistrar tabObserverRegistrar,
            Lazy<CompositorViewHolder> compositorViewHolder,
            ActivityLifecycleDispatcher lifecycleDispatcher, WarmupManager warmupManager,
            CustomTabTabPersistencePolicy persistencePolicy, CustomTabActivityTabFactory tabFactory,
            Lazy<CustomTabObserver> customTabObserver, WebContentsFactory webContentsFactory,
            CustomTabNavigationEventObserver tabNavigationEventObserver,
            CustomTabActivityTabProvider tabProvider,
            ReparentingTaskProvider reparentingTaskProvider,
            Lazy<CustomTabIncognitoManager> customTabIncognitoManager,
            Lazy<AsyncTabParamsManager> asyncTabParamsManager,
            @Named(SAVED_INSTANCE_SUPPLIER) Supplier<Bundle> savedInstanceStateSupplier,
            ActivityWindowAndroid windowAndroid, TabModelInitializer tabModelInitializer,
            SessionRestoreMessageController restoreMsgController) {
        mCustomTabDelegateFactory = customTabDelegateFactory;
        mActivity = activity;
        mConnection = connection;
        mIntentDataProvider = intentDataProvider;
        mTabObserverRegistrar = tabObserverRegistrar;
        mCompositorViewHolder = compositorViewHolder;
        mWarmupManager = warmupManager;
        mTabPersistencePolicy = persistencePolicy;
        mTabFactory = tabFactory;
        mCustomTabObserver = customTabObserver;
        mWebContentsFactory = webContentsFactory;
        mTabNavigationEventObserver = tabNavigationEventObserver;
        mActivityTabProvider = activityTabProvider;
        mTabProvider = tabProvider;
        mReparentingTaskProvider = reparentingTaskProvider;
        mCustomTabIncognitoManager = customTabIncognitoManager;
        mAsyncTabParamsManager = asyncTabParamsManager;
        mSavedInstanceStateSupplier = savedInstanceStateSupplier;
        mWindowAndroid = windowAndroid;
        mTabModelInitializer = tabModelInitializer;

        mSession = mIntentDataProvider.getSession();
        mIntent = mIntentDataProvider.getIntent();

        // Save speculated url, because it will be erased later with mConnection.takeHiddenTab().
        mTabProvider.setSpeculatedUrl(mConnection.getSpeculatedUrl(mSession));

        mRestoreMsgController = restoreMsgController;
        lifecycleDispatcher.register(this);
    }

    /** @return whether allocating a child connection is needed during native initialization. */
    public boolean shouldAllocateChildConnection() {
        boolean hasSpeculated = !TextUtils.isEmpty(mConnection.getSpeculatedUrl(mSession));
        int mode = mTabProvider.getInitialTabCreationMode();
        return mode != TabCreationMode.EARLY && mode != TabCreationMode.HIDDEN
                && !hasSpeculated && !mWarmupManager.hasSpareWebContents();
    }

    public void detachAndStartReparenting(
            Intent intent, Bundle startActivityOptions, Runnable finishCallback) {
        Tab tab = mTabProvider.getTab();
        if (tab == null) {
            assert false;
            return;
        }

        if (getTabModelSelector().getCurrentModel().getCount() <= 1) {
            mTabProvider.removeTab();
        }

        mReparentingTaskProvider.get(tab).begin(
                mActivity, intent, startActivityOptions, finishCallback);
    }

    /**
     * Closes the current tab. This doesn't necessarily lead to closing the entire activity, in
     * case links with target="_blank" were followed. See the comment to
     * {@link CustomTabActivityTabProvider.Observer#onAllTabsClosed}.
     */
    public void closeTab() {
        TabModel model = mTabFactory.getTabModelSelector().getCurrentModel();
        Tab currentTab = mTabProvider.getTab();
        if (!maybeStoreTab(currentTab)) {
            model.closeTab(currentTab, false, false, false);
        }
    }

    public boolean onlyOneTabRemaining() {
        TabModel model = mTabFactory.getTabModelSelector().getCurrentModel();
        return model.getCount() == 1;
    }

    /**
     * Checks if the current tab contains unload events and if so it opens the dialog
     * to ask the user before closing the tab.
     *
     * @return Whether we ran the unload events or not.
     */
    public boolean dispatchBeforeUnloadIfNeeded() {
        Tab currentTab = mTabProvider.getTab();
        if (currentTab.getWebContents().needToFireBeforeUnloadOrUnloadEvents()) {
            currentTab.getWebContents().dispatchBeforeUnload(false);
            return true;
        }
        return false;
    }

    public void closeAndForgetTab() {
        // TODO(https://crbug.com/1379452): Store all the tabs in the tab model.
        if (mTabFactory.getTabModelSelector().getCurrentModel().getCount() > 0) {
            // Ignore the results, as we are closing all the tabs regardless at the end.
            maybeStoreTab(mTabProvider.getTab());
        }

        mTabFactory.getTabModelSelector().closeAllTabs(true);
        mTabPersistencePolicy.deleteMetadataStateFileAsync();
    }

    public void saveState() {
        mTabFactory.getTabModelOrchestrator().saveState();
    }

    public TabModelSelector getTabModelSelector() {
        return mTabFactory.getTabModelSelector();
    }

    @Override
    public void onPreInflationStartup() {
        // This must be requested before adding content.
        mActivity.supportRequestWindowFeature(Window.FEATURE_ACTION_MODE_OVERLAY);

        if (mSavedInstanceStateSupplier.get() == null && mConnection.hasWarmUpBeenFinished()) {
            mTabModelInitializer.initializeTabModels();

            // Hidden tabs shouldn't be used in incognito, since they are always created with
            // regular profile.
            if (mIntentDataProvider.isIncognito()) {
                mTabProvider.setInitialTab(createTab(), TabCreationMode.EARLY);
                return;
            }

            Tab tab = getHiddenTab();
            if (tab == null) {
                tab = createTab();
                mTabProvider.setInitialTab(tab, TabCreationMode.EARLY);
            } else {
                mTabProvider.setInitialTab(tab, TabCreationMode.HIDDEN);
            }
        }
    }

    @Override
    public void onPostInflationStartup() {}

    public void finishNativeInitialization() {
        // If extra headers have been passed, cancel any current speculation, as
        // speculation doesn't support extra headers.
        if (IntentHandler.getExtraHeadersFromIntent(mIntent) != null) {
            mConnection.cancelSpeculation(mSession);
        }

        TabModelOrchestrator tabModelOrchestrator = mTabFactory.getTabModelOrchestrator();
        TabModelSelectorBase tabModelSelector = tabModelOrchestrator.getTabModelSelector();

        TabModel tabModel = tabModelSelector.getModel(mIntentDataProvider.isIncognito());
        tabModel.addObserver(mTabObserverRegistrar);

        finalizeCreatingTab(tabModelOrchestrator, tabModel);
        Tab tab = mTabProvider.getTab();
        assert tab != null;
        assert mTabProvider.getInitialTabCreationMode() != TabCreationMode.NONE;

        // Put Sync in the correct state by calling tab state initialized. crbug.com/581811.
        tabModelSelector.markTabStateInitialized();

        // Notify ServiceTabLauncher if this is an asynchronous tab launch.
        if (mIntent.hasExtra(ServiceTabLauncher.LAUNCH_REQUEST_ID_EXTRA)) {
            ServiceTabLauncher.onWebContentsForRequestAvailable(
                    mIntent.getIntExtra(ServiceTabLauncher.LAUNCH_REQUEST_ID_EXTRA, 0),
                    tab.getWebContents());
        }
    }

    // Creates the tab on native init, if it hasn't been created yet, and does all the additional
    // initialization steps necessary at this stage.
    private void finalizeCreatingTab(TabModelOrchestrator tabModelOrchestrator, TabModel tabModel) {
        Tab earlyCreatedTab = mTabProvider.getTab();

        Tab tab = earlyCreatedTab;
        @TabCreationMode int mode = mTabProvider.getInitialTabCreationMode();

        Tab restoredTab = tryRestoringTab(tabModelOrchestrator);
        if (restoredTab != null) {
            assert earlyCreatedTab == null :
                    "Shouldn't create a new tab when there's one to restore";
            tab = restoredTab;
            mode = TabCreationMode.RESTORED;
        }

        if (tab == null) {
            // No tab was restored or created early, creating a new tab.
            tab = createTab();
            mode = TabCreationMode.DEFAULT;
        }

        assert tab != null;

        if (mode != TabCreationMode.RESTORED) {
            tabModel.addTab(tab, 0, tab.getLaunchType(), TabCreationState.LIVE_IN_FOREGROUND);
        }

        // This cannot be done before because we want to do the reparenting only
        // when we have compositor related controllers.
        if (mode == TabCreationMode.HIDDEN) {
            TabReparentingParams params =
                    (TabReparentingParams) mAsyncTabParamsManager.get().remove(tab.getId());
            mReparentingTaskProvider.get(tab).finish(
                    ReparentingDelegateFactory.createReparentingTaskDelegate(
                            mCompositorViewHolder.get(), mWindowAndroid,
                            mCustomTabDelegateFactory.get()),
                    (params == null ? null : params.getFinalizeCallback()));
        }

        if (tab != earlyCreatedTab) {
            mTabProvider.setInitialTab(tab, mode);
        } // else we've already set the initial tab.

        // Listen to tab swapping and closing.
        mActivityTabProvider.addObserver(mTabProvider::swapTab);
    }

    @Nullable
    private Tab tryRestoringTab(TabModelOrchestrator tabModelOrchestrator) {
        if (mSavedInstanceStateSupplier.get() == null) return null;
        tabModelOrchestrator.loadState(true);
        tabModelOrchestrator.restoreTabs(true);
        Tab tab = tabModelOrchestrator.getTabModelSelector().getCurrentTab();
        if (tab != null) {
            initializeTab(tab);
        }
        return tab;
    }

    /** Encapsulates CustomTabsConnection#takeHiddenTab() with additional initialization logic. */
    @Nullable
    private Tab getHiddenTab() {
        String url = mIntentDataProvider.getUrlToLoad();
        String referrerUrl = IntentHandler.getReferrerUrlIncludingExtraHeaders(mIntent);
        Tab tab = mConnection.takeHiddenTab(mSession, url, referrerUrl);
        if (tab == null) return null;
        RecordHistogram.recordEnumeratedHistogram("CustomTabs.WebContentsStateOnLaunch",
                WebContentsState.PRERENDERED_WEBCONTENTS, WebContentsState.NUM_ENTRIES);
        TabAssociatedApp.from(tab).setAppId(mConnection.getClientPackageNameForSession(mSession));
        initializeTab(tab);
        return tab;
    }

    private Tab createTab() {
        WebContents webContents = takeWebContents();
        // clang-format off
        Tab tab = mTabFactory.createTab(webContents, mCustomTabDelegateFactory.get(),
                (preInitTab) -> TabAssociatedApp.from(preInitTab).setAppId(
                                mConnection.getClientPackageNameForSession(mSession)));
        // clang-format on

        initializeTab(tab);

        if (mIntentDataProvider.getTranslateLanguage() != null) {
            TranslateBridge.setPredefinedTargetLanguage(tab,
                    mIntentDataProvider.getTranslateLanguage(),
                    mIntentDataProvider.shouldAutoTranslate());
        }

        return tab;
    }

    private void recordWebContentsStateOnLaunch(int webContentsStateOnLaunch) {
        RecordHistogram.recordEnumeratedHistogram("CustomTabs.WebContentsStateOnLaunch",
                webContentsStateOnLaunch, WebContentsState.NUM_ENTRIES);
    }

    private WebContents takeWebContents() {
        WebContents webContents = takeAsyncWebContents();
        if (webContents != null) {
            recordWebContentsStateOnLaunch(WebContentsState.TRANSFERRED_WEBCONTENTS);
            webContents.resumeLoadingCreatedWebContents();
            return webContents;
        }

        webContents = mWarmupManager.takeSpareWebContents(
                mIntentDataProvider.isIncognito(), false /*initiallyHidden*/);
        if (webContents != null) {
            recordWebContentsStateOnLaunch(WebContentsState.SPARE_WEBCONTENTS);
            return webContents;
        }

        recordWebContentsStateOnLaunch(WebContentsState.NO_WEBCONTENTS);
        if (mIntentDataProvider.isIncognito()) {
            return mWebContentsFactory.createWebContentsWithWarmRenderer(
                    mCustomTabIncognitoManager.get().getProfile(), false);
        } else {
            Profile profile = Profile.getLastUsedRegularProfile();
            return mWebContentsFactory.createWebContentsWithWarmRenderer(profile, false);
        }
    }

    @Nullable
    private WebContents takeAsyncWebContents() {
        // Async WebContents are not supported for Incognit CCT.
        if (mIntentDataProvider.isIncognito()) return null;
        int assignedTabId = IntentHandler.getTabId(mIntent);
        AsyncTabParams asyncParams = mAsyncTabParamsManager.get().remove(assignedTabId);
        if (asyncParams == null) return null;
        return asyncParams.getWebContents();
    }

    private void initializeTab(Tab tab) {
        // TODO(pkotwicz): Determine whether these should be done for webapps.
        if (!mIntentDataProvider.isWebappOrWebApkActivity()) {
            RedirectHandlerTabHelper.updateIntentInTab(tab, mIntent);
            tab.getView().requestFocus();
        }

        if (!tab.isIncognito()) {
            TabObserver observer = new EmptyTabObserver() {
                @Override
                public void onContentChanged(Tab tab) {
                    if (tab.getWebContents() != null) {
                        mConnection.setClientDataHeaderForNewTab(mSession, tab.getWebContents());
                    }
                }
            };
            tab.addObserver(observer);
            observer.onContentChanged(tab);
        }

        if (CustomTabsConnection.getInstance().isDynamicFeatureEnabled(
                    ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS)) {
            mRealtimeEngagementSignalObserver = new RealtimeEngagementSignalObserver(
                    mTabObserverRegistrar, mConnection, mSession);
        }

        // TODO(pshmakov): invert these dependencies.
        // Please don't register new observers here. Instead, inject TabObserverRegistrar in classes
        // dedicated to your feature, and register there.
        mTabObserverRegistrar.registerTabObserver(mCustomTabObserver.get());
        mTabObserverRegistrar.registerTabObserver(mTabNavigationEventObserver);
        mTabObserverRegistrar.registerPageLoadMetricsObserver(
                new PageLoadMetricsObserver(mConnection, mSession, tab));
        mTabObserverRegistrar.registerPageLoadMetricsObserver(
                new FirstMeaningfulPaintObserver(mCustomTabObserver.get(), tab));

        // Immediately add the observer to PageLoadMetrics to catch early events that may
        // be generated in the middle of tab initialization.
        mTabObserverRegistrar.addObserversForTab(tab);
        prepareTabBackground(tab);
    }

    public void registerTabObserver(TabObserver observer) {
        mTabObserverRegistrar.registerTabObserver(observer);
    }

    /** Sets the initial background color for the Tab, shown before the page content is ready. */
    private void prepareTabBackground(final Tab tab) {
        if (!CustomTabIntentDataProvider.isTrustedCustomTab(mIntent, mSession)) return;

        int backgroundColor = mIntentDataProvider.getColorProvider().getInitialBackgroundColor();
        if (backgroundColor == Color.TRANSPARENT) return;

        // Set the background color.
        tab.getView().setBackgroundColor(backgroundColor);

        // Unset the background when the page has rendered.
        EmptyTabObserver mediaObserver = new EmptyTabObserver() {
            @Override
            public void didFirstVisuallyNonEmptyPaint(final Tab tab) {
                tab.removeObserver(this);

                // Blink has rendered the page by this point, but we need to wait for the compositor
                // frame swap to avoid flash of white content.
                mCompositorViewHolder.get().getCompositorView().surfaceRedrawNeededAsync(() -> {
                    if (!tab.isInitialized()
                            || ActivityUtils.isActivityFinishingOrDestroyed(mActivity)) {
                        return;
                    }
                    tab.getView().setBackgroundResource(0);
                });
            }
        };

        tab.addObserver(mediaObserver);
    }

    /**
     * Store the tab into {@link SessionRestoreManager}.
     * @param tab The tab to be stored.
     * @return Whether storing tab succeeded.
     */
    private boolean maybeStoreTab(@Nullable Tab tab) {
        if (tab == null || mConnection.getSessionRestoreManager() == null) return false;

        SessionRestoreManager sessionRestoreManager = mConnection.getSessionRestoreManager();
        TabInteractionRecorder recorder = TabInteractionRecorder.getFromTab(tab);
        if (recorder == null || !recorder.hadInteraction()) {
            return false;
        }

        // TODO(wenyufu): Add observer to record metrics for tab eviction.
        boolean success = sessionRestoreManager.store(tab);
        if (!success) {
            return false;
        }
        mTabProvider.removeTab();
        return true;
    }
}
