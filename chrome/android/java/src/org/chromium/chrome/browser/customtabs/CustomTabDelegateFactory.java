// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.ACTIVITY_TYPE;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.trusted.TrustedWebActivityDisplayMode.ImmersiveMode;

import dagger.Lazy;

import org.chromium.base.CallbackUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.app.tab_activity_glue.ActivityTabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.browserservices.permissiondelegation.InstalledWebappPermissionManager;
import org.chromium.chrome.browser.browserservices.ui.controller.AuthTabVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulatorFactory;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.init.ChromeActivityNativeDelegate;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.native_page.NativePageFactory;
import org.chromium.chrome.browser.pdf.PdfInfo;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabContextMenuItemDelegate;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.browser_ui.util.ComposedBrowserControlsVisibilityDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.external_intents.ExternalIntentsFeatures;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.util.List;

import javax.inject.Inject;
import javax.inject.Named;

/**
 * A {@link TabDelegateFactory} class to be used in all {@link Tab} owned by a {@link
 * CustomTabActivity}.
 */
@ActivityScope
public class CustomTabDelegateFactory implements TabDelegateFactory {
    /** Action for do-nothing activity for activating WebAPK. */
    private static final String ACTION_ACTIVATE_WEBAPK =
            "org.chromium.chrome.browser.webapps.ActivateWebApkActivity.ACTIVATE";

    /** A custom external navigation delegate that forbids the intent picker from showing up. */
    static class CustomTabNavigationDelegate extends ExternalNavigationDelegateImpl {
        private static final String TAG = "customtabs";
        private final String mClientPackageName;
        private final ExternalAuthUtils mExternalAuthUtils;
        private final Verifier mVerifier;
        private final @ActivityType int mActivityType;
        private final BrowserServicesIntentDataProvider mIntentDataProvider;
        private final Activity mActivity;
        private final Lazy<AuthTabVerifier> mAuthTabVerifier;

        /** Constructs a new instance of {@link CustomTabNavigationDelegate}. */
        CustomTabNavigationDelegate(
                Tab tab,
                ExternalAuthUtils authUtils,
                Verifier verifier,
                @ActivityType int activityType,
                BrowserServicesIntentDataProvider intentDataProvider,
                Lazy<AuthTabVerifier> authTabVerifier,
                Activity activity) {
            super(tab);
            mClientPackageName = TabAssociatedApp.from(tab).getAppId();
            mExternalAuthUtils = authUtils;
            mVerifier = verifier;
            mActivityType = activityType;
            mIntentDataProvider = intentDataProvider;
            mAuthTabVerifier = authTabVerifier;
            mActivity = activity;
        }

        @Override
        public void setPackageForTrustedCallingApp(Intent intent) {
            assert !TextUtils.isEmpty(mClientPackageName);
            intent.setPackage(mClientPackageName);
        }

        @Override
        public boolean shouldAvoidDisambiguationDialog(GURL intentDataUrl) {
            // Don't show the disambiguation dialog if Chrome could handle the intent.
            return UrlUtilities.isAcceptedScheme(intentDataUrl);
        }

        @Override
        public boolean isForTrustedCallingApp(Supplier<List<ResolveInfo>> resolveInfoSupplier) {
            if (TextUtils.isEmpty(mClientPackageName)) return false;
            if (!ExternalIntentsFeatures.TRUSTED_CLIENT_GESTURE_BYPASS.isEnabled()
                    && !mExternalAuthUtils.isGoogleSigned(mClientPackageName)) {
                return false;
            }

            return ExternalNavigationHandler.resolveInfoContainsPackage(
                    resolveInfoSupplier.get(), mClientPackageName);
        }

        @Override
        public boolean shouldDisableExternalIntentRequestsForUrl(GURL url) {
            // http://crbug.com/647569 : Do not forward URL requests to external intents for URLs
            // within the Webapp/TWA's scope.
            // TODO(crbug.com/40549331): Migrate verifier hierarchy to GURL.
            return mVerifier != null && mVerifier.shouldIgnoreExternalIntentHandlers(url.getSpec());
        }

