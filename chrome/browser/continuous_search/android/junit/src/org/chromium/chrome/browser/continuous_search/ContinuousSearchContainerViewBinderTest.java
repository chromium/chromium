// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;

import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Test of {@link ContinuousSearchContainerViewBinder}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ContinuousSearchContainerViewBinderTest {
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mModel = new PropertyModel(ContinuousSearchContainerProperties.ALL_KEYS);
    }

    /**
     * Tests that property updates to the Java view work correctly.
     */
    @Test
    public void testBindJavaView() {
        View view = mock(View.class);
        InOrder inOrder = inOrder(view);
        PropertyModelChangeProcessor.create(
                mModel, view, ContinuousSearchContainerViewBinder::bindJavaView);

        final int yValue = 10;
        mModel.set(ContinuousSearchContainerProperties.VERTICAL_OFFSET, yValue);
        inOrder.verify(view).setY((float) yValue);

        final int visibility = View.VISIBLE;
        mModel.set(ContinuousSearchContainerProperties.ANDROID_VIEW_VISIBILITY, visibility);
        inOrder.verify(view).setVisibility(visibility);
    }

    /**
     * Tests that property updates to the composited view work correctly.
     */
    @Test
    public void testBindCompositedView() {
        ContinuousSearchSceneLayer sceneLayer = mock(ContinuousSearchSceneLayer.class);
        InOrder inOrder = inOrder(sceneLayer);

        final int yValue = 10;
        mModel.set(ContinuousSearchContainerProperties.VERTICAL_OFFSET, yValue);
        final boolean visibility = true;
        mModel.set(ContinuousSearchContainerProperties.COMPOSITED_VIEW_VISIBLE, visibility);

        ContinuousSearchContainerViewBinder.bindCompositedView(
                mModel, sceneLayer, ContinuousSearchContainerProperties.COMPOSITED_VIEW_VISIBLE);
        inOrder.verify(sceneLayer).setVerticalOffset(yValue);
        inOrder.verify(sceneLayer).setIsVisible(visibility);
    }
}
