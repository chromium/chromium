// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.CoreMatchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.ComponentCallbacks;
import android.content.res.Configuration;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.annotation.UiThreadTest;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.ui.KeyboardUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarSceneLayer;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarSceneLayerJni;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.findinpage.FindToolbar;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManagerSupplier;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests for toolbar manager behavior. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ToolbarTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BookmarkBarSceneLayer.Natives mBookmarkBarSceneLayerJniMock;

    private static final String TEST_PAGE = "/chrome/test/data/android/test.html";
    private WebPageStation mPage;
    private ChromeTabbedActivity mActivity;

    @Before
    public void setUp() throws InterruptedException {
        BookmarkBarUtils.setBookmarkBarVisibleForTesting(true);
        BookmarkBarSceneLayerJni.setInstanceForTesting(mBookmarkBarSceneLayerJniMock);

        TabbedRootUiCoordinator.setDisableTopControlsAnimationsForTesting(true);
        mPage = mActivityTestRule.startOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
    }

    @After
    public void tearDown() {
        setAccessibilityEnabled(false);
    }

    private void findInPageFromMenu() {
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), mActivity, R.id.find_in_page_id);

        waitForFindInPageVisibility(true);
    }

    private void waitForFindInPageVisibility(final boolean visible) {
        CriteriaHelper.pollUiThread(
                () -> {
                    FindToolbar findToolbar =
                            (FindToolbar) mActivity.findViewById(R.id.find_toolbar);
                    if (visible) {
                        Criteria.checkThat(findToolbar, Matchers.notNullValue());
                        Criteria.checkThat(findToolbar.isShown(), Matchers.is(true));
                    } else {
                        if (findToolbar == null) return;
                        Criteria.checkThat(findToolbar.isShown(), Matchers.is(false));
                    }
                    Criteria.checkThat(findToolbar.isAnimating(), Matchers.is(false));
                });
    }

    private boolean isErrorPage(final Tab tab) {
        final boolean[] isShowingError = new boolean[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    isShowingError[0] = tab.isShowingErrorPage();
                });
        return isShowingError[0];
    }

    @Test
    @MediumTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
    @Restriction({DeviceFormFactor.PHONE})
    public void testControlContainerTopMarginWhenBookmarkBarIsDisabledOnPhone() {
        testControlContainerTopMargin(/* expectBookmarkBar= */ false);
    }

    @Test
    @MediumTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testControlContainerTopMarginWhenBookmarkBarIsDisabledOnTablet() {
        testControlContainerTopMargin(/* expectBookmarkBar= */ false);
    }

    @Test
    @MediumTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
    @Restriction({DeviceFormFactor.PHONE})
    @DisabledTest
    // TODO(crbug.com/447525636): Re-enable tests.
    public void testControlContainerTopMarginWhenBookmarkBarIsEnabledOnPhone() {
        testControlContainerTopMargin(/* expectBookmarkBar= */ false);
    }

    @Test
    @MediumTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testControlContainerTopMarginWhenBookmarkBarIsEnabledOnTablet() {
        testControlContainerTopMargin(/* expectBookmarkBar= */ true);
    }

    private void testControlContainerTopMargin(boolean expectBookmarkBar) {
        // Verify bookmark bar (in-)existence.
        final @Nullable var bookmarkBar = mActivity.findViewById(R.id.bookmark_bar);
        assertThat(bookmarkBar != null).isEqualTo(expectBookmarkBar);

        // Verify browser controls manager existence.
        final var browserControlsManager =
                BrowserControlsManagerSupplier.getValueOrNullFrom(mActivity.getWindowAndroid());
        assertNotNull(browserControlsManager);

        // Verify control container existence.
        final var toolbarManager = mActivity.getToolbarManager();
        assertNotNull(toolbarManager);
        final var controlContainer =
                (ToolbarControlContainer) toolbarManager.getContainerViewForTesting();
        assertNotNull(controlContainer);

        // Verify control container top margin.
        final int bookmarkBarHeight = bookmarkBar != null ? bookmarkBar.getHeight() : 0;
        final int controlContainerHeight = controlContainer.getHeight();
        final int hairlineHeight = controlContainer.getToolbarHairlineHeight();
        final int topControlsHeight = browserControlsManager.getTopControlsHeight();
        assertEquals(
                "Verify control container top margin.",
                topControlsHeight - (controlContainerHeight - hairlineHeight) - bookmarkBarHeight,
                ((MarginLayoutParams) controlContainer.getLayoutParams()).topMargin);
    }

    @Test
    @MediumTest
    public void testOmniboxScrim() {
        ToolbarManager toolbarManager = mActivity.getToolbarManager();
        ScrimManager scrimManager = mActivity.getRootUiCoordinatorForTesting().getScrimManager();
        scrimManager.disableAnimationForTesting(true);

        assertNull("The scrim should be null.", scrimManager.getViewForTesting());
        assertFalse(
                "All tabs should not currently be obscured.",
                mActivity.getTabObscuringHandler().isTabContentObscured());

        ThreadUtils.runOnUiThreadBlocking(
                () -> toolbarManager.setUrlBarFocus(true, OmniboxFocusReason.OMNIBOX_TAP));

        assertNotNull("The scrim should not be null.", scrimManager.getViewForTesting());
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            "All tabs should currently be obscured.",
                            mActivity.getTabObscuringHandler().isTabContentObscured(),
                            Matchers.is(true));
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> toolbarManager.setUrlBarFocus(false, OmniboxFocusReason.OMNIBOX_TAP));
        assertNull("The scrim should be null.", scrimManager.getViewForTesting());
        assertFalse(
                "All tabs should not currently be obscured.",
                mActivity.getTabObscuringHandler().isTabContentObscured());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1230091")
    public void testNtpNavigatesToErrorPageOnDisconnectedNetwork() {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        String testUrl = testServer.getURL(TEST_PAGE);

        Tab tab = mActivityTestRule.getActivityTab();

        // Load new tab page.
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        Assert.assertEquals(UrlConstants.NTP_URL, ChromeTabUtils.getUrlStringOnUiThread(tab));
        assertFalse(isErrorPage(tab));

        // Stop the server and also disconnect the network.
        testServer.stopAndDestroyServer();
        ThreadUtils.runOnUiThreadBlocking(
                () -> NetworkChangeNotifier.forceConnectivityState(false));

        mActivityTestRule.loadUrl(testUrl);
        Assert.assertEquals(testUrl, ChromeTabUtils.getUrlStringOnUiThread(tab));
        assertTrue(isErrorPage(tab));
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    @Feature({"Omnibox"})
    public void testFindInPageDismissedOnOmniboxFocus() {
        findInPageFromMenu();
        OmniboxTestUtils omnibox = new OmniboxTestUtils(mActivity);
        omnibox.requestFocus();
        waitForFindInPageVisibility(false);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testNtpOmniboxFocusAndUnfocusWithHardwareKeyboardConnected() {
        // Simulate availability of a hardware keyboard.
        mActivity.getResources().getConfiguration().keyboard = Configuration.KEYBOARD_QWERTY;

        // If soft keyboard is requested while hardware keyboard is connected - do not prefocus the
        // Omnibox, as it will automatically call up software keyboard.
        boolean wantPrefocus = !KeyboardUtils.shouldShowImeWithHardwareKeyboard(mActivity);

        // Open a new tab.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivity, false, true);
        // Verify that the omnibox is focused when the NTP is loaded.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mActivity
                                    .getToolbarManager()
                                    .getLocationBar()
                                    .getOmniboxStub()
                                    .isUrlBarFocused(),
                            Matchers.is(wantPrefocus));
                });

        // Navigate away from the NTP.
        mActivityTestRule.loadUrl(UrlConstants.GOOGLE_URL);
        // Verify that the omnibox is unfocused on exit from the NTP.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mActivity
                                    .getToolbarManager()
                                    .getLocationBar()
                                    .getOmniboxStub()
                                    .isUrlBarFocused(),
                            Matchers.is(false));
                });
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testMaybeShowUrlBarFocusIfHardwareKeyboardAvailable_newTabFromTabSwitcher() {
        // Simulate availability of a hardware keyboard.
        mActivity.getResources().getConfiguration().keyboard = Configuration.KEYBOARD_QWERTY;

        // If soft keyboard is requested while hardware keyboard is connected - do not prefocus the
        // Omnibox, as it will automatically call up software keyboard.
        boolean wantPrefocus = !KeyboardUtils.shouldShowImeWithHardwareKeyboard(mActivity);

        // Open a new tab from the tab switcher.
        onViewWaiting(allOf(withId(R.id.tab_switcher_button), isDisplayed()));
        onView(withId(R.id.tab_switcher_button)).perform(click());
        onView(withId(R.id.toolbar_action_button)).check(matches(isDisplayed()));
        onView(withId(R.id.toolbar_action_button)).perform(click());

        LayoutTestUtils.waitForLayout(mActivity.getLayoutManager(), LayoutType.BROWSING);

        // Verify that the omnibox is in the correct focus state when the NTP is loaded.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mActivity
                                    .getToolbarManager()
                                    .getLocationBar()
                                    .getOmniboxStub()
                                    .isUrlBarFocused(),
                            Matchers.is(wantPrefocus));
                });
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    @DisabledTest(message = "Flaky, see crbug.com/464502425")
    public void testToggleTabStripVisibility() {
        int tabStripHeightResource =
                mActivity.getResources().getDimensionPixelSize(R.dimen.tab_strip_height);
        int toolbarLayoutHeight =
                mActivity.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                        + mActivity
                                .getResources()
                                .getDimensionPixelSize(R.dimen.toolbar_hairline_height);
        checkTabStripHeightOnUiThread(tabStripHeightResource);
        ComponentCallbacks tabStripCallback =
                mActivity.getToolbarManager().getTabStripTransitionCoordinator();
        Assert.assertNotNull("Tab strip transition callback is null.", tabStripCallback);

        // Set the screen width bucket and trigger an configuration change to force toggle tab strip
        // visibility. This is an test only strategy, as we don't want to actually change the
        // configuration which might result in an activity restart.
        TabStripTransitionCoordinator.setHeightTransitionThresholdForTesting(10000);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        tabStripCallback.onConfigurationChanged(
                                mActivity.getResources().getConfiguration()));
        checkTabStripHeightOnUiThread(0);
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mActivity
                                        .getToolbarManager()
                                        .getContainerViewForTesting()
                                        .getHeight(),
                                Matchers.equalTo(toolbarLayoutHeight)));
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mActivity
                                        .getToolbarManager()
                                        .getStatusBarColorController()
                                        .getStatusBarColorWithoutStatusIndicator(),
                                Matchers.equalTo(mActivity.getToolbarManager().getPrimaryColor())));

        TabStripTransitionCoordinator.setHeightTransitionThresholdForTesting(1);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        tabStripCallback.onConfigurationChanged(
                                mActivity.getResources().getConfiguration()));
        checkTabStripHeightOnUiThread(tabStripHeightResource);
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mActivity
                                        .getToolbarManager()
                                        .getContainerViewForTesting()
                                        .getHeight(),
                                Matchers.equalTo(toolbarLayoutHeight + tabStripHeightResource)));
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mActivity
                                        .getToolbarManager()
                                        .getStatusBarColorController()
                                        .getStatusBarColorWithoutStatusIndicator(),
                                Matchers.equalTo(
                                        TabUiThemeUtil.getTabStripBackgroundColor(
                                                mActivity, /* isIncognito= */ false))));
    }

    private void checkTabStripHeightOnUiThread(int tabStripHeight) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mActivity.getToolbarManager(), Matchers.notNullValue());
                    Criteria.checkThat(
                            "Tab strip height is different",
                            mActivity.getToolbarManager().getTabStripHeightSupplier().get(),
                            Matchers.equalTo(tabStripHeight));
                });
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testIncognitoNtpAccessibilityOrder_TopControls() throws Exception {
        setAccessibilityEnabled(true);

        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);

        final ToolbarPhone toolbarPhone =
                (ToolbarPhone) mActivity.getToolbarManager().getToolbarLayoutForTesting();
        final View incognitoNtpView = mActivityTestRule.getActivityTab().getView();

        setControlsPosition(ControlsPosition.TOP);
        verifyTopControlsAccessibilityOrder(toolbarPhone, incognitoNtpView);
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testIncognitoNtpAccessibilityOrder_BottomControls() throws Exception {
        setAccessibilityEnabled(true);

        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);

        final ToolbarPhone toolbarPhone =
                (ToolbarPhone) mActivity.getToolbarManager().getToolbarLayoutForTesting();
        final View incognitoNtpView = mActivityTestRule.getActivityTab().getView();

        setControlsPosition(ControlsPosition.BOTTOM);
        verifyBottomControlsAccessibilityOrder(toolbarPhone, incognitoNtpView);
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testRegularNtpAccessibilityOrder_NoEffect() throws Exception {
        setAccessibilityEnabled(true);

        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, false);

        final ToolbarPhone toolbarPhone =
                (ToolbarPhone) mActivity.getToolbarManager().getToolbarLayoutForTesting();
        final View regularNtpView = mActivityTestRule.getActivityTab().getView();

        setControlsPosition(ControlsPosition.BOTTOM);
        verifyAccessibilityOrderIsReset(toolbarPhone, regularNtpView);
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testIncognitoNtpAccessibilityOrder_OnNavigating() throws Exception {
        setAccessibilityEnabled(true);

        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);

        final Tab incognitoNtpTab = mActivityTestRule.getActivityTab();
        ToolbarPhone toolbarPhone =
                (ToolbarPhone) mActivity.getToolbarManager().getToolbarLayoutForTesting();

        mActivityTestRule.loadUrl("about:blank");

        // Verify accessibility order is reset after navigating.
        verifyAccessibilityOrderIsReset(toolbarPhone, null);

        setControlsPosition(ControlsPosition.BOTTOM);

        ThreadUtils.runOnUiThreadBlocking(mActivity::onBackPressed);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mActivityTestRule.getActivityTab(), Matchers.is(incognitoNtpTab));
                });

        NewTabPageTestUtils.waitForNtpLoaded(incognitoNtpTab);

        verifyBottomControlsAccessibilityOrder(toolbarPhone, incognitoNtpTab.getView());
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testIncognitoNtpAccessibilityOrder_OnIncognitoTabsSwitch() throws Exception {
        setAccessibilityEnabled(true);

        final Tab incognitoNtpTab = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        // Open the second incognito tab that will be active on load
        mActivityTestRule.loadUrlInNewTab("about:blank", true);

        setControlsPosition(ControlsPosition.BOTTOM);

        // Switch to the first tab
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel incognitoModel = mActivity.getTabModelSelector().getModel(true);
                    incognitoModel.setIndex(
                            incognitoModel.indexOf(incognitoNtpTab), TabSelectionType.FROM_USER);
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mActivityTestRule.getActivityTab(), Matchers.is(incognitoNtpTab));
                });
        NewTabPageTestUtils.waitForNtpLoaded(incognitoNtpTab);

        ToolbarPhone toolbarPhone =
                (ToolbarPhone) mActivity.getToolbarManager().getToolbarLayoutForTesting();
        View ntpView = incognitoNtpTab.getView();
        verifyBottomControlsAccessibilityOrder(toolbarPhone, ntpView);
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testIncognitoNtpAccessibilityOrder_OnEnterAndExitTabSwitcher() throws Exception {
        setAccessibilityEnabled(true);

        // 1. Load an incognito NTP.
        final Tab incognitoNtpTab = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);

        // 2. Change toolbar position to bottom to verify order is handled correctly.
        setControlsPosition(ControlsPosition.BOTTOM);

        // 3. Verify accessibility order is correct for bottom controls.
        ToolbarPhone toolbarPhone =
                (ToolbarPhone) mActivity.getToolbarManager().getToolbarLayoutForTesting();
        View ntpView = incognitoNtpTab.getView();
        verifyBottomControlsAccessibilityOrder(toolbarPhone, ntpView);

        // 4. Enter the tab switcher.
        LayoutTestUtils.startShowingAndWaitForLayout(
                mActivity.getLayoutManager(), LayoutType.TAB_SWITCHER, false);

        // 5. Verify accessibility order is reset upon entering tab switcher.
        verifyAccessibilityOrderIsReset(toolbarPhone, null);

        // 6. Exit the tab switcher.
        LayoutTestUtils.startShowingAndWaitForLayout(
                mActivity.getLayoutManager(), LayoutType.BROWSING, false);
        NewTabPageTestUtils.waitForNtpLoaded(incognitoNtpTab);

        // 7. Verify accessibility order is restored.
        verifyBottomControlsAccessibilityOrder(toolbarPhone, incognitoNtpTab.getView());
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testIncognitoNtpAccessibilityOrder_ResetOnOpenRegularTab() throws Exception {
        setAccessibilityEnabled(true);

        final Tab incognitoNtpTab = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);

        setControlsPosition(ControlsPosition.BOTTOM);

        ToolbarPhone toolbarPhone =
                (ToolbarPhone) mActivity.getToolbarManager().getToolbarLayoutForTesting();
        View ntpView = incognitoNtpTab.getView();
        verifyBottomControlsAccessibilityOrder(toolbarPhone, ntpView);

        mActivityTestRule.loadUrlInNewTab("about:blank", false);

        verifyAccessibilityOrderIsReset(toolbarPhone, null);
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testIncognitoNtpAccessibilityOrder_OnMultipleControlsPositionChanges()
            throws Exception {
        setAccessibilityEnabled(true);

        final Tab incognitoNtpTab = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);

        final BrowserControlsManager browserControlsManager =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                BrowserControlsManagerSupplier.getValueOrNullFrom(
                                        mActivity.getWindowAndroid()));
        assertNotNull(browserControlsManager);

        // Switch controls position (from top to bottom in loop) multiple times to ensure the
        // accessibility order is correctly updated on each change.
        for (int i = 0; i < 3; i++) {
            setControlsPosition(ControlsPosition.BOTTOM);
            ToolbarPhone toolbarPhone =
                    (ToolbarPhone) mActivity.getToolbarManager().getToolbarLayoutForTesting();
            View ntpView = incognitoNtpTab.getView();
            verifyBottomControlsAccessibilityOrder(toolbarPhone, ntpView);

            setControlsPosition(ControlsPosition.TOP);
            verifyTopControlsAccessibilityOrder(toolbarPhone, ntpView);
        }
    }

    private void setAccessibilityEnabled(boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(enabled));
    }

    private void setControlsPosition(@ControlsPosition int position) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final BrowserControlsManager browserControlsManager =
                            BrowserControlsManagerSupplier.getValueOrNullFrom(
                                    mActivity.getWindowAndroid());
                    assertNotNull(browserControlsManager);
                    if (browserControlsManager.getControlsPosition() == position) return;

                    final int controlsHeight;
                    final int controlsMinHeight;

                    if (position == ControlsPosition.BOTTOM) {
                        controlsHeight = browserControlsManager.getTopControlsHeight();
                        controlsMinHeight = browserControlsManager.getTopControlsMinHeight();
                        browserControlsManager.setControlsPosition(
                                ControlsPosition.BOTTOM,
                                0,
                                0,
                                0,
                                controlsHeight,
                                controlsMinHeight,
                                0);
                    } else {
                        controlsHeight = browserControlsManager.getBottomControlsHeight();
                        controlsMinHeight = browserControlsManager.getBottomControlsMinHeight();
                        browserControlsManager.setControlsPosition(
                                ControlsPosition.TOP,
                                controlsHeight,
                                controlsMinHeight,
                                0,
                                0,
                                0,
                                0);
                    }
                });
    }

    private void verifyTopControlsAccessibilityOrder(
            @NonNull ToolbarPhone toolbar, @NonNull View ntpView) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Toolbar should be first.",
                            toolbar.getAccessibilityTraversalAfter(),
                            Matchers.is(View.NO_ID));
                    Criteria.checkThat(
                            "NTP view should be after toolbar.",
                            ntpView.getAccessibilityTraversalAfter(),
                            Matchers.is(R.id.toolbar));
                });
    }

    private void verifyBottomControlsAccessibilityOrder(
            @NonNull ToolbarPhone toolbar, @NonNull View ntpView) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "NTP view should be first.",
                            ntpView.getAccessibilityTraversalAfter(),
                            Matchers.is(View.NO_ID));
                    Criteria.checkThat(
                            "Toolbar should be after NTP view.",
                            toolbar.getAccessibilityTraversalAfter(),
                            Matchers.is(ntpView.getId()));
                });
    }

    private void verifyAccessibilityOrderIsReset(
            @NonNull ToolbarPhone toolbar, @Nullable View ntpView) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Toolbar accessibility should be reset.",
                            toolbar.getAccessibilityTraversalAfter(),
                            Matchers.is(View.NO_ID));
                    if (ntpView != null) {
                        Criteria.checkThat(
                                "NTP view accessibility should be reset.",
                                ntpView.getAccessibilityTraversalAfter(),
                                Matchers.is(View.NO_ID));
                    }
                });
    }
}
