// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.view.KeyEvent;
import android.view.View;
import android.widget.ListView;

import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;
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
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.sync.FakeSyncServiceImpl;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;

import java.io.IOException;
import java.util.concurrent.Callable;

/** Tests tabbed mode app menu popup. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabbedAppMenuTest {
    private static final int RENDER_TEST_REVISION = 2;

    private static final String RENDER_TEST_DESCRIPTION =
            "Badge on settings menu item icon on identity and sync errors.";

    private static final String TEST_URL = UrlUtils.encodeHtmlDataUri("<html>foo</html>");

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_TEST_REVISION)
                    .setDescription(RENDER_TEST_DESCRIPTION)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_APP_MENU)
                    .build();

    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeJniMock;

    private AppMenuHandler mAppMenuHandler;

    @Before
    public void setUp() {
        // Prevent "GmsCore outdated" error from being exposed in bots with old version.
        PasswordManagerUtilBridgeJni.setInstanceForTesting(mPasswordManagerUtilBridgeJniMock);
        when(mPasswordManagerUtilBridgeJniMock.isGmsCoreUpdateRequired(any(), any()))
                .thenReturn(false);

        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(true);

        // We need list selection; ensure we are not in touch mode.
        InstrumentationRegistry.getInstrumentation().setInTouchMode(false);

        CompositorAnimationHandler.setTestingMode(true);

        mActivityTestRule.startOnUrl(TEST_URL);

        AppMenuTestSupport.overrideOnOptionItemSelectedListener(
                mActivityTestRule.getAppMenuCoordinator(), unused -> {});
        mAppMenuHandler = mActivityTestRule.getAppMenuCoordinator().getAppMenuHandler();

        showAppMenuAndAssertMenuShown();

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    @After
    public void tearDown() {
        ActivityTestUtils.clearActivityOrientation(mActivityTestRule.getActivity());

        CompositorAnimationHandler.setTestingMode(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.setCategoryEnabled(
                            ProfileManager.getLastUsedRegularProfile(),
                            ContentSettingsType.REQUEST_DESKTOP_SITE,
                            false);
                });
    }

    /**
     * Test bounds when accessing the menu through the keyboard. Make sure that the menu stays open
     * when trying to move past the first and last items.
     */
    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardMenuBoundaries() {
        moveToBoundary(false, true);
        assertEquals(getCount() - 1, getCurrentFocusedRow());
        moveToBoundary(true, true);
        assertEquals(0, getCurrentFocusedRow());
        moveToBoundary(false, true);
        assertEquals(getCount() - 1, getCurrentFocusedRow());
    }

    /** Test that typing ENTER immediately opening the menu works. */
    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardMenuEnterOnOpen() {
        hitEnterAndAssertAppMenuDismissed();
    }

    /** Test that hitting ENTER past the top item doesn't crash Chrome. */
    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardEnterAfterMovePastTopItem() {
        moveToBoundary(true, true);
        assertEquals(0, getCurrentFocusedRow());
        hitEnterAndAssertAppMenuDismissed();
    }

    /**
     * Test that hitting ENTER past the bottom item doesn't crash Chrome. Catches regressions for
     * http://crbug.com/181067
     */
    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardEnterAfterMovePastBottomItem() {
        moveToBoundary(false, true);
        assertEquals(getCount() - 1, getCurrentFocusedRow());
        hitEnterAndAssertAppMenuDismissed();
    }

    /**
     * Test that hitting ENTER on the top item actually triggers the top item. Catches regressions
     * for https://crbug.com/191239 for shrunken menus in landscape.
     */
    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardMenuEnterOnTopItem() {
        showAppMenuAndAssertMenuShown();
        moveToBoundary(true, false);
        assertEquals(0, getCurrentFocusedRow());
        hitEnterAndAssertAppMenuDismissed();
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main", "Bookmark", "RenderTest"})
    @Restriction(DeviceFormFactor.PHONE)
    public void testBookmarkMenuItem() throws IOException {
        PropertyModel bookmarkStarPropertyModel =
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mActivityTestRule.getAppMenuCoordinator(), R.id.bookmark_this_page_id);
        Assert.assertFalse(
                "Bookmark item should not be checked.",
                bookmarkStarPropertyModel.get(AppMenuItemProperties.CHECKED));
        assertEquals(
                "Incorrect content description.",
                mActivityTestRule.getActivity().getString(R.string.menu_bookmark),
                bookmarkStarPropertyModel.get(AppMenuItemProperties.TITLE_CONDENSED));
        mRenderTestRule.render(getListView().getChildAt(0), "rounded_corner_icon_row");

        ThreadUtils.runOnUiThreadBlocking(() -> mAppMenuHandler.hideAppMenu());
        AppMenuPropertiesDelegateImpl.setPageBookmarkedForTesting(true);
        showAppMenuAndAssertMenuShown();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        bookmarkStarPropertyModel =
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mActivityTestRule.getAppMenuCoordinator(), R.id.bookmark_this_page_id);
        Assert.assertTrue(
                "Bookmark item should be checked.",
                bookmarkStarPropertyModel.get(AppMenuItemProperties.CHECKED));
        assertEquals(
                "Incorrect content description for bookmarked page.",
                mActivityTestRule.getActivity().getString(R.string.edit_bookmark),
                bookmarkStarPropertyModel.get(AppMenuItemProperties.TITLE_CONDENSED));
        mRenderTestRule.render(
                getListView().getChildAt(0), "rounded_corner_icon_row_page_bookmarked");
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main", "RenderTest"})
    @Restriction(DeviceFormFactor.PHONE)
    public void testRequestDesktopSiteMenuItem_checkbox() throws IOException {
        Tab tab = mActivityTestRule.getActivity().getTabModelSelector().getCurrentTab();
        boolean isRequestDesktopSite =
                tab.getWebContents().getNavigationController().getUseDesktopUserAgent();
        Assert.assertFalse("Default to request mobile site.", isRequestDesktopSite);

        int requestDesktopSiteIndex =
                AppMenuTestSupport.findIndexOfMenuItemById(
                        mActivityTestRule.getAppMenuCoordinator(), R.id.request_desktop_site_id);
        Assert.assertNotEquals("No request desktop site row found.", -1, requestDesktopSiteIndex);

        Callable<Boolean> isVisible =
                () -> {
                    int visibleStart = getListView().getFirstVisiblePosition();
                    int visibleEnd = visibleStart + getListView().getChildCount() - 1;
                    return requestDesktopSiteIndex >= visibleStart
                            && requestDesktopSiteIndex <= visibleEnd;
                };
        CriteriaHelper.pollUiThread(() -> getListView().getChildAt(0) != null);
        if (!ThreadUtils.runOnUiThreadBlocking(isVisible)) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> getListView().smoothScrollToPosition(requestDesktopSiteIndex));
            CriteriaHelper.pollUiThread(isVisible);
        }
        mRenderTestRule.render(
                getListView()
                        .getChildAt(
                                requestDesktopSiteIndex - getListView().getFirstVisiblePosition()),
                "request_desktop_site_uncheck");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.setCategoryEnabled(
                            ProfileManager.getLastUsedRegularProfile(),
                            ContentSettingsType.REQUEST_DESKTOP_SITE,
                            true);
                    tab.reload();
                });
        ChromeTabUtils.waitForTabPageLoaded(tab, TEST_URL);
        isRequestDesktopSite =
                tab.getWebContents().getNavigationController().getUseDesktopUserAgent();
        Assert.assertTrue("Should request desktop site.", isRequestDesktopSite);

        ThreadUtils.runOnUiThreadBlocking(() -> mAppMenuHandler.hideAppMenu());
        showAppMenuAndAssertMenuShown();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        CriteriaHelper.pollUiThread(() -> getListView().getChildAt(0) != null);
        if (!ThreadUtils.runOnUiThreadBlocking(isVisible)) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> getListView().smoothScrollToPosition(requestDesktopSiteIndex));
            CriteriaHelper.pollUiThread(isVisible);
        }
        mRenderTestRule.render(
                getListView()
                        .getChildAt(
                                requestDesktopSiteIndex - getListView().getFirstVisiblePosition()),
                "request_mobile_site_check");
    }

    @Test
    @LargeTest
    @Feature({"Browser", "Main", "RenderTest"})
    public void testSettingsMenuItem_NoBadgeShownForNotSignedInUsers() throws IOException {
        View view = getSettingsMenuItemView();
        Assert.assertNull(view.findViewById(R.id.menu_item_text).getContentDescription());
        mRenderTestRule.render(view, "settings_menu_item_not_signed_in_user");
    }

    @Test
    @LargeTest
    @Feature({"Browser", "Main", "RenderTest"})
    public void testSettingsMenuItem_BadgeShownForSignedInUsersOnIdentityError()
            throws IOException {
        ThreadUtils.runOnUiThreadBlocking(() -> mAppMenuHandler.hideAppMenu());

        FakeSyncServiceImpl fakeSyncService =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            FakeSyncServiceImpl fakeSyncServiceImpl = new FakeSyncServiceImpl();
                            SyncServiceFactory.setInstanceForTesting(fakeSyncServiceImpl);
                            return fakeSyncServiceImpl;
                        });
        // Fake an identity error.
        fakeSyncService.setRequiresClientUpgrade(true);
        // Sign in and wait for sync machinery to be active.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        showAppMenuAndAssertMenuShown();
        View view = getSettingsMenuItemView();
        assertEquals(
                "Content description should mention an error.",
                view.findViewById(R.id.menu_item_text).getContentDescription(),
                mActivityTestRule.getActivity().getString(R.string.menu_settings_account_error));
        mRenderTestRule.render(view, "settings_menu_item_signed_in_user_identity_error");
    }

    @Test
    @LargeTest
    @Feature({"Browser", "Main", "RenderTest"})
    public void testSettingsMenuItem_NoBadgeShownForSignedInUsersIfNoError() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(() -> mAppMenuHandler.hideAppMenu());
        // Sign in and wait for sync machinery to be active.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        showAppMenuAndAssertMenuShown();
        View view = getSettingsMenuItemView();
        Assert.assertNull(view.findViewById(R.id.menu_item_text).getContentDescription());
        mRenderTestRule.render(view, "settings_menu_item_signed_in_user_no_error");
    }

    private void showAppMenuAndAssertMenuShown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AppMenuTestSupport.showAppMenu(
                            mActivityTestRule.getAppMenuCoordinator(), null, false);
                    Assert.assertTrue(mAppMenuHandler.isAppMenuShowing());
                });

        // Make sure the menu is ready to be selected.
        CriteriaHelper.pollUiThread(
                () -> {
                    getListView().setSelection(0);
                    Criteria.checkThat(getCurrentFocusedRow(), Matchers.is(0));
                });
    }

    private void hitEnterAndAssertAppMenuDismissed() {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        pressKey(KeyEvent.KEYCODE_ENTER);
        CriteriaHelper.pollInstrumentationThread(
                () -> !mAppMenuHandler.isAppMenuShowing(), "AppMenu did not dismiss");
    }

    private void moveToBoundary(boolean towardsTop, boolean movePast) {
        // Move to the boundary.
        final int end = towardsTop ? 0 : getCount() - 1;
        int increment = towardsTop ? -1 : 1;
        for (int index = getCurrentFocusedRow(); index != end; index += increment) {
            pressKey(towardsTop ? KeyEvent.KEYCODE_DPAD_UP : KeyEvent.KEYCODE_DPAD_DOWN);
            final int expectedPosition = index + increment;
            CriteriaHelper.pollInstrumentationThread(
                    () -> {
                        Criteria.checkThat(getCurrentFocusedRow(), Matchers.is(expectedPosition));
                    });
        }

        // Try moving past it by one.
        if (movePast) {
            pressKey(towardsTop ? KeyEvent.KEYCODE_DPAD_UP : KeyEvent.KEYCODE_DPAD_DOWN);
            CriteriaHelper.pollInstrumentationThread(
                    () -> Criteria.checkThat(getCurrentFocusedRow(), Matchers.is(end)));
        }

        // The menu should stay open.
        Assert.assertTrue(mAppMenuHandler.isAppMenuShowing());
    }

    private void pressKey(final int keycode) {
        final View view = getListView();
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    view.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, keycode));
                    view.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, keycode));
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private int getCurrentFocusedRow() {
        ListView listView = getListView();
        if (listView == null) return ListView.INVALID_POSITION;
        return listView.getSelectedItemPosition();
    }

    private int getCount() {
        ListView listView = getListView();
        if (listView == null) return 0;
        return listView.getCount();
    }

    private ListView getListView() {
        return AppMenuTestSupport.getListView(mActivityTestRule.getAppMenuCoordinator());
    }

    private View getSettingsMenuItemView() {
        int position =
                AppMenuTestSupport.findIndexOfMenuItemById(
                        mActivityTestRule.getAppMenuCoordinator(), R.id.preferences_id);
        Assert.assertTrue("No settings menu item found.", position != -1);

        CriteriaHelper.pollUiThread(() -> getListView().getChildAt(0) != null);

        Callable<Boolean> isVisible =
                () -> {
                    int visibleStart = getListView().getFirstVisiblePosition();
                    int visibleEnd = visibleStart + getListView().getChildCount() - 1;
                    return position >= visibleStart && position <= visibleEnd;
                };

        if (!ThreadUtils.runOnUiThreadBlocking(isVisible)) {
            ThreadUtils.runOnUiThreadBlocking(() -> getListView().setSelection(position));
            CriteriaHelper.pollUiThread(isVisible);
        }

        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return getListView()
                                    .getChildAt(position - getListView().getFirstVisiblePosition());
                        });
        Assert.assertNotNull("No settings menu item view found.", view);
        return view;
    }
}
