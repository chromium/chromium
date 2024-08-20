// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.util.Pair;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.webapps.AppType;
import org.chromium.components.webapps.R;
import org.chromium.components.webapps.pwa_universal_install.PwaUniversalInstallBottomSheetCoordinator;
import org.chromium.net.test.EmbeddedTestServer;

/** Test the showing of the PWA Universal Install Bottom Sheet dialog. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Fails because of SurveyClientFactory assert")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PwaUniversalInstallBottomSheetIntegrationTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private static final String TAG = "PwaUniInstallIntegrTest";

    private static final String HISTOGRAM_DIALOG_TYPE =
            "WebApk.UniversalInstall.DialogShownForAppType";
    private static final String HISTOGRAM_DIALOG_ACTION = "WebApk.UniversalInstall.DialogAction";
    private static final String HISTOGRAM_TIMOUT_WITH_APP_TYPE =
            "WebApk.UniversalInstall.TimeoutWithAppType";
    private static final String HISTOGRAM_FETCH_TIME_WEBAPK =
            "WebApk.UniversalInstall.WebApk.AppDataFetchTime";
    private static final String HISTOGRAM_FETCH_TIME_HOMEBREW =
            "WebApk.UniversalInstall.Homebrew.AppDataFetchTime";
    private static final String HISTOGRAM_FETCH_TIME_SHORTCUT =
            "WebApk.UniversalInstall.Shortcut.AppDataFetchTime";

    private PwaUniversalInstallBottomSheetCoordinator mPwaUniversalInstallBottomSheetCoordinator;

    private BottomSheetController mBottomSheetController;

    private CallbackHelper mOnInstallCallback = new CallbackHelper();
    private CallbackHelper mOnAddShortcutCallback = new CallbackHelper();
    private CallbackHelper mOnOpenAppCallback = new CallbackHelper();

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        PwaUniversalInstallBottomSheetCoordinator.sEnableManualIconFetchingForTesting = true;

        mActivityTestRule.startMainActivityOnBlankPage();
        runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController =
                            BottomSheetControllerProvider.from(
                                    mActivityTestRule.getActivity().getWindowAndroid());
                });
    }

    @After
    public void tearDown() {
        PwaUniversalInstallBottomSheetCoordinator.sEnableManualIconFetchingForTesting = false;
    }

    private void onInstallCalled() {
        mOnInstallCallback.notifyCalled();
    }

    private void onAddShortcutCalled() {
        mOnAddShortcutCallback.notifyCalled();
    }

    private void onOpenAppCalled() {
        mOnOpenAppCallback.notifyCalled();
    }

    private Pair<Bitmap, Boolean> constructTestIconData() {
        int size = 48;
        Bitmap bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888);
        bitmap.eraseColor(Color.BLUE);
        return Pair.create(bitmap, /* maskable= */ false);
    }

    /*
     * Shows the Universal Install Bottom Sheet.
     * @param showBeforeAppTypeKnown When true, this will show the dialog synchronously from the
     * ctor. This can be used to simulate what happens if the app type check finishes after the
     * dialog has appeared (timeout).
     * @param webAppAlreadyInstalled When true, the dialog will behave as if the app has already
     * been installed.
     */
    private void showPwaUniversalInstallBottomSheet(
            boolean showBeforeAppTypeKnown, boolean webAppAlreadyInstalled) throws Exception {
        runOnUiThreadBlocking(
                () -> {
                    PwaUniversalInstallBottomSheetCoordinator.sShowBeforeAppTypeKnownForTesting =
                            showBeforeAppTypeKnown;
                    mPwaUniversalInstallBottomSheetCoordinator =
                            new PwaUniversalInstallBottomSheetCoordinator(
                                    mActivityTestRule.getActivity(),
                                    mActivityTestRule.getActivity().getCurrentWebContents(),
                                    this::onInstallCalled,
                                    this::onAddShortcutCalled,
                                    this::onOpenAppCalled,
                                    webAppAlreadyInstalled,
                                    mBottomSheetController,
                                    /* arrowId= */ 0,
                                    /* installOverlayId= */ 0,
                                    /* shortcutOverlayId= */ 0);
                });
    }

    private void simulateAppCheckComplete(@AppType int appType, Bitmap icon, boolean adaptive) {
        runOnUiThreadBlocking(
                () -> {
                    mPwaUniversalInstallBottomSheetCoordinator.onAppDataFetched(
                            appType, icon, adaptive);
                });
    }

    private void assertDialogShowsCheckingApp() {
        onView(withText("Install")).check(matches(isDisplayed()));
        onView(withText("Checking if app can be installed…")).check(matches(isDisplayed()));
        onView(withText("Create shortcut")).check(matches(isDisplayed()));
        onView(withText("Shortcuts open in Chrome")).check(matches(isDisplayed()));

        // The spinners should both be visible.
        onView(withId(R.id.spinny_install)).check(matches(isDisplayed()));
        onView(withId(R.id.spinny_shortcut)).check(matches(isDisplayed()));

        // The arrow should be visible.
        onView(withId(R.id.arrow_install)).check(matches(isDisplayed()));
    }

    private void assertDialogShowsNotInstallable() {
        onView(withText("Install")).check(matches(isDisplayed()));
        onView(withText("This app cannot be installed.")).check(matches(isDisplayed()));
        onView(withText("Create shortcut")).check(matches(isDisplayed()));
        onView(withText("Shortcuts open in Chrome")).check(matches(isDisplayed()));

        // The spinners should not be visible (not waiting on anything).
        onView(withId(R.id.spinny_install)).check(matches(not(isDisplayed())));
        onView(withId(R.id.spinny_shortcut)).check(matches(not(isDisplayed())));

        // The arrow should not be visible (not possible to install).
        onView(withId(R.id.arrow_install)).check(matches(not(isDisplayed())));
    }

    private void assertDialogShowsInstallable() {
        onView(withText("Install")).check(matches(isDisplayed()));
        onView(withText("Create shortcut")).check(matches(isDisplayed()));
        onView(withText("Shortcuts open in Chrome")).check(matches(isDisplayed()));

        // The spinners should not be visible (not waiting on anything).
        onView(withId(R.id.spinny_install)).check(matches(not(isDisplayed())));
        onView(withId(R.id.spinny_shortcut)).check(matches(not(isDisplayed())));

        // The arrow should be visible.
        onView(withId(R.id.arrow_install)).check(matches(isDisplayed()));
    }

    private void assertDialogShowsAlreadyInstalledPreIconCheck() {
        onView(withText("This app is already installed")).check(matches(isDisplayed()));
        onView(withText("Click to open the app instead")).check(matches(isDisplayed()));
        onView(withText("Create shortcut")).check(matches(isDisplayed()));
        onView(withText("Shortcuts open in Chrome")).check(matches(isDisplayed()));

        // The spinners should both be visible (still waiting on icons).
        onView(withId(R.id.spinny_install)).check(matches(isDisplayed()));
        onView(withId(R.id.spinny_shortcut)).check(matches(isDisplayed()));

        // The arrow should be visible (it is possible to open the app instead).
        onView(withId(R.id.arrow_install)).check(matches(isDisplayed()));
    }

    private void assertDialogShowsAlreadyInstalledPostIconCheck() {
        onView(withText("This app is already installed")).check(matches(isDisplayed()));
        onView(withText("Click to open the app instead")).check(matches(isDisplayed()));
        onView(withText("Create shortcut")).check(matches(isDisplayed()));
        onView(withText("Shortcuts open in Chrome")).check(matches(isDisplayed()));

        // The spinners should not be visible (not waiting on anything).
        onView(withId(R.id.spinny_install)).check(matches(not(isDisplayed())));
        onView(withId(R.id.spinny_shortcut)).check(matches(not(isDisplayed())));

        // The arrow should be visible (it is possible to open the app instead).
        onView(withId(R.id.arrow_install)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    // Test installing a WebApk with the dialog that shows up after timeout (of the app type check).
    public void testInstallWebappCallbackAfterTimeout() throws Exception {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(HISTOGRAM_TIMOUT_WITH_APP_TYPE, AppType.WEBAPK)
                        .expectIntRecord(HISTOGRAM_DIALOG_TYPE, AppType.WEBAPK)
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 5) // Dialog shown after timeout.
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 1) // Install app.
                        .expectAnyRecord(HISTOGRAM_FETCH_TIME_WEBAPK)
                        .build();

        showPwaUniversalInstallBottomSheet(
                /* showBeforeAppTypeKnown= */ true, /* webAppAlreadyInstalled= */ false);
        assertDialogShowsCheckingApp();

        Pair<Bitmap, Boolean> testIcon = constructTestIconData();
        simulateAppCheckComplete(AppType.WEBAPK, testIcon.first, testIcon.second);
        assertDialogShowsInstallable();

        int currentCallCount = mOnInstallCallback.getCallCount();
        onView(withId(R.id.arrow_install)).perform(click());
        mOnInstallCallback.waitForCallback("Install event not signaled", currentCallCount);
        assertDialogShowing(false);

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    // This is exactly the same test as testInstallWebappCallback, with one exception: the click is
    // on the main target area and not the arrow (but the outcome should be the same).
    public void testForwardedInstallWebappCallbackAfterTimeout() throws Exception {
        showPwaUniversalInstallBottomSheet(
                /* showBeforeAppTypeKnown= */ true, /* webAppAlreadyInstalled= */ false);
        assertDialogShowsCheckingApp();

        Pair<Bitmap, Boolean> testIcon = constructTestIconData();
        simulateAppCheckComplete(AppType.WEBAPK, testIcon.first, testIcon.second);
        assertDialogShowsInstallable();

        int currentCallCount = mOnInstallCallback.getCallCount();
        onView(withId(R.id.option_text_install)).perform(click());
        mOnInstallCallback.waitForCallback("Install event not signaled", currentCallCount);
        assertDialogShowing(false);
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    // Test adding a shortcut with the dialog that shows up after timeout (of the app type check).
    public void testAddShortcutCallbackAfterTimeout() throws Exception {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(HISTOGRAM_TIMOUT_WITH_APP_TYPE, AppType.SHORTCUT)
                        .expectIntRecord(HISTOGRAM_DIALOG_TYPE, AppType.SHORTCUT)
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 5) // Dialog shown after timeout.
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 3) // Create shortcut.
                        .expectAnyRecord(HISTOGRAM_FETCH_TIME_SHORTCUT)
                        .build();

        showPwaUniversalInstallBottomSheet(
                /* showBeforeAppTypeKnown= */ true, /* webAppAlreadyInstalled= */ false);
        assertDialogShowsCheckingApp();

        Pair<Bitmap, Boolean> testIcon = constructTestIconData();
        simulateAppCheckComplete(AppType.SHORTCUT, testIcon.first, testIcon.second);
        assertDialogShowsNotInstallable();

        int currentCallCount = mOnAddShortcutCallback.getCallCount();
        onView(withId(R.id.arrow_shortcut)).perform(click());
        mOnAddShortcutCallback.waitForCallback("Shortcut event not signaled", currentCallCount);
        assertDialogShowing(false);

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    // Test adding a shortcut to an installable webapp, with the dialog that shows up after timeout
    // (of the app type check).
    public void testAddShortcutToWebappCallbackAfterTimeout() throws Exception {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(HISTOGRAM_TIMOUT_WITH_APP_TYPE, AppType.WEBAPK)
                        .expectIntRecord(HISTOGRAM_DIALOG_TYPE, AppType.WEBAPK)
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 5) // Dialog shown after timeout.
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 4) // Create shortcut to app.
                        .expectAnyRecord(HISTOGRAM_FETCH_TIME_WEBAPK)
                        .build();

        showPwaUniversalInstallBottomSheet(
                /* showBeforeAppTypeKnown= */ true, /* webAppAlreadyInstalled= */ false);
        assertDialogShowsCheckingApp();

        Pair<Bitmap, Boolean> testIcon = constructTestIconData();
        simulateAppCheckComplete(AppType.WEBAPK, testIcon.first, testIcon.second);
        assertDialogShowsInstallable();

        int currentCallCount = mOnAddShortcutCallback.getCallCount();
        onView(withId(R.id.arrow_shortcut)).perform(click());
        mOnAddShortcutCallback.waitForCallback("Shortcut event not signaled", currentCallCount);
        assertDialogShowing(false);

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    // This is exactly the same test as testAddShortcutCallback, with one exception: the click is on
    // the main target area and not the arrow (but the outcome should be the same).
    public void testForwardedAddShortcutCallbackAfterTimeout() throws Exception {
        showPwaUniversalInstallBottomSheet(
                /* showBeforeAppTypeKnown= */ true, /* webAppAlreadyInstalled= */ false);
        assertDialogShowsCheckingApp();

        Pair<Bitmap, Boolean> testIcon = constructTestIconData();
        simulateAppCheckComplete(AppType.SHORTCUT, testIcon.first, testIcon.second);
        assertDialogShowsNotInstallable();

        int currentCallCount = mOnAddShortcutCallback.getCallCount();
        onView(withId(R.id.option_text_shortcut)).perform(click());
        mOnAddShortcutCallback.waitForCallback("Shortcut event not signaled", currentCallCount);
        assertDialogShowing(false);
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    // Test opening an installed webapp, with the dialog that shows up after timeout (of the app
    // type check).
    public void testOpenAppCallbackAfterTimeout() throws Exception {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(HISTOGRAM_TIMOUT_WITH_APP_TYPE, AppType.WEBAPK)
                        .expectIntRecord(HISTOGRAM_DIALOG_TYPE, AppType.WEBAPK)
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 5) // Dialog shown after timeout.
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 2) // Open existing.
                        .expectAnyRecord(HISTOGRAM_FETCH_TIME_WEBAPK)
                        .build();

        showPwaUniversalInstallBottomSheet(
                /* showBeforeAppTypeKnown= */ true, /* webAppAlreadyInstalled= */ true);
        assertDialogShowsAlreadyInstalledPreIconCheck();

        Pair<Bitmap, Boolean> testIcon = constructTestIconData();
        simulateAppCheckComplete(AppType.WEBAPK, testIcon.first, testIcon.second);
        assertDialogShowsAlreadyInstalledPostIconCheck();

        int currentCallCount = mOnOpenAppCallback.getCallCount();
        onView(withId(R.id.arrow_install)).perform(click());
        mOnOpenAppCallback.waitForCallback("Open app event not signaled", currentCallCount);
        assertDialogShowing(false);

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    // This is exactly the same test as testOpenAppCallback, with one exception: the click is on the
    // main target area and not the arrow (but the outcome should be the same).
    public void testForwardedOpenAppCallbackAfterTimeout() throws Exception {
        showPwaUniversalInstallBottomSheet(
                /* showBeforeAppTypeKnown= */ true, /* webAppAlreadyInstalled= */ true);
        assertDialogShowsAlreadyInstalledPreIconCheck();

        Pair<Bitmap, Boolean> testIcon = constructTestIconData();
        simulateAppCheckComplete(AppType.WEBAPK, testIcon.first, testIcon.second);
        assertDialogShowsAlreadyInstalledPostIconCheck();

        int currentCallCount = mOnOpenAppCallback.getCallCount();
        onView(withId(R.id.option_text_install)).perform(click());
        mOnOpenAppCallback.waitForCallback("Open app event not signaled", currentCallCount);
        assertDialogShowing(false);
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    // This test makes sure that clicking the install arrow (or the install text) does not trigger
    // an install for a site that doesn't support install (but creating a shortcut works).
    public void testCallbackDisabledIfInstallDisabledAfterTimeout() throws Exception {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(HISTOGRAM_TIMOUT_WITH_APP_TYPE, AppType.SHORTCUT)
                        .expectIntRecord(HISTOGRAM_DIALOG_TYPE, AppType.SHORTCUT)
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 5) // Dialog shown after timeout.
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 3) // Create shortcut.
                        .expectAnyRecord(HISTOGRAM_FETCH_TIME_SHORTCUT)
                        .build();

        showPwaUniversalInstallBottomSheet(
                /* showBeforeAppTypeKnown= */ true, /* webAppAlreadyInstalled= */ false);
        assertDialogShowsCheckingApp();

        Pair<Bitmap, Boolean> testIcon = constructTestIconData();
        simulateAppCheckComplete(AppType.SHORTCUT, testIcon.first, testIcon.second);
        assertDialogShowsNotInstallable();

        // The install arrow should not be visible and clicking Install should not close the dialog.
        onView(withId(R.id.arrow_install)).check(matches(not(isDisplayed())));
        onView(withId(R.id.option_text_install)).perform(click());
        assertDialogShowing(true);

        // But clicking the Shortcut option should close it.
        onView(withId(R.id.option_text_shortcut)).perform(click());
        assertDialogShowing(false);

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    // Test that our dialog does not show if web app type of Shortcut becomes known before opening.
    public void testTypeShortcutSkipsDialog() throws Exception {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(HISTOGRAM_TIMOUT_WITH_APP_TYPE)
                        .expectNoRecords(HISTOGRAM_DIALOG_TYPE)
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 7) // Redirect to Create Shortcut.
                        .expectAnyRecord(HISTOGRAM_FETCH_TIME_SHORTCUT)
                        .build();

        showPwaUniversalInstallBottomSheet(
                /* showBeforeAppTypeKnown= */ false, /* webAppAlreadyInstalled= */ false);
        assertDialogShowing(false);

        int currentCallCount = mOnOpenAppCallback.getCallCount();
        Pair<Bitmap, Boolean> testIcon = constructTestIconData();
        simulateAppCheckComplete(AppType.SHORTCUT, testIcon.first, testIcon.second);
        mOnAddShortcutCallback.waitForCallback("Add Shortcut event not signaled", currentCallCount);
        assertDialogShowing(false);

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    // Test that our dialog does show for WebApk if not on domain root page.
    public void testTypeCraftedWebappShowsDialogOnLeafPage() throws Exception {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(HISTOGRAM_TIMOUT_WITH_APP_TYPE)
                        .expectIntRecord(HISTOGRAM_DIALOG_TYPE, AppType.WEBAPK)
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 0) // Dialog shown.
                        .expectAnyRecord(HISTOGRAM_FETCH_TIME_WEBAPK)
                        .build();

        showPwaUniversalInstallBottomSheet(
                /* showBeforeAppTypeKnown= */ false, /* webAppAlreadyInstalled= */ false);
        assertDialogShowing(false);

        Pair<Bitmap, Boolean> testIcon = constructTestIconData();
        simulateAppCheckComplete(AppType.WEBAPK, testIcon.first, testIcon.second);
        assertDialogShowing(true);

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    // Test that our dialog does show for homebrew webapp if not on domain root page.
    public void testTypeHomebrewWebappShowsDialogOnLeafPage() throws Exception {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(HISTOGRAM_TIMOUT_WITH_APP_TYPE)
                        .expectIntRecord(HISTOGRAM_DIALOG_TYPE, AppType.WEBAPK_DIY)
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 0) // Dialog shown.
                        .expectAnyRecord(HISTOGRAM_FETCH_TIME_HOMEBREW)
                        .build();

        showPwaUniversalInstallBottomSheet(
                /* showBeforeAppTypeKnown= */ false, /* webAppAlreadyInstalled= */ false);
        assertDialogShowing(false);

        Pair<Bitmap, Boolean> testIcon = constructTestIconData();
        simulateAppCheckComplete(AppType.WEBAPK_DIY, testIcon.first, testIcon.second);
        assertDialogShowing(true);

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    // Test that our dialog does not show if web app type of WebApk becomes known before opening
    // when we are on the root of the domain.
    public void testTypeCraftedWebAppSkipsDialogOnRoot() throws Exception {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(HISTOGRAM_TIMOUT_WITH_APP_TYPE)
                        .expectNoRecords(HISTOGRAM_DIALOG_TYPE)
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 8) // Redirect to Install App.
                        .expectAnyRecord(HISTOGRAM_FETCH_TIME_WEBAPK)
                        .build();

        // Navigate to the root of the test server.
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        mActivityTestRule.loadUrl(testServer.getURL("/"));

        showPwaUniversalInstallBottomSheet(
                /* showBeforeAppTypeKnown= */ false, /* webAppAlreadyInstalled= */ false);
        assertDialogShowing(false);

        int currentCallCount = mOnOpenAppCallback.getCallCount();
        Pair<Bitmap, Boolean> testIcon = constructTestIconData();
        simulateAppCheckComplete(AppType.WEBAPK, testIcon.first, testIcon.second);
        mOnInstallCallback.waitForCallback("Install App event not signaled", currentCallCount);
        assertDialogShowing(false);

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    // Test that our dialog does not show if web app type of homebrew webapp becomes known before
    // opening, when we are on the root of the domain.
    public void testTypeHomebrewWebAppSkipsDialogOnRoot() throws Exception {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(HISTOGRAM_TIMOUT_WITH_APP_TYPE)
                        .expectNoRecords(HISTOGRAM_DIALOG_TYPE)
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 9) // Redirect (homebrew app).
                        .expectAnyRecord(HISTOGRAM_FETCH_TIME_HOMEBREW)
                        .build();

        // Navigate to the root of the test server.
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        mActivityTestRule.loadUrl(testServer.getURL("/"));

        showPwaUniversalInstallBottomSheet(
                /* showBeforeAppTypeKnown= */ false, /* webAppAlreadyInstalled= */ false);
        assertDialogShowing(false);

        int currentCallCount = mOnOpenAppCallback.getCallCount();
        Pair<Bitmap, Boolean> testIcon = constructTestIconData();
        simulateAppCheckComplete(AppType.WEBAPK_DIY, testIcon.first, testIcon.second);
        mOnInstallCallback.waitForCallback("Install App event not signaled", currentCallCount);
        assertDialogShowing(false);

        watcher.assertExpected();
    }

    private void assertDialogShowing(boolean expectShowing) {
        if (expectShowing) {
            onViewWaiting(withText("Add to home screen")).check(matches(isDisplayed()));
        } else {
            onView(withText("Add to home screen")).check(doesNotExist());
        }
    }
}
