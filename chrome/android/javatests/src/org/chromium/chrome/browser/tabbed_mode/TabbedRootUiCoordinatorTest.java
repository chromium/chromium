// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import androidx.annotation.Nullable;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;

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
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarSceneLayer;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarSceneLayerJni;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManagerSupplier;
import org.chromium.chrome.browser.privacy_sandbox.ActivityTypeMapper;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridgeJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.testhtmls.NavigatePageStations;
import org.chromium.components.search_engines.SearchEngineChoiceService;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests for {@link TabbedRootUiCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabbedRootUiCoordinatorTest {
    @Rule public ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public MockitoRule mockito = MockitoJUnit.rule();

    private WebPageStation mPage;
    private TabbedRootUiCoordinator mTabbedRootUiCoordinator;

    @Mock private PrivacySandboxBridgeJni mPrivacySandboxBridgeJni;
    @Mock private BookmarkBarSceneLayer.Natives mBookmarkBarSceneLayerJni;
    @Mock private SearchEngineChoiceService mSearchEngineChoiceService;

    @Before
    public void setUp() {
        PrivacySandboxBridgeJni.setInstanceForTesting(mPrivacySandboxBridgeJni);
        BookmarkBarSceneLayerJni.setInstanceForTesting(mBookmarkBarSceneLayerJni);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SearchEngineChoiceService.setInstanceForTests(mSearchEngineChoiceService);
                    doReturn(false).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();
                });

        BookmarkBarUtils.setBookmarkBarVisibleForTesting(true);
        TabbedRootUiCoordinator.setDisableTopControlsAnimationsForTesting(true);
        mPage = mActivityTestRule.startOnBlankPage();
        mTabbedRootUiCoordinator =
                (TabbedRootUiCoordinator) mPage.getActivity().getRootUiCoordinatorForTesting();
    }

    // TODO(crbug.com/40112282): Enable for tablets once we support them.
    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_ACTIVITY_TYPE_STORAGE)
    @Restriction({DeviceFormFactor.PHONE})
    public void testRecordPrivacySandboxActivityTypeIncrementsRecordWhenFlagIsEnabled() {
        verify(mPrivacySandboxBridgeJni)
                .recordActivityType(
                        any(),
                        eq(
                                ActivityTypeMapper.toPrivacySandboxStorageActivityType(
                                        ActivityType.TABBED)));
    }

    // TODO(crbug.com/40112282): Enable for tablets once we support them.
    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_ACTIVITY_TYPE_STORAGE)
    @Restriction({DeviceFormFactor.PHONE})
    public void testRecordPrivacySandboxActivityTypeDoesNotIncrementRecordWhenFlagIsDisabled() {
        verify(mPrivacySandboxBridgeJni, never()).recordActivityType(any(), anyInt());
    }

    @Test
    @MediumTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
    @Restriction({DeviceFormFactor.PHONE})
    public void testTopControlsHeightWithBookmarkBarWhenFlagIsDisabledOnPhone() {
        testTopControlsHeightWithBookmarkBar(/* expectBookmarkBar= */ false);
    }

    @Test
    @MediumTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testTopControlsHeightWithBookmarkBarWhenFlagIsDisabledOnTablet() {
        testTopControlsHeightWithBookmarkBar(/* expectBookmarkBar= */ false);
    }

    @Test
    @MediumTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
    @Restriction({DeviceFormFactor.PHONE})
    @DisabledTest
    // TODO(crbug.com/447525636): Re-enable tests.
    public void testTopControlsHeightWithBookmarkBarWhenFlagIsEnabledOnPhone() {
        testTopControlsHeightWithBookmarkBar(/* expectBookmarkBar= */ false);
    }

    @Test
    @MediumTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testTopControlsHeightWithBookmarkBarWhenFlagIsEnabledOnTablet() {
        testTopControlsHeightWithBookmarkBar(/* expectBookmarkBar= */ true);
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
        final var toolbarManager = mTabbedRootUiCoordinator.getToolbarManager();
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
}