        @Override
        public boolean shouldDisableAllExternalIntents() {
            return mActivityType == ActivityType.AUTH_TAB
                    && ChromeFeatureList.sCctAuthTabDisableAllExternalIntents.isEnabled();
        }

        @Override
        public boolean shouldReturnAsActivityResult(GURL url) {
            if (mActivityType != ActivityType.AUTH_TAB) return false;

            var authTabVerifier = mAuthTabVerifier.get();
            return authTabVerifier.isCustomScheme(url)
                    || authTabVerifier.shouldRedirectHttpsAuthUrl(url);
        }

        @Override
        public void returnAsActivityResult(GURL url) {
            assert mIntentDataProvider.isAuthTab();
            mAuthTabVerifier.get().returnAsActivityResult(url);
        }

        @Override
        public void maybeRecordExternalNavigationSchemeHistogram(GURL url) {
            if (mIntentDataProvider.isAuthTab()) return;

            // Only record for Custom Tabs that we think are launched for auth purposes.
            Uri urlToLoad = Uri.parse(mIntentDataProvider.getUrlToLoad());
            if (!urlToLoad.isHierarchical()) return;

            String redirectUri = urlToLoad.getQueryParameter("redirect_uri");
            if (TextUtils.isEmpty(redirectUri)) return;

            int schemeEnum = CustomTabAuthUrlHeuristics.getAuthSchemeEnum(url.getScheme());
            RecordHistogram.recordEnumeratedHistogram(
                    "CustomTabs.AuthTab.ExternalNavigationScheme",
                    schemeEnum,
                    CustomTabAuthUrlHeuristics.AuthScheme.COUNT);
        }

        public void resumeDelayedVerificationForTesting() {
            mAuthTabVerifier.get().onFinishNativeInitialization();
        }
    }

    private static class CustomTabWebContentsDelegate
            extends ActivityTabWebContentsDelegateAndroid {
        private static final String TAG = "CustomTabWebContentsDelegate";

        private final Activity mActivity;
        private final @ActivityType int mActivityType;
        private final @Nullable String mWebApkScopeUrl;
        private final @Nullable BrowserServicesIntentDataProvider mIntentDataProvider;
        private final @DisplayMode.EnumType int mDisplayMode;
        private final MultiWindowUtils mMultiWindowUtils;
        private final boolean mShouldEnableEmbeddedMediaExperience;
        private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;

        /** See {@link TabWebContentsDelegateAndroid}. */
        public CustomTabWebContentsDelegate(
                Tab tab,
                Activity activity,
                @ActivityType int activityType,
                @Nullable String webApkScopeUrl,
                @Nullable BrowserServicesIntentDataProvider intentDataProvider,
                @DisplayMode.EnumType int displayMode,
                MultiWindowUtils multiWindowUtils,
                boolean shouldEnableEmbeddedMediaExperience,
                ChromeActivityNativeDelegate chromeActivityNativeDelegate,
                boolean isCustomTab,
                BrowserControlsStateProvider browserControlsStateProvider,
                FullscreenManager fullscreenManager,
                TabCreatorManager tabCreatorManager,
                Supplier<TabModelSelector> tabModelSelectorSupplier,
                Supplier<CompositorViewHolder> compositorViewHolderSupplier,
                Supplier<ModalDialogManager> modalDialogManagerSupplier) {
            super(
                    tab,
                    activity,
                    chromeActivityNativeDelegate,
                    isCustomTab,
                    browserControlsStateProvider,
                    fullscreenManager,
                    tabCreatorManager,
                    tabModelSelectorSupplier,
                    compositorViewHolderSupplier,
                    modalDialogManagerSupplier);
            mActivity = activity;
            mActivityType = activityType;
            mWebApkScopeUrl = webApkScopeUrl;
            mIntentDataProvider = intentDataProvider;
            mDisplayMode = displayMode;
            mMultiWindowUtils = multiWindowUtils;
            mShouldEnableEmbeddedMediaExperience = shouldEnableEmbeddedMediaExperience;
            mModalDialogManagerSupplier = modalDialogManagerSupplier;
        }

        @Override
        public boolean shouldResumeRequestsForCreatedWindow() {
            return true;
        }

        @Override
        protected void bringActivityToForeground() {
            ((ActivityManager) mActivity.getSystemService(Context.ACTIVITY_SERVICE))
                    .moveTaskToFront(mActivity.getTaskId(), 0);
        }

        @Override
        protected boolean shouldEnableEmbeddedMediaExperience() {
            return mShouldEnableEmbeddedMediaExperience;
        }

        @Override
        public boolean isTrustedWebActivity(WebContents webContents) {
            // Note that `shouldIgnore` simply checks if `webContents` has an origin that the TWA
            // considers to be trusted.  This is a weaker check than `verify()`, but is also
            // synchronous.  For now, this is a good trade since we're only used for deciding if
            // unmuted autoplay should be allowed without a user gesture.
            GURL url = webContents.getLastCommittedUrl();
            return url != null
                    && mIntentDataProvider != null
                    && mIntentDataProvider.isTrustedWebActivity()
                    && mIntentDataProvider
                            .getAllTrustedWebActivityOrigins()
                            .contains(Origin.create(url.getSpec()));
        }

        @Override
        public @DisplayMode.EnumType int getDisplayMode() {
            return mDisplayMode;
        }

        @Override
        protected String getManifestScope() {
            return mWebApkScopeUrl;
        }

        @Override
        public boolean canShowAppBanners() {
            return mActivityType == ActivityType.CUSTOM_TAB;
        }

        @Override
        protected boolean isInstalledWebappDelegateGeolocation() {
            if ((mActivity instanceof CustomTabActivity cctActivity) && cctActivity.isInTwaMode()) {
                // Whether the corresponding TWA client app enrolled in location delegation.
                return InstalledWebappPermissionManager.hasAndroidLocationPermission(
                                cctActivity.getTwaPackage())
                        != null;
            }
            return false;
        }
    }

