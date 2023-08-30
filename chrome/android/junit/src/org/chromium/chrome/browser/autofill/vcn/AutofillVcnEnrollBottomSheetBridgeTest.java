// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** Unit test for {@link AutofillVcnEnrollBottomSheetBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@SmallTest
public final class AutofillVcnEnrollBottomSheetBridgeTest {
    private static final long NATIVE_AUTOFILL_VCN_ENROLL_BOTTOM_SHEET_BRIDGE = 0xa1fabe7a;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private AutofillVcnEnrollBottomSheetBridge.Natives mBridgeNatives;
    @Mock
    private WebContents mWebContents;
    @Mock
    private ManagedBottomSheetController mBottomSheetController;

    private WindowAndroid mWindow;
    private AutofillVcnEnrollBottomSheetBridge mBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(AutofillVcnEnrollBottomSheetBridgeJni.TEST_HOOKS, mBridgeNatives);
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        mWindow = new WindowAndroid(activity);
        BottomSheetControllerFactory.attach(mWindow, mBottomSheetController);
        mBridge = new AutofillVcnEnrollBottomSheetBridge();
    }

    @After
    public void tearDown() {
        BottomSheetControllerFactory.detach(mBottomSheetController);
        mWindow.destroy();
    }

    @Test
    public void testCannotShowWithNullWebContents() {
        mBridge.requestShowContent(
                NATIVE_AUTOFILL_VCN_ENROLL_BOTTOM_SHEET_BRIDGE, /*webContents=*/null);

        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testCannotShowWithDestroyedWebContents() {
        when(mWebContents.isDestroyed()).thenReturn(true);

        mBridge.requestShowContent(NATIVE_AUTOFILL_VCN_ENROLL_BOTTOM_SHEET_BRIDGE, mWebContents);

        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testCannotShowWithoutTopLevelNativeWindow() {
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(null);

        mBridge.requestShowContent(NATIVE_AUTOFILL_VCN_ENROLL_BOTTOM_SHEET_BRIDGE, mWebContents);

        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testShowBottomSheet() {
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);

        mBridge.requestShowContent(NATIVE_AUTOFILL_VCN_ENROLL_BOTTOM_SHEET_BRIDGE, mWebContents);

        verify(mBottomSheetController)
                .requestShowContent(any(AutofillVcnEnrollBottomSheetMediator.class),
                        /*animate=*/eq(true));
    }

    @Test
    public void testCannotDismissWithoutShowing() {
        mBridge.onDismiss();

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testDismissAfterShowing() {
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);
        mBridge.requestShowContent(NATIVE_AUTOFILL_VCN_ENROLL_BOTTOM_SHEET_BRIDGE, mWebContents);

        mBridge.onDismiss();

        verify(mBridgeNatives).onDismiss(eq(NATIVE_AUTOFILL_VCN_ENROLL_BOTTOM_SHEET_BRIDGE));
    }

    @Test
    public void testSecondDismissDoesNothing() {
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);
        mBridge.requestShowContent(NATIVE_AUTOFILL_VCN_ENROLL_BOTTOM_SHEET_BRIDGE, mWebContents);
        mBridge.onDismiss();
        clearInvocations(mBridgeNatives);

        mBridge.onDismiss();

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testHideWithoutShowing() {
        mBridge.hide();

        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testHideAfterShowing() {
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);
        mBridge.requestShowContent(NATIVE_AUTOFILL_VCN_ENROLL_BOTTOM_SHEET_BRIDGE, mWebContents);

        mBridge.hide();

        verify(mBottomSheetController)
                .hideContent(any(), eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testSecondHideDoesNothing() {
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);
        mBridge.requestShowContent(NATIVE_AUTOFILL_VCN_ENROLL_BOTTOM_SHEET_BRIDGE, mWebContents);
        mBridge.hide();
        clearInvocations(mBottomSheetController);

        mBridge.hide();

        verifyNoInteractions(mBottomSheetController);
    }
}
