// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.R;

/** Unit tests for {@link NtpThemeCollectionsUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpThemeCollectionsUtilsUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private GridLayoutManager mGridLayoutManager;
    @Mock private Callback<Configuration> mMockConfigurationCallback;

    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void testRegisterOrientationListener() {
        // Spy the context to capture the registered listener.
        Context spyContext = spy(mContext);
        ArgumentCaptor<ComponentCallbacks> componentCallbacksCaptor =
                ArgumentCaptor.forClass(ComponentCallbacks.class);

        // Call the method under test.
        ComponentCallbacks returnedCallbacks =
                NtpThemeCollectionsUtils.registerOrientationListener(
                        spyContext, mMockConfigurationCallback);

        // Verify that a listener was registered and capture it.
        verify(spyContext).registerComponentCallbacks(componentCallbacksCaptor.capture());
        ComponentCallbacks capturedCallbacks = componentCallbacksCaptor.getValue();
        assertEquals(returnedCallbacks, capturedCallbacks);

        // Create a new configuration and trigger the callback.
        Configuration newConfig = new Configuration();
        capturedCallbacks.onConfigurationChanged(newConfig);

        // Verify that our mock callback was called with the new configuration.
        verify(mMockConfigurationCallback).onResult(newConfig);
    }

    @Test
    public void testUpdateSpanCount() {
        NtpThemeCollectionsUtils.updateSpanCount(mGridLayoutManager, 400, 180, 20);
        verify(mGridLayoutManager).setSpanCount(3);

        NtpThemeCollectionsUtils.updateSpanCount(mGridLayoutManager, 410, 180, 20);
        verify(mGridLayoutManager, times(2)).setSpanCount(3);

        NtpThemeCollectionsUtils.updateSpanCount(mGridLayoutManager, 100, 180, 20);
        verify(mGridLayoutManager).setSpanCount(1);
    }

    @Test
    public void testUpdateSpanCountOnLayoutChange() {
        RecyclerView recyclerView = new RecyclerView(mContext);
        RecyclerView recyclerViewSpy = spy(recyclerView);
        ViewTreeObserver viewTreeObserver = recyclerView.getViewTreeObserver();
        ViewTreeObserver viewTreeObserverSpy = spy(viewTreeObserver);
        doReturn(viewTreeObserverSpy).when(recyclerViewSpy).getViewTreeObserver();

        GridLayoutManager layoutManager = new GridLayoutManager(mContext, 1);
        GridLayoutManager layoutManagerSpy = spy(layoutManager);
        recyclerViewSpy.setLayoutManager(layoutManagerSpy);

        NtpThemeCollectionsUtils.updateSpanCountOnLayoutChange(
                layoutManagerSpy, recyclerViewSpy, 180, 20);

        ArgumentCaptor<OnGlobalLayoutListener> listenerCaptor =
                ArgumentCaptor.forClass(ViewTreeObserver.OnGlobalLayoutListener.class);
        verify(viewTreeObserverSpy).addOnGlobalLayoutListener(listenerCaptor.capture());
        OnGlobalLayoutListener listener = listenerCaptor.getValue();

        // 1. Not shown, should do nothing.
        doReturn(false).when(recyclerViewSpy).isShown();
        recyclerViewSpy.measure(
                View.MeasureSpec.makeMeasureSpec(410, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(800, View.MeasureSpec.EXACTLY));
        recyclerViewSpy.layout(0, 0, 410, 800);
        listener.onGlobalLayout();

        // 2. Shown but width is 0, should do nothing.
        doReturn(true).when(recyclerViewSpy).isShown();
        recyclerViewSpy.measure(
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(800, View.MeasureSpec.EXACTLY));
        recyclerViewSpy.layout(0, 0, 0, 800);
        listener.onGlobalLayout();

        // At this point, no update should have happened.
        verify(layoutManagerSpy, never()).setSpanCount(anyInt());
        verify(viewTreeObserverSpy, never()).removeOnGlobalLayoutListener(any());

        // 3. Shown and has width, should update span count and remove listener.
        recyclerViewSpy.measure(
                View.MeasureSpec.makeMeasureSpec(410, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(800, View.MeasureSpec.EXACTLY));
        recyclerViewSpy.layout(0, 0, 410, 800);
        listener.onGlobalLayout();
        verify(layoutManagerSpy).setSpanCount(3);
        verify(viewTreeObserverSpy).removeOnGlobalLayoutListener(listener);
    }
}
