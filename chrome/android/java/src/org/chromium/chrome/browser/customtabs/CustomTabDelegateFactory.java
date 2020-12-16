// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.tab_activity_glue.ActivityTabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.permissiondelegation.TrustedWebActivityPermissionManager;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulatorFactory;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabContextMenuItemDelegate;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.tabmodel.AsyncTabCreationParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.webapps.WebDisplayMode;
import org.chromium.chrome.browser.webapps.WebappActivity;
import org.chromium.chrome.browser.webapps.WebappExtras;
import org.chromium.chrome.browser.webapps.WebappIntentUtils;
import org.chromium.chrome.browser.webapps.WebappLauncherActivity;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.browser_ui.util.ComposedBrowserControlsVisibilityDelegate;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * A {@link TabDelegateFactory} class to be used in all {@link Tab} owned
 * by a {@link CustomTabActivity}.
 */
@ActivityScope
public class CustomTabDelegateFactory implements TabDelegateFactory {
    /** Action for do-nothing activity for activating WebAPK. */
    private static final String ACTION_ACTIVATE_WEBAPK =
            "org.chromium.chrome.browser.webapps.ActivateWebApkActivity.ACTIVATE";

    /**
     * A custom external navigation delegate that forbids the intent picker from showing up.
     */
    static class CustomTabNavigationDelegate extends ExternalNavigationDelegateImpl {
        private static final String TAG = "customtabs";
        private final String mClientPackageName;
        private final ExternalAuthUtils mExternalAuthUtils;
        private final Verifier mVerifier;
        private final @ActivityType int mActivityType;

        private boolean mHasActivityStarted;

        /**
         * Constructs a new instance of {@link CustomTabNavigationDelegate}.
         */
        CustomTabNavigationDelegate(Tab tab, ExternalAuthUtils authUtils, Verifier verifier,
                @ActivityType int activityType) {
            super(tab);
            mClientPackageName = TabAssociatedApp.from(tab).getAppId();
            mExternalAuthUtils = authUtils;
            mVerifier = verifier;
            mActivityType = activityType;
        }

        @Override
        public void didStartActivity(Intent intent) {
            mHasActivityStarted = true;
        }

        @Override
        public @StartActivityIfNeededResult int maybeHandleStartActivityIfNeeded(
                Intent intent, boolean proxy) {
            // Note: This method will not be called if shouldDisableExternalIntentRequestsForUrl()
            // returns false.

            boolean isExternalProtocol = !UrlUtilities.isAcceptedScheme(intent.toUri(0));
            boolean hasDefaultHandler = hasDefaultHandler(intent);
            try {
                // For a URL chrome can handle and there is no default set, handle it ourselves.
                if (!hasDefaultHandler) {
                    if (!TextUtils.isEmpty(mClientPackageName)
                            && isPackageSpecializedHandler(mClientPackageName, intent)) {
                        intent.setPackage(mClientPackageName);
                    } else if (!isExternalProtocol) {
                        return StartActivityIfNeededResult.HANDLED_WITHOUT_ACTIVITY_START;
                    }
                }

                if (proxy) {
                    dispatchAuthenticatedIntent(intent);
                    mHasActivityStarted = true;
                    return StartActivityIfNeededResult.HANDLED_WITH_ACTIVITY_START;
                } else {
                    // If android fails to find a handler, handle it ourselves.
                    Context context = getAvailableContext();
                    if (context instanceof Activity
                            && ((Activity) context).startActivityIfNeeded(intent, -1)) {
                        mHasActivityStarted = true;
                        return StartActivityIfNeededResult.HANDLED_WITH_ACTIVITY_START;
                    }
                }
                return StartActivityIfNeededResult.HANDLED_WITHOUT_ACTIVITY_START;
            } catch (SecurityException e) {
                // https://crbug.com/808494: Handle the URL in Chrome if dispatching to another
                // application fails with a SecurityException. This happens due to malformed
                // manifests in another app.
                return StartActivityIfNeededResult.HANDLED_WITHOUT_ACTIVITY_START;
            } catch (RuntimeException e) {
                IntentUtils.logTransactionTooLargeOrRethrow(e, intent);
                return StartActivityIfNeededResult.HANDLED_WITHOUT_ACTIVITY_START;
            }
        }

