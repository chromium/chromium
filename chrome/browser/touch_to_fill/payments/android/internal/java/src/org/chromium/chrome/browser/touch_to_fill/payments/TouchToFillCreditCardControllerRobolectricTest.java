// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link TouchToFillCreditCardCoordinator} and {@link TouchToFillCreditCardMediator} */
@RunWith(BaseRobolectricTestRunner.class)
public class TouchToFillCreditCardControllerRobolectricTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private TouchToFillCreditCardCoordinator mCoordinator;
    private PropertyModel mTouchToFillCreditCardModel;
    Context mContext;

    @Mock
    private BottomSheetController mBottomSheetController;

    @Mock
    TouchToFillCreditCardComponent.Delegate mDelegateMock;

    public TouchToFillCreditCardControllerRobolectricTest() {
        mCoordinator = new TouchToFillCreditCardCoordinator();
        mContext = Robolectric.buildActivity(Activity.class).get();
    }

    @Before
    public void setUp() {
        mCoordinator.initialize(mContext, mBottomSheetController, mDelegateMock);
        mTouchToFillCreditCardModel =
                mCoordinator.getTouchToFillCreditCardPropertyModelForTesting();
    }

    @Test
    public void testScanNewCard() {
        mCoordinator.showSheet(true);
        Runnable scanNewCardCallback = mTouchToFillCreditCardModel.get(
                TouchToFillCreditCardProperties.SCAN_CREDIT_CARD_CALLBACK);
        scanNewCardCallback.run();
        verify(mDelegateMock).scanCreditCard();
    }
}
