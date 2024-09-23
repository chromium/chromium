// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabProfileType.INCOGNITO;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.base.CommandLine;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHost;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostRegistry;

import javax.inject.Inject;

/**
 * Implements incognito tab host for the given instance of Custom Tab activity.
 * This class exists for every custom tab, but its only active if
 * |isEnabledIncognitoCCT| returns true.
 */
@ActivityScope
public class CustomTabIncognitoManager implements NativeInitObserver, DestroyObserver {
    private final Activity mActivity;
    private final CustomTabActivityNavigationController mNavigationController;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;

    @Nullable private IncognitoCustomTabHost mIncognitoTabHost;

    private final IncognitoTabHostRegistry mIncognitoTabHostRegistry;
    private final OneshotSupplier<ProfileProvider> mProfileProviderSupplier;

    @Inject
    public CustomTabIncognitoManager(
            Activity activity,
            BrowserServicesIntentDataProvider intentDataProvider,
            CustomTabActivityNavigationController navigationController,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            IncognitoTabHostRegistry incognitoTabHostRegistry,
            OneshotSupplier<ProfileProvider> profileProviderSupplier) {
        mActivity = activity;
        mIntentDataProvider = intentDataProvider;
        mNavigationController = navigationController;
        mIncognitoTabHostRegistry = incognitoTabHostRegistry;
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
            mIncognitoTabHostRegistry.unregister(mIncognitoTabHost);
        }

        if (mProfileProviderSupplier.get().hasOffTheRecordProfile()) {
            ProfileManager.destroyWhenAppropriate(
                    mProfileProviderSupplier
                            .get()
                            .getOffTheRecordProfile(/* createIfNeeded= */ false));
        }
    }

    public Profile getProfile() {
        return mProfileProviderSupplier.get().getOffTheRecordProfile(/* createIfNeeded= */ true);
    }

    private void initializeIncognito() {
        if (mIntentDataProvider.isOpenedByChrome()) {
            mIncognitoTabHost = new IncognitoCustomTabHost();
            mIncognitoTabHostRegistry.register(mIncognitoTabHost);
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
        public boolean isActiveModel() {
            return true;
        }

        @Override
        public boolean hasIncognitoTabs() {
            return !mActivity.isFinishing();
        }

        @Override
        public void closeAllIncognitoTabs() {
            mNavigationController.finish(CustomTabActivityNavigationController.FinishReason.OTHER);
        }
    }
}
