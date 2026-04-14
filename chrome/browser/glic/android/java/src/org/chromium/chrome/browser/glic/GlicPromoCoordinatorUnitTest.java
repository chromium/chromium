// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;

import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.glic.GlicPromoCoordinator.GlicPromoSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link GlicPromoCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GlicPromoCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Runnable mOnPositiveButtonClicked;
    @Mock private Runnable mOnDismissed;

    @Captor ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor;

    private TestActivity mActivity;
    private GlicPromoCoordinator mGlicPromoCoordinator;
    private GlicPromoSheetContent mBottomSheetContent;
    private View mView;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        // Set a basic theme so views can be inflated without style issues.
        mActivity.setTheme(android.R.style.Theme_Material_Light);
        mGlicPromoCoordinator =
                new GlicPromoCoordinator(
                        mActivity, mBottomSheetController, mOnPositiveButtonClicked, mOnDismissed);
        mView = mGlicPromoCoordinator.getViewForTesting();
        verify(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
        mBottomSheetContent = mGlicPromoCoordinator.getBottomSheetContentForTesting();
    }

    @SmallTest
    @Test
    public void testShowBottomSheet() {
        mGlicPromoCoordinator.showBottomSheet();
        verify(mBottomSheetController).requestShowContent(any(), eq(true));
    }

    @Test
    public void testSheetContent_onBackPressed() {
        mBottomSheetContent.onBackPressed();
        verify(mBottomSheetController).hideContent(any(), eq(true));
    }

    @Test
    public void testBottomSheetObserver() {
        BottomSheetObserver observer = mBottomSheetObserverCaptor.getValue();
        observer.onSheetOpened(StateChangeReason.NONE);

        observer.onSheetClosed(StateChangeReason.TAP_SCRIM);
        verify(mOnDismissed).run();
        verify(mBottomSheetController).removeObserver(eq(observer));

        clearInvocations(mBottomSheetController, mOnDismissed);

        observer.onSheetClosed(StateChangeReason.SWIPE);
        verify(mOnDismissed).run();
        verify(mBottomSheetController).removeObserver(eq(observer));
    }

    @Test
    public void testPositiveButtonClicked() {
        View button = mView.findViewById(R.id.glic_promo_positive_button);
        button.performClick();
        verify(mOnPositiveButtonClicked).run();
        verify(mBottomSheetController).hideContent(any(), eq(true));
    }

    @Test
    public void testDestroy() {
        mGlicPromoCoordinator.destroy();
        verify(mBottomSheetController).hideContent(eq(mBottomSheetContent), eq(false));
        verify(mBottomSheetController).removeObserver(any());
    }
}
