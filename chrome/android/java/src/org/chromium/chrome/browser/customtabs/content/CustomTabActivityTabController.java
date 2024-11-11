// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.SAVED_INSTANCE_SUPPLIER;

import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.browser.customtabs.CustomTabsSessionToken;

import dagger.Lazy;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ServiceTabLauncher;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingDelegateFactory;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTask;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.cookies.CookiesFetcher;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabCookiesFetcher;
import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabNavigationEventObserver;
import org.chromium.chrome.browser.customtabs.CustomTabObserver;
import org.chromium.chrome.browser.customtabs.CustomTabTabPersistencePolicy;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.FirstMeaningfulPaintObserver;
import org.chromium.chrome.browser.customtabs.HiddenTabHolder.HiddenTab;
import org.chromium.chrome.browser.customtabs.PageLoadMetricsObserver;
import org.chromium.chrome.browser.customtabs.TwaOfflineDataProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.RedirectHandlerTabHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.AsyncTabParams;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelInitializer;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import javax.inject.Inject;
import javax.inject.Named;

/** Creates a new Tab or retrieves an existing Tab for the CustomTabActivity, and initializes it. */
@ActivityScope
public class CustomTabActivityTabController implements PauseResumeWithNativeObserver, Destroyable {
    // For CustomTabs.WebContentsStateOnLaunch, see histograms.xml. Append only.
    @IntDef({
        WebContentsState.NO_WEBCONTENTS,
        WebContentsState.PRERENDERED_WEBCONTENTS,
        WebContentsState.SPARE_WEBCONTENTS,
        WebContentsState.TRANSFERRED_WEBCONTENTS
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface WebContentsState {
        int NO_WEBCONTENTS = 0;

        int PRERENDERED_WEBCONTENTS = 1;
        int SPARE_WEBCONTENTS = 2;
        int TRANSFERRED_WEBCONTENTS = 3;
        int NUM_ENTRIES = 4;
    }

    private final OneshotSupplier<ProfileProvider> mProfileProviderSupplier;
    private final Lazy<CustomTabDelegateFactory> mCustomTabDelegateFactory;
    private final AppCompatActivity mActivity;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final TabObserverRegistrar mTabObserverRegistrar;
    private final Lazy<CompositorViewHolder> mCompositorViewHolder;
    private final CustomTabTabPersistencePolicy mTabPersistencePolicy;
    private final CustomTabActivityTabFactory mTabFactory;
    private final CustomTabObserver mCustomTabObserver;
    private final CustomTabNavigationEventObserver mTabNavigationEventObserver;
    private final ActivityTabProvider mActivityTabProvider;
    private final CustomTabActivityTabProvider mTabProvider;
    private final Supplier<Bundle> mSavedInstanceStateSupplier;
    private final ActivityWindowAndroid mWindowAndroid;
    private final TabModelInitializer mTabModelInitializer;
    private final CipherFactory mCipherFactory;

    @Nullable private final CustomTabsSessionToken mSession;
    private final Intent mIntent;
    private CookiesFetcher mCookiesFetcher;

    @Inject
    public CustomTabActivityTabController(
            BaseCustomTabActivity activity,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            Lazy<CustomTabDelegateFactory> customTabDelegateFactory,
            BrowserServicesIntentDataProvider intentDataProvider,
            ActivityTabProvider activityTabProvider,
            Lazy<CompositorViewHolder> compositorViewHolder,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            CustomTabTabPersistencePolicy persistencePolicy,
            CustomTabActivityTabFactory tabFactory,
            @Named(SAVED_INSTANCE_SUPPLIER) Supplier<Bundle> savedInstanceStateSupplier,
            ActivityWindowAndroid windowAndroid,
            TabModelInitializer tabModelInitializer) {
        mProfileProviderSupplier = profileProviderSupplier;
        mCustomTabDelegateFactory = customTabDelegateFactory;
        mActivity = activity;
        mIntentDataProvider = intentDataProvider;
        mTabObserverRegistrar = activity.getTabObserverRegistrar();
        mCompositorViewHolder = compositorViewHolder;
        mTabPersistencePolicy = persistencePolicy;
        mTabFactory = tabFactory;
        mCustomTabObserver = activity.getCustomTabObserver();
        mTabNavigationEventObserver = activity.getCustomTabNavigationEventObserver();
        mActivityTabProvider = activityTabProvider;
        mTabProvider = activity.getCustomTabActivityTabProvider();
        mSavedInstanceStateSupplier = savedInstanceStateSupplier;
        mWindowAndroid = windowAndroid;
        mTabModelInitializer = tabModelInitializer;
        mCipherFactory = activity.getCipherFactory();

        mSession = mIntentDataProvider.getSession();
        mIntent = mIntentDataProvider.getIntent();

        lifecycleDispatcher.register(this);
    }

    /**
     * @return whether allocating a child connection is needed during native initialization.
     */
    public boolean shouldAllocateChildConnection() {
        boolean hasSpeculated = !TextUtils.isEmpty(mTabProvider.getSpeculatedUrl());
        int mode = mTabProvider.getInitialTabCreationMode();
        return mode != TabCreationMode.EARLY
                && mode != TabCreationMode.HIDDEN
                && !hasSpeculated
                && !WarmupManager.getInstance().hasSpareWebContents();
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

        ReparentingTask.from(tab).begin(mActivity, intent, startActivityOptions, finishCallback);
    }

    /**
     * Closes the current tab. This doesn't necessarily lead to closing the entire activity, in case
     * links with target="_blank" were followed. See the comment to {@link
     * CustomTabActivityTabProvider.Observer#onAllTabsClosed}.
     */
    public void closeTab() {
        TabModel model = mTabFactory.getTabModelSelector().getCurrentModel();
        Tab currentTab = mTabProvider.getTab();
        model.closeTabs(TabClosureParams.closeTab(currentTab).allowUndo(false).build());
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
        assert currentTab != null;
        if (currentTab.getWebContents() != null
                && currentTab.getWebContents().needToFireBeforeUnloadOrUnloadEvents()) {
            currentTab.getWebContents().dispatchBeforeUnload(false);
            return true;
        }
        return false;
    }

    public void closeAndForgetTab() {
        mTabFactory.getTabModelSelector().closeAllTabs(true);
        mTabPersistencePolicy.deleteMetadataStateFileAsync();
    }

    public void saveState() {
        mTabFactory.getTabModelOrchestrator().saveState();
    }

    public TabModelSelector getTabModelSelector() {
        return mTabFactory.getTabModelSelector();
    }

    public void setUpInitialTab(Tab hiddenTab) {
        if (mSavedInstanceStateSupplier.get() == null
                && CustomTabsConnection.getInstance().hasWarmUpBeenFinished()) {
            mTabModelInitializer.initializeTabModels();

            if (hiddenTab == null) {
                mTabProvider.setInitialTab(createTab(), TabCreationMode.EARLY);
            } else {
                mTabProvider.setInitialTab(hiddenTab, TabCreationMode.HIDDEN);
                initializeTab(hiddenTab, true);
            }
        }
    }

    private void ensureCookiesFetcher() {
        if (mCookiesFetcher != null) return;
        mCookiesFetcher =
                new CustomTabCookiesFetcher(
                        mProfileProviderSupplier.get(), mCipherFactory, mActivity.getTaskId());
    }

    @Override
    public void onPauseWithNative() {
        if (mIntentDataProvider.isOffTheRecord()) {
            ensureCookiesFetcher();
            mCookiesFetcher.persistCookies();
        }
    }

    @Override
    public void onResumeWithNative() {}

    // Intentionally not using lifecycle.DestroyObserver because that is notified after the tab
    // models have been destroyed and that would result in the Profile destruction triggering
    // the deletion of any saved Cookie state.
    @Override
    public void destroy() {
        if (mCookiesFetcher != null) {
            mCookiesFetcher.destroy();
            mCookiesFetcher = null;
        }
    }

    public void finishNativeInitialization() {
        // If extra headers have been passed, cancel any current speculation, as
        // speculation doesn't support extra headers.
        if (IntentHandler.getExtraHeadersFromIntent(mIntent) != null) {
            CustomTabsConnection.getInstance().cancelSpeculation(mSession);
        }

        // Ensure OTR cookies are restored before attempting to restore / create the initial tab.
        // This logic does not need to happen on pre-warm starts because those instances are never
        // resuming a previously killed task, which are the only instances where restoring cookie
        // state is needed.
        boolean hadCipherData = mCipherFactory.restoreFromBundle(mSavedInstanceStateSupplier.get());
        if (hadCipherData && mIntentDataProvider.isOffTheRecord()) {
            // Ensure the Profile has been created.
            mProfileProviderSupplier.get().getOffTheRecordProfile(true);
            ensureCookiesFetcher();
            mCookiesFetcher.restoreCookies(this::finishTabInitializationPostNative);
        } else {
            finishTabInitializationPostNative();
        }
    }

    private void finishTabInitializationPostNative() {
        TabModelOrchestrator tabModelOrchestrator = mTabFactory.getTabModelOrchestrator();
        TabModelSelectorBase tabModelSelector = tabModelOrchestrator.getTabModelSelector();

        TabModel tabModel = tabModelSelector.getModel(mIntentDataProvider.isOffTheRecord());
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

        updateEngagementSignalsHandler();
    }

    // Creates the tab on native init, if it hasn't been created yet, and does all the additional
    // initialization steps necessary at this stage.
    private void finalizeCreatingTab(TabModelOrchestrator tabModelOrchestrator, TabModel tabModel) {
        Tab earlyCreatedTab = mTabProvider.getTab();

        Tab tab = earlyCreatedTab;
        @TabCreationMode int mode = mTabProvider.getInitialTabCreationMode();

        Tab restoredTab = tryRestoringTab(tabModelOrchestrator);
        if (restoredTab != null) {
            assert earlyCreatedTab == null
                    : "Shouldn't create a new tab when there's one to restore";
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
                    (TabReparentingParams)
                            AsyncTabParamsManagerSingleton.getInstance().remove(tab.getId());
            ReparentingTask.from(tab)
                    .finish(
                            ReparentingDelegateFactory.createReparentingTaskDelegate(
                                    mCompositorViewHolder.get(),
                                    mWindowAndroid,
                                    mCustomTabDelegateFactory.get()),
                            (params == null ? null : params.getFinalizeCallback()));
        }

        if (tab != earlyCreatedTab) {
            mTabProvider.setInitialTab(tab, mode);
        } // else we've already set the initial tab.

        // Listen to tab swapping and closing.
        mActivityTabProvider.addObserver(mTabProvider::swapTab);
    }

    private @Nullable Tab tryRestoringTab(TabModelOrchestrator tabModelOrchestrator) {
        if (mSavedInstanceStateSupplier.get() == null) return null;

        boolean hadCipherData = mCipherFactory.restoreFromBundle(mSavedInstanceStateSupplier.get());
        if (!hadCipherData && mIntentDataProvider.isOffTheRecord()) return null;

        tabModelOrchestrator.loadState(/* ignoreIncognitoFiles= */ false, null);
        tabModelOrchestrator.restoreTabs(true);
        Tab tab = tabModelOrchestrator.getTabModelSelector().getCurrentTab();
        if (tab != null) {
            initializeTab(tab, false);
        }
        return tab;
    }

    /** Encapsulates CustomTabsConnection#takeHiddenTab() with additional initialization logic. */
    public static @Nullable HiddenTab getHiddenTab(
            BrowserServicesIntentDataProvider intentDataProvider) {
        String url = intentDataProvider.getUrlToLoad();
        String referrerUrl =
                IntentHandler.getReferrerUrlIncludingExtraHeaders(intentDataProvider.getIntent());
        CustomTabsSessionToken token = intentDataProvider.getSession();
        HiddenTab hiddenTab =
                CustomTabsConnection.getInstance().takeHiddenTab(token, url, referrerUrl);
        if (hiddenTab == null) return null;
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.WebContentsStateOnLaunch",
                WebContentsState.PRERENDERED_WEBCONTENTS,
                WebContentsState.NUM_ENTRIES);
        TabAssociatedApp.from(hiddenTab.tab)
                .setAppId(CustomTabsConnection.getInstance().getClientPackageNameForSession(token));

        return hiddenTab;
    }

    private Tab createTab() {
        WarmupManager warmupManager = WarmupManager.getInstance();
        Profile profile =
                ProfileProvider.getOrCreateProfile(
                        mProfileProviderSupplier.get(), mIntentDataProvider.isOffTheRecord());
        Tab tab = null;
        if (WarmupManager.getInstance().isCctPrewarmTabFeatureEnabled(true)
                && warmupManager.hasSpareTab(profile, mIntentDataProvider.hasTargetNetwork())) {
            tab = warmupManager.takeSpareTab(profile, TabLaunchType.FROM_EXTERNAL_APP);
            TabAssociatedApp.from(tab)
                    .setAppId(
                            CustomTabsConnection.getInstance()
                                    .getClientPackageNameForSession(mSession));
            ReparentingTask.from(tab)
                    .finish(
                            ReparentingDelegateFactory.createReparentingTaskDelegate(
                                    null, mWindowAndroid, mCustomTabDelegateFactory.get()),
                            null);

            tab.getWebContents().updateWebContentsVisibility(Visibility.VISIBLE);
        } else {
            WebContents webContents = takeWebContents();
            Callback<Tab> tabCallback =
                    preInitTab ->
                            TabAssociatedApp.from(preInitTab)
                                    .setAppId(
                                            CustomTabsConnection.getInstance()
                                                    .getClientPackageNameForSession(mSession));
            tab = mTabFactory.createTab(webContents, mCustomTabDelegateFactory.get(), tabCallback);
        }

        initializeTab(tab, false);

        if (mIntentDataProvider.getTranslateLanguage() != null) {
            TranslateBridge.setPredefinedTargetLanguage(
                    tab,
                    mIntentDataProvider.getTranslateLanguage(),
                    mIntentDataProvider.shouldAutoTranslate());
        }

        return tab;
    }

    private void recordWebContentsStateOnLaunch(int webContentsStateOnLaunch) {
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.WebContentsStateOnLaunch",
                webContentsStateOnLaunch,
                WebContentsState.NUM_ENTRIES);
    }

