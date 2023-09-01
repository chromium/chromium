// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tasks.tab_management.TabUiUnitTestUtils;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.Collection;
import java.util.Stack;

/**
 * Tests for {@link CloseButtonNavigator}.
 */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
public class CloseButtonNavigatorTest {
    @Parameters
    public static Collection<Object[]> data() {
        return Arrays.asList(new Object[][] {{true}, {false}});
    }
    @Rule(order = -2)
    public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Parameter(0)
    public boolean mIsWebapp;

    @Mock public CustomTabActivityTabController mTabController;
    @Mock public CustomTabActivityTabProvider mTabProvider;
    @Mock
    public WebappExtras mWebappExtras;
    @Mock
    public BrowserServicesIntentDataProvider mIntentDataProvider;

    private final Stack<Tab> mTabs = new Stack<>();
    private CloseButtonNavigator mCloseButtonNavigator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        if (!mIsWebapp) {
            mWebappExtras = null;
        }
        doReturn(mWebappExtras).when(mIntentDataProvider).getWebappExtras();
        doReturn(mIsWebapp ? ActivityType.WEBAPP : ActivityType.CUSTOM_TAB)
                .when(mIntentDataProvider)
                .getActivityType();

        mCloseButtonNavigator =
                new CloseButtonNavigator(mTabController, mTabProvider, mIntentDataProvider);