    private final Activity mActivity;
    private final boolean mShouldHideBrowserControls;
    private final boolean mIsOpenedByChrome;
    private final @ActivityType int mActivityType;
    @Nullable private final String mWebApkScopeUrl;
    private final @Nullable BrowserServicesIntentDataProvider mIntentDataProvider;
    private final @DisplayMode.EnumType int mDisplayMode;
    private final boolean mShouldEnableEmbeddedMediaExperience;
    private final BrowserControlsVisibilityDelegate mBrowserStateVisibilityDelegate;
    private final ExternalAuthUtils mExternalAuthUtils;
    private final MultiWindowUtils mMultiWindowUtils;
    private final Verifier mVerifier;
    private final ChromeActivityNativeDelegate mChromeActivityNativeDelegate;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final FullscreenManager mFullscreenManager;
    private final TabCreatorManager mTabCreatorManager;
    private final BrowserControlsManager mBrowserControlsManager;
    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private final Supplier<CompositorViewHolder> mCompositorViewHolderSupplier;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    // Should only be used after inflation.
    private final Lazy<SnackbarManager> mSnackbarManager;
    private final Supplier<ShareDelegate> mShareDelegateSupplier;
    // Should only be used after inflation.
    private final Lazy<BottomSheetController> mBottomSheetController;
    private final Lazy<AuthTabVerifier> mAuthTabVerifier;
    private final boolean mContextMenuEnabled;

    private TabWebContentsDelegateAndroid mWebContentsDelegateAndroid;
    private ExternalNavigationDelegateImpl mNavigationDelegate;
    private Supplier<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;

