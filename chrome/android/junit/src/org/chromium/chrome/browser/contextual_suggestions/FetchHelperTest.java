// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.contextual_suggestions.FetchHelper.Delegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.test.ShadowUrlUtilities;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;

/** Unit tests for FetchHelper. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowUrlUtilities.class})
public final class FetchHelperTest {
    private static final String STARTING_URL = "https://starting.url";
    private static final String DIFFERENT_URL = "https://different.url";

    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private TabModel mTabModel;
    @Mock
    private Tab mTab;
    @Mock
    private WebContents mWebContents;
    @Mock
    private RenderFrameHost mFrameHost;
    @Mock
    private Tab mTab2;
    @Mock
    private WebContents mWebContents2;
    @Mock
    private Delegate mDelegate;
    @Captor
    private ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor
    private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    private TabObserver getTabObserver() {
        return mTabObserverCaptor.getValue();
    }

    private TabModelObserver getTabModelObserver() {
        return mTabModelObserverCaptor.getValue();
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // Default tab setup.
        doReturn(false).when(mTab).isIncognito();
        doReturn(false).when(mTab).isLoading();
        doReturn(77).when(mTab).getId();
        doReturn(STARTING_URL).when(mTab).getUrl();
        doReturn(mWebContents).when(mTab).getWebContents();
        // Default setup for tab 2.
        doReturn(false).when(mTab2).isIncognito();
        doReturn(false).when(mTab2).isLoading();
        doReturn(88).when(mTab2).getId();
        doReturn(DIFFERENT_URL).when(mTab2).getUrl();
        doReturn(mWebContents2).when(mTab2).getWebContents();

        // Default tab model selector setup.
        List<TabModel> tabModels = new ArrayList<>();
        tabModels.add(mTabModel);
        doReturn(tabModels).when(mTabModelSelector).getModels();
        doReturn(mTab).when(mTabModelSelector).getCurrentTab();

        // Ensure Chrome feature list does not switch to native.
        Map<String, Boolean> testFeatures = new HashMap<>();
        testFeatures.put(ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_BUTTON, true);
        ChromeFeatureList.setTestFeatures(testFeatures);
    }

    @Test
    public void createFetchHelper_currentTab_isIncognito() {
        doReturn(true).when(mTab).isIncognito();
        FetchHelper helper = createFetchHelper();
        assertFalse(helper.isObservingTab(mTab));
        verify(mTab, times(0)).addObserver(any(TabObserver.class));
    }

    @Test
    public void createFetchHelper_currentTab_null() {
        doReturn(null).when(mTabModelSelector).getCurrentTab();
        FetchHelper helper = createFetchHelper();
        assertFalse(helper.isObservingTab(null));
    }

    @Test
    public void createFetchHelper_observeCurrentTab() {
        FetchHelper helper = createFetchHelper();
        assertTrue(helper.isObservingTab(mTab));
        assertNotNull(getTabObserver());
    }

    @Test
    public void tabObserver_pageLoadStarted() {
        FetchHelper helper = createFetchHelper();
        getTabObserver().onPageLoadStarted(mTab, STARTING_URL);
        verify(mDelegate, times(1)).clearState();
        // Normally we would change the suffix here, but ShadowUrlUtilities don't support that now.
        getTabObserver().onPageLoadStarted(mTab, DIFFERENT_URL);
        verify(mDelegate, times(2)).clearState();
    }

    @Test
    public void tabObserver_onUrlUpdated() {
        FetchHelper helper = createFetchHelper();
        doReturn(DIFFERENT_URL).when(mTab).getUrl();
        getTabObserver().onUrlUpdated(mTab);
        verify(mDelegate, times(1)).clearState();
        // Normally we would change the suffix here, but ShadowUrlUtilities don't support that now.
        doReturn(STARTING_URL).when(mTab).getUrl();
        getTabObserver().onUrlUpdated(mTab);
        verify(mDelegate, times(2)).clearState();

        // Should be ignored, because URL is the same.
        getTabObserver().onUrlUpdated(mTab);
        verify(mDelegate, times(2)).clearState();

        // Should be ignored, because tab is different.
        doReturn(STARTING_URL).when(mTab2).getUrl();
        getTabObserver().onUrlUpdated(mTab2);
        verify(mDelegate, times(2)).clearState();
    }

    @Test
    public void tabObserver_didFirstVisuallyNonEmptyPaint() {
        delayFetchExecutionTest((tabObserver) -> tabObserver.didFirstVisuallyNonEmptyPaint(mTab));
    }

    @Test
    public void tabObserver_didFirstVisuallyNonEmptyPaint_pageLoadStarted_toSame() {
        delayFetchExecutionTest_pageLoadStarted_toSame(
                (tabObserver) -> tabObserver.didFirstVisuallyNonEmptyPaint(mTab));
    }

    @Test
    public void tabObserver_didFirstVisuallyNonEmptyPaint_pageLoadStarted_toDifferent() {
        delayFetchExecutionTest_pageLoadStarted_toDifferent(
                (tabObserver) -> tabObserver.didFirstVisuallyNonEmptyPaint(mTab));
    }

    @Test
    public void tabObserver_onPageLoadFinished() {
        delayFetchExecutionTest((tabObserver) -> tabObserver.onPageLoadFinished(mTab));
    }

    @Test
    public void tabObserver_onPageLoadFinished_pageLoadStarted_toSame() {
        delayFetchExecutionTest_pageLoadStarted_toSame(
                (tabObserver) -> tabObserver.onPageLoadFinished(mTab));
    }

    @Test
    public void tabObserver_onPageLoadFinished_pageLoadStarted_toDifferent() {
        delayFetchExecutionTest_pageLoadStarted_toDifferent(
                (tabObserver) -> tabObserver.onPageLoadFinished(mTab));
    }

    @Test
    public void tabObserver_onLoadStopped() {
        delayFetchExecutionTest((tabObserver) -> tabObserver.onLoadStopped(mTab, false));
    }

    @Test
    public void tabObserver_onLoadStopped_pageLoadStarted_toSame() {
        delayFetchExecutionTest_pageLoadStarted_toSame(
                (tabObserver) -> tabObserver.onLoadStopped(mTab, false));
    }

    @Test
    public void tabObserver_onLoadStopped_pageLoadStarted_toDifferent() {
        delayFetchExecutionTest_pageLoadStarted_toDifferent(
                (tabObserver) -> tabObserver.onLoadStopped(mTab, false));
    }

    @Test
    public void tabObserver_multipleSignals_triggersOnce() {
        delayFetchExecutionTest((tabObserver) -> {
            tabObserver.didFirstVisuallyNonEmptyPaint(mTab);
            tabObserver.onPageLoadFinished(mTab);
            tabObserver.onLoadStopped(mTab, false);
        });
    }

    @Test
    public void tabModelObserver_addTab_closeTab_isLoading() {
        doReturn(true).when(mTab).isLoading();
        addAndcloseNonSelectedTab(() -> closeTab(mTab));
    }

    @Test
    public void tabModelObserver_addTab_removeTab_isLoading() {
        doReturn(true).when(mTab).isLoading();
        addAndcloseNonSelectedTab(() -> removeTab(mTab));
    }

    @Test
    public void tabModelObserver_addTab_closeTab_doneLoading() {
        doReturn(false).when(mTab).isLoading();
        addAndcloseNonSelectedTab(() -> closeTab(mTab));
    }

    @Test
    public void tabModelObserver_addTab_removeTab_doneLoading() {
        doReturn(false).when(mTab).isLoading();
        addAndcloseNonSelectedTab(() -> removeTab(mTab));
    }

    @Test
    public void tabModelObserver_selectTab_fetch_selectTab_same() {
        doReturn(false).when(mTab).isLoading();
        FetchHelper helper = createFetchHelper();
        verify(mDelegate, times(1)).reportFetchDelayed(eq(mWebContents));
        runUntilFetchPossible();
        verify(mDelegate, times(1)).requestSuggestions(eq(STARTING_URL));
        selectTab(mTab);
        verify(mDelegate, times(1)).requestSuggestions(eq(STARTING_URL));
    }

    @Test
    public void secondTab_selected_withoutBeingAdded() {
        FetchHelper helper = createFetchHelper();
        selectTab(mTab2);
        verify(mTab2, times(1)).addObserver(eq(getTabObserver()));
        assertTrue(helper.isObservingTab(mTab2));
    }

    @Test
    public void secondTab_add_select_close_whenDoneLoading() {
        FetchHelper helper = createFetchHelper();
        addTab(mTab2);
        verify(mTab2, times(1)).addObserver(eq(getTabObserver()));
        assertTrue(helper.isObservingTab(mTab2));
        verify(mDelegate, times(0)).clearState();

        selectTab(mTab2);
        assertTrue(helper.isObservingTab(mTab2));
        verify(mTab2, times(1)).addObserver(eq(getTabObserver()));
        verify(mDelegate, times(1)).clearState();
        verify(mDelegate, times(1)).reportFetchDelayed(eq(mWebContents2));

        closeTab(mTab2);
        assertFalse(helper.isObservingTab(mTab2));
        verify(mDelegate, times(2)).clearState();
        verify(mDelegate, times(1)).reportFetchDelayed(eq(mWebContents2));
        verify(mDelegate, times(0)).requestSuggestions(any(String.class));
    }

    @Test
    public void secondTab_select_close_whenLoading() {
        doReturn(true).when(mTab2).isLoading();

        FetchHelper helper = createFetchHelper();
        selectTab(mTab2);
        assertTrue(helper.isObservingTab(mTab2));
        verify(mTab2, times(1)).addObserver(eq(getTabObserver()));
        verify(mDelegate, times(1)).clearState();
        verify(mDelegate, times(0)).reportFetchDelayed(eq(mWebContents2));

        closeTab(mTab2);
        assertFalse(helper.isObservingTab(mTab2));
        verify(mDelegate, times(2)).clearState();
        verify(mDelegate, times(0)).reportFetchDelayed(eq(mWebContents2));
        verify(mDelegate, times(0)).requestSuggestions(any(String.class));
    }

    @Test
    public void secondTab_add_select_fetch_close_whenDoneLoading() {
        FetchHelper helper = createFetchHelper();
        addTab(mTab2);
        selectTab(mTab2);
        runUntilFetchPossible();
        verify(mDelegate, times(1)).requestSuggestions(eq(DIFFERENT_URL));

        closeTab(mTab2);
        verify(mDelegate, times(2)).clearState();
        verify(mDelegate, times(1)).reportFetchDelayed(eq(mWebContents2));
        verify(mDelegate, times(1)).requestSuggestions(any(String.class));
    }

    @Test
    public void secondTab_add_select_fetch_close_whenLoading() {
        doReturn(true).when(mTab2).isLoading();
        FetchHelper helper = createFetchHelper();
        addTab(mTab2);
        selectTab(mTab2);
        runUntilFetchPossible();

        closeTab(mTab2);
        verify(mDelegate, times(2)).clearState();
        verify(mDelegate, times(0)).reportFetchDelayed(eq(mWebContents2));
        verify(mDelegate, times(0)).requestSuggestions(eq(DIFFERENT_URL));
    }

    @Test
    public void switchTabs_afterFetchOfVisibleTriggered() {
        doReturn(true).when(mTab).isLoading();
        FetchHelper helper = createFetchHelper();
        addTab(mTab2);
        selectTab(mTab2);
        runUntilFetchPossible();
        verify(mDelegate, times(1)).clearState();
        verify(mDelegate, times(1)).reportFetchDelayed(eq(mWebContents2));
        verify(mDelegate, times(1)).requestSuggestions(eq(DIFFERENT_URL));

        selectTab(mTab);
        verify(mDelegate, times(2)).clearState();
        verify(mDelegate, times(0)).reportFetchDelayed(eq(mWebContents));
        verify(mDelegate, times(0)).requestSuggestions(eq(STARTING_URL));

        closeTab(mTab2);
        verify(mDelegate, times(2)).clearState();
    }

    @Test
    public void switchTabs_afterFetchOfNotVisibleWouldHaveTriggered() {
        doReturn(true).when(mTab).isLoading();
        doReturn(true).when(mTab2).isLoading();
        FetchHelper helper = createFetchHelper();
        addTab(mTab2);

        // First tab finishes load in the background.
        getTabObserver().onPageLoadFinished(mTab);
        verify(mDelegate, times(1)).reportFetchDelayed(eq(mWebContents));

        // Switching tabs, therefore the pending fetch is on a background tab.
        selectTab(mTab2);
        verify(mDelegate, times(1)).clearState();

        // This should cancel/not-trigger the fetch in background.
        runUntilFetchPossible();
        verify(mDelegate, times(0)).requestSuggestions(any(String.class));

        // This time fetch is possible immediately.
        selectTab(mTab);
        verify(mDelegate, times(2)).clearState();
        verify(mDelegate, times(1)).reportFetchDelayed(eq(mWebContents));
        verify(mDelegate, times(1)).requestSuggestions(eq(STARTING_URL));
    }

    @Test
    public void canonicalUrl_isResolved() {
        doReturn(false).when(mTab).isLoading();
        doReturn(mFrameHost).when(mWebContents).getMainFrame();
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                if (invocation.getArguments()[0] instanceof Callback) {
                    @SuppressWarnings("unchecked")
                    Callback<String> callback = (Callback<String>) invocation.getArguments()[0];
                    callback.onResult(DIFFERENT_URL);
                }
                return null;
            }
        })
                .when(mFrameHost)
                .getCanonicalUrlForSharing(any());
        FetchHelper helper = createFetchHelper();

        getTabObserver().onPageLoadFinished(mTab);
        verify(mFrameHost, times(0)).getCanonicalUrlForSharing(any());
        runUntilFetchPossible();
        verify(mFrameHost, times(1)).getCanonicalUrlForSharing(any());
        verify(mDelegate, times(1)).requestSuggestions(DIFFERENT_URL);
    }

    @Test
    public void canonicalUrl_readinessStateIsNull() {
        doReturn(true).when(mTab).isLoading();

        FetchHelper helper = createFetchHelper();

        doReturn(mFrameHost).when(mWebContents).getMainFrame();
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                if (invocation.getArguments()[0] instanceof Callback) {
                    closeTab(mTab);
                    @SuppressWarnings("unchecked")
                    Callback<String> callback = (Callback<String>) invocation.getArguments()[0];
                    callback.onResult(DIFFERENT_URL);
                }
                return null;
            }
        })
                .when(mFrameHost)
                .getCanonicalUrlForSharing(any());
        getTabObserver().onPageLoadFinished(mTab);
        runUntilFetchPossible();
        verify(mDelegate, times(0)).requestSuggestions(DIFFERENT_URL);
    }

    @Test
    public void getMinimumFetchDelayMillis_Default() {
        assertEquals(FetchHelper.getMinimumFetchDelayMillis(),
                TimeUnit.SECONDS.toMillis(FetchHelper.MINIMUM_FETCH_DELAY_SECONDS));
    }

    private void addTab(Tab tab) {
        getTabModelObserver().didAddTab(tab, TabLaunchType.FROM_LINK);
    }

    private void selectTab(Tab tab) {
        getTabModelObserver().didSelectTab(tab, TabSelectionType.FROM_USER, 0);
    }

    private void closeTab(Tab tab) {
        getTabModelObserver().willCloseTab(tab, true);
    }

    private void removeTab(Tab tab) {
        getTabModelObserver().tabRemoved(tab);
    }

    private void runUntilFetchPossible() {
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }

    private FetchHelper createFetchHelper() {
        FetchHelper helper = spy(new FetchHelper(mDelegate, mTabModelSelector));
        when(helper.requireCurrentPageFromSRP()).thenReturn(false);
        when(helper.requireNavChainFromSRP()).thenReturn(false);

        if (mTabModelSelector.getCurrentTab() != null && !mTab.isIncognito()) {
            verify(mTab, times(1)).addObserver(mTabObserverCaptor.capture());
        }
        verify(mTabModel, times(1)).addObserver(mTabModelObserverCaptor.capture());
        return helper;
    }

    private void delayFetchExecutionTest(Consumer<TabObserver> consumer) {
        FetchHelper helper = createFetchHelper();
        verify(mTab, times(1)).addObserver(mTabObserverCaptor.capture());
        consumer.accept(getTabObserver());
        verify(mDelegate, times(0)).requestSuggestions(eq(STARTING_URL));
        verify(mDelegate, times(1)).reportFetchDelayed(eq(mWebContents));

        runUntilFetchPossible();
        verify(mDelegate, times(1)).requestSuggestions(eq(STARTING_URL));
    }

    private void delayFetchExecutionTest_pageLoadStarted_toSame(Consumer<TabObserver> consumer) {
        FetchHelper helper = createFetchHelper();
        verify(mTab, times(1)).addObserver(mTabObserverCaptor.capture());
        consumer.accept(getTabObserver());

        mTabObserverCaptor.getValue().onPageLoadStarted(mTab, STARTING_URL);
        verify(mDelegate, times(1)).clearState();

        runUntilFetchPossible();
        verify(mDelegate, times(1)).requestSuggestions(eq(STARTING_URL));
    }

    private void delayFetchExecutionTest_pageLoadStarted_toDifferent(
            Consumer<TabObserver> consumer) {
        FetchHelper helper = createFetchHelper();
        verify(mTab, times(1)).addObserver(mTabObserverCaptor.capture());
        consumer.accept(getTabObserver());

        mTabObserverCaptor.getValue().onPageLoadStarted(mTab, DIFFERENT_URL);
        verify(mDelegate, times(1)).clearState();

        // Request suggestions should not be called.
        runUntilFetchPossible();
        verify(mDelegate, times(0)).requestSuggestions(eq(STARTING_URL));
    }

    private void addAndcloseNonSelectedTab(Runnable closeTabRunnable) {
        // Starting with null tab so we can add one.
        doReturn(null).when(mTabModelSelector).getCurrentTab();
        FetchHelper helper = createFetchHelper();
        verify(mTabModel, times(1)).addObserver(mTabModelObserverCaptor.capture());

        addTab(mTab);
        assertTrue(helper.isObservingTab(mTab));
        verify(mDelegate, times(0)).requestSuggestions(eq(STARTING_URL));
        verify(mDelegate, times(0)).reportFetchDelayed(eq(mWebContents));

        closeTabRunnable.run();
        assertFalse(helper.isObservingTab(mTab));
        verify(mDelegate, times(0)).requestSuggestions(eq(STARTING_URL));
        verify(mDelegate, times(0)).reportFetchDelayed(eq(mWebContents));
        verify(mDelegate, times(0)).clearState();
    }
}
