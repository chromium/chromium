// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_suggestion.toolbar.GroupSuggestionsButtonController;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;
import java.util.Map;

/** Unit tests for {@link TabGroupingActionProvider} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupingActionProviderTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Activity mActivity;
    @Mock private TabWindowManager mTabWindowManager;

    @Mock private GroupSuggestionsButtonController mController;
    private ObservableSupplierImpl<GroupSuggestionsButtonController> mControllerSupplier;

    private static final int WINDOW_ID = 1234;

    @Before
    public void setUp() throws Exception {
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        final Context context = RuntimeEnvironment.getApplication();
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(context));
        mControllerSupplier = new ObservableSupplierImpl<>(mController);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testGetAction() {
        when(mController.shouldShowButton(any(), anyInt())).thenReturn(false);
        when(mTabWindowManager.getIdForWindow(mActivity)).thenReturn(WINDOW_ID);

        var provider = new TabGroupingActionProvider(mControllerSupplier);
        var signalAccumulator =
                new SignalAccumulator(
                        new Handler(),
                        mTab,
                        Map.of(AdaptiveToolbarButtonVariant.TAB_GROUPING, provider));
        provider.getAction(mTab, signalAccumulator);
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mController).shouldShowButton(any(), eq(WINDOW_ID));
        assertFalse(signalAccumulator.getSignal(AdaptiveToolbarButtonVariant.TAB_GROUPING));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testGetAction_notShownOnTablets() {
        when(mController.shouldShowButton(any(), anyInt())).thenReturn(false);
        when(mTabWindowManager.getIdForWindow(mActivity)).thenReturn(WINDOW_ID);

        var provider = new TabGroupingActionProvider(mControllerSupplier);
        var signalAccumulator =
                new SignalAccumulator(
                        new Handler(),
                        mTab,
                        Map.of(AdaptiveToolbarButtonVariant.TAB_GROUPING, provider));
        provider.getAction(mTab, signalAccumulator);
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mController, never()).shouldShowButton(any(), eq(WINDOW_ID));
        assertFalse(signalAccumulator.getSignal(AdaptiveToolbarButtonVariant.TAB_GROUPING));
    }
}