    private WebContents takeWebContents() {
        WebContents webContents = takeAsyncWebContents();
        if (webContents != null) {
            recordWebContentsStateOnLaunch(WebContentsState.TRANSFERRED_WEBCONTENTS);
            webContents.resumeLoadingCreatedWebContents();
            return webContents;
        }

        webContents =
                WarmupManager.getInstance()
                        .takeSpareWebContents(
                                mIntentDataProvider.isOffTheRecord(),
                                /* initiallyHidden= */ false,
                                mIntentDataProvider.hasTargetNetwork());
        if (webContents != null) {
            recordWebContentsStateOnLaunch(WebContentsState.SPARE_WEBCONTENTS);
            return webContents;
        }

        recordWebContentsStateOnLaunch(WebContentsState.NO_WEBCONTENTS);
        return WebContentsFactory.createWebContentsWithWarmRenderer(
                ProfileProvider.getOrCreateProfile(
                        mProfileProviderSupplier.get(), mIntentDataProvider.isOffTheRecord()),
                /* initiallyHidden= */ false,
                mIntentDataProvider.getTargetNetwork());
    }

    private @Nullable WebContents takeAsyncWebContents() {
        // Async WebContents are not supported for Incognito/Ephemeral CCT.
        if (mIntentDataProvider.isOffTheRecord()) return null;
        // Async WebContents are not supported for multi-network CCT. In this case it's better to
        // always create WebContents from scratch, otherwise we might break the "WebContents
        // associated with a CCT tab targeting a network will always have
        // WebContents::GetTargetNetwork == that target network" invariant (see
        // WebContentsImpl::CreateWithOpener for more info).
        if (mIntentDataProvider.hasTargetNetwork()) return null;
        int assignedTabId = IntentHandler.getTabId(mIntent);
        AsyncTabParams asyncParams =
                AsyncTabParamsManagerSingleton.getInstance().remove(assignedTabId);
        if (asyncParams == null) return null;
        return asyncParams.getWebContents();
    }

