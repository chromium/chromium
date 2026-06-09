// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;

/** Unit tests for {@link MostVisitedTilesCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MostVisitedTilesCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private View mMvTilesContainerLayout;
    @Mock private MostVisitedTilesLayout mMvTilesLayout;
    @Mock private MostVisitedTilesMediator mMediator;

    private Activity mActivity;
    private MostVisitedTilesCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        when(mMvTilesContainerLayout.findViewById(R.id.mv_tiles_layout)).thenReturn(mMvTilesLayout);
        when(mMvTilesLayout.getContext()).thenReturn(mActivity);
        mCoordinator =
                new MostVisitedTilesCoordinator(
                        mActivity,
                        mActivityLifecycleDispatcher,
                        mMvTilesContainerLayout,
                        null,
                        null);
        mCoordinator.setMediatorForTesting(mMediator);
    }

    @Test
    public void testUpdateMvtVisibility() {
        mCoordinator.updateMvtVisibility();
        verify(mMediator).updateMvtVisibility();
    }

    @Test
    public void testUpdateMvtWidth_WithWidth() {
        int totalWidth = 1000;
        when(mMvTilesContainerLayout.getVisibility()).thenReturn(View.VISIBLE);
        mCoordinator.updateMvtWidth(totalWidth);
        verify(mMediator).updateMvtWidth(eq(totalWidth));

        clearInvocations(mMediator);
        when(mMvTilesContainerLayout.getVisibility()).thenReturn(View.GONE);
        mCoordinator.updateMvtWidth(totalWidth);
        verify(mMediator, never()).updateMvtWidth(any(Integer.class));
    }

    @Test
    public void testUpdateTilesLayoutMargins() {
        mCoordinator.updateTilesLayoutMargins(/* shouldShowLogo= */ true, /* isTablet= */ false);
        verify(mMediator).updateTilesLayoutMargins(eq(true), eq(false));
    }
}
