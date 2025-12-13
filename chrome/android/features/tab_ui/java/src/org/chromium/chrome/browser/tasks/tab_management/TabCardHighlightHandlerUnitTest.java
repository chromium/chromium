// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabCardHighlightState;

/** Unit tests for {@link TabCardHighlightHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabCardHighlightHandlerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View mCardWrapper;

    private Context mContext;
    private TabCardHighlightHandler mManager;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        when(mCardWrapper.getContext()).thenReturn(mContext);
        mManager = new TabCardHighlightHandler(mCardWrapper);
    }

    @Test
    public void testMaybeAnimateForHighlightState_Highlighted() {
        mManager.maybeAnimateForHighlightState(
                TabCardHighlightState.HIGHLIGHTED, /* isIncognito= */ false);

        verify(mCardWrapper).setVisibility(View.VISIBLE);
        verify(mCardWrapper).setBackground(any());
    }

    @Test
    public void testMaybeAnimateForHighlightState_ToBeHighlighted() {
        mManager.maybeAnimateForHighlightState(
                TabCardHighlightState.TO_BE_HIGHLIGHTED, /* isIncognito= */ false);

        ShadowLooper.runUiThreadTasks();

        verify(mCardWrapper).setBackground(any());
    }

    @Test
    public void testMaybeAnimateForHighlightState_NotHighlighted() {
        mManager.maybeAnimateForHighlightState(
                TabCardHighlightState.NOT_HIGHLIGHTED, /* isIncognito= */ false);

        ShadowLooper.runUiThreadTasks();

        verify(mCardWrapper).setBackground(null);
        verify(mCardWrapper, atLeastOnce()).setAlpha(1f);
    }

    @Test
    public void testClearHighlight() {
        mManager.clearHighlight();

        verify(mCardWrapper).setBackground(null);
        verify(mCardWrapper).setVisibility(View.GONE);
    }
}