        /**
         * Resolve the default external handler of an intent.
         * @return Whether the default external handler is found: if chrome turns out to be the
         *         default handler, this method will return false.
         */
        private boolean hasDefaultHandler(Intent intent) {
            ResolveInfo info = PackageManagerUtils.resolveActivity(intent, 0);
            if (info == null) return false;

            final String chromePackage = mApplicationContext.getPackageName();
            // If a default handler is found and it is not chrome itself, fire the intent.
            return info.match != 0 && !chromePackage.equals(info.activityInfo.packageName);
        }

        @Override
        public boolean isIntentForTrustedCallingApp(Intent intent) {
            if (TextUtils.isEmpty(mClientPackageName)) return false;
            if (!mExternalAuthUtils.isGoogleSigned(mClientPackageName)) return false;

            return isPackageSpecializedHandler(mClientPackageName, intent);
        }

        /**
         * @return Whether an external activity has started to handle a url. For testing only.
         */
        @VisibleForTesting
        public boolean hasExternalActivityStarted() {
            return mHasActivityStarted;
        }

        @Override
        public boolean shouldDisableExternalIntentRequestsForUrl(String url) {
            // http://crbug.com/647569 : Do not forward URL requests to external intents for URLs
            // within the Webapp/TWA's scope.
            return mVerifier != null && mVerifier.shouldIgnoreExternalIntentHandlers(url);
        }
    }

