// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp.NewTabPageLayoutProperties.DELEGATE;
import static org.chromium.chrome.browser.ntp.NewTabPageLayoutProperties.ON_LAYOUT_CHANGE_LISTENER;
import static org.chromium.chrome.browser.ntp.NewTabPageLayoutProperties.SEARCH_BOX_VIEW;
import static org.chromium.chrome.browser.ntp.NewTabPageLayoutProperties.TOP_INSET_PX;
import static org.chromium.chrome.browser.ntp.NewTabPageLayoutProperties.TRANSITION_Y;

import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link NewTabPageLayoutViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NewTabPageLayoutViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private NewTabPageLayout mView;
    @Mock private NewTabPageLayout.Delegate mDelegate;

    private PropertyModel mModel;

    @Before
    public void setUp() {
        mModel = new PropertyModel.Builder(NewTabPageLayoutProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(mModel, mView, NewTabPageLayoutViewBinder::bind);
    }

    @Test
    public void testDelegate() {
        mModel.set(DELEGATE, mDelegate);
        verify(mView).setDelegate(eq(mDelegate));
    }

    @Test
    public void testTopInset() {
        when(mView.getPaddingStart()).thenReturn(10);
        when(mView.getPaddingEnd()).thenReturn(20);
        when(mView.getPaddingBottom()).thenReturn(30);

        int topInsetPx = 40;
        mModel.set(TOP_INSET_PX, topInsetPx);
        verify(mView).setPaddingRelative(10, topInsetPx, 20, 30);
    }

    @Test
    public void testSearchBoxView() {
        View searchBoxView = mock(View.class);
        mModel.set(SEARCH_BOX_VIEW, searchBoxView);
        verify(mView).setSearchBoxView(eq(searchBoxView));
    }

    @Test
    public void testTransitionY() {
        float transitionY = 100.1f;
        mModel.set(TRANSITION_Y, transitionY);
        verify(mView).setTranslationYOfFakeboxAndAbove(eq(transitionY));
    }

    @Test
    public void testOnLayoutChangeListener() {
        View.OnLayoutChangeListener listener = mock(View.OnLayoutChangeListener.class);
        mModel.set(ON_LAYOUT_CHANGE_LISTENER, listener);
        verify(mView).addOnLayoutChangeListener(eq(listener));

        when(mView.getTag(R.id.ntp_view_layout_change_listener_tag)).thenReturn(listener);
        clearInvocations(mView);
        mModel.set(ON_LAYOUT_CHANGE_LISTENER, null);
        verify(mView).removeOnLayoutChangeListener(eq(listener));
        verify(mView, never()).addOnLayoutChangeListener(any(View.OnLayoutChangeListener.class));
    }
}
