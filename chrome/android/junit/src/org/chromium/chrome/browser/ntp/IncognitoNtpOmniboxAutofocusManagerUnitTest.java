// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.isNotNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Collections;
import java.util.function.Function;

/** Unit tests for {@link IncognitoNtpOmniboxAutofocusManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class IncognitoNtpOmniboxAutofocusManagerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private OmniboxStub mOmniboxStub;
    @Mock private LayoutManagerImpl mLayoutManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private Tab mTab;
    @Mock private View mTabView;
    @Mock private Function<Tab, View> mNtpViewProvider;
    @Mock private NewTabPageScrollView mNewTabPageScrollView;
    @Mock private Function<Tab, NewTabPageScrollView> mNtpScrollViewProvider;

    @Mock
    private Function<View, IncognitoNtpUtils.IncognitoNtpContentMetrics> mNtpContentMetricsProvider;

    private Context mContext;
    private IncognitoNtpOmniboxAutofocusManager mManager;
    private TabModelObserver mTabModelObserver;
    private TabObserver mTabObserver;
    private LayoutStateProvider.LayoutStateObserver mLayoutStateObserver;
    private final GURL mNtpGurl = JUnitTestGURLs.NTP_URL;
    private final GURL mOtherGurl = JUnitTestGURLs.URL_1;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = ContextUtils.getApplicationContext();

        when(mTabModelSelector.getModels())
                .thenReturn(Collections.singletonList(mIncognitoTabModel));
        when(mIncognitoTabModel.isIncognitoBranded()).thenReturn(true);
        when(mTab.isIncognitoBranded()).thenReturn(true);
        when(mTab.getView()).thenReturn(mTabView);
        when(mNtpScrollViewProvider.apply(any())).thenReturn(mNewTabPageScrollView);
    }

    @After
    public void tearDown() {
        if (mManager != null) {
            mManager.destroy();
        }

        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(false);
    }

    /** Sets up the IncognitoNtpOmniboxAutofocusManager and captures its observers. */
    private void setUpManager() {
        mManager =
                IncognitoNtpOmniboxAutofocusManager.maybeCreate(
                        mContext,
                        mOmniboxStub,
                        mLayoutManager,
                        mTabModelSelector,
                        mNtpViewProvider,
                        mNtpScrollViewProvider,
                        mNtpContentMetricsProvider);
        assertNotNull(mManager);

        ArgumentCaptor<TabModelObserver> tabModelObserverCaptor =
                ArgumentCaptor.forClass(TabModelObserver.class);
        verify(mIncognitoTabModel).addObserver(tabModelObserverCaptor.capture());
        mTabModelObserver = tabModelObserverCaptor.getValue();

        ArgumentCaptor<LayoutStateProvider.LayoutStateObserver> layoutStateObserverCaptor =
                ArgumentCaptor.forClass(LayoutStateProvider.LayoutStateObserver.class);
        verify(mLayoutManager).addObserver(layoutStateObserverCaptor.capture());
        mLayoutStateObserver = layoutStateObserverCaptor.getValue();
    }

    /**
     * Simulates loading a URL in the main test tab (`mTab`). This is used for the first tab in a
     * test, or for tests that only involve a single tab.
     *
     * @param url The URL to load in the main test tab (`mTab`).
     */
    private void simulateLoadUrlInMainTab(GURL url) {
        mTabObserver = simulateDidAddTab(mTab, url);
        simulatePageLoadFinished(mTab, mTabView, mTabObserver, url);
    }

    /**
     * Simulates opening a new incognito tab and loading a URL in it. This helper method is used for
     * testing scenarios with multiple tabs.
     *
     * @param url The URL to load in the new tab.
     */
    private void simulateOpenNewTabAndLoadUrl(GURL url) {
        Tab tab = Mockito.mock(Tab.class);
        View tabView = Mockito.mock(View.class);
        when(tab.isIncognitoBranded()).thenReturn(true);
        when(tab.getView()).thenReturn(tabView);
        TabObserver observer = simulateDidAddTab(tab, url);
        simulatePageLoadFinished(tab, tabView, observer, url);
    }

    /**
     * Simulates `didAddTab` for a given tab and captures the {@link TabObserver} that is added.
     *
     * @param tab The tab to add.
     * @param url The URL that will be loaded.
     * @return The captured {@link TabObserver}.
     */
    private TabObserver simulateDidAddTab(Tab tab, GURL url) {
        when(tab.getUrl()).thenReturn(url);
        mTabModelObserver.didAddTab(tab, 0, 0, false);
        ArgumentCaptor<TabObserver> tabObserverCaptor = ArgumentCaptor.forClass(TabObserver.class);
        verify(tab).addObserver(tabObserverCaptor.capture());
        return tabObserverCaptor.getValue();
    }

    /**
     * Simulates the `onPageLoadFinished` event and the subsequent UI post-task for a given tab.
     *
     * @param tab The tab that is finishing the load.
     * @param tabView The view associated with the tab.
     * @param observer The observer for the tab.
     * @param url The URL that is being loaded.
     */
    private void simulatePageLoadFinished(Tab tab, View tabView, TabObserver observer, GURL url) {
        when(mTabModelSelector.getCurrentTab()).thenReturn(tab);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(tab.getUrl()).thenReturn(url);

        observer.onPageLoadFinished(tab, url);

        if (url.equals(mNtpGurl)) {
            ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
            verify(tabView).post(runnableCaptor.capture());
            runnableCaptor.getValue().run();
        }
    }

    private void verifyAutofocus(boolean shouldBeCalled) {
        if (shouldBeCalled) {
            verify(mOmniboxStub).beginInput(isNotNull());
        } else {
            verify(mOmniboxStub, never()).beginInput(any());
        }
    }

    @Test
    @DisableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testMaybeCreate_featureDisabled() {
        mManager =
                IncognitoNtpOmniboxAutofocusManager.maybeCreate(
                        mContext,
                        mOmniboxStub,
                        mLayoutManager,
                        mTabModelSelector,
                        mNtpViewProvider,
                        mNtpScrollViewProvider,
                        mNtpContentMetricsProvider);
        assertNull(mManager);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testMaybeCreate_featureEnabled() {
        mManager =
                IncognitoNtpOmniboxAutofocusManager.maybeCreate(
                        mContext,
                        mOmniboxStub,
                        mLayoutManager,
                        mTabModelSelector,
                        mNtpViewProvider,
                        mNtpScrollViewProvider,
                        mNtpContentMetricsProvider);
        assertNotNull(mManager);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testMaybeCreate_nullOmniboxStub() {
        mManager =
                IncognitoNtpOmniboxAutofocusManager.maybeCreate(
                        mContext,
                        null,
                        mLayoutManager,
                        mTabModelSelector,
                        mNtpViewProvider,
                        mNtpScrollViewProvider,
                        mNtpContentMetricsProvider);
        assertNull(mManager);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testObserversNotRegistered_whenAccessibilityEnabled() {
        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true);

        mManager =
                IncognitoNtpOmniboxAutofocusManager.maybeCreate(
                        mContext,
                        mOmniboxStub,
                        mLayoutManager,
                        mTabModelSelector,
                        mNtpViewProvider,
                        mNtpScrollViewProvider,
                        mNtpContentMetricsProvider);
        assertNotNull(mManager);

        // Verify that observers are not registered.
        verify(mOmniboxStub, never()).addUrlFocusChangeListener(any());
        verify(mLayoutManager, never()).addObserver(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testHandlePageLoadFinished_nonIncognitoTab_autofocusFails() {
        setUpManager();

        when(mTab.isIncognitoBranded()).thenReturn(false);
        mTabModelObserver.didAddTab(mTab, 0, 0, false);
        verify(mTab, never()).addObserver(any());

        verifyAutofocus(false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testHandlePageLoadFinished_incognitoNonNtpTab_autofocusFails() {
        setUpManager();
        simulateLoadUrlInMainTab(mOtherGurl);
        verifyAutofocus(false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testTryAutofocus_autofocusSucceeds() {
        setUpManager();
        simulateLoadUrlInMainTab(mNtpGurl);
        verifyAutofocus(true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testTryAutofocus_notCurrentTab_autofocusFails() {
        setUpManager();
        mTabObserver = simulateDidAddTab(mTab, mNtpGurl);

        when(mTabModelSelector.getCurrentTab()).thenReturn(null);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);

        mTabObserver.onPageLoadFinished(mTab, mNtpGurl);

        verify(mTabView, never()).post(any(Runnable.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testTryAutofocus_wrongLayoutType_autofocusFails() {
        setUpManager();
        mTabObserver = simulateDidAddTab(mTab, mNtpGurl);

        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.TAB_SWITCHER);

        mTabObserver.onPageLoadFinished(mTab, mNtpGurl);

        verify(mTabView, never()).post(any(Runnable.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testTryAutofocus_layoutInTransition_autofocusFails() {
        setUpManager();
        mTabObserver = simulateDidAddTab(mTab, mNtpGurl);

        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        mLayoutStateObserver.onStartedShowing(LayoutType.BROWSING);

        mTabObserver.onPageLoadFinished(mTab, mNtpGurl);

        verify(mTabView, never()).post(any(Runnable.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testAutofocus_whenReturnedAfterNavigating_autofocusFails() {
        setUpManager();

        // 1. First NTP load, autofocus should succeed.
        simulateLoadUrlInMainTab(mNtpGurl);
        verifyAutofocus(true);

        // After the first autofocus, the tab is marked as processed.
        // Reset mocks to verify that autofocus does not happen again.
        Mockito.reset(mOmniboxStub, mTabView);

        // 2. Simulate navigating away and coming back to the NTP.
        // The manager should not autofocus again because the tab has been processed.
        mTabObserver.onPageLoadFinished(mTab, mNtpGurl);

        verify(mTabView, never()).post(any(Runnable.class));
        verifyAutofocus(false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testAutofocusCondition_noConditionsConfigured_autofocusSucceeds() {
        setUpManager();
        simulateLoadUrlInMainTab(mNtpGurl);
        verifyAutofocus(true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":not_first_tab/true")
    public void testAutofocusCondition_notFirstTab_autofocusSucceedsOnSecondNtp() {
        setUpManager();

        // Open first tab with a non-NTP URL. This should not be counted.
        simulateLoadUrlInMainTab(mOtherGurl);
        verifyAutofocus(false);

        // Open second tab and load NTP. This is the first NTP, so no autofocus.
        simulateOpenNewTabAndLoadUrl(mNtpGurl);
        verifyAutofocus(false);

        // Open a third tab and load NTP. This is the second NTP, so autofocus should be triggered.
        simulateOpenNewTabAndLoadUrl(mNtpGurl);
        verifyAutofocus(true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_prediction/true")
    public void testAutofocusCondition_withPrediction_autofocusFails() {
        IncognitoNtpOmniboxAutofocusManager.setAutofocusAllowedWithPredictionForTesting(false);
        setUpManager();
        simulateLoadUrlInMainTab(mNtpGurl);
        verifyAutofocus(false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_prediction/true")
    public void testAutofocusCondition_withPrediction_autofocusSucceeds() {
        IncognitoNtpOmniboxAutofocusManager.setAutofocusAllowedWithPredictionForTesting(true);
        setUpManager();
        simulateLoadUrlInMainTab(mNtpGurl);
        verifyAutofocus(true);
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_hardware_keyboard/true")
    public void testAutofocusCondition_withHardwareKeyboard_autofocusFails() {
        IncognitoNtpOmniboxAutofocusManager.setIsHardwareKeyboardAttachedForTesting(false);
        setUpManager();
        simulateLoadUrlInMainTab(mNtpGurl);
        verifyAutofocus(false);
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_hardware_keyboard/true")
    public void testAutofocusCondition_withHardwareKeyboard_autofocusSucceeds() {
        IncognitoNtpOmniboxAutofocusManager.setIsHardwareKeyboardAttachedForTesting(true);
        setUpManager();
        simulateLoadUrlInMainTab(mNtpGurl);
        verifyAutofocus(true);
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP
                    + ":not_first_tab/true/with_prediction/true")
    public void
            testAutofocusCondition_combination_notFirstTabFails_predictionSucceeds_autofocusSucceeds() {
        // First tab, so not_first_tab fails.
        IncognitoNtpOmniboxAutofocusManager.setAutofocusAllowedWithPredictionForTesting(true);
        setUpManager();
        simulateLoadUrlInMainTab(mNtpGurl);
        // Should autofocus because one of the conditions is met.
        verifyAutofocus(true);
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP
                    + ":not_first_tab/true/with_prediction/true/with_hardware_keyboard/true")
    public void testAutofocusCondition_allEnabled_allFailed_autofocusFails() {
        // Set all conditions to fail.
        // 1. with_prediction:
        IncognitoNtpOmniboxAutofocusManager.setAutofocusAllowedWithPredictionForTesting(false);
        // 2. with_hardware_keyboard:
        IncognitoNtpOmniboxAutofocusManager.setIsHardwareKeyboardAttachedForTesting(false);
        // 3. not_first_tab: This is the first tab, so it fails.
        setUpManager();
        simulateLoadUrlInMainTab(mNtpGurl);
        // Should not autofocus because all conditions fail.
        verifyAutofocus(false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testTabClosure_removesTabObserver() {
        setUpManager();
        mTabObserver = simulateDidAddTab(mTab, mNtpGurl);

        // Closing the tab should remove TabObserver.
        mTabModelObserver.tabClosureCommitted(mTab);

        verify(mTab).removeObserver(mTabObserver);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testUrlFocusChangeListener_addsAndRemovesNtpScrollViewTouchListener() {
        mManager =
                IncognitoNtpOmniboxAutofocusManager.maybeCreate(
                        mContext,
                        mOmniboxStub,
                        mLayoutManager,
                        mTabModelSelector,
                        mNtpViewProvider,
                        mNtpScrollViewProvider,
                        mNtpContentMetricsProvider);
        assertNotNull(mManager);
        ArgumentCaptor<UrlFocusChangeListener> listenerCaptor =
                ArgumentCaptor.forClass(UrlFocusChangeListener.class);
        verify(mOmniboxStub).addUrlFocusChangeListener(listenerCaptor.capture());
        UrlFocusChangeListener urlFocusChangeListener = listenerCaptor.getValue();

        View ntpView = Mockito.mock(View.class);
        NewTabPageScrollView ntpScrollView = Mockito.mock(NewTabPageScrollView.class);
        when(mNtpViewProvider.apply(mTab)).thenReturn(ntpView);
        when(mNtpScrollViewProvider.apply(mTab)).thenReturn(ntpScrollView);
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);
        when(mTab.getUrl()).thenReturn(mNtpGurl);

        // 1. Gain focus
        urlFocusChangeListener.onUrlFocusChange(true);
        verify(ntpScrollView).setOnTouchListener(any(View.OnTouchListener.class));

        // 2. Lose focus
        urlFocusChangeListener.onUrlFocusChange(false);
        verify(ntpScrollView).setOnTouchListener(null);
    }
}
