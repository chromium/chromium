// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestName;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.ViewFinder;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarSceneLayer;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarSceneLayerJni;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManagerSupplier;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicNavigationUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarPrefs;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.testhtmls.NavigatePageStations;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.policy.test.annotations.Policies.Add;
import org.chromium.components.search_engines.SearchEngineChoiceService;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.ViewUtils;

/** Tests for {@link TabbedRootUiCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabbedRootUiCoordinatorTest {
    @Rule public ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public TestName mTestName = new TestName();

    @Rule public MockitoRule mockito = MockitoJUnit.rule();

    private WebPageStation mPage;
    private TabbedRootUiCoordinator mTabbedRootUiCoordinator;

    @Mock private BookmarkBarSceneLayer.Natives mBookmarkBarSceneLayerJni;
    @Mock private SearchEngineChoiceService mSearchEngineChoiceService;
    @Mock private Tracker mTracker;

    @Before
    public void setUp() {
        BookmarkBarSceneLayerJni.setInstanceForTesting(mBookmarkBarSceneLayerJni);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SearchEngineChoiceService.setInstanceForTests(mSearchEngineChoiceService);
                    doReturn(false).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();
                });

        BookmarkBarUtils.setBookmarkBarVisibleForTesting(true);
        TabbedRootUiCoordinator.setDisableTopControlsAnimationsForTesting(true);
        GlicEnabling.setEnabledForTesting(false);
    }

    @After
    public void tearDown() {
        mBrowserTestRule.signOut();
        TrackerFactory.setTrackerForTests(null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeSharedPreferences.getInstance()
                            .removeKey(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED);
                    ChromeSharedPreferences.getInstance()
                            .removeKey(
                                    ChromePreferenceKeys.ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS);
                    if (mTabbedRootUiCoordinator != null
                            && mTabbedRootUiCoordinator.getGlicPromoCoordinatorForTesting()
                                    != null) {
                        mTabbedRootUiCoordinator.getGlicPromoCoordinatorForTesting().destroy();
                    }
                });
        GlicNavigationUtils.setLauncher(null);
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    @DisabledTest
    // TODO(crbug.com/447525636): Re-enable tests.
    public void testTopControlsHeightWithBookmarkBarOnPhone() {
        mPage = mActivityTestRule.startOnBlankPage();
        mTabbedRootUiCoordinator =
                (TabbedRootUiCoordinator) mPage.getActivity().getRootUiCoordinatorForTesting();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    testTopControlsHeightWithBookmarkBar(/* expectBookmarkBar= */ false);
                });
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testTopControlsHeightWithBookmarkBarOnTablet() {
        mPage = mActivityTestRule.startOnBlankPage();
        mTabbedRootUiCoordinator =
                (TabbedRootUiCoordinator) mPage.getActivity().getRootUiCoordinatorForTesting();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final ChromeTabbedActivity activity = mActivityTestRule.getActivity();

                    // Enable the bookmark bar setting for the test.
                    BookmarkBarUtils.setDevicePrefShowBookmarksBar(
                            activity.getProfileProviderSupplier().get().getOriginalProfile(),
                            true,
                            /* fromKeyboardShortcut= */ false);
                    testTopControlsHeightWithBookmarkBar(/* expectBookmarkBar= */ true);
                });
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.SUPPORT_FORCED_SIGNIN_POLICY)
    @Add({@Policies.Item(key = "BrowserSignin", string = "2")})
    public void testForcedSignin() {
        mBrowserTestRule.addAccountThenSigninAndEnableHistorySync(TestAccounts.ACCOUNT1);

        // The user is already signed in at first, so the fullscreen signin prompt is not displayed.
        mPage = mActivityTestRule.startOnBlankPage();
        mTabbedRootUiCoordinator =
                (TabbedRootUiCoordinator) mPage.getActivity().getRootUiCoordinatorForTesting();
        ViewFinder.waitForNoView(withText(R.string.signin_fre_title_signin_forced_by_policy));

        // The fullscreen prompt should be displayed upon signout.
        mBrowserTestRule.signOut();
        ViewUtils.waitForVisibleView(withText(R.string.signin_fre_title_signin_forced_by_policy));
    }

    private void testTopControlsHeightWithBookmarkBar(boolean expectBookmarkBar) {
        // Verify bookmark bar (in-)existence.
        final var activity = mActivityTestRule.getActivity();
        final @Nullable var bookmarkBar = activity.findViewById(R.id.bookmark_bar);
        assertThat(bookmarkBar != null).isEqualTo(expectBookmarkBar);

        // Verify browser controls manager existence.
        final var browserControlsManager =
                BrowserControlsManagerSupplier.getValueOrNullFrom(activity.getWindowAndroid());
        assertNotNull(browserControlsManager);

        // Verify toolbar existence.
        final var toolbarManager = mTabbedRootUiCoordinator.getToolbarManagerSupplier().get();
        assertNotNull(toolbarManager);
        final var toolbar = toolbarManager.getToolbar();
        assertNotNull(toolbar);

        // Verify top controls height.
        final int tabStripHeight = toolbar.getTabStripHeight();
        final int toolbarHeight = toolbar.getHeight();
        final int bookmarkBarHeight = bookmarkBar != null ? bookmarkBar.getHeight() : 0;
        assertEquals(
                "Verify top controls height.",
                tabStripHeight + toolbarHeight + bookmarkBarHeight,
                browserControlsManager.getTopControlsHeight());
    }

    @Test
    @MediumTest
    public void testActivityTitle() {
        mPage = mActivityTestRule.startOnBlankPage();
        mTabbedRootUiCoordinator =
                (TabbedRootUiCoordinator) mPage.getActivity().getRootUiCoordinatorForTesting();

        final ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        EmbeddedTestServer testServer = mActivityTestRule.getTestServer();

        Tab tab1 =
                mActivityTestRule.loadUrlInNewTab(testServer.getURL(NavigatePageStations.PATH_ONE));
        CriteriaHelper.pollUiThread(() -> tab1.getTitle().equals("One"));
        assertTrue(
                "Activity title should contain tab title.",
                activity.getTitle().toString().contains("One"));

        Tab tab2 =
                mActivityTestRule.loadUrlInNewTab(testServer.getURL(NavigatePageStations.PATH_TWO));
        CriteriaHelper.pollUiThread(() -> tab2.getTitle().equals("Two"));
        assertTrue(
                "Activity title should contain tab title.",
                activity.getTitle().toString().contains("Two"));

        mActivityTestRule.loadUrl(testServer.getURL(NavigatePageStations.PATH_THREE));
        CriteriaHelper.pollUiThread(() -> tab2.getTitle().equals("Three"));
        assertTrue(
                "Activity title should contain tab title.",
                activity.getTitle().toString().contains("Three"));

        String tabSwitcherLabel =
                activity.getResources().getString(R.string.accessibility_tab_switcher_title);
        TabUiTestHelper.enterTabSwitcher(activity);
        assertTrue(
                "Activity title should contain GTS label.",
                activity.getTitle().toString().contains(tabSwitcherLabel));
    }

    @Test
    @MediumTest
    public void testMaybeShowGlicPromo_WouldTrigger_ToolbarNotPinned() {
        mPage = mActivityTestRule.startOnBlankPage();
        mTabbedRootUiCoordinator =
                (TabbedRootUiCoordinator) mPage.getActivity().getRootUiCoordinatorForTesting();

        GlicEnabling.setEnabledForTesting(true);
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED);

        // Mock tracker wouldTriggerHelpUi to return true.
        doReturn(true)
                .when(mTracker)
                .wouldTriggerHelpUi(
                        FeatureConstants.ADAPTIVE_BUTTON_PIN_GLIC_TOOLBAR_BUTTON_FEATURE);
        TrackerFactory.setTrackerForTests(mTracker);

        // Ensure toolbar is not pinned.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AdaptiveToolbarPrefs.saveToolbarButtonManualOverride(
                            AdaptiveToolbarButtonVariant.AUTO);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabbedRootUiCoordinator.maybeShowGlicPromo();
                });

        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED, false));
        // Toolbar should be manually overridden to GLIC.
        assertEquals(
                AdaptiveToolbarButtonVariant.GLIC, AdaptiveToolbarPrefs.getCustomizationSetting());
        // Promo coordinator should be null (not shown).
        assertNull(mTabbedRootUiCoordinator.getGlicPromoCoordinatorForTesting());

        // Verify that the trigger event was notified to the tracker.
        verify(mTracker).notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_GLIC_IPH_TRIGGER);
    }

    @Test
    @MediumTest
    public void testMaybeShowGlicPromo_WouldTrigger_ToolbarPinned() {
        mPage = mActivityTestRule.startOnBlankPage();
        mTabbedRootUiCoordinator =
                (TabbedRootUiCoordinator) mPage.getActivity().getRootUiCoordinatorForTesting();

        GlicEnabling.setEnabledForTesting(true);
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED);

        // Mock tracker wouldTriggerHelpUi to return true.
        doReturn(true)
                .when(mTracker)
                .wouldTriggerHelpUi(
                        FeatureConstants.ADAPTIVE_BUTTON_PIN_GLIC_TOOLBAR_BUTTON_FEATURE);
        TrackerFactory.setTrackerForTests(mTracker);

        // Ensure toolbar is pinned.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AdaptiveToolbarPrefs.saveToolbarButtonManualOverride(
                            AdaptiveToolbarButtonVariant.NEW_TAB);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabbedRootUiCoordinator.maybeShowGlicPromo();
                });

        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED, false));
        // Toolbar should still be NEW_TAB.
        assertEquals(
                AdaptiveToolbarButtonVariant.NEW_TAB,
                AdaptiveToolbarPrefs.getCustomizationSetting());
        // Promo coordinator should be non-null (shown).
        assertNotNull(mTabbedRootUiCoordinator.getGlicPromoCoordinatorForTesting());

        // Verify that the trigger event was notified to the tracker.
        verify(mTracker).notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_GLIC_IPH_TRIGGER);
    }

    @Test
    @MediumTest
    public void testMaybeShowGlicPromo_WouldNotTrigger() {
        mPage = mActivityTestRule.startOnBlankPage();
        mTabbedRootUiCoordinator =
                (TabbedRootUiCoordinator) mPage.getActivity().getRootUiCoordinatorForTesting();

        GlicEnabling.setEnabledForTesting(true);
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED);

        // Mock tracker wouldTriggerHelpUi to return false.
        doReturn(false)
                .when(mTracker)
                .wouldTriggerHelpUi(
                        FeatureConstants.ADAPTIVE_BUTTON_PIN_GLIC_TOOLBAR_BUTTON_FEATURE);
        TrackerFactory.setTrackerForTests(mTracker);

        // Toolbar is AUTO (not pinned).
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AdaptiveToolbarPrefs.saveToolbarButtonManualOverride(
                            AdaptiveToolbarButtonVariant.AUTO);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabbedRootUiCoordinator.maybeShowGlicPromo();
                });

        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED, false));
        // Toolbar should still be AUTO.
        assertEquals(
                AdaptiveToolbarButtonVariant.AUTO, AdaptiveToolbarPrefs.getCustomizationSetting());
        // Promo coordinator should be non-null (shown).
        assertNotNull(mTabbedRootUiCoordinator.getGlicPromoCoordinatorForTesting());

        // Verify that the trigger event was notified to the tracker.
        verify(mTracker).notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_GLIC_IPH_TRIGGER);
    }
}
