// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import android.app.Activity;

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
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator.PaymentHandlerUiObserver;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.payments.ui.InputProtector;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Unit tests for how {@link PaymentHandlerMediator} decides the height of the payment app's web
 * view.
 *
 * <p>The height comes from the bottom sheet itself, and only once the sheet has settled into a
 * state and can report its own measurements.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PaymentHandlerMediatorUnitTest {
    /** Matches R.dimen.sheet_tab_toolbar_height on the device this was measured on. */
    private static final int TOOLBAR_HEIGHT_PX = 140;

    /** The full-state sheet height a Pixel Tablet reports once the sheet has settled. */
    private static final int MAX_OFFSET_PX = 1375;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock(extraInterfaces = WebContentsObserver.Observable.class)
    private WebContents mPaymentRequestWebContents;

    @Mock(extraInterfaces = WebContentsObserver.Observable.class)
    private WebContents mPaymentHandlerWebContents;

    @Mock private PaymentHandlerUiObserver mUiObserver;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private InputProtector mInputProtector;
    @Mock private Runnable mHider;

    private PropertyModel mModel;
    private PaymentHandlerMediator mMediator;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mModel = new PropertyModel.Builder(PaymentHandlerProperties.ALL_KEYS).build();
        mMediator =
                new PaymentHandlerMediator(
                        mModel,
                        mHider,
                        mPaymentRequestWebContents,
                        mPaymentHandlerWebContents,
                        mUiObserver,
                        mBottomSheetController,
                        TOOLBAR_HEIGHT_PX,
                        activity,
                        mInputProtector);
    }

    private int contentVisibleHeight() {
        return mModel.get(PaymentHandlerProperties.CONTENT_VISIBLE_HEIGHT_PX);
    }

    @Test
    public void testHeightIsNotSetBeforeTheSheetSettles() {
        // The sheet has not been laid out when the mediator is built, so anything it reports at
        // that point is wrong.
        assertEquals(0, contentVisibleHeight());
    }

    @Test
    public void testHeightComesFromTheSheetWhenItReachesFull() {
        when(mBottomSheetController.getMaxOffset()).thenReturn(MAX_OFFSET_PX);

        mMediator.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);

        assertEquals(MAX_OFFSET_PX - TOOLBAR_HEIGHT_PX, contentVisibleHeight());
    }

    @Test
    public void testHalfStateStillUsesTheFullHeight() {
        // The sheet slides up and down rather than resizing, so the web view keeps the height it
        // needs at full state even while only half of it is on screen.
        when(mBottomSheetController.getMaxOffset()).thenReturn(MAX_OFFSET_PX);

        mMediator.onSheetStateChanged(SheetState.HALF, StateChangeReason.NONE);

        assertEquals(MAX_OFFSET_PX - TOOLBAR_HEIGHT_PX, contentVisibleHeight());
    }

    @Test
    public void testHeightIsIgnoredWhileTheSheetStillMeasuresZero() {
        when(mBottomSheetController.getMaxOffset()).thenReturn(0);

        mMediator.onSheetStateChanged(SheetState.PEEK, StateChangeReason.NONE);

        assertEquals(0, contentVisibleHeight());
    }

    @Test
    public void testHeightIsIgnoredWhenTheSheetIsShorterThanItsToolbar() {
        // Subtracting the toolbar would give a negative height, which would collapse the web view.
        when(mBottomSheetController.getMaxOffset()).thenReturn(TOOLBAR_HEIGHT_PX - 1);

        mMediator.onSheetStateChanged(SheetState.PEEK, StateChangeReason.NONE);

        assertEquals(0, contentVisibleHeight());
    }

    @Test
    public void testHeightIsRecomputedWhenTheSheetChangesSize() {
        // For example when the window is resized on a desktop, or the device is rotated.
        when(mBottomSheetController.getMaxOffset()).thenReturn(MAX_OFFSET_PX);
        mMediator.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);
        assertEquals(MAX_OFFSET_PX - TOOLBAR_HEIGHT_PX, contentVisibleHeight());

        when(mBottomSheetController.getMaxOffset()).thenReturn(800);
        mMediator.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);

        assertEquals(800 - TOOLBAR_HEIGHT_PX, contentVisibleHeight());
    }
}
