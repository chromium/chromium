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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
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

    private BottomSheetController mBottomSheetController;

    private CallbackHelper mOnInstallCallback = new CallbackHelper();
    private CallbackHelper mOnAddShortcutCallback = new CallbackHelper();
    private CallbackHelper mOnOpenAppCallback = new CallbackHelper();

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        mActivityTestRule.startMainActivityOnBlankPage();
        runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController =
                            BottomSheetControllerProvider.from(
                                    mActivityTestRule.getActivity().getWindowAndroid());
                });
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
        PwaUniversalInstallBottomSheetCoordinator.setIconCallForTesting(
                this::constructTestIconData);
        PwaUniversalInstallBottomSheetCoordinator pwaUniversalInstallBottomSheetCoordinator =
                new PwaUniversalInstallBottomSheetCoordinator(
                        mActivityTestRule.getActivity(),
                        mActivityTestRule.getActivity().getCurrentWebContents(),
                        this::onInstallCalled,
                        this::onAddShortcutCalled,
                        this::onOpenAppCalled,
                        webAppAlreadyInstalled,
                        mBottomSheetController,
                        /* arrowId= */ 0);
        Assert.assertTrue(
                runOnUiThreadBlocking(
                        () -> {
                            return pwaUniversalInstallBottomSheetCoordinator.show();
                        }));

        assertDialogShowing(true);
    }

    private void assertInitialStateCorrectForInstall() {
        onView(withText("Install")).check(matches(isDisplayed()));
        onView(withText("Create shortcut")).check(matches(isDisplayed()));
        onView(withText("Shortcuts open in Chrome")).check(matches(isDisplayed()));

        // Ensure this does not show alongside the Install label:
        onView(withText("Click to open the app instead")).check(matches(not(isDisplayed())));
    }

    private void assertInitialStateCorrectForOpen() {
        onView(withText("This app is already installed")).check(matches(isDisplayed()));
        onView(withText("Click to open the app instead")).check(matches(isDisplayed()));
        onView(withText("Create shortcut")).check(matches(isDisplayed()));
        onView(withText("Shortcuts open in Chrome")).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    public void testInstallWebappCallback() throws Exception {
        showPwaUniversalInstallBottomSheet(/* webAppAlreadyInstalled= */ false);
        assertInitialStateCorrectForInstall();

        onView(withId(R.id.arrow_install)).perform(click());
        mOnInstallCallback.waitForNext("Install event not signaled");
        assertDialogShowing(false);
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    // This is exactly the same test as above, with one exception: the click is on the main target
    // area and not the arrow (but the outcome should be the same).
    public void testForwardedInstallWebappCallback() throws Exception {
        showPwaUniversalInstallBottomSheet(/* webAppAlreadyInstalled= */ false);
        assertInitialStateCorrectForInstall();

        onView(withId(R.id.option_text_install)).perform(click());
        mOnInstallCallback.waitForNext("Install event not signaled");
        assertDialogShowing(false);
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    public void testAddShortcutCallback() throws Exception {
        showPwaUniversalInstallBottomSheet(/* webAppAlreadyInstalled= */ false);
        assertInitialStateCorrectForInstall();

        onView(withId(R.id.arrow_shortcut)).perform(click());
        mOnAddShortcutCallback.waitForNext("Shortcut event not signaled");
        assertDialogShowing(false);
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    // This is exactly the same test as above, with one exception: the click is on the main target
    // area and not the arrow (but the outcome should be the same).
    public void testForwardedAddShortcutCallback() throws Exception {
        showPwaUniversalInstallBottomSheet(/* webAppAlreadyInstalled= */ false);
        assertInitialStateCorrectForInstall();

        onView(withId(R.id.option_text_shortcut)).perform(click());
        mOnAddShortcutCallback.waitForNext("Shortcut event not signaled");
        assertDialogShowing(false);
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    public void testOpenAppCallback() throws Exception {
        showPwaUniversalInstallBottomSheet(/* webAppAlreadyInstalled= */ true);
        assertInitialStateCorrectForOpen();

        onView(withId(R.id.arrow_install)).perform(click());
        mOnOpenAppCallback.waitForNext("Open app event not signaled");
        assertDialogShowing(false);
    }

    @Test
    @SmallTest
    @Feature({"PwaUniversalInstall"})
    // This is exactly the same test as above, with one exception: the click is on the main target
    // area and not the arrow (but the outcome should be the same).
    public void testForwardedOpenAppCallback() throws Exception {
        showPwaUniversalInstallBottomSheet(/* webAppAlreadyInstalled= */ true);
        assertInitialStateCorrectForOpen();

        onView(withId(R.id.option_text_install)).perform(click());
        mOnOpenAppCallback.waitForNext("Open app event not signaled");
        assertDialogShowing(false);
    }

    private void assertDialogShowing(boolean expectShowing) {
        if (expectShowing) {
            onViewWaiting(withText("Add to home screen")).check(matches(isDisplayed()));
        } else {
            onView(withText("Add to home screen")).check(doesNotExist());
        }
    }
}
