// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.os.Handler;
import android.os.Looper;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.dom_distiller.DistillerHeuristicsType;
import org.chromium.chrome.browser.dom_distiller.DomDistillerTabUtils;
import org.chromium.chrome.browser.dom_distiller.DomDistillerTabUtilsJni;
import org.chromium.chrome.browser.dom_distiller.ReaderModeActionRateLimiter;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.dom_distiller.ReaderModeMetrics;
import org.chromium.chrome.browser.dom_distiller.TabDistillabilityProvider;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.segmentation_platform.ContextualPageActionController.ActionProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.components.ukm.UkmRecorderJni;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.concurrent.TimeoutException;

/** Unit tests for {@link ReaderModeActionProvider} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DisableFeatures(DomDistillerFeatures.READER_MODE_USE_READABILITY)
public class ReaderModeActionProviderTest {

    private static final GURL TEST_URL = new GURL("https://test.com");
    private static final GURL TEST_DISTILLER_URL = new GURL("chrome-distiller://test.com");

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mMockTab;
    @Mock private WebContents mMockWebContents;
    @Mock private NavigationController mMockNavigationController;
    @Mock private ReaderModeManager mMockReaderModeManager;
    @Mock private SignalAccumulator mMockSignalAccumulator;
    @Mock private Profile mProfile;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private PrefService mPrefService;
    @Mock private UkmRecorder.Natives mUkmRecorderJniMock;
    @Mock private DomDistillerTabUtilsJni mDomDistillerTabUtilsJni;
    @Mock private DomDistillerUrlUtilsJni mDomDistillerUrlUtilsJni;
    @Mock private OneshotSupplier<Boolean> mButtonVisibilitySupplier;
    @Mock private ReaderModeActionRateLimiter mReaderModeActionRateLimiter;

    @Before
    @SuppressWarnings("DirectInvocationOnMock")
    public void setUp() {
        initializeReaderModeBackend();
        ReaderModeActionRateLimiter.setInstanceForTesting(mReaderModeActionRateLimiter);

        mMockTab.getUserDataHost()
                .setUserData(ReaderModeManager.USER_DATA_KEY, mMockReaderModeManager);
        when(mMockTab.getWebContents()).thenReturn(mMockWebContents);
        when(mMockTab.getUrl()).thenReturn(TEST_URL);
        when(mMockWebContents.getNavigationController()).thenReturn(mMockNavigationController);

        UkmRecorderJni.setInstanceForTesting(mUkmRecorderJniMock);
        DomDistillerTabUtilsJni.setInstanceForTesting(mDomDistillerTabUtilsJni);
        DomDistillerUrlUtilsJni.setInstanceForTesting(mDomDistillerUrlUtilsJni);
    }

    private void initializeReaderModeBackend() {
        UserDataHost userDataHost = new UserDataHost();
        when(mMockTab.getUserDataHost()).thenReturn(userDataHost);
        when(mMockTab.getProfile()).thenReturn(mProfile);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(Pref.READER_FOR_ACCESSIBILITY)).thenReturn(false);

        TabDistillabilityProvider.createForTab(mMockTab);
        DomDistillerTabUtils.setExcludeMobileFriendlyForTesting(true);
    }

    private void setReaderModeBackendSignal(boolean isDistillable) {
        TabDistillabilityProvider tabDistillabilityProvider =
                TabDistillabilityProvider.get(mMockTab);
        tabDistillabilityProvider.onIsPageDistillableResult(
                TEST_URL,
                isDistillable,
                /* isLast= */ true,
                /* isLongArticle= */ false,
                /* isMobileOptimized= */ false);
    }

    @Test
    public void testIsDistillableInvokesCallback() throws TimeoutException {
        HashMap<Integer, ActionProvider> providers = new HashMap<>();
        var provider = new ReaderModeActionProvider(mButtonVisibilitySupplier);
        providers.put(AdaptiveToolbarButtonVariant.READER_MODE, provider);
        SignalAccumulator accumulator = new SignalAccumulator(new Handler(), mMockTab, providers);
        setReaderModeBackendSignal(true);
        provider.getAction(mMockTab, accumulator);
        ShadowLooper.idleMainLooper();

        Assert.assertTrue(accumulator.getSignal(AdaptiveToolbarButtonVariant.READER_MODE));
    }

    @Test
    public void testChromeSchemeUrl_isNotDistillableImmediateResult() throws TimeoutException {
        when(mMockTab.getUrl()).thenReturn(new GURL("chrome://newtab"));

        HashMap<Integer, ActionProvider> providers = new HashMap<>();
        var provider = new ReaderModeActionProvider(mButtonVisibilitySupplier);
        providers.put(AdaptiveToolbarButtonVariant.READER_MODE, provider);
        SignalAccumulator accumulator = new SignalAccumulator(new Handler(), mMockTab, providers);
        provider.getAction(mMockTab, accumulator);
        ShadowLooper.idleMainLooper();

        Assert.assertFalse(accumulator.getSignal(AdaptiveToolbarButtonVariant.READER_MODE));
    }


    @Test
    public void testWaitForDistillabilityResult() throws TimeoutException {
        var provider = new ReaderModeActionProvider(mButtonVisibilitySupplier);
        // Get action before distillability is determined.
        provider.getAction(mMockTab, mMockSignalAccumulator);
        ShadowLooper.idleMainLooper();

        verify(mMockSignalAccumulator, never())
                .setSignal(eq(AdaptiveToolbarButtonVariant.READER_MODE), anyBoolean());

        // We should wait for distillability before setting a signal.
        setReaderModeBackendSignal(true);
        verify(mMockSignalAccumulator).setSignal(AdaptiveToolbarButtonVariant.READER_MODE, true);
    }

    @Test
    public void testReaderModeSignalRecordsMetricsOnCPASuccess() {
        when(mMockSignalAccumulator.hasTimedOut()).thenReturn(false);
        var provider = new ReaderModeActionProvider(mButtonVisibilitySupplier);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "DomDistiller.Android.AnyPageSignalWithinTimeout", true)
                        .expectBooleanRecord(
                                "DomDistiller.Android.DistillablePageSignalWithinTimeout", true)
                        // First step in the CPA funnel which shows the page is eligible for
                        // distillation.
                        .expectIntRecord(
                                ReaderModeMetrics
                                        .READER_MODE_CONTEXTUAL_PAGE_ACTION_EVENT_HISTOGRAM,
                                ReaderModeMetrics.ReaderModeContextualPageActionEvent.ELIGIBLE)
                        .expectAnyRecord("DomDistiller.Time.TimeToProvideResultToAccumulator")
                        .build();
        setReaderModeBackendSignal(true);
        provider.getAction(mMockTab, mMockSignalAccumulator);
        ShadowLooper.idleMainLooper();

        verify(mMockSignalAccumulator).setSignal(AdaptiveToolbarButtonVariant.READER_MODE, true);
        watcher.assertExpected();
        verify(mUkmRecorderJniMock)
                .recordEventWithMultipleMetrics(
                        any(), eq("DomDistiller.Android.DistillabilityLatency"), any());
    }

    @Test
    public void testReaderModeSignalRecordsMetricsOnCPATimeout() {
        when(mMockSignalAccumulator.hasTimedOut()).thenReturn(true);
        var provider = new ReaderModeActionProvider(mButtonVisibilitySupplier);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "DomDistiller.Android.AnyPageSignalWithinTimeout", false)
                        .expectBooleanRecord(
                                "DomDistiller.Android.DistillablePageSignalWithinTimeout", false)
                        .expectAnyRecord("DomDistiller.Time.TimeToProvideResultToAccumulator")
                        .build();
        setReaderModeBackendSignal(true);
        provider.getAction(mMockTab, mMockSignalAccumulator);
        ShadowLooper.idleMainLooper();

        verify(mMockSignalAccumulator).setSignal(AdaptiveToolbarButtonVariant.READER_MODE, true);
        watcher.assertExpected();
    }

    @Test
    public void testReaderModeDisabledOnDesktopPages() {
        DomDistillerTabUtils.setDistillerHeuristicsForTesting(DistillerHeuristicsType.OG_ARTICLE);
        when(mMockNavigationController.getUseDesktopUserAgent()).thenReturn(true);

        var provider = new ReaderModeActionProvider(mButtonVisibilitySupplier);

        setReaderModeBackendSignal(true);
        provider.getAction(mMockTab, mMockSignalAccumulator);
        ShadowLooper.idleMainLooper();

        verify(mMockSignalAccumulator).setSignal(AdaptiveToolbarButtonVariant.READER_MODE, false);
    }

    @Test
    public void testReaderModeDisabledOnDesktopPages_exceptIfHeuristicIsAlwaysTrue() {
        // Set heuristic to flag all sites as distillable.
        DomDistillerTabUtils.setDistillerHeuristicsForTesting(DistillerHeuristicsType.ALWAYS_TRUE);

        WebContents mockWebContents = mock(WebContents.class);
        NavigationController mockNavigationController = mock(NavigationController.class);
        // Set "request desktop page" on.
        when(mockNavigationController.getUseDesktopUserAgent()).thenReturn(true);
        when(mockWebContents.getNavigationController()).thenReturn(mockNavigationController);
        when(mMockTab.getWebContents()).thenReturn(mockWebContents);

        var provider = new ReaderModeActionProvider(mButtonVisibilitySupplier);

        setReaderModeBackendSignal(true);
        provider.getAction(mMockTab, mMockSignalAccumulator);
        ShadowLooper.idleMainLooper();

        verify(mMockSignalAccumulator).setSignal(AdaptiveToolbarButtonVariant.READER_MODE, true);
    }

    @Test
    public void testReaderModeManagerNoUpdateUiShown() {
        var provider = new ReaderModeActionProvider(mButtonVisibilitySupplier);
        provider.onActionShown(mMockTab, AdaptiveToolbarButtonVariant.READER_MODE);
        shadowOf(Looper.getMainLooper()).runOneTask();
        verify(mMockReaderModeManager).onContextualPageActionShown(mButtonVisibilitySupplier, true);
        clearInvocations(mMockReaderModeManager);
    }

    @Test
    public void testOnActionShownActionShownInvokedForTimedOutAccumulator() {
        when(mMockSignalAccumulator.hasTimedOut()).thenReturn(true);
        var provider = new ReaderModeActionProvider(mButtonVisibilitySupplier);
        provider.getAction(mMockTab, mMockSignalAccumulator);
        ShadowLooper.idleMainLooper();
        provider.onActionShown(mMockTab, AdaptiveToolbarButtonVariant.UNKNOWN);
        shadowOf(Looper.getMainLooper()).runOneTask();

        verify(mMockReaderModeManager)
                .onContextualPageActionShown(mButtonVisibilitySupplier, false);
        clearInvocations(mMockReaderModeManager);
    }

    @Test
    public void testDestroy() {
        var provider = new ReaderModeActionProvider(mButtonVisibilitySupplier);
        provider.getAction(mMockTab, mMockSignalAccumulator);
        provider.destroy();
        ShadowLooper.idleMainLooper();

        setReaderModeBackendSignal(true);
        verify(mMockSignalAccumulator, never())
                .setSignal(eq(AdaptiveToolbarButtonVariant.READER_MODE), anyBoolean());
    }

    @Test
    @EnableFeatures(DomDistillerFeatures.READER_MODE_USE_READABILITY + ":use_heuristic/true")
    public void testReadabiltyHeuristicUsed() throws TimeoutException {
        ArgumentCaptor<Callback<Boolean>> readabilityHeuristicCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);

        var provider = new ReaderModeActionProvider(mButtonVisibilitySupplier);
        provider.getAction(mMockTab, mMockSignalAccumulator);
        ShadowLooper.idleMainLooper();
        verify(mDomDistillerTabUtilsJni)
                .runReadabilityHeuristicsOnWebContents(
                        any(), readabilityHeuristicCallbackCaptor.capture());
        Assert.assertNotNull(readabilityHeuristicCallbackCaptor.getValue());

        verify(mMockSignalAccumulator, never())
                .setSignal(eq(AdaptiveToolbarButtonVariant.READER_MODE), anyBoolean());

        // When using the readability heurisitic, the distillability provider should be ignored.
        setReaderModeBackendSignal(true);
        verify(mMockSignalAccumulator, Mockito.times(0))
                .setSignal(AdaptiveToolbarButtonVariant.READER_MODE, true);
        readabilityHeuristicCallbackCaptor.getValue().onResult(true);
        verify(mMockSignalAccumulator).setSignal(AdaptiveToolbarButtonVariant.READER_MODE, true);
    }

    @Test
    @EnableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP)
    public void testDistillableButSupressed() {
        when(mReaderModeActionRateLimiter.isActionSuppressed()).thenReturn(true);
        var provider = new ReaderModeActionProvider(mButtonVisibilitySupplier);
        // Get action before distillability is determined.
        provider.getAction(mMockTab, mMockSignalAccumulator);
        ShadowLooper.idleMainLooper();
        setReaderModeBackendSignal(true);
        verify(mMockSignalAccumulator, Mockito.times(0))
                .setSignal(AdaptiveToolbarButtonVariant.READER_MODE, true);
    }

    @Test
    @EnableFeatures({
        DomDistillerFeatures.READER_MODE_DISTILL_IN_APP,
        DomDistillerFeatures.READER_MODE_USE_READABILITY + ":use_heuristic/true"
    })
    public void testDistillableButSupressed_ReadabiltyHeuristicUsed() throws TimeoutException {
        when(mReaderModeActionRateLimiter.isActionSuppressed()).thenReturn(true);
        ArgumentCaptor<Callback<Boolean>> readabilityHeuristicCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);

        var provider = new ReaderModeActionProvider(mButtonVisibilitySupplier);
        provider.getAction(mMockTab, mMockSignalAccumulator);
        ShadowLooper.idleMainLooper();
        verify(mDomDistillerTabUtilsJni)
                .runReadabilityHeuristicsOnWebContents(
                        any(), readabilityHeuristicCallbackCaptor.capture());
        Assert.assertNotNull(readabilityHeuristicCallbackCaptor.getValue());

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                ReaderModeMetrics
                                        .READER_MODE_CONTEXTUAL_PAGE_ACTION_EVENT_HISTOGRAM,
                                ReaderModeMetrics.ReaderModeContextualPageActionEvent.ELIGIBLE)
                        .expectIntRecord(
                                ReaderModeMetrics
                                        .READER_MODE_CONTEXTUAL_PAGE_ACTION_EVENT_HISTOGRAM,
                                ReaderModeMetrics.ReaderModeContextualPageActionEvent.SUPPRESSED)
                        .build();

        readabilityHeuristicCallbackCaptor.getValue().onResult(true);

        watcher.assertExpected();
        verify(mMockSignalAccumulator, Mockito.times(0))
                .setSignal(AdaptiveToolbarButtonVariant.READER_MODE, true);
    }

    @Test
    @EnableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP)
    public void testActionAlwaysAvailableInReadingMode() {
        var provider = new ReaderModeActionProvider(mButtonVisibilitySupplier);

        setReaderModeBackendSignal(false);
        provider.getAction(mMockTab, mMockSignalAccumulator);
        ShadowLooper.idleMainLooper();

        verify(mMockSignalAccumulator).setSignal(AdaptiveToolbarButtonVariant.READER_MODE, false);

        when(mMockTab.getUrl()).thenReturn(TEST_DISTILLER_URL);
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(true);
        provider.getAction(mMockTab, mMockSignalAccumulator);
        ShadowLooper.idleMainLooper();

        verify(mMockSignalAccumulator).setSignal(AdaptiveToolbarButtonVariant.READER_MODE, true);
    }

    @Test
    @EnableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP + ":show_cpa/false")
    public void testActionNotVisibleWhenParamDisabled() {
        when(mReaderModeActionRateLimiter.isActionSuppressed()).thenReturn(true);
        var provider = new ReaderModeActionProvider(mButtonVisibilitySupplier);
        provider.getAction(mMockTab, mMockSignalAccumulator);
        verify(mMockSignalAccumulator).setSignal(AdaptiveToolbarButtonVariant.READER_MODE, false);
    }
}
