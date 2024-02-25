// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauthn;

import android.content.Context;
import android.widget.Button;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Answers;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {
            AuthenticatorIncognitoConfirmationBottomsheetTest.ShadowBottomSheetControllerProvider
                    .class
        })
public class AuthenticatorIncognitoConfirmationBottomsheetTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.WARN);
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock(answer = Answers.RETURNS_DEEP_STUBS)
    private WebContents mWebContents;

    private Runnable mPositiveCallback;
    private Runnable mNegativeCallback;
    private boolean mUserResponded;
    private boolean mUserPositive;

    private AuthenticatorIncognitoConfirmationBottomsheet mBottomsheet;

    /** The shadow of BottomSheetControllerProvider. */
    @Implements(BottomSheetControllerProvider.class)
    static class ShadowBottomSheetControllerProvider {
        private static BottomSheetController sBottomSheetController;

        @Implementation
        public static BottomSheetController from(WindowAndroid windowAndroid) {
            return sBottomSheetController;
        }

        private static void setBottomSheetController(BottomSheetController controller) {
            sBottomSheetController = controller;
        }
    }

    @Before
    public void setUp() {
        WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);
        setWindowAndroid(windowAndroid, mWebContents);
        Mockito.doReturn(new WeakReference<Context>(RuntimeEnvironment.application))
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

        ShadowBottomSheetControllerProvider.setBottomSheetController(
                createBottomSheetController(/* requestShowContentResponse= */ true));
    }

    @After
    public void tearDown() {
        if (mBottomsheet != null) mBottomsheet.close(false);
    }

    private void createBottomsheet() {
        mBottomsheet = new AuthenticatorIncognitoConfirmationBottomsheet(mWebContents);
    }

    private BottomSheetController createBottomSheetController(boolean requestShowContentResponse) {
        BottomSheetController controller = Mockito.mock(BottomSheetController.class);
        Mockito.doReturn(requestShowContentResponse)
                .when(controller)
                .requestShowContent(Mockito.any(BottomSheetContent.class), Mockito.anyBoolean());
        return controller;
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

    private void setContext(Context context) {
        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        Mockito.doReturn(new WeakReference<Context>(context)).when(windowAndroid).getContext();
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
}
