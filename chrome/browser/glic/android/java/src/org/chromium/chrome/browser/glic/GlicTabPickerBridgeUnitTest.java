// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Intent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.browser_ui.util.ChromeItemPickerExtras;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.IntentCallback;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Unit tests for {@link GlicTabPickerBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlicTabPickerBridgeUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WindowAndroid mWindowAndroidMock;
    @Mock private Activity mActivityMock;
    @Mock private TabModelSelector mTabModelSelectorMock;
    @Mock private Tab mTab1Mock;
    @Mock private Tab mTab2Mock;
    @Mock private Tab mTab3Mock;
    @Mock private Callback<List<Tab>> mCallbackMock;

    @Captor private ArgumentCaptor<Intent> mIntentCaptor;
    @Captor private ArgumentCaptor<IntentCallback> mIntentCallbackCaptor;
    @Captor private ArgumentCaptor<List<Tab>> mResultCaptor;

    @Before
    public void setUp() {
        when(mWindowAndroidMock.getActivity()).thenReturn(new WeakReference<>(mActivityMock));
        when(mTab1Mock.getId()).thenReturn(30);
        when(mTab2Mock.getId()).thenReturn(40);
        when(mTab3Mock.getId()).thenReturn(50);
        when(mTabModelSelectorMock.getTabById(30)).thenReturn(mTab1Mock);
        when(mTabModelSelectorMock.getTabById(40)).thenReturn(mTab2Mock);
        when(mTabModelSelectorMock.getTabById(50)).thenReturn(mTab3Mock);
        TabModelSelectorSupplier.setInstanceForTesting(mTabModelSelectorMock);
    }

    @Test
    public void testOpenTabPicker_NullWindowAndroid() {
        GlicTabPickerBridge.openTabPicker(null, Collections.emptyList(), mCallbackMock);
        verify(mCallbackMock).onResult(isNull());
    }

    @Test
    public void testOpenTabPicker_NullActivity() {
        when(mWindowAndroidMock.getActivity()).thenReturn(new WeakReference<>(null));
        GlicTabPickerBridge.openTabPicker(
                mWindowAndroidMock, Collections.emptyList(), mCallbackMock);
        verify(mCallbackMock).onResult(isNull());
    }

    @Test
    public void testOpenTabPicker_ShowIntentFailed() {
        when(mWindowAndroidMock.showIntent(any(Intent.class), any(), isNull())).thenReturn(false);

        GlicTabPickerBridge.openTabPicker(mWindowAndroidMock, List.of(mTab1Mock), mCallbackMock);
        verify(mCallbackMock).onResult(isNull());
    }

    @Test
    public void testOpenTabPicker_SuccessPath() {
        when(mWindowAndroidMock.showIntent(
                        mIntentCaptor.capture(), mIntentCallbackCaptor.capture(), isNull()))
                .thenReturn(true);

        List<Tab> alreadySelected = List.of(mTab1Mock, mTab2Mock);

        GlicTabPickerBridge.openTabPicker(mWindowAndroidMock, alreadySelected, mCallbackMock);

        Intent launchedIntent = mIntentCaptor.getValue();
        ArrayList<Integer> capturedPreselected =
                launchedIntent.getIntegerArrayListExtra(
                        ChromeItemPickerExtras.EXTRA_PRESELECTED_TAB_IDS);

        assertArrayEquals(new Integer[] {30, 40}, capturedPreselected.toArray(new Integer[0]));

        // Simulate successful tab selection result
        Intent resultIntent = new Intent();
        ArrayList<Integer> selectedResult = new ArrayList<>();
        selectedResult.add(30);
        selectedResult.add(50);
        resultIntent.putIntegerArrayListExtra(
                ChromeItemPickerExtras.EXTRA_ATTACHMENT_TAB_IDS, selectedResult);

        mIntentCallbackCaptor.getValue().onIntentCompleted(Activity.RESULT_OK, resultIntent);

        verify(mCallbackMock).onResult(mResultCaptor.capture());
        List<Tab> resultTabs = mResultCaptor.getValue();
        assertEquals(2, resultTabs.size());
        assertEquals(mTab1Mock, resultTabs.get(0));
        assertEquals(mTab3Mock, resultTabs.get(1));
    }

    @Test
    public void testOpenTabPicker_Canceled() {
        when(mWindowAndroidMock.showIntent(
                        any(Intent.class), mIntentCallbackCaptor.capture(), isNull()))
                .thenReturn(true);

        GlicTabPickerBridge.openTabPicker(
                mWindowAndroidMock, Collections.emptyList(), mCallbackMock);

        mIntentCallbackCaptor.getValue().onIntentCompleted(Activity.RESULT_CANCELED, null);

        verify(mCallbackMock).onResult(isNull());
    }

    @Test
    public void testOpenTabPicker_DeselectAllTabs() {
        when(mWindowAndroidMock.showIntent(
                        mIntentCaptor.capture(), mIntentCallbackCaptor.capture(), isNull()))
                .thenReturn(true);

        List<Tab> alreadySelected = List.of(mTab1Mock, mTab2Mock);

        GlicTabPickerBridge.openTabPicker(mWindowAndroidMock, alreadySelected, mCallbackMock);

        // Simulate confirming with no tabs selected
        Intent resultIntent = new Intent();
        resultIntent.putIntegerArrayListExtra(
                ChromeItemPickerExtras.EXTRA_ATTACHMENT_TAB_IDS, new ArrayList<>());

        mIntentCallbackCaptor.getValue().onIntentCompleted(Activity.RESULT_OK, resultIntent);

        verify(mCallbackMock).onResult(mResultCaptor.capture());
        List<Tab> resultTabs = mResultCaptor.getValue();
        assertEquals(0, resultTabs.size());
    }
}
