// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.compositor.scene_layer.SceneLayer;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Unit tests for {@link CompositorModelChangeProcessor}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CompositorModelChangeProcessorUnitTest {
    private static final PropertyModel.WritableBooleanPropertyKey PROPERTY_CHANGED =
            new PropertyModel.WritableBooleanPropertyKey();

    @Mock
    private SceneLayer mView;
    @Mock
    private PropertyModelChangeProcessor.ViewBinder mViewBinder;
    @Mock
    private LayoutManagerHost mLayoutManagerHost;

    private CompositorModelChangeProcessor.FrameRequestSupplier mFrameSupplier;
    private CompositorModelChangeProcessor mCompositorMCP;
    private PropertyModel mModel;
    private AtomicBoolean mPropertyChangedValue = new AtomicBoolean(false);

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mFrameSupplier =
                new CompositorModelChangeProcessor.FrameRequestSupplier(mLayoutManagerHost);
        mModel = new PropertyModel(PROPERTY_CHANGED);

        mCompositorMCP = CompositorModelChangeProcessor.create(
                mModel, mView, mViewBinder, mFrameSupplier, false);
    }

    @Test
    public void testBindAndRequestFrame() {
        mModel.set(PROPERTY_CHANGED, mPropertyChangedValue.getAndSet(!mPropertyChangedValue.get()));
        verify(mLayoutManagerHost).requestRender();

        mFrameSupplier.set(System.currentTimeMillis());
        verify(mViewBinder).bind(eq(mModel), eq(mView), eq(null));
    }

    @Test
    public void testBindAndNoRequestFrame() {
        mFrameSupplier.set(System.currentTimeMillis());

        verify(mViewBinder).bind(eq(mModel), eq(mView), eq(null));
        verify(mLayoutManagerHost, never()).requestRender();
    }

    @Test
    public void testRequestFrameAndNoBindOnPropertyChanged() {
        mModel.set(PROPERTY_CHANGED, mPropertyChangedValue.getAndSet(!mPropertyChangedValue.get()));

        verify(mLayoutManagerHost).requestRender();
        verify(mViewBinder, never()).bind(any(), any(), any());
    }
}