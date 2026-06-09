// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.util.SizeF;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.media.immersive_playback.ImmersiveVideoFormatRadioGroup;
import org.chromium.chrome.browser.modules.xr.R;
import org.chromium.content_public.browser.ImmersiveProjectionType;
import org.chromium.content_public.browser.ImmersiveStereoMode;
import org.chromium.ui.xr.scenecore.XrEntityHolder;
import org.chromium.ui.xr.scenecore.XrPanelEntityHolder;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionManager;

/** Tests for {@link ImmersiveVideoFormatCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImmersiveVideoFormatCoordinatorTest {
    @Mock private XrSceneCoreSessionManager mSessionManager;
    @Mock private ImmersiveVideoFormatCoordinator.Delegate mDelegate;
    @Mock private XrPanelEntityHolder<?> mHolder;
    @Mock private XrEntityHolder<?> mParentEntity;
    @Mock private ImmersiveVideoFormatView mFormatView;
    @Mock private ImmersiveVideoFormatRadioGroup mFormatRadioGroup;

    private Activity mActivity;
    private ImmersiveVideoFormatCoordinator mCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).create().get();

        when(mSessionManager.createPanelEntity(any(), any())).thenReturn(mHolder);
        when(mFormatView.findViewById(R.id.format_radio_group)).thenReturn(mFormatRadioGroup);

        mCoordinator =
                new TestImmersiveVideoFormatCoordinator(
                        mActivity, mSessionManager, mDelegate, mFormatView);
    }

    private static class TestImmersiveVideoFormatCoordinator
            extends ImmersiveVideoFormatCoordinator {
        private final ImmersiveVideoFormatView mMockView;

        public TestImmersiveVideoFormatCoordinator(
                Activity activity,
                XrSceneCoreSessionManager sessionManager,
                Delegate delegate,
                ImmersiveVideoFormatView mockView) {
            super(activity, sessionManager, delegate);
            mMockView = mockView;
        }

        @Override
        ImmersiveVideoFormatView createView() {
            return mMockView;
        }
    }

    @Test
    public void testCreate() {
        assertNotNull(mCoordinator);
        assertFalse(mCoordinator.isShowing());
    }

    @Test
    public void testShow_InitializesAndSetsParent() {
        doReturn(mParentEntity).when(mHolder).getParent();

        mCoordinator.show(
                mParentEntity,
                new SizeF(1f, 1f),
                ImmersiveStereoMode.MONO,
                ImmersiveProjectionType.QUAD);

        assertTrue(mCoordinator.isShowing());
        verify(mHolder).setParent(mParentEntity);
        verify(mHolder).setEntityEnabled(true);
    }

    @Test
    public void testDismiss_HidesAndDetaches() {
        mCoordinator.show(
                mParentEntity,
                new SizeF(1f, 1f),
                ImmersiveStereoMode.MONO,
                ImmersiveProjectionType.QUAD);
        doReturn(null).when(mHolder).getParent();
        mCoordinator.dismiss();

        assertFalse(mCoordinator.isShowing());
        verify(mHolder).setEntityEnabled(false);
        verify(mHolder).setParent(null);
    }

    @Test
    public void testDispose_DisposesHolder() {
        mCoordinator.show(
                mParentEntity,
                new SizeF(1f, 1f),
                ImmersiveStereoMode.MONO,
                ImmersiveProjectionType.QUAD);
        mCoordinator.dispose();

        verify(mHolder).dispose();
    }
}
