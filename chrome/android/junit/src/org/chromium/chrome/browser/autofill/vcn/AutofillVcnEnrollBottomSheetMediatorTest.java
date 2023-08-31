// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.app.Activity;
import android.content.Context;
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
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.ui.base.WindowAndroid;

/** Unit test for {@link AutofillVcnEnrollBottomSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@SmallTest
public final class AutofillVcnEnrollBottomSheetMediatorTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private ManagedBottomSheetController mBottomSheetController;

    private WindowAndroid mWindow;
    private View mContentView;
    private ScrollView mScrollView;
    private View mAcceptButton;
    private View mCancelButton;
    private boolean mAccepted;
    private boolean mCancelled;
    private boolean mDismissed;
    private AutofillVcnEnrollBottomSheetMediator mMediator;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        mWindow = new WindowAndroid(activity);
        BottomSheetControllerFactory.attach(mWindow, mBottomSheetController);
        Context context = mWindow.getContext().get();
        mContentView = new View(context);
        mScrollView = new ScrollView(context);
        mAcceptButton = new View(context);
        mCancelButton = new View(context);
        mAccepted = false;
        mCancelled = false;
        mDismissed = false;
        mMediator = new AutofillVcnEnrollBottomSheetMediator(mContentView, mScrollView,
                mAcceptButton, mCancelButton, this::onAccept, this::onCancel, this::onDismiss);
    }

    private void onAccept() {
        mAccepted = true;
    }
    private void onCancel() {
        mCancelled = true;
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
                .requestShowContent(/*content=*/eq(mMediator), /*animate=*/eq(true));
    }

    @Test
    public void testClickAcceptButtonAfterShowing() {
        mMediator.requestShowContent(mWindow);

        mMediator.onClick(mAcceptButton);

        assertTrue(mAccepted);
        verifyBottomSheetControllerHideContentCalled();
    }

    @Test
    public void testClickCancelButtonAfterShowing() {
        mMediator.requestShowContent(mWindow);

        mMediator.onClick(mCancelButton);

        assertTrue(mCancelled);
        verifyBottomSheetControllerHideContentCalled();
    }

    @Test
    public void testHideBottomSheetAfterShowing() {
        mMediator.requestShowContent(mWindow);
        mMediator.hide();

        verifyBottomSheetControllerHideContentCalled();
    }

    private void verifyBottomSheetControllerHideContentCalled() {
        verify(mBottomSheetController)
                .hideContent(/*content=*/eq(mMediator), /*animate=*/eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testClickAcceptButtonWithoutShowing() {
        mMediator.onClick(mAcceptButton);

        assertTrue(mAccepted);
        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testClickCancelButtonWithoutShowing() {
        mMediator.onClick(mCancelButton);

        assertTrue(mCancelled);
        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testHideBottomSheetWithoutShowing() {
        mMediator.hide();

        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testContentView() {
        assertThat(mMediator.getContentView(), equalTo(mContentView));
    }

    @Test
    public void testBottomSheetHasNoToolbar() {
        assertThat(mMediator.getToolbarView(), nullValue());
    }

    @Test
    public void testNoVerticalScrollOffset() {
        assertThat(mMediator.getVerticalScrollOffset(), equalTo(0));
    }

    @Test
    public void testVerticalScrollOffset() {
        mScrollView.setScrollY(1337);

        assertThat(mMediator.getVerticalScrollOffset(), equalTo(1337));
    }

    @Test
    public void testDismissBottomSheet() {
        mMediator.destroy();

        assertTrue(mDismissed);
    }

    @Test
    public void testBottomSheetPriority() {
        assertThat(mMediator.getPriority(), equalTo(BottomSheetContent.ContentPriority.HIGH));
    }

    @Test
    public void testCannotSwipeToDismissBottomSheet() {
        assertThat(mMediator.swipeToDismissEnabled(), equalTo(false));
    }

    @Test
    public void testBottomSheetAccessibilityContentDescriotion() {
        assertThat(mMediator.getSheetContentDescriptionStringId(),
                equalTo(R.string.autofill_virtual_card_enroll_content_description));
    }

    @Test
    public void testBottomSheetFullHeightAccessibilityDescription() {
        assertThat(mMediator.getSheetFullHeightAccessibilityStringId(),
                equalTo(R.string.autofill_virtual_card_enroll_full_height_content_description));
    }

    @Test
    public void testBottomSheetClosedAccessibilityDescription() {
        assertThat(mMediator.getSheetClosedAccessibilityStringId(),
                equalTo(R.string.autofill_virtual_card_enroll_closed_description));
    }

    @Test
    public void testBottomSheetCannotPeek() {
        assertThat(mMediator.getPeekHeight(), equalTo(BottomSheetContent.HeightMode.DISABLED));
    }

    @Test
    public void testContentDeterminesBottomSheetHeight() {
        assertThat(mMediator.getFullHeightRatio(),
                equalTo((float) BottomSheetContent.HeightMode.WRAP_CONTENT));
    }
}
