// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import static android.os.Looper.getMainLooper;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.os.Handler;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.dom_distiller.DomDistillerTabUtils;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.dom_distiller.TabDistillabilityProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.segmentation_platform.ContextualPageActionController.ActionProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Unit tests for {@link ReaderModeActionProvider}
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.CONTEXTUAL_PAGE_ACTIONS})
public class ReaderModeActionProviderTest {
    @Rule
    public final TestRule mFeatureProcessor = new Features.JUnitProcessor();

    @Mock
    private Tab mMockTab;
    @Mock
    private ReaderModeManager mMockReaderModeManager;
    @Mock
    private SignalAccumulator mMockSignalAccumulator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        initializeReaderModeBackend();

        mMockTab.getUserDataHost().setUserData(
                ReaderModeManager.USER_DATA_KEY, mMockReaderModeManager);
        doReturn(false).when(mMockReaderModeManager).isReaderModeUiRateLimited();
    }

    private void initializeReaderModeBackend() {
        UserDataHost userDataHost = new UserDataHost();
        when(mMockTab.getUserDataHost()).thenReturn(userDataHost);
        TabDistillabilityProvider.createForTab(mMockTab);
        DomDistillerTabUtils.setExcludeMobileFriendlyForTesting(true);
    }

    private void setReaderModeBackendSignal(boolean isDistillable) {
        TabDistillabilityProvider tabDistillabilityProvider =
                TabDistillabilityProvider.get(mMockTab);
        tabDistillabilityProvider.onIsPageDistillableResult(isDistillable, true, false);
    }

    @Test
    public void testIsDistillableInvokesCallback() throws TimeoutException {
        List<ActionProvider> providers = new ArrayList<>();
        ReaderModeActionProvider provider = new ReaderModeActionProvider();
        providers.add(provider);
        SignalAccumulator accumulator = new SignalAccumulator(new Handler(), mMockTab, providers);
        setReaderModeBackendSignal(true);
        provider.getAction(mMockTab, accumulator);
        Assert.assertTrue(accumulator.hasReaderMode());
    }

    @Test
    public void testProviderDelaysSettingOnShown() throws TimeoutException {
        TestValues testValues = new TestValues();
        testValues.addFieldTrialParamOverride(ChromeFeatureList.CONTEXTUAL_PAGE_ACTIONS,
                "reader_mode_session_rate_limiting", "true");
        FeatureList.setTestValues(testValues);
        when(mMockReaderModeManager.isReaderModeUiRateLimited()).thenReturn(false);

        ReaderModeActionProvider provider = new ReaderModeActionProvider();

        // Call onActionShown and wait 10 milliseconds.
        provider.onActionShown(mMockTab, AdaptiveToolbarButtonVariant.READER_MODE);
        shadowOf(getMainLooper()).idleFor(10, TimeUnit.MILLISECONDS);

        // ReaderModeManager shouldn't be notified yet.
        verify(mMockReaderModeManager, never()).setReaderModeUiShown();
    }

    @Test
    public void testProviderSetsOnShownAfterDelay() throws TimeoutException {
        TestValues testValues = new TestValues();
        testValues.addFieldTrialParamOverride(ChromeFeatureList.CONTEXTUAL_PAGE_ACTIONS,
                "reader_mode_session_rate_limiting", "true");
        FeatureList.setTestValues(testValues);
        when(mMockReaderModeManager.isReaderModeUiRateLimited()).thenReturn(false);

        ReaderModeActionProvider provider = new ReaderModeActionProvider();

        // Call onActionShown and wait 5 seconds.
        provider.onActionShown(mMockTab, AdaptiveToolbarButtonVariant.READER_MODE);
        shadowOf(getMainLooper()).idleFor(5, TimeUnit.SECONDS);

        // ReaderModeManager should have been notified.
        verify(mMockReaderModeManager).setReaderModeUiShown();
    }
}