    private static class CustomTabWebContentsDelegate
            extends ActivityTabWebContentsDelegateAndroid {
        private static final String TAG = "CustomTabWebContentsDelegate";
        private final ChromeActivity mActivity;
        private final @ActivityType int mActivityType;
        private final @Nullable String mWebApkScopeUrl;
        private final @WebDisplayMode int mDisplayMode;
        private final MultiWindowUtils mMultiWindowUtils;
        private final boolean mShouldEnableEmbeddedMediaExperience;
        private final @Nullable PendingIntent mFocusIntent;

        /**
         * See {@link TabWebContentsDelegateAndroid}.
         */
        public CustomTabWebContentsDelegate(Tab tab, ChromeActivity activity,
                @ActivityType int activityType, @Nullable String webApkScopeUrl,
                @WebDisplayMode int displayMode, MultiWindowUtils multiWindowUtils,
                boolean shouldEnableEmbeddedMediaExperience, @Nullable PendingIntent focusIntent) {
            super(tab, activity);
            mActivity = activity;
            mActivityType = activityType;
            mWebApkScopeUrl = webApkScopeUrl;
            mDisplayMode = displayMode;
            mMultiWindowUtils = multiWindowUtils;
            mShouldEnableEmbeddedMediaExperience = shouldEnableEmbeddedMediaExperience;
            mFocusIntent = focusIntent;
        }

        @Override
        public boolean shouldResumeRequestsForCreatedWindow() {
            return true;
        }

        @Override
        protected void bringActivityToForeground() {
            // TODO: Unify WebAPK and CCT behavior.
            if (isWebappOrWebApk(mActivityType)) {
                bringActivityToForegroundWebapp();
            } else {
                bringActivityToForegroundCustomTab();
            }
        }

        private void bringActivityToForegroundCustomTab() {
            // super.bringActivityToForeground creates an Intent to ChromeLauncherActivity for the
            // current Tab. This will then bring to the foreground the Chrome Task that contains
            // the Chrome Activity displaying the tab (this will usually be a ChromeTabbedActivity).

            // Since Custom Tabs will be launched as part of the client's Task, we can't launch a
            // Chrome Intent specifically to the Chrome Activity in that Task. Therefore we must
            // use an Intent the client has provided to bring its Task to the foreground.

            // If we've not been provided with an Intent to focus the client's Task, we can't do
            // anything.
            if (mFocusIntent == null) return;

            try {
                mFocusIntent.send();
            } catch (PendingIntent.CanceledException e) {
                Log.e(TAG, "CanceledException when sending pending intent.");
            }
        }

        private void bringActivityToForegroundWebapp() {
            WebappActivity webappActivity = (WebappActivity) mActivity;
            BrowserServicesIntentDataProvider intentDataProvider =
                    webappActivity.getIntentDataProvider();

            // Create an Intent that will be fired toward the WebappLauncherActivity, which in turn
            // will fire an Intent to launch the correct WebappActivity. On L+ this could probably
            // be changed to call AppTask.moveToFront(), but for backwards compatibility we relaunch
            // it the hard way.
            String startUrl = intentDataProvider.getUrlToLoad();

            if (intentDataProvider.isWebApkActivity()) {
                Intent activateIntent = null;
                activateIntent = new Intent(ACTION_ACTIVATE_WEBAPK);
                activateIntent.setPackage(ContextUtils.getApplicationContext().getPackageName());
                IntentUtils.safeStartActivity(mActivity, activateIntent);
                return;
            }

            Intent intent = new Intent();
            intent.setAction(WebappLauncherActivity.ACTION_START_WEBAPP);
            intent.setPackage(mActivity.getPackageName());
            WebappIntentUtils.copyWebappLaunchIntentExtras(mActivity.getIntent(), intent);

            intent.putExtra(ShortcutHelper.EXTRA_MAC, ShortcutHelper.getEncodedMac(startUrl));
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            IntentUtils.safeStartActivity(ContextUtils.getApplicationContext(), intent);
        }

        @Override
        protected boolean shouldEnableEmbeddedMediaExperience() {
            return mShouldEnableEmbeddedMediaExperience;
        }

        @Override
        public void openNewTab(GURL url, String extraHeaders, ResourceRequestBody postData,
                int disposition, boolean isRendererInitiated) {
            // If attempting to open an incognito tab, always send the user to tabbed mode.
            if (disposition == WindowOpenDisposition.OFF_THE_RECORD) {
                if (isRendererInitiated) {
                    throw new IllegalStateException(
                            "Invalid attempt to open an incognito tab from the renderer");
                }
                LoadUrlParams loadUrlParams = new LoadUrlParams(url.getSpec());
                loadUrlParams.setVerbatimHeaders(extraHeaders);
                loadUrlParams.setPostData(postData);
                loadUrlParams.setIsRendererInitiated(isRendererInitiated);

                Class<? extends ChromeTabbedActivity> tabbedClass =
                        mMultiWindowUtils.getTabbedActivityForIntent(
                                null, ContextUtils.getApplicationContext());
                AsyncTabCreationParams tabParams = new AsyncTabCreationParams(loadUrlParams,
                        new ComponentName(ContextUtils.getApplicationContext(), tabbedClass));
                new TabDelegate(true).createNewTab(tabParams,
                        TabLaunchType.FROM_LONGPRESS_FOREGROUND, TabModel.INVALID_TAB_INDEX);
                return;
            }

            super.openNewTab(url, extraHeaders, postData, disposition, isRendererInitiated);
        }

        @Override
        public @WebDisplayMode int getDisplayMode() {
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
            if ((mActivity instanceof CustomTabActivity)
                    && ((CustomTabActivity) mActivity).isInTwaMode()) {
                // Whether the corresponding TWA client app enrolled in location delegation.
                return TrustedWebActivityPermissionManager.hasAndroidLocationPermission(
                               ((CustomTabActivity) mActivity).getTwaPackage())
                        != null;
            }
            return false;
        }
    }