        // Set up our mTabs to act as the mock tab model:
        // - mTabController.closeTab removes the top tab.
        // - mTabProvider.getTab returns the top tab.
        Mockito.doAnswer((invocation) -> {
            mTabs.pop();
            return null;  // Annoyingly we have to return something.
        }).when(mTabController).closeTab();
        when(mTabProvider.getTab()).thenAnswer(invocation -> {
            if (mTabs.empty()) return null;
            return mTabs.peek();
        });
    }

    @After
    public void tearDown() {
        UmaRecorderHolder.resetForTesting();
    }

    private Tab createTabWithNavigationHistory(GURL... urls) {
        NavigationHistory history = new NavigationHistory();

        for (GURL url : urls) {
            history.addEntry(new NavigationEntry(0, url, GURL.emptyGURL(), GURL.emptyGURL(), "",
                    null, 0, 0, /* isInitialEntry=*/false));
        }

        // Point to the most recent entry in history.
        history.setCurrentEntryIndex(history.getEntryCount() - 1);

        Tab tab = mock(Tab.class);
        WebContents webContents = mock(WebContents.class);
        NavigationController navigationController = mock(NavigationController.class);

        when(tab.getUrl())
                .thenAnswer(invocation
                        -> history.getEntryAtIndex(history.getCurrentEntryIndex()).getUrl());
        when(tab.getWebContents()).thenReturn(webContents);
        when(webContents.getNavigationController()).thenReturn(navigationController);
        when(navigationController.getNavigationHistory()).thenReturn(history);
        setParentTabId(tab, Tab.INVALID_TAB_ID);

        return tab;
    }

    private void setParentTabId(Tab childTab, int parentTabId) {
        CriticalPersistedTabData criticalPersistedTabData =
                CriticalPersistedTabData.build(childTab);
        criticalPersistedTabData.setParentId(parentTabId);
        TabUiUnitTestUtils.prepareTab(
                childTab, CriticalPersistedTabData.class, criticalPersistedTabData);
    }

    private NavigationController currentTabsNavigationController() {
        // The navigation controller will be a mock object created in the above method.
        return mTabs.peek().getWebContents().getNavigationController();
    }

    /** Example criteria. */
    private static boolean isRed(String url) {
        return url.contains("red");
    }

    @Test
    public void noCriteria_singleTab() {
        mTabs.push(createTabWithNavigationHistory(JUnitTestGURLs.BLUE_1, JUnitTestGURLs.BLUE_2));

        mCloseButtonNavigator.navigateOnClose();

        assertTrue(mTabs.empty());
        assertOnAllTabsClosedRecorded(1);
    }

    @Test
    public void noCriteria_multipleTabs() {
        mTabs.push(createTabWithNavigationHistory(JUnitTestGURLs.BLUE_1));
        mTabs.push(createTabWithNavigationHistory(JUnitTestGURLs.BLUE_2));
        setParentTabId(mTabs.get(1), mTabs.get(0).getId());

        mCloseButtonNavigator.navigateOnClose();

        if (mIsWebapp) {
            assertEquals(1, mTabs.size());
            verify(currentTabsNavigationController(), never()).goToNavigationIndex(anyInt());
        } else {
            assertTrue(mTabs.empty());
            assertOnAllTabsClosedRecorded(2);
        }
    }

    @Test
    public void noMatchingUrl_singleTab() {
        mCloseButtonNavigator.setLandingPageCriteria(CloseButtonNavigatorTest::isRed);
        mTabs.push(createTabWithNavigationHistory(JUnitTestGURLs.BLUE_1, JUnitTestGURLs.BLUE_2));

        mCloseButtonNavigator.navigateOnClose();

        assertTrue(mTabs.empty());
        assertOnAllTabsClosedRecorded(1);
    }

    @Test
    public void noMatchingUrl_multipleTabs() {
        mCloseButtonNavigator.setLandingPageCriteria(CloseButtonNavigatorTest::isRed);
        mTabs.push(createTabWithNavigationHistory(JUnitTestGURLs.BLUE_1));
        mTabs.push(createTabWithNavigationHistory(JUnitTestGURLs.BLUE_2));
        setParentTabId(mTabs.get(1), mTabs.get(0).getId());

        mCloseButtonNavigator.navigateOnClose();

        if (mIsWebapp) {
            assertEquals(1, mTabs.size());
            verify(currentTabsNavigationController(), never()).goToNavigationIndex(anyInt());
        } else {
            assertTrue(mTabs.empty());
            assertOnAllTabsClosedRecorded(2);
        }
    }

    @Test
    public void matchingUrl_singleTab() {
        mCloseButtonNavigator.setLandingPageCriteria(CloseButtonNavigatorTest::isRed);
        mTabs.push(createTabWithNavigationHistory(JUnitTestGURLs.RED_1, JUnitTestGURLs.RED_2,
                JUnitTestGURLs.BLUE_1, JUnitTestGURLs.BLUE_2));

        mCloseButtonNavigator.navigateOnClose();

        assertFalse(mTabs.isEmpty());
        assertOnAllTabsClosedRecorded(0);
        verify(currentTabsNavigationController()).goToNavigationIndex(eq(1));
        // Ensure it was only called with that value.
        verify(currentTabsNavigationController()).goToNavigationIndex(anyInt());
    }

    @Test
    public void matchingUrl_startOfNextTab() {
        mCloseButtonNavigator.setLandingPageCriteria(CloseButtonNavigatorTest::isRed);
        mTabs.push(createTabWithNavigationHistory(JUnitTestGURLs.RED_1, JUnitTestGURLs.RED_2));
        mTabs.push(createTabWithNavigationHistory(JUnitTestGURLs.BLUE_1, JUnitTestGURLs.BLUE_2));
        setParentTabId(mTabs.get(1), mTabs.get(0).getId());

        mCloseButtonNavigator.navigateOnClose();

        assertEquals(1, mTabs.size());
        assertOnAllTabsClosedRecorded(0);
        verify(currentTabsNavigationController(), never()).goToNavigationIndex(anyInt());
    }

    @Test
    public void matchingUrl_middleOfNextTab() {
        mCloseButtonNavigator.setLandingPageCriteria(CloseButtonNavigatorTest::isRed);
        mTabs.push(createTabWithNavigationHistory(JUnitTestGURLs.RED_1, JUnitTestGURLs.BLUE_1));
        mTabs.push(createTabWithNavigationHistory(JUnitTestGURLs.BLUE_2, JUnitTestGURLs.BLUE_3));
        setParentTabId(mTabs.get(1), mTabs.get(0).getId());

        mCloseButtonNavigator.navigateOnClose();

        assertEquals(1, mTabs.size());
        assertOnAllTabsClosedRecorded(0);
        if (mIsWebapp) {
            verify(currentTabsNavigationController(), never()).goToNavigationIndex(anyInt());
        } else {
            verify(currentTabsNavigationController()).goToNavigationIndex(eq(0));
            verify(currentTabsNavigationController()).goToNavigationIndex(anyInt());
        }
    }

    @Test
    public void middleOfHistory() {
        mCloseButtonNavigator.setLandingPageCriteria(CloseButtonNavigatorTest::isRed);
        mTabs.push(createTabWithNavigationHistory(JUnitTestGURLs.RED_1, JUnitTestGURLs.RED_2,
                JUnitTestGURLs.BLUE_1, JUnitTestGURLs.BLUE_2, JUnitTestGURLs.RED_3));

        mTabs.peek().getWebContents().getNavigationController().getNavigationHistory()
                .setCurrentEntryIndex(3);

        mCloseButtonNavigator.navigateOnClose();

        assertEquals(1, mTabs.size());
        assertOnAllTabsClosedRecorded(0);
        verify(currentTabsNavigationController()).goToNavigationIndex(eq(1));
        verify(currentTabsNavigationController()).goToNavigationIndex(anyInt());
    }

    @Test
    public void navigateFromLandingPage() {
        mCloseButtonNavigator.setLandingPageCriteria(CloseButtonNavigatorTest::isRed);
        mTabs.push(createTabWithNavigationHistory(JUnitTestGURLs.RED_1, JUnitTestGURLs.RED_2,
                JUnitTestGURLs.BLUE_1, JUnitTestGURLs.BLUE_2, JUnitTestGURLs.RED_3));

        mCloseButtonNavigator.navigateOnClose();

        assertEquals(1, mTabs.size());
        assertOnAllTabsClosedRecorded(0);
        verify(currentTabsNavigationController()).goToNavigationIndex(eq(1));
        verify(currentTabsNavigationController()).goToNavigationIndex(anyInt());
    }

    private void assertOnAllTabsClosedRecorded(int count) {
        String histogram = "CustomTabs.TabCounts.OnClosingAllTabs";
        if (count > 0) {
            assertEquals(String.format("<%s> not recorded with sample <%d>.", histogram, count), 1,
                    RecordHistogram.getHistogramValueCountForTesting(histogram, count));
        } else {
            assertEquals(String.format("<%s> should not be recorded.", histogram), 0,
                    RecordHistogram.getHistogramTotalCountForTesting(histogram));
        }
    }
}
