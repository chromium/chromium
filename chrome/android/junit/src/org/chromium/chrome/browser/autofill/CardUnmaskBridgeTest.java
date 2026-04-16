// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.Mockito.verify;

import android.app.Activity;

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
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/** Unit tests for {@link CardUnmaskBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CardUnmaskBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private CardUnmaskBridge.Natives mNativeMock;
    @Mock private AutofillImageFetcher mAutofillImageFetcherMock;

    private WindowAndroid mWindowAndroid;
    private CardUnmaskBridge mCardUnmaskBridge;
    private static final long NATIVE_POINTER = 12345L;

    @Before
    public void setUp() {
        CardUnmaskBridgeJni.setInstanceForTesting(mNativeMock);

        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        mWindowAndroid = new WindowAndroid(activity, false);

        mCardUnmaskBridge =
                new CardUnmaskBridge(
                        NATIVE_POINTER,
                        mAutofillImageFetcherMock,
                        /* title= */ "Title",
                        /* instructions= */ "Instructions",
                        /* cardIconId= */ 0,
                        /* cardName= */ "Card Name",
                        /* cardLastFourDigits= */ "1234",
                        /* cardExpiration= */ "12/34",
                        /* cardArtUrl= */ GURL.emptyGURL(),
                        /* confirmButtonLabel= */ "Confirm",
                        /* cvcIconId= */ 0,
                        /* cvcImageAnnouncement= */ "CVC",
                        /* isVirtualCard= */ false,
                        /* shouldRequestExpirationDate= */ false,
                        /* shouldOfferWebauthn= */ false,
                        /* defaultUseScreenlockChecked= */ false,
                        /* successMessageDurationMilliseconds= */ 0L,
                        mWindowAndroid);
    }

    @After
    public void tearDown() {
        mWindowAndroid.destroy();
    }

    @Test
    public void testCheckUserInputValidity_CallsJni() {
        mCardUnmaskBridge.checkUserInputValidity("123");
        verify(mNativeMock).checkUserInputValidity(NATIVE_POINTER, "123");
    }

    @Test
    public void testOnUserInput_CallsJni() {
        mCardUnmaskBridge.onUserInput("123", "12", "2034", true, false);
        verify(mNativeMock).onUserInput(NATIVE_POINTER, "123", "12", "2034", true, false);
    }

    @Test
    public void testOnNewCardLinkClicked_CallsJni() {
        mCardUnmaskBridge.onNewCardLinkClicked();
        verify(mNativeMock).onNewCardLinkClicked(NATIVE_POINTER);
    }

    @Test
    public void testGetExpectedCvcLength_CallsJni() {
        mCardUnmaskBridge.getExpectedCvcLength();
        verify(mNativeMock).getExpectedCvcLength(NATIVE_POINTER);
    }

    @Test
    public void testDismissed_CallsJniAndClearsPointer() {
        mCardUnmaskBridge.dismissed();
        verify(mNativeMock).promptDismissed(NATIVE_POINTER);

        // Subsequent calls should not reach JNI.
        assertFalse(mCardUnmaskBridge.checkUserInputValidity("123"));
        mCardUnmaskBridge.onUserInput("123", "12", "2034", true, false);
        mCardUnmaskBridge.onNewCardLinkClicked();
        assertEquals(0, mCardUnmaskBridge.getExpectedCvcLength());
        mCardUnmaskBridge.dismissed();

        // Verify no other calls were made to JNI after the first dismiss.
        org.mockito.Mockito.verifyNoMoreInteractions(mNativeMock);
    }
}
