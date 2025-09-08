// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabProfileType.INCOGNITO;

import android.app.Activity;

import org.chromium.base.CommandLine;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHost;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostRegistry;

/**
 * Implements incognito tab host for the given instance of Custom Tab activity. This class exists
 * for every custom tab, but its only active if |isEnabledIncognitoCCT| returns true.
 */
@NullMarked
public class CustomTabIncognitoManager implements NativeInitObserver, DestroyObserver {
    private final Activity mActivity;
    private final CustomTabActivityNavigationController mNavigationController;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;

    private @Nullable IncognitoCustomTabHost mIncognitoTabHost;

    private final OneshotSupplier<ProfileProvider> mProfileProviderSupplier;

    public CustomTabIncognitoManager(
            Activity activity,
            CustomTabActivityNavigationController navigationController,
            BrowserServicesIntentDataProvider intentDataProvider,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mActivity = activity;
        mIntentDataProvider = intentDataProvider;
        mNavigationController = navigationController;
        mProfileProviderSupplier = profileProviderSupplier;

        lifecycleDispatcher.register(this);
    }

    @Override
    public void onFinishNativeInitialization() {
        if (mIntentDataProvider.getCustomTabMode() == INCOGNITO) {
            initializeIncognito();
        }
    }

    @Override
    public void onDestroy() {
        if (mIncognitoTabHost != null) {
            IncognitoTabHostRegistry.getInstance().unregister(mIncognitoTabHost);
        }

        Profile otrProfile = assumeNonNull(mProfileProviderSupplier.get()).getOffTheRecordProfile();
        if (otrProfile != null) {
            ProfileManager.destroyWhenAppropriate(otrProfile);
        }
    }

    private void initializeIncognito() {
        if (mIntentDataProvider.isOpenedByChrome()) {
            mIncognitoTabHost = new IncognitoCustomTabHost();
            IncognitoTabHostRegistry.getInstance().register(mIncognitoTabHost);
        }

        maybeCreateIncognitoTabSnapshotController();
    }

    private void maybeCreateIncognitoTabSnapshotController() {
        if (!CommandLine.getInstance()
                .hasSwitch(ChromeSwitches.ENABLE_INCOGNITO_SNAPSHOTS_IN_ANDROID_RECENTS)) {
            new IncognitoCustomTabSnapshotController(
                    mActivity, () -> mIntentDataProvider.getCustomTabMode() == INCOGNITO);
        }
    }

    /**
     * This class registers itself with {@link IncognitoTabHostRegistry} only if the Incognito
     * custom tab was opened by Chrome.
     */
    private class IncognitoCustomTabHost implements IncognitoTabHost {
        @Override
        public boolean hasIncognitoTabs() {
            return !mActivity.isFinishing();
        }

        @Override
        public void closeAllIncognitoTabs() {
            mNavigationController.finish(CustomTabActivityNavigationController.FinishReason.OTHER);
        }

        @Override
        public void closeAllIncognitoTabsOnInit() {
            closeAllIncognitoTabs();
        }

        @Override
        public boolean isActiveModel() {
            return true;
        }
    }
}
