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

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.util.Pair;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.webapps.AppType;
import org.chromium.components.webapps.R;
import org.chromium.components.webapps.pwa_universal_install.PwaUniversalInstallBottomSheetCoordinator;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

/** Test the showing of the PWA Universal Install Bottom Sheet dialog. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures({ChromeFeatureList.PWA_UNIVERSAL_INSTALL_UI})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PwaUniversalInstallBottomSheetIntegrationTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @ClassRule
    public static DisableAnimationsTestRule sDisableAnimationsRule =
            new DisableAnimationsTestRule();

    private static final String TAG = "PwaUniInstallIntegrTest";

    private static final String HISTOGRAM_DIALOG_TYPE =
            "WebApk.UniversalInstall.DialogShownForAppType";
    private static final String HISTOGRAM_DIALOG_ACTION = "WebApk.UniversalInstall.DialogAction";

    private PwaUniversalInstallBottomSheetCoordinator mPwaUniversalInstallBottomSheetCoordinator;

    private BottomSheetController mBottomSheetController;

    private CallbackHelper mOnInstallCallback = new CallbackHelper();
    private CallbackHelper mOnAddShortcutCallback = new CallbackHelper();
    private CallbackHelper mOnOpenAppCallback = new CallbackHelper();

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        PwaUniversalInstallBottomSheetCoordinator.sEnableManualIconFetching = true;

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
        PwaUniversalInstallBottomSheetCoordinator.sEnableManualIconFetching = false;
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

    private void showPwaUniversalInstallBottomSheet(boolean webAppAlreadyInstalled)
            throws Exception {
        Assert.assertTrue(
                runOnUiThreadBlocking(
                        () -> {
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
                            return mPwaUniversalInstallBottomSheetCoordinator.show();
                        }));

        assertDialogShowing(true);
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
    public void testInstallWebappCallback() throws Exception {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(HISTOGRAM_DIALOG_TYPE, AppType.WEBAPK)
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 0) // Dialog shown.
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 1) // Install app.
                        .build();

        showPwaUniversalInstallBottomSheet(/* webAppAlreadyInstalled= */ false);
        assertDialogShowsCheckingApp();

        Pair<Bitmap, Boolean> testIcon = constructTestIconData();
        simulateAppCheckComplete(AppType.WEBAPK, testIcon.first, testIcon.second);
        assertDialogShowsInstallable();

        onView(withId(R.id.arrow_install)).perform(click());
        mOnInstallCallback.waitForNext("Install event not signaled");
        assertDialogShowing(false);

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    // This is exactly the same test as above, with one exception: the click is on the main target
    // area and not the arrow (but the outcome should be the same).
    public void testForwardedInstallWebappCallback() throws Exception {
        showPwaUniversalInstallBottomSheet(/* webAppAlreadyInstalled= */ false);
        assertDialogShowsCheckingApp();

        Pair<Bitmap, Boolean> testIcon = constructTestIconData();
        simulateAppCheckComplete(AppType.WEBAPK, testIcon.first, testIcon.second);
        assertDialogShowsInstallable();

        onView(withId(R.id.option_text_install)).perform(click());
        mOnInstallCallback.waitForNext("Install event not signaled");
        assertDialogShowing(false);
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    public void testAddShortcutCallback() throws Exception {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(HISTOGRAM_DIALOG_TYPE, AppType.SHORTCUT)
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 0) // Dialog shown.
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 3) // Create shortcut.
                        .build();

        showPwaUniversalInstallBottomSheet(/* webAppAlreadyInstalled= */ false);
        assertDialogShowsCheckingApp();

        Pair<Bitmap, Boolean> testIcon = constructTestIconData();
        simulateAppCheckComplete(AppType.SHORTCUT, testIcon.first, testIcon.second);
        assertDialogShowsNotInstallable();

        onView(withId(R.id.arrow_shortcut)).perform(click());
        mOnAddShortcutCallback.waitForNext("Shortcut event not signaled");
        assertDialogShowing(false);

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    public void testAddShortcutToWebappCallback() throws Exception {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(HISTOGRAM_DIALOG_TYPE, AppType.WEBAPK)
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 0) // Dialog shown.
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 4) // Create shortcut to app.
                        .build();

        showPwaUniversalInstallBottomSheet(/* webAppAlreadyInstalled= */ false);
        assertDialogShowsCheckingApp();

        Pair<Bitmap, Boolean> testIcon = constructTestIconData();
        simulateAppCheckComplete(AppType.WEBAPK, testIcon.first, testIcon.second);
        assertDialogShowsInstallable();

        onView(withId(R.id.arrow_shortcut)).perform(click());
        mOnAddShortcutCallback.waitForNext("Shortcut event not signaled");
        assertDialogShowing(false);

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    // This is exactly the same test as above, with one exception: the click is on the main target
    // area and not the arrow (but the outcome should be the same).
    public void testForwardedAddShortcutCallback() throws Exception {
        showPwaUniversalInstallBottomSheet(/* webAppAlreadyInstalled= */ false);
        assertDialogShowsCheckingApp();

        Pair<Bitmap, Boolean> testIcon = constructTestIconData();
        simulateAppCheckComplete(AppType.SHORTCUT, testIcon.first, testIcon.second);
        assertDialogShowsNotInstallable();

        onView(withId(R.id.option_text_shortcut)).perform(click());
        mOnAddShortcutCallback.waitForNext("Shortcut event not signaled");
        assertDialogShowing(false);
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    public void testOpenAppCallback() throws Exception {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(HISTOGRAM_DIALOG_TYPE, AppType.WEBAPK)
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 0) // Dialog shown.
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 2) // Open existing.
                        .build();

        showPwaUniversalInstallBottomSheet(/* webAppAlreadyInstalled= */ true);
        assertDialogShowsAlreadyInstalledPreIconCheck();

        Pair<Bitmap, Boolean> testIcon = constructTestIconData();
        simulateAppCheckComplete(AppType.WEBAPK, testIcon.first, testIcon.second);
        assertDialogShowsAlreadyInstalledPostIconCheck();

        onView(withId(R.id.arrow_install)).perform(click());
        mOnOpenAppCallback.waitForNext("Open app event not signaled");
        assertDialogShowing(false);

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    // This is exactly the same test as above, with one exception: the click is on the main target
    // area and not the arrow (but the outcome should be the same).
    public void testForwardedOpenAppCallback() throws Exception {
        showPwaUniversalInstallBottomSheet(/* webAppAlreadyInstalled= */ true);
        assertDialogShowsAlreadyInstalledPreIconCheck();

        Pair<Bitmap, Boolean> testIcon = constructTestIconData();
        simulateAppCheckComplete(AppType.WEBAPK, testIcon.first, testIcon.second);
        assertDialogShowsAlreadyInstalledPostIconCheck();

        onView(withId(R.id.option_text_install)).perform(click());
        mOnOpenAppCallback.waitForNext("Open app event not signaled");
        assertDialogShowing(false);
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    public void testCallbackDisabledIfInstallDisabled() throws Exception {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(HISTOGRAM_DIALOG_TYPE, AppType.SHORTCUT)
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 0) // Dialog shown.
                        .expectIntRecord(HISTOGRAM_DIALOG_ACTION, 3) // Create shortcut.
                        .build();

        showPwaUniversalInstallBottomSheet(/* webAppAlreadyInstalled= */ false);
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

    private void assertDialogShowing(boolean expectShowing) {
        if (expectShowing) {
            onViewWaiting(withText("Add to home screen")).check(matches(isDisplayed()));
        } else {
            onView(withText("Add to home screen")).check(doesNotExist());
        }
    }
}
