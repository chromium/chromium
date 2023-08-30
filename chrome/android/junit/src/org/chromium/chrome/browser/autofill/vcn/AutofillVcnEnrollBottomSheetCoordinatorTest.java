// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.robolectric.Robolectric.buildActivity;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.ui.base.WindowAndroid;

/** Unit test for {@link AutofillVcnEnrollBottomSheetCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@SmallTest
public final class AutofillVcnEnrollBottomSheetCoordinatorTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private ManagedBottomSheetController mBottomSheetController;

    private WindowAndroid mWindow;
    private AutofillVcnEnrollBottomSheetCoordinator mCoordinator;

    @Before
    public void setUp() {
        Activity activity = buildActivity(Activity.class).create().get();
        mWindow = new WindowAndroid(activity);
        BottomSheetControllerFactory.attach(mWindow, mBottomSheetController);
        mCoordinator = new AutofillVcnEnrollBottomSheetCoordinator(mWindow.getContext().get(),
                "Message text", "Accept Button Text", "Cancel Button Text", /*onAccept=*/() -> {},
                /*onCancel=*/() -> {}, /*onDismiss=*/() -> {});
    }

    @After
    public void tearDown() {
        BottomSheetControllerFactory.detach(mBottomSheetController);
        mWindow.destroy();
    }

    @Test
    public void testShow() {
        mCoordinator.requestShowContent(mWindow);

        verify(mBottomSheetController)
                .requestShowContent(any(AutofillVcnEnrollBottomSheetMediator.class),
                        /*animate=*/eq(true));
    }

    @Test
    public void testHideAfterShow() {
        mCoordinator.requestShowContent(mWindow);
        mCoordinator.hide();

        verify(mBottomSheetController)
                .hideContent(any(AutofillVcnEnrollBottomSheetMediator.class),
                        /*animate=*/eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testHideWithoutShow() {
        mCoordinator.hide();

        verifyNoInteractions(mBottomSheetController);
    }
}