    private final ChromeActivity<?> mActivity;
    private final boolean mShouldHideBrowserControls;
    private final boolean mIsOpenedByChrome;
    private final @ActivityType int mActivityType;
    @Nullable
    private final String mWebApkScopeUrl;
    private final @WebDisplayMode int mDisplayMode;
    private final boolean mShouldEnableEmbeddedMediaExperience;
    private final BrowserControlsVisibilityDelegate mBrowserStateVisibilityDelegate;
    private final ExternalAuthUtils mExternalAuthUtils;
    private final MultiWindowUtils mMultiWindowUtils;
    private final PendingIntent mFocusIntent;
    private final Verifier mVerifier;
    private final boolean mShouldShowOpenInChromeMenuItemInContextMenu;

    private TabWebContentsDelegateAndroid mWebContentsDelegateAndroid;
    private ExternalNavigationDelegateImpl mNavigationDelegate;
    private Lazy<EphemeralTabCoordinator> mEphemeralTabCoordinator;

    /**
     * @param activity {@link ChromeActivity} instance.
     * @param shouldHideBrowserControls Whether or not the browser controls may auto-hide.
     * @param isOpenedByChrome Whether the CustomTab was originally opened by Chrome.
     * @param webApkScopeUrl The URL of the WebAPK web manifest scope. Null if the delegate is not
     *                       for a WebAPK.
     * @param displayMode  The activity's display mode.
     * @param shouldEnableEmbeddedMediaExperience Whether embedded media experience is enabled.
     * @param visibilityDelegate The delegate that handles browser control visibility associated
     *                           with browser actions (as opposed to tab state).
     * @param authUtils To determine whether apps are Google signed.
     * @param multiWindowUtils To use to determine which ChromeTabbedActivity to open new tabs in.
     * @param focusIntent A PendingIntent to launch to focus the client.
     * @param verifier Decides how to handle navigation to a new URL.
     * @param ephemeralTabCoordinatorSupplier A provider of {@link EphemeralTabCoordinator} that
     *                                        shows preview tab.
     * @param shouldShowOpenInChromeMenuItemInContextMenu Whether 'open in chrome' is shown.
     */
    private CustomTabDelegateFactory(ChromeActivity<?> activity, boolean shouldHideBrowserControls,
            boolean isOpenedByChrome, @Nullable String webApkScopeUrl,
            @WebDisplayMode int displayMode, boolean shouldEnableEmbeddedMediaExperience,
            BrowserControlsVisibilityDelegate visibilityDelegate, ExternalAuthUtils authUtils,
            MultiWindowUtils multiWindowUtils, @Nullable PendingIntent focusIntent,
            Verifier verifier, Lazy<EphemeralTabCoordinator> ephemeralTabCoordinator,
            boolean shouldShowOpenInChromeMenuItemInContextMenu) {
        mActivity = activity;
        mShouldHideBrowserControls = shouldHideBrowserControls;
        mIsOpenedByChrome = isOpenedByChrome;
        mActivityType = (activity == null) ? ActivityType.CUSTOM_TAB : activity.getActivityType();
        mWebApkScopeUrl = webApkScopeUrl;
        mDisplayMode = displayMode;
        mShouldEnableEmbeddedMediaExperience = shouldEnableEmbeddedMediaExperience;
        mBrowserStateVisibilityDelegate = visibilityDelegate;
        mExternalAuthUtils = authUtils;
        mMultiWindowUtils = multiWindowUtils;
        mFocusIntent = focusIntent;
        mVerifier = verifier;
        mEphemeralTabCoordinator = ephemeralTabCoordinator;
        mShouldShowOpenInChromeMenuItemInContextMenu = shouldShowOpenInChromeMenuItemInContextMenu;
    }

