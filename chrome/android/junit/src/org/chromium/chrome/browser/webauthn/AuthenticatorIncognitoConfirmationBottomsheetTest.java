// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauthn;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.widget.Button;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Answers;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetFeatureMap;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

@RunWith(BaseRobolectricTestRunner.class)
public class AuthenticatorIncognitoConfirmationBottomsheetTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.WARN);

    @Mock(answer = Answers.RETURNS_DEEP_STUBS)
    private WebContents mWebContents;

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private BottomSheetContent mOtherBottomSheetContent;
    @Mock private Runnable mPositiveCallbackMock;
    @Mock private Runnable mNegativeCallbackMock;
    @Captor private ArgumentCaptor<BottomSheetObserver> mObserverCaptor;
    @Captor private ArgumentCaptor<BottomSheetContent> mContentCaptor;

    private Runnable mPositiveCallback;
    private Runnable mNegativeCallback;
    private boolean mUserResponded;
    private boolean mUserPositive;

    private AuthenticatorIncognitoConfirmationBottomsheet mBottomsheet;

    @Before
    public void setUp() {
        WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);
        setWindowAndroid(windowAndroid, mWebContents);
        Mockito.doReturn(new WeakReference<>(RuntimeEnvironment.application))
                .when(windowAndroid)
                .getContext();

        mPositiveCallback =
                () -> {
                    mUserResponded = true;
                    mUserPositive = true;
                };
        mNegativeCallback =
                () -> {
                    mUserResponded = true;
                    mUserPositive = false;
                };

        setUpBottomSheetController(/* requestShowContentResponse= */ true);
    }

    @After
    public void tearDown() {
        if (mBottomsheet != null) mBottomsheet.close(false);
    }

    private void createBottomsheet() {
        mBottomsheet = new AuthenticatorIncognitoConfirmationBottomsheet(mWebContents);
    }

    private void setUpBottomSheetController(boolean requestShowContentResponse) {
        doAnswer(
                        invocation -> {
                            if (requestShowContentResponse) {
                                BottomSheetContent content = invocation.getArgument(0);
                                doReturn(content)
                                        .when(mBottomSheetController)
                                        .getCurrentSheetContent();
                            }
                            return requestShowContentResponse;
                        })
                .when(mBottomSheetController)
                .requestShowContent(any(BottomSheetContent.class), anyBoolean());
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);
    }

    private boolean show() {
        return show(/* enableOptOut= */ false);
    }

    private boolean show(boolean enableOptOut) {
        if (mBottomsheet == null) return false;

        mUserResponded = false;
        mUserPositive = false;

        return mBottomsheet.show(mPositiveCallback, mNegativeCallback);
    }

    private void setWindowAndroid(WindowAndroid windowAndroid, WebContents webContents) {
        Mockito.doReturn(windowAndroid).when(webContents).getTopLevelNativeWindow();
    }

    @Test
    public void testShow() {
        createBottomsheet();
        Assert.assertFalse(mBottomsheet.mIsShowing);
        show();
        Assert.assertNotNull(mBottomsheet.mContentView);
        Assert.assertTrue(mBottomsheet.mIsShowing);
    }

    @Test
    public void testContinue() {
        createBottomsheet();
        show();
        ((Button) mBottomsheet.mContentView.findViewById(R.id.continue_button)).performClick();
        Assert.assertFalse(mBottomsheet.mIsShowing);
        Assert.assertTrue(mUserResponded);
        Assert.assertTrue(mUserPositive);
    }

    @Test
    public void testCancel() {
        createBottomsheet();
        show();
        ((Button) mBottomsheet.mContentView.findViewById(R.id.cancel_button)).performClick();
        Assert.assertFalse(mBottomsheet.mIsShowing);
        Assert.assertTrue(mUserResponded);
        Assert.assertFalse(mUserPositive);
    }

    private BottomSheetObserver showAndCaptureObserver() {
        createBottomsheet();
        show();
        Assert.assertTrue(mBottomsheet.mIsShowing);

        verify(mBottomSheetController).addObserver(mObserverCaptor.capture());
        return mObserverCaptor.getValue();
    }

    @Test
    public void testOnSheetStateChangedHidden_currentSheetContent_closesSheet() {
        BottomSheetObserver observer = showAndCaptureObserver();

        observer.onSheetStateChanged(
                BottomSheetController.SheetState.HIDDEN,
                BottomSheetController.StateChangeReason.SWIPE);

        Assert.assertFalse(mBottomsheet.mIsShowing);
        Assert.assertTrue(mUserResponded);
        Assert.assertFalse(mUserPositive);
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testOnSheetStateChangedHidden_otherSheetContent_ignoresHidden() {
        BottomSheetObserver observer = showAndCaptureObserver();

        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mOtherBottomSheetContent);
        observer.onSheetStateChanged(
                BottomSheetController.SheetState.HIDDEN,
                BottomSheetController.StateChangeReason.SWIPE);

        Assert.assertTrue(mBottomsheet.mIsShowing);
        Assert.assertFalse(mUserResponded);
    }

    @Test
    @DisableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testOnSheetStateChangedHidden_deferContentSwapDisabled_closesSheet() {
        BottomSheetObserver observer = showAndCaptureObserver();

        // Without deferral, the controller already replaced the hidden content.
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(null);
        observer.onSheetStateChanged(
                BottomSheetController.SheetState.HIDDEN,
                BottomSheetController.StateChangeReason.SWIPE);

        Assert.assertFalse(mBottomsheet.mIsShowing);
        Assert.assertTrue(mUserResponded);
        Assert.assertFalse(mUserPositive);
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testBottomSheetContentDestroy_closesSheet() {
        BottomSheetObserver observer = showAndCaptureObserver();
        verify(mBottomSheetController).requestShowContent(mContentCaptor.capture(), anyBoolean());

        // The content is dropped while another content is still shown, e.g. by
        // clearRequestsAndHide().
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mOtherBottomSheetContent);
        mContentCaptor.getValue().destroy();

        Assert.assertFalse(mBottomsheet.mIsShowing);
        Assert.assertTrue(mUserResponded);
        Assert.assertFalse(mUserPositive);
        verify(mBottomSheetController).removeObserver(observer);
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testBottomSheetContentDestroy_currentSheetContent_ignored() {
        showAndCaptureObserver();
        verify(mBottomSheetController).requestShowContent(mContentCaptor.capture(), anyBoolean());

        // The current content is closed when it is hidden, see onSheetStateChanged().
        mContentCaptor.getValue().destroy();

        Assert.assertTrue(mBottomsheet.mIsShowing);
        Assert.assertFalse(mUserResponded);
    }

    @Test
    @DisableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testBottomSheetContentDestroy_deferContentSwapDisabled_noop() {
        showAndCaptureObserver();
        verify(mBottomSheetController).requestShowContent(mContentCaptor.capture(), anyBoolean());

        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mOtherBottomSheetContent);
        mContentCaptor.getValue().destroy();

        Assert.assertTrue(mBottomsheet.mIsShowing);
        Assert.assertFalse(mUserResponded);
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_DEFER_CONTENT_SWAP_ON_HIDDEN)
    public void testClose_hideContentDestroysContent_runsCallbackOnce() {
        createBottomsheet();
        mBottomsheet.show(mPositiveCallbackMock, mNegativeCallbackMock);
        doAnswer(
                        invocation -> {
                            // The controller replaces the hidden content before destroying it.
                            doReturn(null).when(mBottomSheetController).getCurrentSheetContent();
                            BottomSheetContent content = invocation.getArgument(0);
                            content.destroy();
                            return null;
                        })
                .when(mBottomSheetController)
                .hideContent(any(BottomSheetContent.class), anyBoolean());

        mBottomsheet.close(/* success= */ true);

        verify(mPositiveCallbackMock).run();
        verify(mNegativeCallbackMock, never()).run();
    }
}
