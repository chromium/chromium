// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import static org.mockito.Mockito.verify;

import android.view.ViewStub;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link ActorOverlayCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ActorOverlayCoordinatorTest {
    @Mock private ViewStub mViewStub;
    @Mock private ActorOverlayView mView;

    private ActorOverlayCoordinator mCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Mockito.when(mViewStub.inflate()).thenReturn(mView);

        mCoordinator = new ActorOverlayCoordinator(mViewStub);
    }

    @Test
    public void testConstruction() {
        Assert.assertNotNull(mCoordinator.getMediator());
        Assert.assertEquals(mView, mCoordinator.getView());
        verify(mViewStub).inflate();
    }

    @Test
    public void testVisibility() {
        ActorOverlayMediator mediator = mCoordinator.getMediator();
        mediator.setOverlayVisible(true);
        verify(mView).setVisible(true);

        mediator.setOverlayVisible(false);
        verify(mView).setVisible(false);
    }
}
