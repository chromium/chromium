// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import static org.mockito.AdditionalMatchers.gt;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.when;

import android.graphics.drawable.GradientDrawable;
import android.view.View;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/**
 * Test of {@link ContinuousSearchContainerViewBinder}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ContinuousSearchListViewBinderTest {
    @Mock
    private UrlUtilities.Natives mUrlUtilitiesJniMock;

    private PropertyModel mModel;

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Before
    public void setUp() {
        mModel = new PropertyModel(ContinuousSearchListProperties.ITEM_KEYS);
        mJniMocker.mock(UrlUtilitiesJni.TEST_HOOKS, mUrlUtilitiesJniMock);
    }

    @Test
    public void testBindListItem() {
        View view = mock(View.class);
        TextView textView = mock(TextView.class);
        GradientDrawable drawable = mock(GradientDrawable.class);
        InOrder inOrder = inOrder(view, textView, drawable);
        PropertyModelChangeProcessor.create(
                mModel, view, ContinuousSearchListViewBinder::bindListItem);

        doReturn(textView).when(view).findViewById(anyInt());

        final String label = "Label";
        mModel.set(ContinuousSearchListProperties.LABEL, label);
        inOrder.verify(textView).setText(eq(label));

        GURL url = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);
        final String domain = "example.com";
        doReturn(domain)
                .when(mUrlUtilitiesJniMock)
                .getDomainAndRegistry(eq(url.getSpec()), anyBoolean());
        mModel.set(ContinuousSearchListProperties.URL, url);
        inOrder.verify(textView).setText(eq(domain));

        int color = 0xAABBCC;
        mModel.set(ContinuousSearchListProperties.IS_SELECTED, false);
        inOrder.verify(view).getBackground();
        mModel.set(ContinuousSearchListProperties.BORDER_COLOR, color);
        inOrder.verify(view).getBackground();
        when(view.getBackground()).thenReturn(drawable);
        mModel.set(ContinuousSearchListProperties.IS_SELECTED, true);
        inOrder.verify(view, times(2)).getBackground();
        inOrder.verify(drawable).mutate();
        inOrder.verify(drawable).setStroke(gt(0), eq(color));

        View.OnClickListener listener = (v) -> {};
        mModel.set(ContinuousSearchListProperties.CLICK_LISTENER, listener);
        inOrder.verify(view).setOnClickListener(eq(listener));

        color = 0xCCBBAA;
        mModel.set(ContinuousSearchListProperties.BACKGROUND_COLOR, color);
        inOrder.verify(view, times(2)).getBackground();
        inOrder.verify(drawable).mutate();
        inOrder.verify(drawable).setColor(color);

        int id = 90;
        mModel.set(ContinuousSearchListProperties.TITLE_TEXT_STYLE, id);
        inOrder.verify(textView).setTextAppearance(any(), eq(id));

        id = 67;
        mModel.set(ContinuousSearchListProperties.DESCRIPTION_TEXT_STYLE, id);
        inOrder.verify(textView).setTextAppearance(any(), eq(id));
    }
}
