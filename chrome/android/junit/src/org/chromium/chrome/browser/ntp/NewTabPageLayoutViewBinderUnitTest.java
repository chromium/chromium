// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp.NewTabPageLayoutProperties.COMPOSEPLATE_AND_MVT_CONTAINER_WIDTH_PX;
import static org.chromium.chrome.browser.ntp.NewTabPageLayoutProperties.DELEGATE;
import static org.chromium.chrome.browser.ntp.NewTabPageLayoutProperties.ON_LAYOUT_CHANGE_LISTENER;
import static org.chromium.chrome.browser.ntp.NewTabPageLayoutProperties.SEARCH_BOX_VIEW;
import static org.chromium.chrome.browser.ntp.NewTabPageLayoutProperties.TOP_INSET_PX;
import static org.chromium.chrome.browser.ntp.NewTabPageLayoutProperties.TRANSITION_Y;

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
import org.robolectric.Shadows;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link NewTabPageLayoutViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NewTabPageLayoutViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private NewTabPageLayout.Delegate mDelegate;

    private Context mContext;
    private NewTabPageLayout mView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mView = new NewTabPageLayout(mContext, null);
        mModel = new PropertyModel.Builder(NewTabPageLayoutProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(mModel, mView, NewTabPageLayoutViewBinder::bind);
    }

    @Test
    public void testDelegate() {
        mModel.set(DELEGATE, mDelegate);
        mView.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY));
        verify(mDelegate).onMeasure(100);
    }

    @Test
    public void testTopInset() {
        mView.setPaddingRelative(10, 0, 20, 30);

        int topInsetPx = 40;
        mModel.set(TOP_INSET_PX, topInsetPx);
        assertEquals(10, mView.getPaddingStart());
        assertEquals(topInsetPx, mView.getPaddingTop());
        assertEquals(20, mView.getPaddingEnd());
        assertEquals(30, mView.getPaddingBottom());
    }

    @Test
    public void testSearchBoxViewAndTransitionY() {
        View aboveView = new View(mContext);
        View searchBoxView = new View(mContext);
        View belowView = new View(mContext);
        mView.addView(aboveView);
        mView.addView(searchBoxView);
        mView.addView(belowView);

        mModel.set(SEARCH_BOX_VIEW, searchBoxView);
        float transitionY = 100.1f;
        mModel.set(TRANSITION_Y, transitionY);

        // Only the views up to and including the search box should be translated.
        assertEquals(transitionY, aboveView.getTranslationY(), 0f);
        assertEquals(transitionY, searchBoxView.getTranslationY(), 0f);
        assertEquals(0f, belowView.getTranslationY(), 0f);
    }

    @Test
    public void testComposeplateAndMvtContainerWidth() {
        View containerView = new View(mContext);
        containerView.setId(R.id.composeplate_and_mvt_container);
        mView.addView(containerView);

        int widthPx = 500;
        mModel.set(COMPOSEPLATE_AND_MVT_CONTAINER_WIDTH_PX, widthPx);
        assertEquals(widthPx, containerView.getLayoutParams().width);
    }

    @Test
    public void testOnLayoutChangeListener() {
        View.OnLayoutChangeListener listener = mock(View.OnLayoutChangeListener.class);
        mModel.set(ON_LAYOUT_CHANGE_LISTENER, listener);
        assertTrue(Shadows.shadowOf(mView).getOnLayoutChangeListeners().contains(listener));
        assertEquals(listener, mView.getTag(R.id.ntp_view_layout_change_listener_tag));

        mModel.set(ON_LAYOUT_CHANGE_LISTENER, null);
        assertTrue(Shadows.shadowOf(mView).getOnLayoutChangeListeners().isEmpty());
        assertNull(mView.getTag(R.id.ntp_view_layout_change_listener_tag));
    }
}