    private void initializeTab(Tab tab, boolean isHiddenTab) {
        // TODO(pkotwicz): Determine whether these should be done for webapps.
        if (!mIntentDataProvider.isWebappOrWebApkActivity()) {
            RedirectHandlerTabHelper.updateIntentInTab(tab, mIntent);
            tab.getView().requestFocus();
        }

        if (mIntentDataProvider.isTrustedWebActivity()
                && TwaOfflineDataProvider.from(tab) == null) {
            TwaOfflineDataProvider.createFor(
                    tab,
                    mIntentDataProvider.getUrlToLoad(),
                    mIntentDataProvider.getTrustedWebActivityAdditionalOrigins(),
                    mIntentDataProvider.getClientPackageName());
        }

        if (!tab.isOffTheRecord()) {
            TabObserver observer =
                    new EmptyTabObserver() {
                        @Override
                        public void onContentChanged(Tab tab) {
                            if (tab.getWebContents() != null) {
                                CustomTabsConnection.getInstance()
                                        .setClientDataHeaderForNewTab(
                                                mSession, tab.getWebContents());
                            }
                        }
                    };
            tab.addObserver(observer);
            observer.onContentChanged(tab);
        }

        if (!isHiddenTab) {
            addTabNavigationObservers(
                    mTabObserverRegistrar,
                    mCustomTabObserver,
                    mTabNavigationEventObserver,
                    tab,
                    mSession);
        }

        prepareTabBackground(tab);
        mCustomTabObserver.setLongPressLinkSelectText(tab, mIntentDataProvider.isAuthTab());
    }

