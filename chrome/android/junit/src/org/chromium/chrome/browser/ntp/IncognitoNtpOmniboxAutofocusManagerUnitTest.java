// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
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
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.omnibox.AutocompleteRequestType;
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
    }

    @After
    public void tearDown() {
        if (mManager != null) {
            mManager.destroy();
        }

        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(false);
    }

    /**
     * Sets up the IncognitoNtpOmniboxAutofocusManager, captures its observers, and simulates a new
     * incognito tab being added.
     */
    private void setUpManagerAndAddNewTab() {
        mManager =
                IncognitoNtpOmniboxAutofocusManager.maybeCreate(
                        mContext,
                        mOmniboxStub,
                        mLayoutManager,
                        mTabModelSelector,
                        mNtpViewProvider,
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

        mTabModelObserver.didAddTab(mTab, 0, 0, false);

        ArgumentCaptor<TabObserver> tabObserverCaptor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab).addObserver(tabObserverCaptor.capture());
        mTabObserver = tabObserverCaptor.getValue();
    }

    /**
     * Sets up the necessary conditions for an autofocus check and simulates the NTP finishing its
     * page load, which is the event that triggers the autofocus logic.
     */
    private void finishLoadingNtp() {
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mTab.getUrl()).thenReturn(mNtpGurl);

        mTabObserver.onPageLoadFinished(mTab, mNtpGurl);
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
                        mNtpContentMetricsProvider);
        assertNotNull(mManager);

        // Verify that observers are not registered.
        verify(mOmniboxStub, never()).addUrlFocusChangeListener(any());
        verify(mLayoutManager, never()).addObserver(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testHandlePageLoadFinished_nonIncognitoTab_autofocusFails() {
        setUpManagerAndAddNewTab();
        when(mTab.isIncognitoBranded()).thenReturn(false);

        mTabObserver.onPageLoadFinished(mTab, mNtpGurl);

        verify(mOmniboxStub, never())
                .setUrlBarFocus(
                        true, null, OmniboxFocusReason.OMNIBOX_TAP, AutocompleteRequestType.SEARCH);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testHandlePageLoadFinished_incognitoNonNtpTab_autofocusFails() {
        setUpManagerAndAddNewTab();
        when(mTab.getUrl()).thenReturn(mOtherGurl);

        mTabObserver.onPageLoadFinished(mTab, mOtherGurl);

        verify(mOmniboxStub, never())
                .setUrlBarFocus(
                        true, null, OmniboxFocusReason.OMNIBOX_TAP, AutocompleteRequestType.SEARCH);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testTryAutofocus_autofocusSucceeds() {
        setUpManagerAndAddNewTab();
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        when(mTab.getUrl()).thenReturn(mNtpGurl);

        mTabObserver.onPageLoadFinished(mTab, mNtpGurl);

        verify(mTabView).post(runnableCaptor.capture());
        runnableCaptor.getValue().run();
        verify(mOmniboxStub)
                .setUrlBarFocus(
                        true, null, OmniboxFocusReason.OMNIBOX_TAP, AutocompleteRequestType.SEARCH);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testTryAutofocus_notCurrentTab_autofocusFails() {
        setUpManagerAndAddNewTab();
        when(mTabModelSelector.getCurrentTab()).thenReturn(null);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mTab.getUrl()).thenReturn(mNtpGurl);

        mTabObserver.onPageLoadFinished(mTab, mNtpGurl);

        verify(mTabView, never()).post(any(Runnable.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testTryAutofocus_wrongLayoutType_autofocusFails() {
        setUpManagerAndAddNewTab();
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.TAB_SWITCHER);
        when(mTab.getUrl()).thenReturn(mNtpGurl);

        mTabObserver.onPageLoadFinished(mTab, mNtpGurl);

        verify(mTabView, never()).post(any(Runnable.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testTryAutofocus_layoutInTransition_autofocusFails() {
        setUpManagerAndAddNewTab();
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        mLayoutStateObserver.onStartedShowing(LayoutType.BROWSING);
        when(mTab.getUrl()).thenReturn(mNtpGurl);

        mTabObserver.onPageLoadFinished(mTab, mNtpGurl);

        verify(mTabView, never()).post(any(Runnable.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testAutofocus_whenReturnedAfterNavigating_autofocusFails() {
        setUpManagerAndAddNewTab();

        // 1. First NTP load, autofocus should succeed.
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mTab.getUrl()).thenReturn(mNtpGurl);

        mTabObserver.onPageLoadFinished(mTab, mNtpGurl);

        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mTabView).post(runnableCaptor.capture());
        runnableCaptor.getValue().run();
        verify(mOmniboxStub)
                .setUrlBarFocus(
                        true, null, OmniboxFocusReason.OMNIBOX_TAP, AutocompleteRequestType.SEARCH);

        // After the first autofocus, the tab is marked as processed.
        // Reset mocks to verify that autofocus does not happen again.
        Mockito.reset(mOmniboxStub, mTabView);

        // 2. Simulate navigating away and coming back to the NTP.
        // The manager should not autofocus again because the tab has been processed.
        mTabObserver.onPageLoadFinished(mTab, mNtpGurl);

        verify(mTabView, never()).post(any(Runnable.class));
        verify(mOmniboxStub, never()).setUrlBarFocus(anyBoolean(), any(), anyInt(), anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testAutofocusCondition_noConditionsConfigured_autofocusSucceeds() {
        setUpManagerAndAddNewTab();
        finishLoadingNtp();

        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mTabView).post(runnableCaptor.capture());
        runnableCaptor.getValue().run();

        verify(mOmniboxStub)
                .setUrlBarFocus(
                        true, null, OmniboxFocusReason.OMNIBOX_TAP, AutocompleteRequestType.SEARCH);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":not_first_tab/true")
    public void testAutofocusCondition_notFirstTab_andFirstTabOpened_autofocusFails() {
        // Open first tab.
        setUpManagerAndAddNewTab();
        finishLoadingNtp();

        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mTabView).post(runnableCaptor.capture());
        runnableCaptor.getValue().run();

        verify(mOmniboxStub, never()).setUrlBarFocus(anyBoolean(), any(), anyInt(), anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":not_first_tab/true")
    public void testAutofocusCondition_notFirstTab_andSecondTabOpened_autofocusSucceeds() {
        // Open first tab.
        setUpManagerAndAddNewTab();
        // Open second tab.
        mTabModelObserver.didAddTab(mTab, 0, 0, false);
        finishLoadingNtp();

        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mTabView).post(runnableCaptor.capture());
        runnableCaptor.getValue().run();

        verify(mOmniboxStub)
                .setUrlBarFocus(
                        true, null, OmniboxFocusReason.OMNIBOX_TAP, AutocompleteRequestType.SEARCH);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_prediction/true")
    public void testAutofocusCondition_withPrediction_autofocusFails() {
        IncognitoNtpOmniboxAutofocusManager.sAutofocusAllowedWithPredictionForTesting = false;
        setUpManagerAndAddNewTab();
        finishLoadingNtp();

        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mTabView).post(runnableCaptor.capture());
        runnableCaptor.getValue().run();

        verify(mOmniboxStub, never()).setUrlBarFocus(anyBoolean(), any(), anyInt(), anyInt());
        IncognitoNtpOmniboxAutofocusManager.sAutofocusAllowedWithPredictionForTesting = null;
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_prediction/true")
    public void testAutofocusCondition_withPrediction_autofocusSucceeds() {
        IncognitoNtpOmniboxAutofocusManager.sAutofocusAllowedWithPredictionForTesting = true;
        setUpManagerAndAddNewTab();
        finishLoadingNtp();

        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mTabView).post(runnableCaptor.capture());
        runnableCaptor.getValue().run();

        verify(mOmniboxStub)
                .setUrlBarFocus(
                        true, null, OmniboxFocusReason.OMNIBOX_TAP, AutocompleteRequestType.SEARCH);
        IncognitoNtpOmniboxAutofocusManager.sAutofocusAllowedWithPredictionForTesting = null;
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_hardware_keyboard/true")
    public void testAutofocusCondition_withHardwareKeyboard_autofocusFails() {
        IncognitoNtpOmniboxAutofocusManager.setIsHardwareKeyboardAttachedForTesting(false);
        setUpManagerAndAddNewTab();
        finishLoadingNtp();

        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mTabView).post(runnableCaptor.capture());
        runnableCaptor.getValue().run();

        verify(mOmniboxStub, never()).setUrlBarFocus(anyBoolean(), any(), anyInt(), anyInt());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_hardware_keyboard/true")
    public void testAutofocusCondition_withHardwareKeyboard_autofocusSucceeds() {
        IncognitoNtpOmniboxAutofocusManager.setIsHardwareKeyboardAttachedForTesting(true);
        setUpManagerAndAddNewTab();
        finishLoadingNtp();

        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mTabView).post(runnableCaptor.capture());
        runnableCaptor.getValue().run();

        verify(mOmniboxStub)
                .setUrlBarFocus(
                        true, null, OmniboxFocusReason.OMNIBOX_TAP, AutocompleteRequestType.SEARCH);
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP
                    + ":not_first_tab/true/with_prediction/true")
    public void
            testAutofocusCondition_combination_notFirstTabFails_predictionSucceeds_autofocusSucceeds() {
        // First tab, so not_first_tab fails.
        IncognitoNtpOmniboxAutofocusManager.sAutofocusAllowedWithPredictionForTesting = true;
        setUpManagerAndAddNewTab();
        finishLoadingNtp();

        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mTabView).post(runnableCaptor.capture());
        runnableCaptor.getValue().run();

        // Should autofocus because one of the conditions is met.
        verify(mOmniboxStub)
                .setUrlBarFocus(
                        true, null, OmniboxFocusReason.OMNIBOX_TAP, AutocompleteRequestType.SEARCH);
        IncognitoNtpOmniboxAutofocusManager.sAutofocusAllowedWithPredictionForTesting = null;
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP
                    + ":not_first_tab/true/with_prediction/true/with_hardware_keyboard/true")
    public void testAutofocusCondition_allEnabled_allFailed_autofocusFails() {
        // Set all conditions to fail.
        // 1. with_prediction:
        IncognitoNtpOmniboxAutofocusManager.sAutofocusAllowedWithPredictionForTesting = false;
        // 2. with_hardware_keyboard:
        IncognitoNtpOmniboxAutofocusManager.setIsHardwareKeyboardAttachedForTesting(false);
        // 3. not_first_tab: This is the first tab, so it fails.
        setUpManagerAndAddNewTab();

        finishLoadingNtp();

        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mTabView).post(runnableCaptor.capture());
        runnableCaptor.getValue().run();

        // Should not autofocus because all conditions fail.
        verify(mOmniboxStub, never()).setUrlBarFocus(anyBoolean(), any(), anyInt(), anyInt());

        IncognitoNtpOmniboxAutofocusManager.sAutofocusAllowedWithPredictionForTesting = null;
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testTabClosure_removesTabObserver() {
        // Adds the observer to mTab.
        setUpManagerAndAddNewTab();

        // Now, closing the tab should remove it.
        mTabModelObserver.tabClosureCommitted(mTab);

        verify(mTab).removeObserver(mTabObserver);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testUrlFocusChangeListener_addsAndRemovesNtpViewTouchListener() {
        mManager =
                IncognitoNtpOmniboxAutofocusManager.maybeCreate(
                        mContext,
                        mOmniboxStub,
                        mLayoutManager,
                        mTabModelSelector,
                        mNtpViewProvider,
                        mNtpContentMetricsProvider);
        assertNotNull(mManager);
        ArgumentCaptor<UrlFocusChangeListener> listenerCaptor =
                ArgumentCaptor.forClass(UrlFocusChangeListener.class);
        verify(mOmniboxStub).addUrlFocusChangeListener(listenerCaptor.capture());
        UrlFocusChangeListener urlFocusChangeListener = listenerCaptor.getValue();

        View ntpView = Mockito.mock(View.class);
        when(mNtpViewProvider.apply(mTab)).thenReturn(ntpView);
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);
        when(mTab.getUrl()).thenReturn(mNtpGurl);

        // 1. Gain focus
        urlFocusChangeListener.onUrlFocusChange(true);
        verify(ntpView).setOnTouchListener(any(View.OnTouchListener.class));

        // 2. Lose focus
        urlFocusChangeListener.onUrlFocusChange(false);
        verify(ntpView).setOnTouchListener(null);
    }
}
