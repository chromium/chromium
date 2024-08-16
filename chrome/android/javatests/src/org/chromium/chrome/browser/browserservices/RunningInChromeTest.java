// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.containsString;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.createSession;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.createTrustedWebActivityIntent;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.isTrustedWebActivity;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.spoofVerification;
import static org.chromium.chrome.browser.notifications.NotificationConstants.NOTIFICATION_ID_TWA_DISCLOSURE_INITIAL;
import static org.chromium.chrome.browser.notifications.NotificationConstants.NOTIFICATION_ID_TWA_DISCLOSURE_SUBSEQUENT;

import android.content.Intent;

import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityCommonsModule;
import org.chromium.chrome.browser.dependency_injection.ModuleOverridesRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy.NotificationEntry;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.TimeoutException;

/** Instrumentation tests to make sure the Running in Chrome prompt is shown. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class RunningInChromeTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String PACKAGE_NAME =
            ContextUtils.getApplicationContext().getPackageName();

    private static final Set<Integer> TEST_SNACKBARS =
            new HashSet<>(
                    Arrays.asList(
                            Snackbar.UMA_TWA_PRIVACY_DISCLOSURE,
                            Snackbar.UMA_TWA_PRIVACY_DISCLOSURE_V2));

    private final CustomTabActivityTestRule mCustomTabActivityTestRule =
            new CustomTabActivityTestRule();
    private final EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();
    private final MockNotificationManagerProxy mMockNotificationManager =
            new MockNotificationManagerProxy();

    private final TestRule mModuleOverridesRule =
            new ModuleOverridesRule()
                    .setOverride(
                            ChromeActivityCommonsModule.Factory.class,
                            (activity,
                                    bottomSheetController,
                                    tabModelSelectorSupplier,
                                    browserControlsManager,
                                    browserControlsVisibilityManager,
                                    browserControlsSizer,
                                    fullscreenManager,
                                    layoutManagerSupplier,
                                    lifecycleDispatcher,
                                    snackbarManagerSupplier,
                                    profileProvider,
                                    activityTabProvider,
                                    tabContentManager,
                                    activityWindowAndroid,
                                    compositorViewHolderSupplier,
                                    tabCreatorManager,
                                    tabCreatorSupplier,
                                    statusBarColorController,
                                    screenOrientationProvider,
                                    notificationManagerProxySupplier,
                                    tabContentManagerSupplier,
                                    legacyTabStartupMetricsTracker,
                                    startupMetricsTrackerSupplier,
                                    compositorViewHolderInitializer,
                                    chromeActivityNativeDelegate,
                                    modalDialogManagerSupplier,
                                    browserControlsStateProvider,
                                    savedInstanceStateSupplier,
                                    autofillUiBottomInsetSupplier,
                                    shareDelegateSupplier,
                                    tabModelInitializer,
                                    activityType) -> {
                                return new ChromeActivityCommonsModule(
                                        activity,
                                        bottomSheetController,
                                        tabModelSelectorSupplier,
                                        browserControlsManager,
                                        browserControlsVisibilityManager,
                                        browserControlsSizer,
                                        fullscreenManager,
                                        layoutManagerSupplier,
                                        lifecycleDispatcher,
                                        snackbarManagerSupplier,
                                        profileProvider,
                                        activityTabProvider,
                                        tabContentManager,
                                        activityWindowAndroid,
                                        compositorViewHolderSupplier,
                                        tabCreatorManager,
                                        tabCreatorSupplier,
                                        statusBarColorController,
                                        screenOrientationProvider,
                                        () -> mMockNotificationManager,
                                        tabContentManagerSupplier,
                                        legacyTabStartupMetricsTracker,
                                        startupMetricsTrackerSupplier,
                                        compositorViewHolderInitializer,
                                        chromeActivityNativeDelegate,
                                        modalDialogManagerSupplier,
                                        browserControlsStateProvider,
                                        savedInstanceStateSupplier,
                                        autofillUiBottomInsetSupplier,
                                        shareDelegateSupplier,
                                        tabModelInitializer,
                                        activityType);
                            });

    @Rule
    public RuleChain mRuleChain =
            RuleChain.emptyRuleChain()
                    .around(mCustomTabActivityTestRule)
                    .around(mEmbeddedTestServerRule)
                    .around(mModuleOverridesRule);

    private String mTestPage;
    private BrowserServicesStore mStore;

    @Before
    public void setUp() {
        // Native needs to be initialized to start the test server.
        LibraryLoader.getInstance().ensureInitialized();

        mEmbeddedTestServerRule.setServerUsesHttps(true); // TWAs only work with HTTPS.
        mTestPage = mEmbeddedTestServerRule.getServer().getURL(TEST_PAGE);

        mMockNotificationManager.setNotificationsEnabled(false);

        mStore =
                new BrowserServicesStore(
                        ChromeApplicationImpl.getComponent().resolveChromeSharedPreferences());
        mStore.removeTwaDisclosureAcceptanceForPackage(PACKAGE_NAME);
    }

    @Test
    @MediumTest
    public void showsNewRunningInChrome() throws TimeoutException {
        launch(createTrustedWebActivityIntent(mTestPage));

        clearOtherSnackbars();

        assertTrue(isTrustedWebActivity(mCustomTabActivityTestRule.getActivity()));
        Espresso.onView(withText(containsString(getString()))).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void showsNotification() throws TimeoutException {
        mMockNotificationManager.setNotificationsEnabled(true);

        launch(createTrustedWebActivityIntent(mTestPage));

        String scope = Origin.createOrThrow(mTestPage).toString();
        CriteriaHelper.pollUiThread(() -> showingNotification(scope));
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_AUTO)
    public void showsNoNotificationOnAutomotive() throws TimeoutException {
        mMockNotificationManager.setNotificationsEnabled(true);

        launch(createTrustedWebActivityIntent(mTestPage));

        String scope = Origin.createOrThrow(mTestPage).toString();
        CriteriaHelper.pollUiThread(() -> !showingNotification(scope));
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void dismissesNotification_onNavigation() throws TimeoutException {
        mMockNotificationManager.setNotificationsEnabled(true);

        launch(createTrustedWebActivityIntent(mTestPage));

        String scope = Origin.createOrThrow(mTestPage).toString();
        CriteriaHelper.pollUiThread(() -> showingNotification(scope));

        mCustomTabActivityTestRule.loadUrl("https://www.example.com/");

        CriteriaHelper.pollUiThread(() -> !showingNotification(scope));
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void dismissesNotification_onActivityClose() throws TimeoutException {
        mMockNotificationManager.setNotificationsEnabled(true);

        launch(createTrustedWebActivityIntent(mTestPage));

        String scope = Origin.createOrThrow(mTestPage).toString();
        CriteriaHelper.pollUiThread(() -> showingNotification(scope));

        mCustomTabActivityTestRule.getActivity().finish();

        CriteriaHelper.pollUiThread(() -> !showingNotification(scope));
    }

    private boolean showingNotification(String tag) {
        for (NotificationEntry entry : mMockNotificationManager.getNotifications()) {
            if (!entry.tag.equals(tag)) continue;

            if (entry.id == NOTIFICATION_ID_TWA_DISCLOSURE_INITIAL) return true;
            if (entry.id == NOTIFICATION_ID_TWA_DISCLOSURE_SUBSEQUENT) return true;
        }

        return false;
    }

    public void launch(Intent intent) throws TimeoutException {
        String url = intent.getData().toString();
        spoofVerification(PACKAGE_NAME, url);
        createSession(intent, PACKAGE_NAME);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
    }

    private void clearOtherSnackbars() {
        // On the trybots a Downloads Snackbar is shown over the one we want to test.
        SnackbarManager manager = mCustomTabActivityTestRule.getActivity().getSnackbarManager();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    while (true) {
                        Snackbar snackbar = manager.getCurrentSnackbarForTesting();

                        if (snackbar == null) break;
                        if (TEST_SNACKBARS.contains(snackbar.getIdentifierForTesting())) break;

                        manager.dismissSnackbars(snackbar.getController());
                    }
                });
    }

    private String getString() {
        return mCustomTabActivityTestRule
                .getActivity()
                .getResources()
                .getString(R.string.twa_running_in_chrome);
    }
}