    public static void addTabNavigationObservers(
            TabObserverRegistrar registrar,
            CustomTabObserver customTabObserver,
            CustomTabNavigationEventObserver customTabNavigationEventObserver,
            Tab tab,
            CustomTabsSessionToken token) {
        registrar.registerTabObserver(customTabObserver);
        registrar.registerTabObserver(customTabNavigationEventObserver);
        registrar.registerPageLoadMetricsObserver(new PageLoadMetricsObserver(token, tab));
        registrar.registerPageLoadMetricsObserver(
                new FirstMeaningfulPaintObserver(customTabObserver, tab));

        // Immediately add the observer to PageLoadMetrics to catch early events that may
        // be generated in the middle of tab initialization.
        registrar.addObserversForTab(tab);
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
        EmptyTabObserver mediaObserver =
                new EmptyTabObserver() {
                    @Override
                    public void didFirstVisuallyNonEmptyPaint(final Tab tab) {
                        tab.removeObserver(this);

                        Runnable finishedCallback =
                                () -> {
                                    if (tab.isInitialized()
                                            && !ActivityUtils.isActivityFinishingOrDestroyed(
                                                    mActivity)) {
                                        tab.getView().setBackgroundResource(0);
                                    }
                                };
                        // Blink has rendered the page by this point, but we need to wait for the
                        // compositor frame swap to avoid flash of white content.
                        mCompositorViewHolder
                                .get()
                                .getCompositorView()
                                .surfaceRedrawNeededAsync(finishedCallback);
                    }
                };

        tab.addObserver(mediaObserver);
    }

    public void updateEngagementSignalsHandler() {
        var handler = CustomTabsConnection.getInstance().getEngagementSignalsHandler(mSession);
        if (handler == null) return;
        handler.setTabObserverRegistrar(mTabObserverRegistrar);
    }
}
