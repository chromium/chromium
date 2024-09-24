// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.ComponentCallbacks;
import android.content.res.Configuration;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.findinpage.FindToolbar;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.KeyboardUtils;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests for toolbar manager behavior. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ToolbarTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TEST_PAGE = "/chrome/test/data/android/test.html";

    @Before
    public void setUp() throws InterruptedException {
        TabbedRootUiCoordinator.setDisableTopControlsAnimationsForTesting(true);
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    private void findInPageFromMenu() {
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                R.id.find_in_page_id);

        waitForFindInPageVisibility(true);
    }

    private void waitForFindInPageVisibility(final boolean visible) {
        CriteriaHelper.pollUiThread(
                () -> {
                    FindToolbar findToolbar =
                            (FindToolbar)
                                    mActivityTestRule.getActivity().findViewById(R.id.find_toolbar);
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
    public void testOmniboxScrim() {
        ChromeActivity activity = mActivityTestRule.getActivity();
        ToolbarManager toolbarManager = activity.getToolbarManager();
        ScrimCoordinator scrimCoordinator =
                activity.getRootUiCoordinatorForTesting().getScrimCoordinatorForTesting();
        scrimCoordinator.disableAnimationForTesting(true);

        assertNull("The scrim should be null.", scrimCoordinator.getViewForTesting());
        assertFalse(
                "All tabs should not currently be obscured.",
                activity.getTabObscuringHandler().isTabContentObscured());

        ThreadUtils.runOnUiThreadBlocking(() -> toolbarManager.setUrlBarFocus(true, 0));

        assertNotNull("The scrim should not be null.", scrimCoordinator.getViewForTesting());
        assertTrue(
                "All tabs should currently be obscured.",
                activity.getTabObscuringHandler().isTabContentObscured());

        ThreadUtils.runOnUiThreadBlocking(() -> toolbarManager.setUrlBarFocus(false, 0));

        assertNull("The scrim should be null.", scrimCoordinator.getViewForTesting());
        assertFalse(
                "All tabs should not currently be obscured.",
                activity.getTabObscuringHandler().isTabContentObscured());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1230091")
    public void testNtpNavigatesToErrorPageOnDisconnectedNetwork() {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        String testUrl = testServer.getURL(TEST_PAGE);

        Tab tab = mActivityTestRule.getActivity().getActivityTab();

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
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"Omnibox"})
    public void testFindInPageDismissedOnOmniboxFocus() {
        findInPageFromMenu();
        OmniboxTestUtils omnibox = new OmniboxTestUtils(mActivityTestRule.getActivity());
        omnibox.requestFocus();
        waitForFindInPageVisibility(false);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET)
    public void testNtpOmniboxFocusAndUnfocusWithHardwareKeyboardConnected() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        // Simulate availability of a hardware keyboard.
        activity.getResources().getConfiguration().keyboard = Configuration.KEYBOARD_QWERTY;

        // If soft keyboard is requested while hardware keyboard is connected - do not prefocus the
        // Omnibox, as it will automatically call up software keyboard.
        boolean wantPrefocus = !KeyboardUtils.shouldShowImeWithHardwareKeyboard(activity);

        // Open a new tab.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), activity, false, true);
        // Verify that the omnibox is focused when the NTP is loaded.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            activity.getToolbarManager()
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
                            activity.getToolbarManager()
                                    .getLocationBar()
                                    .getOmniboxStub()
                                    .isUrlBarFocused(),
                            Matchers.is(false));
                });
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET)
    public void testMaybeShowUrlBarFocusIfHardwareKeyboardAvailable_newTabFromTabSwitcher() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        // Simulate availability of a hardware keyboard.
        activity.getResources().getConfiguration().keyboard = Configuration.KEYBOARD_QWERTY;

        // If soft keyboard is requested while hardware keyboard is connected - do not prefocus the
        // Omnibox, as it will automatically call up software keyboard.
        boolean wantPrefocus = !KeyboardUtils.shouldShowImeWithHardwareKeyboard(activity);

        // Open a new tab from the tab switcher.
        onViewWaiting(allOf(withId(R.id.tab_switcher_button), isDisplayed()));
        onView(withId(R.id.tab_switcher_button)).perform(click());
        onView(withText(R.string.menu_new_tab)).check(matches(isDisplayed()));
        onView(withText(R.string.menu_new_tab)).perform(click());

        LayoutTestUtils.waitForLayout(activity.getLayoutManager(), LayoutType.BROWSING);

        // Verify that the omnibox is in the correct focus state when the NTP is loaded.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            activity.getToolbarManager()
                                    .getLocationBar()
                                    .getOmniboxStub()
                                    .isUrlBarFocused(),
                            Matchers.is(wantPrefocus));
                });
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_LAYOUT_OPTIMIZATION)
    @Restriction(DeviceFormFactor.TABLET)
    public void testToggleTabStripVisibility() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        int tabStripHeightResource =
                activity.getResources().getDimensionPixelSize(R.dimen.tab_strip_height);
        int toolbarLayoutHeight =
                activity.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                        + activity.getResources()
                                .getDimensionPixelSize(R.dimen.toolbar_hairline_height);
        checkTabStripHeightOnUiThread(tabStripHeightResource);
        ComponentCallbacks tabStripCallback =
                activity.getToolbarManager().getTabStripTransitionCoordinator();
        Assert.assertNotNull("Tab strip transition callback is null.", tabStripCallback);

        // Set the screen width bucket and trigger an configuration change to force toggle tab strip
        // visibility. This is an test only strategy, as we don't want to actually change the
        // configuration which might result in an activity restart.
        TabStripTransitionCoordinator.setHeightTransitionThresholdForTesting(10000);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        tabStripCallback.onConfigurationChanged(
                                activity.getResources().getConfiguration()));
        checkTabStripHeightOnUiThread(0);
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                activity.getToolbarManager()
                                        .getContainerViewForTesting()
                                        .getHeight(),
                                Matchers.equalTo(toolbarLayoutHeight)));
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                activity.getToolbarManager()
                                        .getStatusBarColorController()
                                        .getStatusBarColorWithoutStatusIndicator(),
                                Matchers.equalTo(activity.getToolbarManager().getPrimaryColor())));

        TabStripTransitionCoordinator.setHeightTransitionThresholdForTesting(1);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        tabStripCallback.onConfigurationChanged(
                                activity.getResources().getConfiguration()));
        checkTabStripHeightOnUiThread(tabStripHeightResource);
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                activity.getToolbarManager()
                                        .getContainerViewForTesting()
                                        .getHeight(),
                                Matchers.equalTo(toolbarLayoutHeight + tabStripHeightResource)));
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                activity.getToolbarManager()
                                        .getStatusBarColorController()
                                        .getStatusBarColorWithoutStatusIndicator(),
                                Matchers.equalTo(
                                        TabUiThemeUtil.getTabStripBackgroundColor(
                                                activity, /* isIncognito= */ false))));
    }

    private void checkTabStripHeightOnUiThread(int tabStripHeight) {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(activity.getToolbarManager(), Matchers.notNullValue());
                    Criteria.checkThat(
                            "Tab strip height is different",
                            activity.getToolbarManager().getTabStripHeightSupplier().get(),
                            Matchers.equalTo(tabStripHeight));
                });
    }
}
