// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.support.annotation.Nullable;
import android.support.customtabs.CustomTabsService;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.customtabs.CustomTabBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.TabObserverRegistrar;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.init.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;

import javax.inject.Inject;

/**
 * Class to handle the state and logic for CustomTabActivity to do with Trusted Web Activities.
 *
 * Lifecycle: There should be a 1-1 relationship between this class and
 * {@link org.chromium.chrome.browser.customtabs.CustomTabActivity}.
 */
@ActivityScope
public class TrustedWebActivityUi
        implements InflationObserver, PauseResumeWithNativeObserver, NativeInitObserver {
    /** The Digital Asset Link relationship used for Trusted Web Activities. */
    private final static int RELATIONSHIP = CustomTabsService.RELATION_HANDLE_ALL_URLS;

    private final TrustedWebActivityDisclosure mDisclosure;
    private final TrustedWebActivityOpenTimeRecorder mOpenTimeRecorder =
            new TrustedWebActivityOpenTimeRecorder();
    private final ChromeFullscreenManager mFullscreenManager;
    private final ClientAppDataRecorder mClientAppDataRecorder;
    private final CustomTabsConnection mCustomTabsConnection;
    private final CustomTabIntentDataProvider mIntentDataProvider;
    private final ActivityTabProvider mActivityTabProvider;
    private final CustomTabBrowserControlsVisibilityDelegate mControlsVisibilityDelegate;

    private boolean mInTrustedWebActivity = true;

    private int mControlsHidingToken = FullscreenManager.INVALID_TOKEN;

    /** A {@link TabObserver} that checks whether we are on a verified Origin on page navigation. */
    private final TabObserver mVerifyOnPageLoadObserver = new EmptyTabObserver() {
        @Override
        public void onDidFinishNavigation(Tab tab, String url, boolean isInMainFrame,
                boolean isErrorPage, boolean hasCommitted, boolean isSameDocument,
                boolean isFragmentNavigation, Integer pageTransition, int errorCode,
                int httpStatusCode) {
            if (!hasCommitted || !isInMainFrame) return;

            String packageName = getClientPackageName();
            assert packageName != null;

            // This doesn't perform a network request or attempt new verification - it checks to
            // see if a verification already exists for the given inputs.
            Origin origin = new Origin(url);
            boolean verified =
                    OriginVerifier.isValidOrigin(packageName, origin, RELATIONSHIP);
            if (verified) registerClientAppData(packageName, origin);
            setTrustedWebActivityMode(verified);
        }
    };

    @Inject
    public TrustedWebActivityUi(TrustedWebActivityDisclosure disclosure,
            ChromeFullscreenManager fullscreenManager, ClientAppDataRecorder clientAppDataRecorder,
            CustomTabIntentDataProvider intentDataProvider,
            CustomTabsConnection customTabsConnection,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            TabObserverRegistrar tabObserverRegistrar, ActivityTabProvider activityTabProvider,
            CustomTabBrowserControlsVisibilityDelegate controlsVisibilityDelegate) {
        mFullscreenManager = fullscreenManager;
        mClientAppDataRecorder = clientAppDataRecorder;
        mDisclosure = disclosure;
        mCustomTabsConnection = customTabsConnection;
        mIntentDataProvider = intentDataProvider;
        mActivityTabProvider = activityTabProvider;
        mControlsVisibilityDelegate = controlsVisibilityDelegate;
        tabObserverRegistrar.registerTabObserver(mVerifyOnPageLoadObserver);
        lifecycleDispatcher.register(this);
    }

    @Nullable
    private String getClientPackageName() {
        return mCustomTabsConnection.getClientPackageNameForSession(
                mIntentDataProvider.getSession());
    }

    /**
     * Shows the disclosure Snackbar if needed on the first Tab. Subsequent navigations will update
     * the disclosure state automatically.
     */
    private void initialShowSnackbarIfNeeded() {
        String packageName = getClientPackageName();
        assert packageName != null;

        // If we have left Trusted Web Activity mode (through onDidFinishNavigation), we don't need
        // to show the Snackbar.
        if (!mInTrustedWebActivity) return;

        mDisclosure.showIfNeeded(packageName);
    }

    /**
     * Perform verification for the URL that the CustomTabActivity starts on.
     */
    public void attemptVerificationForInitialUrl(String url, Tab tab) {
        String packageName = getClientPackageName();
        assert packageName != null;

        Origin origin = new Origin(url);

        new OriginVerifier((packageName2, origin2, verified, online) -> {
            if (!origin.equals(new Origin(tab.getUrl()))) return;

            BrowserServicesMetrics.recordTwaOpened();
            if (verified) registerClientAppData(packageName, origin);
            setTrustedWebActivityMode(verified);
        }, packageName, RELATIONSHIP).start(origin);
    }

    @Override
    public void onPreInflationStartup() {}

    @Override
    public void onPostInflationStartup() {
        if (mInTrustedWebActivity) {
            // Hide Android controls as soon as they are inflated.
            mControlsHidingToken = mFullscreenManager.hideAndroidControls();
            mControlsVisibilityDelegate.setTrustedWebActivityMode(true);
        }
    }

    @Override
    public void onResumeWithNative() {
        mOpenTimeRecorder.onResume();
    }

    @Override
    public void onPauseWithNative() {
        mOpenTimeRecorder.onPause();
    }

    @Override
    public void onFinishNativeInitialization() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.TRUSTED_WEB_ACTIVITY_POST_MESSAGE)) {
            mCustomTabsConnection.resetPostMessageHandlerForSession(
                    mIntentDataProvider.getSession(), null);
        }

        attemptVerificationForInitialUrl(
                mIntentDataProvider.getUrlToLoad(), mActivityTabProvider.getActivityTab());

        initialShowSnackbarIfNeeded();
    }

    /**
     * Updates the UI appropriately for whether or not Trusted Web Activity mode is enabled.
     */
    private void setTrustedWebActivityMode(boolean enabled) {
        if (mInTrustedWebActivity == enabled) return;

        mInTrustedWebActivity = enabled;
        mControlsVisibilityDelegate.setTrustedWebActivityMode(mInTrustedWebActivity);

        if (enabled) {
            mControlsHidingToken =
                    mFullscreenManager.hideAndroidControlsAndClearOldToken(mControlsHidingToken);
            mDisclosure.showIfNeeded(getClientPackageName());
        } else {
            mFullscreenManager.releaseAndroidControlsHidingToken(mControlsHidingToken);
            // Force showing the controls for a bit when leaving Trusted Web Activity mode.
            mFullscreenManager.getBrowserVisibilityDelegate().showControlsTransient();
            mDisclosure.dismiss();
        }
    }

    /**
     * Register that we have Chrome data relevant to the Client app.
     *
     * We do this here, when the Trusted Web Activity UI is shown instead of in OriginVerifier when
     * verification completes because when an origin is being verified, we don't know whether it is
     * for the purposes of Trusted Web Activities or for Post Message (where this behaviour is not
     * required).
     *
     * Additionally we do it on every page navigation because an app can be verified for more than
     * one Origin, eg:
     * 1) App verifies with https://www.myfirsttwa.com/.
     * 2) App verifies with https://www.mysecondtwa.com/.
     * 3) App launches a TWA to https://www.myfirsttwa.com/.
     * 4) App navigates to https://www.mysecondtwa.com/.
     *
     * At step 2, we don't know why the app is verifying with that origin (it could be for TWAs or
     * for PostMessage). Only at step 4 do we know that Chrome should associate the browsing data
     * for that origin with that app.
     */
    private void registerClientAppData(String packageName, Origin origin) {
        mClientAppDataRecorder.register(packageName, origin);
    }
}
