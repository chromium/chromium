// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_INITIAL_ACTIVITY_HEIGHT_PX;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.DeviceRestriction;

@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(
        reason =
                "Some tests are Testing CCT start up behavior. "
                        + "Unit test conversion tracked in crbug.com/1217031")
public class CustomTabPrivacySandboxDialogTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";

    @Rule
    public AutomotiveContextWrapperTestRule mAutomotiveRule =
            new AutomotiveContextWrapperTestRule();

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private String mTestPage;
    private EmbeddedTestServer mTestServer;
    private CustomTabsConnection mConnectionToCleanup;

    private CustomTabActivity getActivity() {
        return mCustomTabActivityTestRule.getActivity();
    }

    /**
     * @see CustomTabsIntentTestUtils#createMinimalCustomTabIntent(Context, String).
     */
    private Intent createMinimalCustomTabIntent() {
        return CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                ApplicationProvider.getApplicationContext(), mTestPage);
    }

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));
        mAutomotiveRule.setIsAutomotive(false);
        Context appContext = getInstrumentation().getTargetContext().getApplicationContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(appContext);
        mTestPage = mTestServer.getURL(TEST_PAGE);
        LibraryLoader.getInstance().ensureInitialized();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));
        SharedPreferencesManager pref = ChromeSharedPreferences.getInstance();
        pref.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_TASK_ID);
        pref.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_URL);
        pref.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLIENT_PACKAGE);

        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION);
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TIMESTAMP);

        // finish() is called on a non-UI thread by the testing harness. Must hide the menu
        // first, otherwise the UI is manipulated on a non-UI thread.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (getActivity() == null) return;
                    AppMenuCoordinator coordinator =
                            mCustomTabActivityTestRule.getAppMenuCoordinator();
                    // CCT doesn't always have a menu (ex. in the media viewer).
                    if (coordinator == null) return;
                    AppMenuHandler handler = coordinator.getAppMenuHandler();
                    if (handler != null) handler.hideAppMenu();
                });

        if (mConnectionToCleanup != null) {
            CustomTabsTestUtils.cleanupSessions(mConnectionToCleanup);
        }
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-notice-row-for-testing/true/notice-required/true"
    })
    @DisableFeatures({ChromeFeatureList.COOKIE_DEPRECATION_FACILITATED_TESTING})
    public void adsNoticeCCT() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Startup.Android.PrivacySandbox.ShouldShowAdsNoticeCCT", true);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        onViewWaiting(withId(R.id.privacy_sandbox_dialog)).check(matches(isDisplayed()));
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-notice-row-for-testing/true/notice-required/true"
    })
    @DisableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
        ChromeFeatureList.COOKIE_DEPRECATION_FACILITATED_TESTING
    })
    public void adsNoticeCCT_WithoutAdsNoticeFeature() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Startup.Android.PrivacySandbox.ShouldShowAdsNoticeCCT", true);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        onView(withId(R.id.privacy_sandbox_dialog)).check(doesNotExist());
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @DisableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
        ChromeFeatureList.COOKIE_DEPRECATION_FACILITATED_TESTING
    })
    public void adsNoticeCCT_WithoutAdsNoticeAndForceShowNoticeFeatures() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Startup.Android.PrivacySandbox.ShouldShowAdsNoticeCCT", false);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        onView(withId(R.id.privacy_sandbox_dialog)).check(doesNotExist());
        watcher.assertExpected();
    }

    private void doTestLaunchPartialCustomTabWithInitialHeight() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.newSession(token);
        connection.overridePackageNameForSessionForTesting(token, "org.chromium.testapp");
        intent.putExtra(EXTRA_INITIAL_ACTIVITY_HEIGHT_PX, 50);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
    }

    @Test
    @SmallTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    @EnableFeatures({
        ChromeFeatureList.CCT_RESIZABLE_FOR_THIRD_PARTIES,
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-notice-row-for-testing/true/notice-required/true"
    })
    @DisableFeatures({ChromeFeatureList.COOKIE_DEPRECATION_FACILITATED_TESTING})
    public void adsNoticeCCT_PartialShouldNotShowNotice() throws Exception {
        doTestLaunchPartialCustomTabWithInitialHeight();
        onView(withId(R.id.privacy_sandbox_dialog)).check(doesNotExist());
    }

    private void startActivityForResultCCT() {
        CustomTabsIntent customTabsIntent = new CustomTabsIntent.Builder().build();
        Intent intent = customTabsIntent.intent;
        intent.setData(Uri.parse("https://example.com"));
        String packageName = ContextUtils.getApplicationContext().getPackageName();
        intent.setPackage(packageName);
        mChromeTabbedActivityTestRule.startMainActivityOnBlankPage();

        ApplicationTestUtils.waitForActivityWithClass(
                CustomTabActivity.class,
                Stage.CREATED,
                () -> {
                    mChromeTabbedActivityTestRule.getActivity().startActivityForResult(intent, 0);
                });
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT + ":app-id/org.chromium.chrome.tests",
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-notice-row-for-testing/true/notice-required/true"
    })
    @DisableFeatures({ChromeFeatureList.COOKIE_DEPRECATION_FACILITATED_TESTING})
    public void adsNoticeCCT_appIdCheckDoesShowDialog() {
        HistogramWatcher shouldShowWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Startup.Android.PrivacySandbox.ShouldShowAdsNoticeCCT", true);
        HistogramWatcher appIDCheckWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Startup.Android.PrivacySandbox.AdsNoticeCCTAppIDCheck", true);
        startActivityForResultCCT();
        // Set checkRootDialog=true to prevent flakiness after api 30 with espresso 30+.
        onViewWaiting(withId(R.id.privacy_sandbox_dialog), true).check(matches(isDisplayed()));
        shouldShowWatcher.pollInstrumentationThreadUntilSatisfied();
        appIDCheckWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT
                + ":app-id/org.chromium.chrome.tests/include-mode-b/false",
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-notice-row-for-testing/true/notice-required/true"
    })
    @DisableFeatures({ChromeFeatureList.COOKIE_DEPRECATION_FACILITATED_TESTING})
    public void adsNoticeCCT_includeModeBParamFalseDoesShowDialogWhenNotInModeB() {
        HistogramWatcher shouldShowWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Startup.Android.PrivacySandbox.ShouldShowAdsNoticeCCT", true);
        startActivityForResultCCT();
        // Set checkRootDialog=true to prevent flakiness after api 30 with espresso 30+.
        onViewWaiting(withId(R.id.privacy_sandbox_dialog), true).check(matches(isDisplayed()));
        shouldShowWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT
                + ":app-id/org.chromium.chrome.tests/include-mode-b/true",
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-notice-row-for-testing/true/notice-required/true",
        ChromeFeatureList.COOKIE_DEPRECATION_FACILITATED_TESTING
    })
    public void adsNoticeCCT_includeModeBParamTrueDoesShowDialogWhenInModeB() {
        HistogramWatcher shouldShowWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Startup.Android.PrivacySandbox.ShouldShowAdsNoticeCCT", true);
        startActivityForResultCCT();
        // Set checkRootDialog=true to prevent flakiness after api 30 with espresso 30+.
        onViewWaiting(withId(R.id.privacy_sandbox_dialog), true).check(matches(isDisplayed()));
        shouldShowWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT
                + ":app-id/org.chromium.chrome.tests/include-mode-b/true",
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-notice-row-for-testing/true/notice-required/true",
        ChromeFeatureList.COOKIE_DEPRECATION_FACILITATED_TESTING
    })
    @DisableFeatures({ChromeFeatureList.COOKIE_DEPRECATION_FACILITATED_TESTING})
    public void adsNoticeCCT_includeModeBParamTrueDoesShowDialogWhenNotInModeB() {
        HistogramWatcher shouldShowWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Startup.Android.PrivacySandbox.ShouldShowAdsNoticeCCT", true);
        startActivityForResultCCT();
        // Set checkRootDialog=true to prevent flakiness after api 30 with espresso 30+.
        onViewWaiting(withId(R.id.privacy_sandbox_dialog), true).check(matches(isDisplayed()));
        shouldShowWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT
                + ":app-id/org.chromium.chrome.tests/include-mode-b/false",
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-notice-row-for-testing/true/notice-required/true",
        ChromeFeatureList.COOKIE_DEPRECATION_FACILITATED_TESTING
    })
    public void adsNoticeCCT_includeModeBParamFalseDoesNotShowDialogWhenInModeB() {
        HistogramWatcher shouldShowWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Startup.Android.PrivacySandbox.ShouldShowAdsNoticeCCT", true);
        startActivityForResultCCT();
        onView(withId(R.id.privacy_sandbox_dialog)).check(doesNotExist());
        shouldShowWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT + ":app-id/different.app.id",
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-notice-row-for-testing/true/notice-required/true"
    })
    @DisableFeatures({ChromeFeatureList.COOKIE_DEPRECATION_FACILITATED_TESTING})
    public void adsNoticeCCT_appIdCheckDoesNotShowDialog() {
        HistogramWatcher shouldShowWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Startup.Android.PrivacySandbox.ShouldShowAdsNoticeCCT", true);
        HistogramWatcher appIDCheckWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Startup.Android.PrivacySandbox.AdsNoticeCCTAppIDCheck", false);
        startActivityForResultCCT();
        onView(withId(R.id.privacy_sandbox_dialog)).check(doesNotExist());
        shouldShowWatcher.pollInstrumentationThreadUntilSatisfied();
        appIDCheckWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT
                + ":app-id/org.chromium.chrome.tests/include-mode-b/true",
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-notice-row-for-testing/true/notice-required/true"
    })
    public void adsNoticeCCT_AppIDNullShowDialog() {
        HistogramWatcher shouldShowWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Startup.Android.PrivacySandbox.ShouldShowAdsNoticeCCT", true);
        HistogramWatcher appIDCheckWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Startup.Android.PrivacySandbox.AdsNoticeCCTAppIDCheck", false);
        // Starting a CCT with mCustomTabActivityTestRule, causes the package name set to null
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        // If the package name is null, we do not show the dialog
        onView(withId(R.id.privacy_sandbox_dialog)).check(doesNotExist());
        shouldShowWatcher.pollInstrumentationThreadUntilSatisfied();
        appIDCheckWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-notice-row-for-testing/true/notice-required/true"
    })
    public void adsNoticeCCT_PWAShouldNotShowDialog() throws Exception {
        CustomTabActivityTypeTestUtils.launchActivity(
                ActivityType.WEBAPP,
                CustomTabActivityTypeTestUtils.createActivityTestRule(ActivityType.WEBAPP),
                "about:blank");
        onView(withId(R.id.privacy_sandbox_dialog)).check(doesNotExist());
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-notice-row-for-testing/true/notice-required/true"
    })
    public void adsNoticeCCT_TWAShouldNotShowDialog() throws Exception {
        CustomTabActivityTypeTestUtils.launchActivity(
                ActivityType.TRUSTED_WEB_ACTIVITY,
                CustomTabActivityTypeTestUtils.createActivityTestRule(
                        ActivityType.TRUSTED_WEB_ACTIVITY),
                "about:blank");
        onView(withId(R.id.privacy_sandbox_dialog)).check(doesNotExist());
    }
}
