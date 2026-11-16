// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.app.Application;
import android.content.Intent;
import android.os.Bundle;
import android.view.Display;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.incognito.IncognitoDataTestUtils;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthSettingUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.ui.ExclusiveAccessManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.net.test.EmbeddedTestServerRule;

/**
 * Tests that a recreated {@link CustomTabActivity} restores state saved by the previous instance.
 *
 * <p>A cross-display {@code requestFullscreen()} recreates the activity after the window moves to
 * the other display, and the saved fullscreen options are re-entered on the new display. Multiple
 * displays cannot be emulated here, so the tests inject the saved options into the outgoing
 * activity's bundle and recreate through the same path a density change takes ({@link
 * org.chromium.chrome.browser.app.ChromeActivity#onNightModeStateChanged} calls {@code
 * doRecreateActivity()}, which reparents the tab).
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Tests recreate the Custom Tab activity and restore its saved state.")
public class CustomTabActivityRestoreSavedStateTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";

    // INVALID_DISPLAY makes the re-enter in the recreated activity a same-display request.
    private static final FullscreenOptions SAVED_FULLSCREEN_OPTIONS =
            new FullscreenOptions(
                    /* showNavigationBar= */ false,
                    /* showStatusBar= */ false,
                    /* displayId= */ Display.INVALID_DISPLAY);

    @Rule
    public IncognitoCustomTabActivityTestRule mCustomTabActivityTestRule =
            new IncognitoCustomTabActivityTestRule();

    @Rule public EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();

    private String mTestPage;
    private boolean mReauthEnabled;

    @Before
    public void setUp() throws Exception {
        FirstRunStatus.setFirstRunFlowComplete(true);
        mTestPage = mEmbeddedTestServerRule.getServer().getURL(TEST_PAGE);
        IncognitoDataTestUtils.fireAndWaitForCctWarmup();
    }

    @After
    public void tearDown() {
        if (!mReauthEnabled) return;
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                .setBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID, false));
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(false);
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(false);
    }

    @Test
    @MediumTest
    public void testRegularCustomTab_reentersSavedFullscreen() {
        CustomTabActivity activity =
                launch(
                        CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                                ApplicationProvider.getApplicationContext(), mTestPage));

        CustomTabActivity recreated =
                recreateWithSavedFullscreenOptions(activity, /* injectFullscreenOptions= */ true);

        CriteriaHelper.pollUiThread(
                () -> recreated.getFullscreenManager().getPersistentFullscreenMode(),
                "Saved fullscreen should be re-entered after recreation");
    }

    /**
     * An off-the-record Custom Tab restored with its cipher key adds its tab to the model only
     * after cookies are restored asynchronously, so there is no current tab when the saved
     * fullscreen would be re-entered. It must not crash, and it comes back not fullscreen.
     */
    @Test
    @MediumTest
    public void testIncognitoCustomTab_withoutTabAtRestore_doesNotCrash() {
        CustomTabActivity activity = launch(createIncognitoIntent());
        waitForCipherKey(activity);

        CustomTabActivity recreated =
                recreateWithSavedFullscreenOptions(activity, /* injectFullscreenOptions= */ true);

        // Without the cipher key the tab is added synchronously and this test would not exercise
        // the missing-tab path.
        Bundle savedInstanceState = recreated.getSavedInstanceState();
        assertNotNull(savedInstanceState);
        assertTrue(
                "The bundle must hold the cipher key so that tab creation is deferred",
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new CipherFactory().restoreFromBundle(savedInstanceState)));
        assertTrue(recreated.getActivityTab().isIncognito());
        assertFalse(
                ThreadUtils.runOnUiThreadBlocking(
                        () -> recreated.getFullscreenManager().getPersistentFullscreenMode()));
    }

    /**
     * The saved incognito re-auth flag is restored, so a recreated incognito Custom Tab with
     * Incognito lock enabled shows the re-auth page, as ChromeTabbedActivity does.
     */
    @Test
    @MediumTest
    public void testIncognitoCustomTab_withIncognitoLock_showsReauthAfterRecreation() {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(true);
        mReauthEnabled = true;
        CustomTabActivity activity = launch(createIncognitoIntent());
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                .setBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID, true));

        CustomTabActivity recreated =
                recreateWithSavedFullscreenOptions(activity, /* injectFullscreenOptions= */ false);

        CriteriaHelper.pollUiThread(
                () -> {
                    IncognitoReauthController controller =
                            recreated
                                    .getRootUiCoordinatorForTesting()
                                    .getIncognitoReauthControllerSupplier()
                                    .get();
                    return controller != null && controller.isReauthPageShowing();
                },
                "Re-auth page should be shown in the recreated activity");
    }

    private Intent createIncognitoIntent() {
        return CustomTabsIntentTestUtils.createMinimalIncognitoCustomTabIntent(
                ApplicationProvider.getApplicationContext(), mTestPage);
    }

    private CustomTabActivity launch(Intent intent) {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        return mCustomTabActivityTestRule.getActivity();
    }

    /**
     * Recreates {@code activity} through {@code doRecreateActivity()}, optionally adding saved
     * fullscreen options to its instance state, and returns the new activity once it is loaded.
     */
    private CustomTabActivity recreateWithSavedFullscreenOptions(
            CustomTabActivity activity, boolean injectFullscreenOptions) {
        Application application = activity.getApplication();
        Application.ActivityLifecycleCallbacks injector =
                new SaveInstanceStateInjector(activity, injectFullscreenOptions);
        application.registerActivityLifecycleCallbacks(injector);
        try {
            CustomTabActivity recreated =
                    ApplicationTestUtils.waitForActivityWithClass(
                            CustomTabActivity.class,
                            Stage.RESUMED,
                            activity::onNightModeStateChanged);
            mCustomTabActivityTestRule.setActivity(recreated);
            // Not waitForActivityCompletelyLoaded(): deferred startup is not run again for a
            // reparented tab.
            mCustomTabActivityTestRule.waitForActivityNativeInitializationComplete();
            CriteriaHelper.pollUiThread(
                    () -> recreated.getActivityTab() != null,
                    "The tab should be attached to the recreated activity");
            return recreated;
        } finally {
            application.unregisterActivityLifecycleCallbacks(injector);
        }
    }

    /**
     * The cipher key is saved only if it already exists. It is created lazily on the first
     * encryption, e.g. when the incognito tab state is saved after the page loads. Wait for the key
     * itself so that it is in the bundle when the activity is recreated. The tab's dirtiness is not
     * a usable signal: a tab is CLEAN only from the start of a save until its next change.
     */
    private static void waitForCipherKey(CustomTabActivity activity) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Bundle bundle = new Bundle();
                    activity.getCipherFactoryForTesting().saveToBundle(bundle);
                    return new CipherFactory().restoreFromBundle(bundle);
                },
                "The cipher key should be created when the incognito tab state is saved");
    }

    /**
     * Adds saved fullscreen options to the target activity's instance state, as {@code
     * ExclusiveAccessManager#saveFullscreenState} does after a cross-display fullscreen request. A
     * plain recreation cannot produce them: the activity exits fullscreen on stop, before its state
     * is saved.
     */
    private static class SaveInstanceStateInjector
            implements Application.ActivityLifecycleCallbacks {
        private final Activity mTarget;
        private final boolean mInjectFullscreenOptions;

        SaveInstanceStateInjector(Activity target, boolean injectFullscreenOptions) {
            mTarget = target;
            mInjectFullscreenOptions = injectFullscreenOptions;
        }

        @Override
        public void onActivitySaveInstanceState(Activity activity, Bundle outState) {
            if (activity != mTarget || !mInjectFullscreenOptions) return;
            outState.putParcelable(
                    ExclusiveAccessManager.LATEST_FULLSCREEN_OPTIONS, SAVED_FULLSCREEN_OPTIONS);
        }

        @Override
        public void onActivityCreated(Activity activity, Bundle savedInstanceState) {}

        @Override
        public void onActivityStarted(Activity activity) {}

        @Override
        public void onActivityResumed(Activity activity) {}

        @Override
        public void onActivityPaused(Activity activity) {}

        @Override
        public void onActivityStopped(Activity activity) {}

        @Override
        public void onActivityDestroyed(Activity activity) {}
    }
}