    /**
     * @param activity {@link Activity} instance.
     * @param shouldHideBrowserControls Whether or not the browser controls may auto-hide.
     * @param isOpenedByChrome Whether the CustomTab was originally opened by Chrome.
     * @param webApkScopeUrl The URL of the WebAPK web manifest scope. Null if the delegate is not
     *     for a WebAPK.
     * @param intentDataProvider Used to verify if an origin is trusted for TWAs.
     * @param displayMode The activity's display mode.
     * @param shouldEnableEmbeddedMediaExperience Whether embedded media experience is enabled.
     * @param visibilityDelegate The delegate that handles browser control visibility associated
     *     with browser actions (as opposed to tab state).
     * @param authUtils To determine whether apps are Google signed.
     * @param multiWindowUtils To use to determine which ChromeTabbedActivity to open new tabs in.
     * @param verifier Decides how to handle navigation to a new URL.
     * @param chromeActivityNativeDelegate Delegate for native initialziation.
     * @param browserControlsStateProvider Provides state of the browser controls.
     * @param fullscreenManager Manages the fullscreen state.
     * @param tabCreatorManager Manages the tab creators.
     * @param tabModelSelectorSupplier Supplies the tab model selector.
     * @param compositorViewHolderSupplier Supplies the compositor view holder.
     * @param modalDialogManagerSupplier Supplies the modal dialogManager.
     * @param snackbarManager Manages the snackbar.
     * @param shareDelegateSupplier Supplies the share delegate.
     * @param activityType The type of the current activity.
     * @param bottomSheetController Controls the bottom sheet.
     * @param contextMenuEnabled Whether the context menu will be enabled.
     * @param browserControlsManager Manages the browser controls.
     */
    private CustomTabDelegateFactory(
            Activity activity,
            boolean shouldHideBrowserControls,
            boolean isOpenedByChrome,
            @Nullable String webApkScopeUrl,
            @Nullable BrowserServicesIntentDataProvider intentDataProvider,
            @DisplayMode.EnumType int displayMode,
            boolean shouldEnableEmbeddedMediaExperience,
            BrowserControlsVisibilityDelegate visibilityDelegate,
            ExternalAuthUtils authUtils,
            MultiWindowUtils multiWindowUtils,
            Verifier verifier,
            ChromeActivityNativeDelegate chromeActivityNativeDelegate,
            BrowserControlsStateProvider browserControlsStateProvider,
            FullscreenManager fullscreenManager,
            TabCreatorManager tabCreatorManager,
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            Supplier<CompositorViewHolder> compositorViewHolderSupplier,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            Lazy<SnackbarManager> snackbarManager,
            Supplier<ShareDelegate> shareDelegateSupplier,
            @Named(ACTIVITY_TYPE) @ActivityType int activityType,
            Lazy<BottomSheetController> bottomSheetController,
            Lazy<AuthTabVerifier> authTabVerifier,
            boolean contextMenuEnabled,
            BrowserControlsManager browserControlsManager) {
        mActivity = activity;
        mShouldHideBrowserControls = shouldHideBrowserControls;
        mIsOpenedByChrome = isOpenedByChrome;
        mWebApkScopeUrl = webApkScopeUrl;
        mIntentDataProvider = intentDataProvider;
        mDisplayMode = displayMode;
        mShouldEnableEmbeddedMediaExperience = shouldEnableEmbeddedMediaExperience;
        mBrowserStateVisibilityDelegate = visibilityDelegate;
        mExternalAuthUtils = authUtils;
        mMultiWindowUtils = multiWindowUtils;
        mVerifier = verifier;
        mChromeActivityNativeDelegate = chromeActivityNativeDelegate;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mFullscreenManager = fullscreenManager;
        mTabCreatorManager = tabCreatorManager;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mCompositorViewHolderSupplier = compositorViewHolderSupplier;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mSnackbarManager = snackbarManager;
        mShareDelegateSupplier = shareDelegateSupplier;
        mActivityType = activityType;
        mBottomSheetController = bottomSheetController;
        mAuthTabVerifier = authTabVerifier;
        mBrowserControlsManager = browserControlsManager;
        mContextMenuEnabled = contextMenuEnabled;
    }

