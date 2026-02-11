// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link TabBottomSheetCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabBottomSheetCoordinatorTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mMockBottomSheetController;
    @Captor private ArgumentCaptor<TabBottomSheetContent> mBottomSheetContentArgumentCaptor;
    @Captor private ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverArgumentCaptor;

    private Context mContext;
    private TabBottomSheetCoordinator mCoordinator;
    private PropertyModel mCoordinatorModel;
    private View mToolbarView;
    private View mWebUiView;
    private View mFuseboxView;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();

        mCoordinator = new TabBottomSheetCoordinator(mContext, mMockBottomSheetController);
        mCoordinatorModel = mCoordinator.getModelForTesting();

        mToolbarView = new FrameLayout(mContext);
        mWebUiView = new FrameLayout(mContext);
        mFuseboxView = new FrameLayout(mContext);
    }

    @After
    public void tearDown() {
        if (mCoordinator != null) {
            mCoordinator.destroy();
        }
    }

    /**
     * Helper to simulate a successful request to show content and get the Coordinator's observer.
     *
     * @return The BottomSheetObserver instance used by the Coordinator.
     */
    private BottomSheetObserver simulateShowSuccessAndGetObserver() {
        when(mMockBottomSheetController.requestShowContent(any(BottomSheetContent.class), eq(true)))
                .thenReturn(true);
        mCoordinator.tryToShowBottomSheet(mToolbarView, mWebUiView, mFuseboxView);
        verify(mMockBottomSheetController)
                .addObserver(mBottomSheetObserverArgumentCaptor.capture());
        BottomSheetObserver coordinatorObserver = mBottomSheetObserverArgumentCaptor.getValue();
        assertNotNull(
                "Coordinator's observer should be set after successful show.", coordinatorObserver);
        verify(mMockBottomSheetController).addObserver(eq(coordinatorObserver));
        return coordinatorObserver;
    }

    @Test
    public void testShowBottomSheet_Success_ShowsAndObserves() {
        simulateShowSuccessAndGetObserver();
        verify(mMockBottomSheetController)
                .requestShowContent(mBottomSheetContentArgumentCaptor.capture(), eq(true));
        assertNotNull(mBottomSheetContentArgumentCaptor.getValue());
        assertTrue(mCoordinator.isSheetCurrentlyManagedForTesting());
    }

    @Test
    public void testShowBottomSheet_Fails_Cleanup() {
        when(mMockBottomSheetController.requestShowContent(any(BottomSheetContent.class), eq(true)))
                .thenReturn(false);
        mCoordinator.tryToShowBottomSheet(mToolbarView, mWebUiView, mFuseboxView);
        verify(mMockBottomSheetController)
                .requestShowContent(any(BottomSheetContent.class), eq(true));
        verify(mMockBottomSheetController, never()).addObserver(any(BottomSheetObserver.class));
        assertFalse(mCoordinator.isSheetCurrentlyManagedForTesting());
    }

    @Test
    public void testDestroy_WhenShown_HidesAndCleansUp() {
        simulateShowSuccessAndGetObserver();
        assertTrue(mCoordinator.isSheetCurrentlyManagedForTesting());
        mCoordinator.destroy();

        verify(mMockBottomSheetController)
                .hideContent(
                        any(TabBottomSheetContent.class), eq(false), eq(StateChangeReason.NONE));
        assertFalse(mCoordinator.isSheetCurrentlyManagedForTesting());
    }

    @Test
    public void testDestroy_WhenNotShown_CleansUp() {
        when(mMockBottomSheetController.requestShowContent(any(BottomSheetContent.class), eq(true)))
                .thenReturn(false);
        mCoordinator.tryToShowBottomSheet(mToolbarView, mWebUiView, mFuseboxView);
        mCoordinator.destroy();

        verify(mMockBottomSheetController, never()).hideContent(any(), anyBoolean(), anyInt());
        assertFalse(mCoordinator.isSheetCurrentlyManagedForTesting());
    }

    @Test
    public void testShowBottomSheet_ContentHasCustomLifecycle() {
        simulateShowSuccessAndGetObserver();
        verify(mMockBottomSheetController)
                .requestShowContent(mBottomSheetContentArgumentCaptor.capture(), eq(true));
        TabBottomSheetContent content = mBottomSheetContentArgumentCaptor.getValue();
        assertNotNull(content);
        assertTrue(content.hasCustomLifecycle());
    }
}
