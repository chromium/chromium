// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.popups;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import android.app.Activity;
import android.app.role.RoleManager;
import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.view.View;
import android.view.WindowManager;

import androidx.test.espresso.ViewAssertion;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.TransitAsserts;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.tab_activity_glue.PopupCreatorImpl;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.popups.testhtmls.PopupLauncherPageStation;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskTrackerFactory;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.page.BasePageStation.Builder;
import org.chromium.chrome.test.transit.page.CctPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.testhtmls.PopupOnClickPageStation;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.concurrent.TimeoutException;
import java.util.function.Supplier;

/** Tests whether popup windows appear as CCTs. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@DoNotBatch(reason = "Safer to not batch as we are using multiple Android tasks")
@EnableFeatures(BlinkFeatures.ANDROID_DESKTOP_WEB_PREFS_LARGE_DISPLAYS)
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
@DisableIf.Device(DeviceFormFactor.DESKTOP) // https://crbug.com/577194295
public class PopupMultiwindowPTTest {
    @Rule
    public FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private WebPageStation mEntryPage;

    // Window bounds get converted from dips to px and vice-versa during CUJs so let's match these
    // values with some leeway.
    private static final int BOUNDS_TOLERANCE = 5;
    private static final Builder<CctPageStation> SIMPLE_POPUP_BUILDER =
            CctPageStation.newBuilder()
                    .withEntryPoint()
                    .withExpectedUrlSubstring("simple.html")
                    .withExpectedTitle("Simple");

    @Before
    public void setUp() {
        mEntryPage = mCtaTestRule.startOnBlankPage();
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testBasic() throws Exception {
        forceUnflaggedLaunchPath();
        PopupOnClickPageStation page =
                PopupOnClickPageStation.loadInCurrentTab(
                        mCtaTestRule.getActivityTestRule(), mEntryPage);

        int initialTabCount = mCtaTestRule.tabsCount(/* incognito= */ false);

        CctPageStation popup = page.clickLinkToOpenPopupWithBoundsExpectNewWindow();

        // Assert that no tab has been created in lieu of a window
        assertEquals(
                "The number of tabs in the original window is unexpected",
                initialTabCount,
                mCtaTestRule.tabsCount(/* incognito= */ false));

        assertPopupHasOpener(popup);

        TransitAsserts.assertFinalDestinations(page, popup);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testBasicIncognito() {
        forceUnflaggedLaunchPath();
        final IncognitoNewTabPageStation incognitoEntryPoint =
                mEntryPage.openNewIncognitoTabOrWindowFast();

        final PopupOnClickPageStation page =
                PopupOnClickPageStation.loadInCurrentTab(
                        mCtaTestRule.getActivityTestRule(), incognitoEntryPoint);
        assertTrue(page.isIncognito());

        final int initialTabCount = mCtaTestRule.tabsCount(/* incognito= */ true);

        final CctPageStation popup = page.clickLinkToOpenPopupWithBoundsExpectNewWindow();

        // Assert that no tab has been created in lieu of a window
        assertEquals(
                "The number of tabs in the original window is unexpected",
                initialTabCount,
                mCtaTestRule.tabsCount(/* incognito= */ true));
        assertTrue(popup.isIncognito());

        // Close the popup while the opener still holds the shared incognito profile.
        popup.finishActivity();

        if (page.getActivity().isIncognitoWindow()) {
            TransitAsserts.assertFinalDestinations(mEntryPage, page);
        } else {
            TransitAsserts.assertFinalDestinations(page);
        }
    }

    /**
     * Toolbar close is shown iff the popup is not in a desktop window; minimize is never offered.
     * Forces the unflagged launch path so the test also runs on tablets.
     */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testPopupToolbarCloseButton_matchesWindowingMode() {
        forceUnflaggedLaunchPath();
        PopupOnClickPageStation page =
                PopupOnClickPageStation.loadInCurrentTab(
                        mCtaTestRule.getActivityTestRule(), mEntryPage);

        CctPageStation popup = page.clickLinkToOpenPopupWithBoundsExpectNewWindow();
        CustomTabActivity cct = popup.getActivity();
        View popupDecorView = cct.getWindow().getDecorView();

        boolean inDesktopWindow =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                AppHeaderUtils.isAppInDesktopWindow(
                                        cct.getRootUiCoordinatorForTesting()
                                                .getDesktopWindowStateManager()));

        // Toolbar close is shown iff the OS caption bar is not.
        if (inDesktopWindow) {
            onView(withId(R.id.close_button))
                    .inRoot(withDecorView(is(popupDecorView)))
                    .check(isAbsentOrNotDisplayed());
        } else {
            onView(withId(R.id.close_button))
                    .inRoot(withDecorView(is(popupDecorView)))
                    .check(matches(isDisplayed()));
        }

        // POPUP CCTs never offer minimize.
        onView(withId(R.id.custom_tabs_minimize_button))
                .inRoot(withDecorView(is(popupDecorView)))
                .check(isAbsentOrNotDisplayed());

        TransitAsserts.assertFinalDestinations(page, popup);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    public void testPopupOpensIfInFreeformWindowing() throws Exception {
        assumeHasBrowserRole();
        final PopupLauncherPageStation page =
                PopupLauncherPageStation.loadInCurrentTab(
                        mCtaTestRule.getActivityTestRule(), mEntryPage);

        final CctPageStation popup =
                page.clickOpenPopup().inNewTask().arriveAt(SIMPLE_POPUP_BUILDER.build());

        // Assert that no tab has been created in lieu of a window
        assertEquals(
                "The number of tabs in the original window is unexpected",
                1,
                mCtaTestRule.tabsCount(/* incognito= */ false));

        assertPopupHasOpener(popup);
        assertEquals(
                "Opener must hold a live handle to the popup",
                "true",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        page.webContentsElement.get(),
                        "basicPopupWindow !== null && !basicPopupWindow.closed"));

        TransitAsserts.assertFinalDestinations(page, popup);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    public void testPopupDoesNotOpenIfInFullscreen() {
        assumeHasBrowserRole();
        final PopupLauncherPageStation page =
                PopupLauncherPageStation.loadInCurrentTab(
                        mCtaTestRule.getActivityTestRule(), mEntryPage);

        assertEquals(
                "The number of tabs in the original window is unexpected",
                1,
                mCtaTestRule.tabsCount(/* incognito= */ false));

        final WebPageStation newPage =
                WebPageStation.newBuilder()
                        .initOpeningNewTab()
                        .withExpectedUrlSubstring("simple.html")
                        .withExpectedTitle("Simple")
                        .build();
        page.clickOpenPopup().arriveAt(newPage);

        // Assert that a new tab has been created
        assertEquals(
                "The number of tabs in the original window is unexpected",
                2,
                mCtaTestRule.tabsCount(/* incognito= */ false));

        TransitAsserts.assertFinalDestinations(newPage);
    }

    /**
     * Non-default-browser variant on a desktop display: without REPOSITION_SELF_WINDOWS the popup
     * is launched without setMovableTaskRequired but still yields a freeform popup window, because
     * the opener itself is in a desktop window on the same display.
     */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    public void testPopupOpensAsNonMovableTaskWithoutRepositionPermission() {
        AndroidTaskUtils.setCanRepositionSelfWindowsForTesting(false);
        final HistogramWatcher watcher =
                expectNoPermissionOutcome(
                        PopupCreatorImpl.PopupLaunchOutcome
                                .NON_MOVABLE_TASK_LAUNCHED_NO_PERMISSION);

        final PopupLauncherPageStation page =
                PopupLauncherPageStation.loadInCurrentTab(
                        mCtaTestRule.getActivityTestRule(), mEntryPage);

        final CctPageStation popup =
                page.clickOpenPopup().inNewTask().arriveAt(SIMPLE_POPUP_BUILDER.build());

        watcher.assertExpected();

        // Assert that no tab has been created in lieu of a window
        assertEquals(
                "The number of tabs in the original window is unexpected",
                1,
                mCtaTestRule.tabsCount(/* incognito= */ false));

        final int popupTaskId = popup.getActivity().getTaskId();
        assertTrue(
                "The popup should be a freeform window",
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                ChromeAndroidTaskTrackerFactory.getInstance()
                                        .get(popupTaskId)
                                        .isFreeformWindow()));

        TransitAsserts.assertFinalDestinations(page, popup);
    }

    /**
     * Non-default-browser variant with the opener not in a desktop window (phone or tablet):
     * window.open() falls back to a tab instead of launching a non-movable popup task.
     */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    public void testPopupFallsBackToTabWithoutPermissionOutsideDesktopWindow() {
        AndroidTaskUtils.setCanRepositionSelfWindowsForTesting(false);
        final HistogramWatcher watcher =
                expectNoPermissionOutcome(PopupCreatorImpl.PopupLaunchOutcome.FALLBACK_TO_TAB);

        final PopupLauncherPageStation page =
                PopupLauncherPageStation.loadInCurrentTab(
                        mCtaTestRule.getActivityTestRule(), mEntryPage);

        assertEquals(
                "The number of tabs in the original window is unexpected",
                1,
                mCtaTestRule.tabsCount(/* incognito= */ false));

        final WebPageStation newPage =
                WebPageStation.newBuilder()
                        .initOpeningNewTab()
                        .withExpectedUrlSubstring("simple.html")
                        .withExpectedTitle("Simple")
                        .build();
        page.clickOpenPopup().arriveAt(newPage);

        watcher.assertExpected();

        // Assert that a new tab has been created
        assertEquals(
                "The number of tabs in the original window is unexpected",
                2,
                mCtaTestRule.tabsCount(/* incognito= */ false));

        TransitAsserts.assertFinalDestinations(newPage);
    }

    /**
     * Tests whether specifying window features as {@code 'left=100, top=100, width=300,
     * height=300'} in a {@code window.open()} call opens a new window so that its top-left corner
     * is at (100, 100) in screen coordinates and its viewport is 300dp wide and tall.
     *
     * @see <a
     *     href="https://drafts.csswg.org/cssom-view/#the-features-argument-to-the-open()-method">The
     *     W3C spec</a>
     */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    public void testPopupOpensWithLaunchBounds() {
        assumeHasBrowserRole();
        final PopupLauncherPageStation page =
                PopupLauncherPageStation.loadInCurrentTab(
                        mCtaTestRule.getActivityTestRule(), mEntryPage);

        // Opens a pop-up with features 'left=100, top=100, width=300, height=300'. These dimensions
        // are duplicated in popup_launcher.html.
        final CctPageStation popup = SIMPLE_POPUP_BUILDER.build();
        popup.declareEnterCondition(new WindowPositionCondition(popup, 100, 100));
        popup.declareEnterCondition(new WebApiWindowPositionCondition(popup, 100, 100));
        popup.declareEnterCondition(new ViewportSizeCondition(popup, 300, 300));
        popup.declareEnterCondition(new WebApiViewportSizeCondition(popup, 300, 300));
        page.clickOpenPopupWithBounds().inNewTask().arriveAt(popup);

        TransitAsserts.assertFinalDestinations(page, popup);
    }

    /**
     * Tests whether specifying window features as {@code 'left=50, top=60, width=100, height=100'}
     * in a {@code window.open()} call opens a new window so that its top-left corner is at (50, 60)
     * in screen coordinates and the window is 220dp wide and tall.
     *
     * <p>All windows on Android must be at least 220dp wide and tall due to compatibility
     * requirements.
     *
     * @see <a
     *     href="https://source.android.com/docs/compatibility/16/android-16-cdd#3814_multi-windows">Android
     *     Compatibility Definition Document</a>
     */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    public void testPopupOpensWithLaunchBounds_osWindowSizeLimit() {
        assumeHasBrowserRole();
        final PopupLauncherPageStation page =
                PopupLauncherPageStation.loadInCurrentTab(
                        mCtaTestRule.getActivityTestRule(), mEntryPage);

        // Opens a pop-up with features 'left=50, top=60, width=100, height=100'. These dimensions
        // are duplicated in popup_launcher.html.
        final CctPageStation popup = SIMPLE_POPUP_BUILDER.build();
        popup.declareEnterCondition(new WindowPositionCondition(popup, 50, 60));
        popup.declareEnterCondition(new WebApiWindowPositionCondition(popup, 50, 60));
        popup.declareEnterCondition(new WindowSizeCondition(popup, 220, 220));
        popup.declareEnterCondition(new WebApiWindowSizeCondition(popup, 220, 220));
        page.clickOpenPopupWithSmallBounds().inNewTask().arriveAt(popup);

        TransitAsserts.assertFinalDestinations(page, popup);
    }

    /**
     * Tests whether specifying window features as {@code 'left=50, top=60, width=386, height=220'}
     * in a {@code window.open()} call opens a new window so that its top-left corner is at (50, 60)
     * in screen coordinates and the window's viewport is 386dp wide and 220dp tall.
     *
     * <p>386dp is the minimum width a user can drag a desktop window to in AOSP (WM Shell's {@code
     * desktop_mode_minimum_window_width}); 220dp is the minimum window size required by the CDD.
     * Narrower popups have no room for the popup header, so their viewport is taller than
     * requested.
     *
     * @see <a
     *     href="https://source.android.com/docs/compatibility/16/android-16-cdd#3814_multi-windows">Android
     *     Compatibility Definition Document</a>
     */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    public void testPopupOpensWithLaunchBounds_minUserResizableWidth() {
        assumeHasBrowserRole();
        final PopupLauncherPageStation page =
                PopupLauncherPageStation.loadInCurrentTab(
                        mCtaTestRule.getActivityTestRule(), mEntryPage);

        // Opens a pop-up with features 'left=50, top=60, width=386, height=220'. These dimensions
        // are duplicated in popup_launcher.html.
        final CctPageStation popup = SIMPLE_POPUP_BUILDER.build();
        popup.declareEnterCondition(new WindowPositionCondition(popup, 50, 60));
        popup.declareEnterCondition(new WebApiWindowPositionCondition(popup, 50, 60));
        popup.declareEnterCondition(new ViewportSizeCondition(popup, 386, 220));
        popup.declareEnterCondition(new WebApiViewportSizeCondition(popup, 386, 220));
        page.clickOpenPopupWithMinUserResizableWidth().inNewTask().arriveAt(popup);

        TransitAsserts.assertFinalDestinations(page, popup);
    }

    /** Tests whether the {@code window.close()} web API works. */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    public void testPopupClosesOnWindowClose() throws Exception {
        assumeHasBrowserRole();
        final PopupLauncherPageStation page =
                PopupLauncherPageStation.loadInCurrentTab(
                        mCtaTestRule.getActivityTestRule(), mEntryPage);

        final CctPageStation popup =
                page.clickOpenPopup().inNewTask().arriveAt(SIMPLE_POPUP_BUILDER.build());

        page.bringWindowToFront();

        popup.getActivityElement().expectActivityDestroyed();
        page.clickClosePopup().withContext(popup).reachLastStop();

        TransitAsserts.assertFinalDestinations(page);
    }

    /** Tests whether the {@code window.focus()} web API works. */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    public void testPopupFocusesOnWindowFocus() throws Exception {
        assumeHasBrowserRole();
        final PopupLauncherPageStation page =
                PopupLauncherPageStation.loadInCurrentTab(
                        mCtaTestRule.getActivityTestRule(), mEntryPage);

        final CctPageStation popup =
                page.clickOpenPopup().inNewTask().arriveAt(SIMPLE_POPUP_BUILDER.build());

        page.bringWindowToFront();

        final CctPageStation focusedPopup = CctPageStation.newBuilder().initFrom(popup).build();
        focusedPopup.declareEnterCondition(
                new ActivityFocusedCondition<>(focusedPopup.getActivityElement()));
        page.clickFocusPopup().withContext(popup).arriveAt(focusedPopup);

        TransitAsserts.assertFinalDestinations(page, focusedPopup);
    }

    /** Tests whether the {@code window.moveTo()} web API works. */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    public void testPopupMovesOnWindowMoveTo() throws Exception {
        assumeHasBrowserRole();
        final PopupLauncherPageStation page =
                PopupLauncherPageStation.loadInCurrentTab(
                        mCtaTestRule.getActivityTestRule(), mEntryPage);

        // Opens a pop-up with features 'left=100, top=100, width=300, height=300'. These dimensions
        // are duplicated in popup_launcher.html.
        final CctPageStation popup = SIMPLE_POPUP_BUILDER.build();
        popup.declareEnterCondition(new WindowPositionCondition(popup, 100, 100));
        popup.declareEnterCondition(new WebApiWindowPositionCondition(popup, 100, 100));
        popup.declareEnterCondition(new ViewportSizeCondition(popup, 300, 300));
        popup.declareEnterCondition(new WebApiViewportSizeCondition(popup, 300, 300));
        page.clickOpenPopupWithBounds().inNewTask().arriveAt(popup);

        page.bringWindowToFront();

        final CctPageStation movedPopup = CctPageStation.newBuilder().initFrom(popup).build();
        movedPopup.declareEnterCondition(new WindowPositionCondition(movedPopup, 75, 75));
        movedPopup.declareEnterCondition(new WebApiWindowPositionCondition(movedPopup, 75, 75));
        movedPopup.declareEnterCondition(new ViewportSizeCondition(movedPopup, 300, 300));
        movedPopup.declareEnterCondition(new WebApiViewportSizeCondition(movedPopup, 300, 300));
        page.clickMovePopupTo().withContext(popup).arriveAt(movedPopup);

        TransitAsserts.assertFinalDestinations(page, movedPopup);
    }

    /** Tests whether the {@code window.moveBy()} web API works. */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    public void testPopupMovesOnWindowMoveBy() throws Exception {
        assumeHasBrowserRole();
        final PopupLauncherPageStation page =
                PopupLauncherPageStation.loadInCurrentTab(
                        mCtaTestRule.getActivityTestRule(), mEntryPage);

        // Opens a pop-up with features 'left=100, top=100, width=300, height=300'. These dimensions
        // are duplicated in popup_launcher.html.
        final CctPageStation popup = SIMPLE_POPUP_BUILDER.build();
        popup.declareEnterCondition(new WindowPositionCondition(popup, 100, 100));
        popup.declareEnterCondition(new WebApiWindowPositionCondition(popup, 100, 100));
        popup.declareEnterCondition(new ViewportSizeCondition(popup, 300, 300));
        popup.declareEnterCondition(new WebApiViewportSizeCondition(popup, 300, 300));
        page.clickOpenPopupWithBounds().inNewTask().arriveAt(popup);

        page.bringWindowToFront();

        final CctPageStation movedPopup = CctPageStation.newBuilder().initFrom(popup).build();
        movedPopup.declareEnterCondition(new WindowPositionCondition(movedPopup, 130, 80));
        movedPopup.declareEnterCondition(new WebApiWindowPositionCondition(movedPopup, 130, 80));
        movedPopup.declareEnterCondition(new ViewportSizeCondition(movedPopup, 300, 300));
        movedPopup.declareEnterCondition(new WebApiViewportSizeCondition(movedPopup, 300, 300));
        page.clickMovePopupBy().withContext(popup).arriveAt(movedPopup);

        TransitAsserts.assertFinalDestinations(page, movedPopup);
    }

    /** Tests whether the {@code window.resizeTo()} web API works. */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    public void testPopupResizesOnWindowResizeTo() throws Exception {
        assumeHasBrowserRole();
        final PopupLauncherPageStation page =
                PopupLauncherPageStation.loadInCurrentTab(
                        mCtaTestRule.getActivityTestRule(), mEntryPage);

        // Opens a pop-up with features 'left=100, top=100, width=300, height=300'. These dimensions
        // are duplicated in popup_launcher.html.
        final CctPageStation popup = SIMPLE_POPUP_BUILDER.build();
        popup.declareEnterCondition(new WindowPositionCondition(popup, 100, 100));
        popup.declareEnterCondition(new WebApiWindowPositionCondition(popup, 100, 100));
        popup.declareEnterCondition(new ViewportSizeCondition(popup, 300, 300));
        popup.declareEnterCondition(new WebApiViewportSizeCondition(popup, 300, 300));
        page.clickOpenPopupWithBounds().inNewTask().arriveAt(popup);

        page.bringWindowToFront();

        final CctPageStation resizedPopup = CctPageStation.newBuilder().initFrom(popup).build();
        resizedPopup.declareEnterCondition(new WindowPositionCondition(resizedPopup, 100, 100));
        resizedPopup.declareEnterCondition(
                new WebApiWindowPositionCondition(resizedPopup, 100, 100));
        resizedPopup.declareEnterCondition(new WindowSizeCondition(resizedPopup, 420, 380));
        resizedPopup.declareEnterCondition(new WebApiWindowSizeCondition(resizedPopup, 420, 380));
        page.clickResizePopupTo().withContext(popup).arriveAt(resizedPopup);

        TransitAsserts.assertFinalDestinations(page, resizedPopup);
    }

    /**
     * Tests whether the {@code window.resizeTo()} web API drops the request if the target size is
     * too small to be compliant with Android's compatibility requirements.
     *
     * <p>All windows on Android must be at least 220dp wide and tall due to compatibility
     * requirements.
     *
     * @see <a
     *     href="https://source.android.com/docs/compatibility/16/android-16-cdd#3814_multi-windows">Android
     *     Compatibility Definition Document</a>
     */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    public void testNoOpOnWindowResizeTo_osWindowSizeLimit() throws Exception {
        assumeHasBrowserRole();
        final PopupLauncherPageStation page =
                PopupLauncherPageStation.loadInCurrentTab(
                        mCtaTestRule.getActivityTestRule(), mEntryPage);

        // Opens a pop-up with features 'left=100, top=100, width=300, height=300'. These dimensions
        // are duplicated in popup_launcher.html.
        final CctPageStation popup = SIMPLE_POPUP_BUILDER.build();
        popup.declareEnterCondition(new WindowPositionCondition(popup, 100, 100));
        popup.declareEnterCondition(new WebApiWindowPositionCondition(popup, 100, 100));
        popup.declareEnterCondition(new ViewportSizeCondition(popup, 300, 300));
        popup.declareEnterCondition(new WebApiViewportSizeCondition(popup, 300, 300));
        page.clickOpenPopupWithBounds().inNewTask().arriveAt(popup);

        page.bringWindowToFront();

        final CctPageStation resizedPopup = CctPageStation.newBuilder().initFrom(popup).build();
        resizedPopup.declareEnterCondition(new WindowPositionCondition(resizedPopup, 100, 100));
        resizedPopup.declareEnterCondition(
                new WebApiWindowPositionCondition(resizedPopup, 100, 100));
        resizedPopup.declareEnterCondition(new ViewportSizeCondition(resizedPopup, 300, 300));
        resizedPopup.declareEnterCondition(new WebApiViewportSizeCondition(resizedPopup, 300, 300));
        page.clickResizePopupToSmallBounds()
                .withContext(popup)
                .withPossiblyAlreadyFulfilled()
                .arriveAt(resizedPopup);

        TransitAsserts.assertFinalDestinations(page, resizedPopup);
    }

    /**
     * Tests whether the {@code window.resizeBy()} web API works. resizeBy() changes the outer
     * window size, so the window size is asserted rather than the viewport size.
     */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    public void testPopupResizesOnWindowResizeBy() throws Exception {
        assumeHasBrowserRole();
        final PopupLauncherPageStation page =
                PopupLauncherPageStation.loadInCurrentTab(
                        mCtaTestRule.getActivityTestRule(), mEntryPage);

        // Opens a pop-up with features 'left=100, top=100, width=300, height=300'. These dimensions
        // are duplicated in popup_launcher.html.
        final CctPageStation popup = SIMPLE_POPUP_BUILDER.build();
        popup.declareEnterCondition(new WindowPositionCondition(popup, 100, 100));
        popup.declareEnterCondition(new WebApiWindowPositionCondition(popup, 100, 100));
        popup.declareEnterCondition(new ViewportSizeCondition(popup, 300, 300));
        popup.declareEnterCondition(new WebApiViewportSizeCondition(popup, 300, 300));
        page.clickOpenPopupWithBounds().inNewTask().arriveAt(popup);

        page.bringWindowToFront();

        // Resizes the pop-up by (-15, 35). These deltas are duplicated in popup_launcher.html.
        final Rect initialWindowBoundsDp =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> getWindowBoundsDpFromContext(popup.getActivity()));
        final int expectedWidthDp = initialWindowBoundsDp.width() - 15;
        final int expectedHeightDp = initialWindowBoundsDp.height() + 35;

        final CctPageStation resizedPopup = CctPageStation.newBuilder().initFrom(popup).build();
        resizedPopup.declareEnterCondition(new WindowPositionCondition(resizedPopup, 100, 100));
        resizedPopup.declareEnterCondition(
                new WebApiWindowPositionCondition(resizedPopup, 100, 100));
        resizedPopup.declareEnterCondition(
                new WindowSizeCondition(resizedPopup, expectedWidthDp, expectedHeightDp));
        resizedPopup.declareEnterCondition(
                new WebApiWindowSizeCondition(resizedPopup, expectedWidthDp, expectedHeightDp));
        page.clickResizePopupBy().withContext(popup).arriveAt(resizedPopup);

        TransitAsserts.assertFinalDestinations(page, resizedPopup);
    }

    private static HistogramWatcher expectNoPermissionOutcome(
            @PopupCreatorImpl.PopupLaunchOutcome int outcome) {
        return HistogramWatcher.newBuilder()
                .expectIntRecord("Android.MultiWindowMode.PopupLaunch.Outcome.FromTab", outcome)
                .build();
    }

    private static void forceUnflaggedLaunchPath() {
        PopupCreatorImpl.setSetMovableTaskRequiredForPopupsForTesting(false);
    }

    private static void assertPopupHasOpener(CctPageStation popup) throws TimeoutException {
        assertEquals(
                "Popup must have a window.opener",
                "true",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        popup.webContentsElement.get(), "window.opener !== null"));
    }

    /**
     * Passes if the view is not displayed or was never inflated (CCT toolbar buttons are inflated
     * on first show, so a never-shown button does not exist).
     */
    private static ViewAssertion isAbsentOrNotDisplayed() {
        return (view, noViewFoundException) -> {
            if (noViewFoundException != null) return;
            matches(not(isDisplayed())).check(view, null);
        };
    }

    private static void assumeHasBrowserRole() {
        assumeTrue(
                "This test requires the APK to be the default browser. "
                        + "Please run "
                        + "'adb shell cmd role add-role-holder android.app.role.BROWSER "
                        + ContextUtils.getApplicationContext().getPackageName()
                        + "'",
                hasBrowserRole());
    }

    private static boolean hasBrowserRole() {
        final Context appContext = ContextUtils.getApplicationContext();
        final var roleManager = appContext.getSystemService(RoleManager.class);
        return roleManager.isRoleHeld(RoleManager.ROLE_BROWSER);
    }

    private static Rect getWindowBoundsDpFromContext(Context context) {
        final Rect windowBoundsPx =
                context.getSystemService(WindowManager.class).getCurrentWindowMetrics().getBounds();
        final float displayDipScale = context.getResources().getDisplayMetrics().density;
        final Rect windowBoundsDp =
                DisplayUtil.scaleToEnclosingRect(windowBoundsPx, 1.0f / displayDipScale);

        return windowBoundsDp;
    }

    /** Helper method to execute JS, handle raw string formatting, and parse the integer safely. */
    private static int getDimensionFromJs(WebContents webContents, String snippet)
            throws TimeoutException, NumberFormatException {
        String resultString =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, snippet);

        if (resultString == null || resultString.isEmpty() || resultString.equals("null")) {
            throw new NumberFormatException("JS returned null or empty for: " + snippet);
        }

        return Integer.parseInt(resultString.replace("\"", ""));
    }

    private static class WindowPositionCondition extends Condition {
        private final Supplier<? extends Activity> mActivitySupplier;
        private final int mLeft;
        private final int mTop;

        private WindowPositionCondition(CctPageStation station, int left, int top) {
            super(/* isRunOnUiThread= */ true);
            mActivitySupplier = dependOnSupplier(station.getActivityElement(), "Activity");
            mLeft = left;
            mTop = top;
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            final Rect windowBoundsDp = getWindowBoundsDpFromContext(mActivitySupplier.get());

            if (Math.abs(windowBoundsDp.left - mLeft) > BOUNDS_TOLERANCE) {
                return notFulfilled(
                        "Window origin x coordinate mismatch, expected: <"
                                + mLeft
                                + ">, actual: <"
                                + windowBoundsDp.left
                                + ">, tolerance: <"
                                + BOUNDS_TOLERANCE
                                + ">");
            }

            if (Math.abs(windowBoundsDp.top - mTop) > BOUNDS_TOLERANCE) {
                return notFulfilled(
                        "Window origin y coordinate mismatch, expected: <"
                                + mTop
                                + ">, actual: <"
                                + windowBoundsDp.top
                                + ">, tolerance: <"
                                + BOUNDS_TOLERANCE
                                + ">");
            }

            return fulfilled("Window origin coordinates within tolerance");
        }

        @Override
        public String buildDescription() {
            return "Window origin coordinates close to (" + mLeft + ", " + mTop + ")";
        }
    }

    private static class WindowSizeCondition extends Condition {
        private final Supplier<? extends Activity> mActivitySupplier;
        private final int mWidth;
        private final int mHeight;

        private WindowSizeCondition(CctPageStation station, int width, int height) {
            super(/* isRunOnUiThread= */ true);
            mActivitySupplier = dependOnSupplier(station.getActivityElement(), "Activity");
            mWidth = width;
            mHeight = height;
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            final Rect windowBoundsDp = getWindowBoundsDpFromContext(mActivitySupplier.get());

            if (Math.abs(windowBoundsDp.width() - mWidth) > BOUNDS_TOLERANCE) {
                return notFulfilled(
                        "Window width mismatch, expected: <"
                                + mWidth
                                + ">, actual: <"
                                + windowBoundsDp.width()
                                + ">, tolerance: <"
                                + BOUNDS_TOLERANCE
                                + ">");
            }

            if (Math.abs(windowBoundsDp.height() - mHeight) > BOUNDS_TOLERANCE) {
                return notFulfilled(
                        "Window height mismatch, expected: <"
                                + mHeight
                                + ">, actual: <"
                                + windowBoundsDp.height()
                                + ">, tolerance: <"
                                + BOUNDS_TOLERANCE
                                + ">");
            }

            return fulfilled("Window size within tolerance");
        }

        @Override
        public String buildDescription() {
            return "Window size close to (" + mWidth + ", " + mHeight + ")";
        }
    }

    private static class ViewportSizeCondition extends Condition {
        private final Supplier<WebContents> mWebContentsSupplier;
        private final int mWidth;
        private final int mHeight;

        private ViewportSizeCondition(CctPageStation station, int width, int height) {
            super(/* isRunOnUiThread= */ true);
            mWebContentsSupplier = dependOnSupplier(station.webContentsElement, "WebContents");
            mWidth = width;
            mHeight = height;
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            final WebContents webContents = mWebContentsSupplier.get();

            if (Math.abs(webContents.getWidth() - mWidth) > BOUNDS_TOLERANCE) {
                return notFulfilled(
                        "Viewport width mismatch, expected: <"
                                + mWidth
                                + ">, actual: <"
                                + webContents.getWidth()
                                + ">, tolerance: <"
                                + BOUNDS_TOLERANCE
                                + ">");
            }

            if (Math.abs(webContents.getHeight() - mHeight) > BOUNDS_TOLERANCE) {
                return notFulfilled(
                        "Viewport height mismatch, expected: <"
                                + mHeight
                                + ">, actual: <"
                                + webContents.getHeight()
                                + ">, tolerance: <"
                                + BOUNDS_TOLERANCE
                                + ">");
            }

            return fulfilled("Viewport size within tolerance");
        }

        @Override
        public String buildDescription() {
            return "Viewport size close to (" + mWidth + ", " + mHeight + ")";
        }
    }

    private static class ActivityFocusedCondition<ActivityT extends Activity> extends Condition {
        private final Supplier<ActivityT> mActivitySupplier;

        private ActivityFocusedCondition(Supplier<ActivityT> activitySupplier) {
            super(/* isRunOnUiThread= */ true);
            mActivitySupplier = dependOnSupplier(activitySupplier, "Activity");
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            if (!mActivitySupplier.get().getWindow().getDecorView().hasWindowFocus()) {
                return notFulfilled("Activity window is not focused");
            }

            return fulfilled("Activity window is focused");
        }

        @Override
        public String buildDescription() {
            return "Activity window is focused";
        }
    }

    private static double getZoomFactor() {
        if (ContentFeatureList.sAndroidDesktopZoomScaling.isEnabled() && DeviceInfo.isDesktop()) {
            return ContentFeatureList.sAndroidDesktopZoomScalingFactor.getValue() / 100.0;
        }
        return 1.0;
    }

    private static class WebApiWindowSizeCondition extends Condition {
        private final Supplier<? extends WebContents> mWebContentsSupplier;
        private final int mWidth;
        private final int mHeight;

        private WebApiWindowSizeCondition(CctPageStation station, int width, int height) {
            super(/* isRunOnUiThread= */ false);
            mWebContentsSupplier = dependOnSupplier(station.webContentsElement, "WebContents");
            mWidth = width;
            mHeight = height;
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            int webApiWindowWidth;
            int webApiWindowHeight;

            try {
                webApiWindowWidth =
                        getDimensionFromJs(mWebContentsSupplier.get(), "window.outerWidth");
                webApiWindowHeight =
                        getDimensionFromJs(mWebContentsSupplier.get(), "window.outerHeight");
            } catch (TimeoutException e) {
                return notFulfilled("Timeout during JS execution: " + e.getMessage());
            } catch (NumberFormatException e) {
                return notFulfilled("Failed to parse dimension from JS: " + e.getMessage());
            }

            if (Math.abs(webApiWindowWidth - mWidth) > BOUNDS_TOLERANCE) {
                return notFulfilled(
                        "window.outerWidth mismatch, expected: <"
                                + mWidth
                                + ">, actual: <"
                                + webApiWindowWidth
                                + ">, tolerance: <"
                                + BOUNDS_TOLERANCE
                                + ">");
            }

            if (Math.abs(webApiWindowHeight - mHeight) > BOUNDS_TOLERANCE) {
                return notFulfilled(
                        "window.outerHeight mismatch, expected: <"
                                + mHeight
                                + ">, actual: <"
                                + webApiWindowHeight
                                + ">, tolerance: <"
                                + BOUNDS_TOLERANCE
                                + ">");
            }

            return fulfilled("Window size reported by Web APIs within tolerance");
        }

        @Override
        public String buildDescription() {
            return "Window size reported by Web APIs close to (" + mWidth + ", " + mHeight + ")";
        }
    }

    private static class WebApiWindowPositionCondition extends Condition {
        private final Supplier<? extends WebContents> mWebContentsSupplier;
        private final int mLeft;
        private final int mTop;

        private WebApiWindowPositionCondition(CctPageStation station, int left, int top) {
            super(/* isRunOnUiThread= */ false);
            mWebContentsSupplier = dependOnSupplier(station.webContentsElement, "WebContents");
            mLeft = left;
            mTop = top;
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            int webApiWindowLeft;
            int webApiWindowTop;

            try {
                webApiWindowLeft = getDimensionFromJs(mWebContentsSupplier.get(), "window.screenX");
                webApiWindowTop = getDimensionFromJs(mWebContentsSupplier.get(), "window.screenY");
            } catch (TimeoutException e) {
                return notFulfilled("Timeout during JS execution: " + e.getMessage());
            } catch (NumberFormatException e) {
                return notFulfilled("Failed to parse dimension from JS: " + e.getMessage());
            }

            if (Math.abs(webApiWindowLeft - mLeft) > BOUNDS_TOLERANCE) {
                return notFulfilled(
                        "window.screenX mismatch, expected: <"
                                + mLeft
                                + ">, actual: <"
                                + webApiWindowLeft
                                + ">, tolerance: <"
                                + BOUNDS_TOLERANCE
                                + ">");
            }

            if (Math.abs(webApiWindowTop - mTop) > BOUNDS_TOLERANCE) {
                return notFulfilled(
                        "window.screenY mismatch, expected: <"
                                + mTop
                                + ">, actual: <"
                                + webApiWindowTop
                                + ">, tolerance: <"
                                + BOUNDS_TOLERANCE
                                + ">");
            }

            return fulfilled("Window position reported by Web APIs within tolerance");
        }

        @Override
        public String buildDescription() {
            return "Window position reported by Web APIs close to (" + mLeft + ", " + mTop + ")";
        }
    }

    private static class WebApiViewportSizeCondition extends Condition {
        private final Supplier<WebContents> mWebContentsSupplier;
        private final int mWidth;
        private final int mHeight;

        private WebApiViewportSizeCondition(CctPageStation station, int width, int height) {
            super(/* isRunOnUiThread= */ false);
            mWebContentsSupplier = dependOnSupplier(station.webContentsElement, "WebContents");
            mWidth = width;
            mHeight = height;
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            int webApiViewportWidth;
            int webApiViewportHeight;

            try {
                webApiViewportWidth =
                        getDimensionFromJs(mWebContentsSupplier.get(), "window.innerWidth");
                webApiViewportHeight =
                        getDimensionFromJs(mWebContentsSupplier.get(), "window.innerHeight");
            } catch (TimeoutException e) {
                return notFulfilled("Timeout during JS execution: " + e.getMessage());
            } catch (NumberFormatException e) {
                return notFulfilled("Failed to parse dimension from JS: " + e.getMessage());
            }

            double zoomFactor = getZoomFactor();
            int expectedWidth = (int) Math.round(mWidth / zoomFactor);
            int expectedHeight = (int) Math.round(mHeight / zoomFactor);

            if (Math.abs(webApiViewportWidth - expectedWidth) > BOUNDS_TOLERANCE) {
                return notFulfilled(
                        "window.innerWidth mismatch, expected: <"
                                + expectedWidth
                                + "> (original: <"
                                + mWidth
                                + ">), actual: <"
                                + webApiViewportWidth
                                + ">, tolerance: <"
                                + BOUNDS_TOLERANCE
                                + ">");
            }

            if (Math.abs(webApiViewportHeight - expectedHeight) > BOUNDS_TOLERANCE) {
                return notFulfilled(
                        "window.innerHeight mismatch, expected: <"
                                + expectedHeight
                                + "> (original: <"
                                + mHeight
                                + ">), actual: <"
                                + webApiViewportHeight
                                + ">, tolerance: <"
                                + BOUNDS_TOLERANCE
                                + ">");
            }

            return fulfilled("Viewport size reported by Web APIs within tolerance");
        }

        @Override
        public String buildDescription() {
            return "Viewport size reported by Web APIs close to (" + mWidth + ", " + mHeight + ")";
        }
    }
}