    @Inject
    public CustomTabDelegateFactory(
            Activity activity,
            BrowserServicesIntentDataProvider intentDataProvider,
            CustomTabBrowserControlsVisibilityDelegate visibilityDelegate,
            ExternalAuthUtils authUtils,
            MultiWindowUtils multiWindowUtils,
            Verifier verifier,
            ChromeActivityNativeDelegate chromeActivityNativeDelegate,
            BrowserControlsStateProvider browserControlsStateProvider,
            FullscreenManager fullscreenManager,
            TabCreatorManager tabCreatorManager,
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            Supplier<CompositorViewHolder> compositorViewHolderSupplier,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            Lazy<SnackbarManager> snackbarManager,
            Supplier<ShareDelegate> shareDelegateSupplier,
            @Named(ACTIVITY_TYPE) @ActivityType int activityType,
            Lazy<BottomSheetController> bottomSheetController,
            Lazy<AuthTabVerifier> authTabVerifier,
            BrowserControlsManager browserControlsManager) {
        this(
                activity,
                intentDataProvider.shouldEnableUrlBarHiding(),
                intentDataProvider.isOpenedByChrome(),
                getWebApkScopeUrl(intentDataProvider),
                intentDataProvider,
                getDisplayMode(intentDataProvider),
                intentDataProvider.shouldEnableEmbeddedMediaExperience(),
                visibilityDelegate,
                authUtils,
                multiWindowUtils,
                verifier,
                chromeActivityNativeDelegate,
                browserControlsStateProvider,
                fullscreenManager,
                tabCreatorManager,
                tabModelSelectorSupplier,
                compositorViewHolderSupplier,
                modalDialogManagerSupplier,
                snackbarManager,
                shareDelegateSupplier,
                activityType,
                bottomSheetController,
                authTabVerifier,
                !intentDataProvider.isAuthTab(),
                browserControlsManager);
    }

    /**
     * Creates a basic/empty {@link TabDelegateFactory} for use when creating a hidden tab. It will
     * be replaced when the hidden Tab becomes shown.
     */
    public static CustomTabDelegateFactory createEmpty() {
        return new CustomTabDelegateFactory(
                null,
                false,
                false,
                null,
                null,
                DisplayMode.BROWSER,
                false,
                null,
                null,
                null,
                null,
                null,
                null,
                null,
                null,
                () -> null,
                () -> null,
                () -> null,
                null,
                null,
                ActivityType.CUSTOM_TAB,
                null,
                null,
                false,
                null);
    }

    @Override
    public BrowserControlsVisibilityDelegate createBrowserControlsVisibilityDelegate(Tab tab) {
        TabStateBrowserControlsVisibilityDelegate tabDelegate =
                new TabStateBrowserControlsVisibilityDelegate(tab) {
                    @Override
                    protected int calculateVisibilityConstraints() {
                        @BrowserControlsState
                        int constraints = super.calculateVisibilityConstraints();
                        if (constraints == BrowserControlsState.BOTH
                                && !mShouldHideBrowserControls) {
                            return BrowserControlsState.SHOWN;
                        }
                        return constraints;
                    }
                };

        // mBrowserStateVisibilityDelegate == null for background tabs for which
        // fullscreen state info from BrowserStateVisibilityDelegate is not available.
        return mBrowserStateVisibilityDelegate == null
                ? tabDelegate
                : new ComposedBrowserControlsVisibilityDelegate(
                        tabDelegate, mBrowserStateVisibilityDelegate);
    }

