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
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.dom_distiller.DistillerHeuristicsType;
import org.chromium.chrome.browser.dom_distiller.DomDistillerTabUtils;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.dom_distiller.TabDistillabilityProvider;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.segmentation_platform.ContextualPageActionController.ActionProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.components.ukm.UkmRecorderJni;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;

import java.util.HashMap;
import java.util.concurrent.TimeoutException;

/** Unit tests for {@link ReaderModeActionProvider} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ReaderModeActionProviderTest {

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

    @Before
    public void setUp() {
        initializeReaderModeBackend();
        UkmRecorderJni.setInstanceForTesting(mUkmRecorderJniMock);

        mMockTab.getUserDataHost()
                .setUserData(ReaderModeManager.USER_DATA_KEY, mMockReaderModeManager);
        when(mMockTab.getWebContents()).thenReturn(mMockWebContents);
        when(mMockWebContents.getNavigationController()).thenReturn(mMockNavigationController);
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
        tabDistillabilityProvider.onIsPageDistillableResult(isDistillable, true, false, false);
    }

    @Test
    public void testIsDistillableInvokesCallback() throws TimeoutException {
        HashMap<Integer, ActionProvider> providers = new HashMap<>();
        ReaderModeActionProvider provider = new ReaderModeActionProvider(() -> true);
        providers.put(AdaptiveToolbarButtonVariant.READER_MODE, provider);
        SignalAccumulator accumulator = new SignalAccumulator(new Handler(), mMockTab, providers);
        setReaderModeBackendSignal(true);
        provider.getAction(mMockTab, accumulator);
        ShadowLooper.idleMainLooper();

        Assert.assertTrue(accumulator.getSignal(AdaptiveToolbarButtonVariant.READER_MODE));
    }

    @Test
    public void testWaitForDistillabilityResult() throws TimeoutException {
        ReaderModeActionProvider provider = new ReaderModeActionProvider(() -> true);
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
        ReaderModeActionProvider provider = new ReaderModeActionProvider(() -> true);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                ReaderModeActionProvider
                                        .SIGNAL_ACCUMULATOR_WITHIN_TIMEOUT_HISTOGRAM,
                                true)
                        .expectBooleanRecord(
                                ReaderModeActionProvider
                                        .SIGNAL_ACCUMULATOR_DISTILLABLE_WITHIN_TIMEOUT_HISTOGRAM,
                                true)
                        .expectAnyRecord(ReaderModeActionProvider.READER_MODE_SIGNAL_TIME_HISTOGRAM)
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
        ReaderModeActionProvider provider = new ReaderModeActionProvider(() -> true);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                ReaderModeActionProvider
                                        .SIGNAL_ACCUMULATOR_WITHIN_TIMEOUT_HISTOGRAM,
                                false)
                        .expectBooleanRecord(
                                ReaderModeActionProvider
                                        .SIGNAL_ACCUMULATOR_DISTILLABLE_WITHIN_TIMEOUT_HISTOGRAM,
                                false)
                        .expectAnyRecord(ReaderModeActionProvider.READER_MODE_SIGNAL_TIME_HISTOGRAM)
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

        ReaderModeActionProvider provider = new ReaderModeActionProvider(() -> true);

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

        ReaderModeActionProvider provider = new ReaderModeActionProvider(() -> true);

        setReaderModeBackendSignal(true);
        provider.getAction(mMockTab, mMockSignalAccumulator);
        ShadowLooper.idleMainLooper();

        verify(mMockSignalAccumulator).setSignal(AdaptiveToolbarButtonVariant.READER_MODE, true);
    }

    @Test
    public void testReaderModeManagerNoUpdateUiShown() {
        ReaderModeActionProvider provider = new ReaderModeActionProvider(() -> true);
        provider.onActionShown(mMockTab, AdaptiveToolbarButtonVariant.READER_MODE);
        shadowOf(Looper.getMainLooper()).runOneTask();
        verify(mMockReaderModeManager).onContextualPageActionShown(true);
        clearInvocations(mMockReaderModeManager);

        // Ensure adaptive button UI is not visible.
        provider = new ReaderModeActionProvider(() -> false);
        provider.onActionShown(mMockTab, AdaptiveToolbarButtonVariant.READER_MODE);
        shadowOf(Looper.getMainLooper()).runOneTask();
        verify(mMockReaderModeManager).onContextualPageActionShown(false);
    }

    @Test
    public void testDestroy() {
        ReaderModeActionProvider provider = new ReaderModeActionProvider(() -> true);
        provider.getAction(mMockTab, mMockSignalAccumulator);
        provider.destroy();
        ShadowLooper.idleMainLooper();

        setReaderModeBackendSignal(true);
        verify(mMockSignalAccumulator, never())
                .setSignal(eq(AdaptiveToolbarButtonVariant.READER_MODE), anyBoolean());
    }
}
