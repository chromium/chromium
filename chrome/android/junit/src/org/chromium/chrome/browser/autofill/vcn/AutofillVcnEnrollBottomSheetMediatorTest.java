// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.widget.ScrollView;

import androidx.test.filters.SmallTest;

import org.junit.After;
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
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.ui.base.WindowAndroid;

/** Unit test for {@link AutofillVcnEnrollBottomSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@SmallTest
public final class AutofillVcnEnrollBottomSheetMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AutofillVcnEnrollBottomSheetContent mContent;
    @Mock private AutofillVcnEnrollBottomSheetLifecycle mLifecycle;
    @Mock private ManagedBottomSheetController mBottomSheetController;

    private WindowAndroid mWindow;
    private View mContentView;
    private ScrollView mScrollView;
    private boolean mDismissed;
    private AutofillVcnEnrollBottomSheetMediator mMediator;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        mWindow = new WindowAndroid(activity);
        BottomSheetControllerFactory.attach(mWindow, mBottomSheetController);
        when(mLifecycle.canBegin()).thenReturn(true);
        mMediator = new AutofillVcnEnrollBottomSheetMediator(mContent, mLifecycle);
    }

    private void onDismiss() {
        mDismissed = true;
    }

    @After
    public void tearDown() {
        BottomSheetControllerFactory.detach(mBottomSheetController);
        mWindow.destroy();
    }

    @Test
    public void testShowBottomSheet() {
        mMediator.requestShowContent(mWindow);

        verify(mBottomSheetController)
                .requestShowContent(/* content= */ eq(mContent), /* animate= */ eq(true));
    }

    @Test
    public void testCannotShowBottomSheet() {
        when(mLifecycle.canBegin()).thenReturn(false); // E.g., when in tab overview.

        mMediator.requestShowContent(mWindow);

        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testHideBottomSheetAfterShowing() {
        mMediator.requestShowContent(mWindow);
        mMediator.hide();

        verify(mBottomSheetController)
                .hideContent(
                        /* content= */ eq(mContent),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testHideBottomSheetWithoutShowing() {
        mMediator.hide();

        verifyNoInteractions(mBottomSheetController);
    }
}