    @Override
    public TabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab) {
        mWebContentsDelegateAndroid =
                new CustomTabWebContentsDelegate(
                        tab,
                        mActivity,
                        mActivityType,
                        mWebApkScopeUrl,
                        mIntentDataProvider,
                        mDisplayMode,
                        mMultiWindowUtils,
                        mShouldEnableEmbeddedMediaExperience,
                        mChromeActivityNativeDelegate,
                        /* isCustomTab= */ true,
                        mBrowserControlsStateProvider,
                        mFullscreenManager,
                        mTabCreatorManager,
                        mTabModelSelectorSupplier,
                        mCompositorViewHolderSupplier,
                        mModalDialogManagerSupplier);
        return mWebContentsDelegateAndroid;
    }

    @Override
    public ExternalNavigationHandler createExternalNavigationHandler(Tab tab) {
        if (mIsOpenedByChrome) {
            mNavigationDelegate = new ExternalNavigationDelegateImpl(tab);
        } else {
            mNavigationDelegate =
                    new CustomTabNavigationDelegate(
                            tab,
                            mExternalAuthUtils,
                            mVerifier,
                            mActivityType,
                            mIntentDataProvider,
                            mAuthTabVerifier,
                            mActivity);
        }
        return new ExternalNavigationHandler(mNavigationDelegate);
    }

    @VisibleForTesting
    TabContextMenuItemDelegate createTabContextMenuItemDelegate(Tab tab) {
        TabModelSelector tabModelSelector = mTabModelSelectorSupplier.get();
        return new TabContextMenuItemDelegate(
                mActivity,
                tab,
                tabModelSelector,
                mEphemeralTabCoordinatorSupplier,
                CallbackUtils.emptyRunnable(),
                () -> mSnackbarManager.get(),
                () -> mBottomSheetController.get(),
                mModalDialogManagerSupplier);
    }

    @Override
    public ContextMenuPopulatorFactory createContextMenuPopulatorFactory(Tab tab) {
        if (!mContextMenuEnabled) return null;

        @ChromeContextMenuPopulator.ContextMenuMode
        int contextMenuMode = getContextMenuMode(mActivityType);
        return new ChromeContextMenuPopulatorFactory(
                createTabContextMenuItemDelegate(tab),
                mShareDelegateSupplier,
                contextMenuMode,
                ExternalAuthUtils.getInstance());
    }

    @Override
    public NativePage createNativePage(
            String url, NativePage candidatePage, Tab tab, PdfInfo pdfInfo) {
        // Navigation comes from user pressing "Back to safety" on an interstitial so close the tab.
        // See crbug.com/1270695
        if (UrlConstants.NTP_URL.equals(url) && tab.isShowingErrorPage()) {
            mActivity.finish();
        }

        return NativePageFactory.createNativePageForCustomTab(
                url,
                candidatePage,
                tab,
                pdfInfo,
                mBrowserControlsManager,
                mTabModelSelectorSupplier.get(),
                mActivity);
    }

    public void setEphemeralTabCoordinatorSupplier(Supplier<EphemeralTabCoordinator> supplier) {
        mEphemeralTabCoordinatorSupplier = supplier;
    }

    /**
     * @return The {@link CustomTabNavigationDelegate} in this tab. For test purpose only.
     */
    @VisibleForTesting
    ExternalNavigationDelegateImpl getExternalNavigationDelegate() {
        return mNavigationDelegate;
    }

    public WebContentsDelegateAndroid getWebContentsDelegate() {
        return mWebContentsDelegateAndroid;
    }

    /**
     * Gets the scope URL if the {@link BrowserServicesIntentDataProvider} is for a WebAPK. Returns
     * null otherwise.
     */
    private static @Nullable String getWebApkScopeUrl(
            BrowserServicesIntentDataProvider intentDataProvider) {
        if (!intentDataProvider.isWebApkActivity()) return null;

        WebappExtras webappExtras = intentDataProvider.getWebappExtras();
        return (webappExtras != null) ? webappExtras.scopeUrl : null;
    }

    /** Returns the DisplayMode for the passed-in {@link BrowserServicesIntentDataProvider}. */
    public static @DisplayMode.EnumType int getDisplayMode(
            BrowserServicesIntentDataProvider intentDataProvider) {
        if (intentDataProvider.getTwaDisplayMode() instanceof ImmersiveMode) {
            return DisplayMode.FULLSCREEN;
        }
        WebappExtras webappExtras = intentDataProvider.getWebappExtras();
        if (webappExtras != null) {
            return webappExtras.displayMode;
        }
        return intentDataProvider.isTrustedWebActivity()
                ? DisplayMode.STANDALONE
                : DisplayMode.BROWSER;
    }

    private static boolean isWebappOrWebApk(@ActivityType int activityType) {
        return activityType == ActivityType.WEBAPP || activityType == ActivityType.WEB_APK;
    }

    private static @ChromeContextMenuPopulator.ContextMenuMode int getContextMenuMode(
            @ActivityType int activityType) {
        return isWebappOrWebApk(activityType)
                ? ChromeContextMenuPopulator.ContextMenuMode.WEB_APP
                : ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB;
    }
}