    @Inject
    public CustomTabDelegateFactory(ChromeActivity<?> activity,
            BrowserServicesIntentDataProvider intentDataProvider,
            CustomTabBrowserControlsVisibilityDelegate visibilityDelegate,
            ExternalAuthUtils authUtils, MultiWindowUtils multiWindowUtils, Verifier verifier,
            Lazy<EphemeralTabCoordinator> ephemeralTabCoordinator) {
        this(activity, intentDataProvider.shouldEnableUrlBarHiding(),
                intentDataProvider.isOpenedByChrome(), getWebApkScopeUrl(intentDataProvider),
                getDisplayMode(intentDataProvider),
                intentDataProvider.shouldEnableEmbeddedMediaExperience(), visibilityDelegate,
                authUtils, multiWindowUtils, intentDataProvider.getFocusIntent(), verifier,
                ephemeralTabCoordinator,
                intentDataProvider.shouldShowOpenInChromeMenuItemInContextMenu());
    }

    /**
     * Creates a basic/empty CustomTabDelegateFactory for use when creating a hidden tab. It will
     * be replaced when the hidden Tab becomes shown.
     */
    static CustomTabDelegateFactory createDummy() {
        return new CustomTabDelegateFactory(null, false, false, null, WebDisplayMode.BROWSER, false,
                null, null, null, null, null, () -> null, true);
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
        mWebContentsDelegateAndroid = new CustomTabWebContentsDelegate(tab, mActivity,
                mActivityType, mWebApkScopeUrl, mDisplayMode, mMultiWindowUtils,
                mShouldEnableEmbeddedMediaExperience, mFocusIntent);
        return mWebContentsDelegateAndroid;
    }

    @Override
    public ExternalNavigationHandler createExternalNavigationHandler(Tab tab) {
        if (mIsOpenedByChrome) {
            mNavigationDelegate = new ExternalNavigationDelegateImpl(tab);
        } else {
            mNavigationDelegate = new CustomTabNavigationDelegate(
                    tab, mExternalAuthUtils, mVerifier, mActivityType);
        }
        return new ExternalNavigationHandler(mNavigationDelegate);
    }

    @VisibleForTesting
    TabContextMenuItemDelegate createTabContextMenuItemDelegate(Tab tab) {
        TabModelSelector tabModelSelector =
                mActivity != null ? mActivity.getTabModelSelector() : null;
        final boolean isIncognito = tab.isIncognito();
        return new TabContextMenuItemDelegate(tab, tabModelSelector,
                EphemeralTabCoordinator.isSupported() ? mEphemeralTabCoordinator::get : ()
                        -> null,
                () -> {}, mActivity == null ? null : mActivity::getSnackbarManager) {
            @Override
            public boolean supportsOpenInChromeFromCct() {
                return mShouldShowOpenInChromeMenuItemInContextMenu && !isIncognito;
            }
        };
    }

    @Override
    public ContextMenuPopulatorFactory createContextMenuPopulatorFactory(Tab tab) {
        @ChromeContextMenuPopulator.ContextMenuMode
        int contextMenuMode = getContextMenuMode(mActivityType);
        Supplier<ShareDelegate> shareDelegateSupplier =
                mActivity == null ? null : mActivity.getShareDelegateSupplier();
        return new ChromeContextMenuPopulatorFactory(createTabContextMenuItemDelegate(tab),
                shareDelegateSupplier, contextMenuMode, ExternalAuthUtils.getInstance());
    }

    @Override
    public NativePage createNativePage(String url, NativePage candidatePage, Tab tab) {
        // Custom tab does not create native pages.
        return null;
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

    /**
     * Returns the WebDisplayMode for the passed-in {@link BrowserServicesIntentDataProvider}.
     */
    public static @WebDisplayMode int getDisplayMode(
            BrowserServicesIntentDataProvider intentDataProvider) {
        WebappExtras webappExtras = intentDataProvider.getWebappExtras();
        if (webappExtras != null) {
            return webappExtras.displayMode;
        }
        return intentDataProvider.isTrustedWebActivity() ? WebDisplayMode.STANDALONE
                                                         : WebDisplayMode.BROWSER;
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
