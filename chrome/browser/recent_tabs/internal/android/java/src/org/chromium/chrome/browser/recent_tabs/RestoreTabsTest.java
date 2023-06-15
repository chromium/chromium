// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static androidx.test.espresso.Espresso.pressBack;

import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionWindow;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/**
 * Integration tests for the RestoreTabs feature.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@EnableFeatures(ChromeFeatureList.RESTORE_TABS_ON_FRE)
@DoNotBatch(reason = "Tests startup behaviors that trigger per-session")
public class RestoreTabsTest {
    private static final String RESTORE_TABS_FEATURE = FeatureConstants.RESTORE_TABS_ON_FRE_FEATURE;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public JniMocker jniMocker = new JniMocker();

    @Spy
    ForeignSessionHelper.Natives mForeignSessionHelperJniSpy;
    // Tell R8 not to break the ability to mock the class.
    @Spy
    ForeignSessionHelperJni mUnused;

    @Mock
    private Tracker mMockTracker;

    private BottomSheetController mBottomSheetController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        RestoreTabsFeatureHelper.RESTORE_TABS_PROMO_SKIP_FEATURE_ENGAGEMENT.setForTesting(true);
        TrackerFactory.setTrackerForTests(mMockTracker);

        mForeignSessionHelperJniSpy = Mockito.spy(ForeignSessionHelperJni.get());
        jniMocker.mock(ForeignSessionHelperJni.TEST_HOOKS, mForeignSessionHelperJniSpy);
        doReturn(true).when(mForeignSessionHelperJniSpy).isTabSyncEnabled(anyLong());

        mBottomSheetController = mActivityTestRule.getActivity()
                                         .getRootUiCoordinatorForTesting()
                                         .getBottomSheetController();
    }

    @After
    public void tearDown() {
        RestoreTabsFeatureHelper.RESTORE_TABS_PROMO_SKIP_FEATURE_ENGAGEMENT.setForTesting(false);
        TrackerFactory.setTrackerForTests(null);
    }

    @Test
    @MediumTest
    public void testRestoreTabsPromo_triggerBottomSheetView() {
        // Test using triggerHelpUI methods instead of skip_feature_engagement param
        RestoreTabsFeatureHelper.RESTORE_TABS_PROMO_SKIP_FEATURE_ENGAGEMENT.setForTesting(false);

        // Setup mock data
        ForeignSessionTab tab = new ForeignSessionTab(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1), "title", 32L, 0);
        List<ForeignSessionTab> tabs = new ArrayList<>();
        tabs.add(tab);
        ForeignSessionWindow window = new ForeignSessionWindow(31L, 1, tabs);
        List<ForeignSessionWindow> windows = new ArrayList<>();
        windows.add(window);
        ForeignSession session =
                new ForeignSession("tag", "John's iPhone 6", 32L, windows, FormFactor.PHONE);
        List<ForeignSession> sessions = new ArrayList<>();
        sessions.add(session);

        doReturn(true).when(mMockTracker).wouldTriggerHelpUI(eq(RESTORE_TABS_FEATURE));
        doReturn(true).when(mMockTracker).shouldTriggerHelpUI(eq(RESTORE_TABS_FEATURE));
        doAnswer(invocation -> {
            List<ForeignSession> invoked_sessions = invocation.getArgument(1);
            invoked_sessions.addAll(sessions);
            return true;
        })
                .when(mForeignSessionHelperJniSpy)
                .getMobileAndTabletForeignSessions(anyLong(), anyList());

        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat("Bottom sheet never fully loaded",
                    mBottomSheetController.getCurrentSheetContent(),
                    Matchers.instanceOf(RestoreTabsPromoSheetContent.class));
        });
        Assert.assertTrue(mBottomSheetController.getCurrentSheetContent()
                                  instanceof RestoreTabsPromoSheetContent);

        pressBack();
        verify(mMockTracker, times(1)).dismissed(eq(RESTORE_TABS_FEATURE));
        RestoreTabsFeatureHelper.RESTORE_TABS_PROMO_SKIP_FEATURE_ENGAGEMENT.setForTesting(true);
    }

    @Test
    @MediumTest
    public void testRestoreTabsPromo_noSyncedDevicesNoTrigger() {
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        verify(mMockTracker, never()).shouldTriggerHelpUI(eq(RESTORE_TABS_FEATURE));
        Assert.assertFalse(mBottomSheetController.isSheetOpen());
        verify(mMockTracker, never()).dismissed(eq(RESTORE_TABS_FEATURE));
    }
}
