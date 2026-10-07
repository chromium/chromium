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
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.media.immersive_playback.ImmersiveVideoFormatRadioGroup;
import org.chromium.chrome.browser.modules.xr.R;
import org.chromium.chrome.browser.xr.scenecore.XrModuleProviderImpl;
import org.chromium.chrome.browser.xr.scenecore.XrPixelDensityImpl;
import org.chromium.content_public.browser.ImmersiveProjectionType;
import org.chromium.content_public.browser.ImmersiveStereoMode;
import org.chromium.ui.xr.scenecore.XrEntityHolder;
import org.chromium.ui.xr.scenecore.XrPanelEntityHolder;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionManager;

/** Tests for {@link ImmersiveVideoFormatCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ImmersiveVideoFormatCoordinatorTest {
    private static class TestImmersiveVideoFormatCoordinator
            extends ImmersiveVideoFormatCoordinator {
        private final ImmersiveVideoFormatView mView;

        public TestImmersiveVideoFormatCoordinator(
                Activity activity,
                XrSceneCoreSessionManager sessionManager,
                Delegate delegate,
                ImmersiveVideoFormatView view) {
            super(activity, sessionManager, delegate);
            mView = view;
        }

        @Override
        ImmersiveVideoFormatView createView() {
            return mView;
        }
    }

    @Mock private XrSceneCoreSessionManager mSessionManager;
    @Mock private ImmersiveVideoFormatCoordinator.Delegate mDelegate;
    @Mock private XrPanelEntityHolder<?> mHolder;
    @Mock private XrEntityHolder<?> mParentEntity;
    private ImmersiveVideoFormatView mFormatView;

    private Activity mActivity;
    private ImmersiveVideoFormatCoordinator mCoordinator;

    @Before
    public void setUp() {
        XrModuleProviderImpl.initialize();
        MockitoAnnotations.openMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mFormatView = new ImmersiveVideoFormatView(mActivity);
        mActivity.setContentView(mFormatView);

        when(mSessionManager.createPanelEntity(any(), any())).thenReturn(mHolder);
        when(mSessionManager.getPixelDensity())
                .thenReturn(XrPixelDensityImpl.createForTesting(1000f, 1000f));

        mCoordinator =
                new TestImmersiveVideoFormatCoordinator(
                        mActivity, mSessionManager, mDelegate, mFormatView);
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

    @Test
    public void testRequestFocusForAccessibility() {
        mCoordinator.show(
                mParentEntity,
                new SizeF(1f, 1f),
                ImmersiveStereoMode.MONO,
                ImmersiveProjectionType.QUAD);
        ImmersiveVideoFormatRadioGroup radioGroup = mFormatView.getRadioGroup();
        View standardOption = radioGroup.findViewById(R.id.standard_option);
        View sphereOption = radioGroup.findViewById(R.id.sphere_option);
        // Robolectric runs in touch mode, so views must be focusable in touch mode to take focus.
        standardOption.setFocusableInTouchMode(true);
        sphereOption.setFocusableInTouchMode(true);
        sphereOption.requestFocus();
        assertTrue(sphereOption.isFocused());

        mCoordinator.requestFocusForAccessibility();
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        // Focus moves to the selected (standard) option.
        assertTrue(standardOption.isFocused());
    }
}
