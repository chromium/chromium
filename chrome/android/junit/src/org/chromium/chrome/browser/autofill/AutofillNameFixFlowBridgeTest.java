// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

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

/** Unit tests for {@link AutofillNameFixFlowBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillNameFixFlowBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AutofillNameFixFlowBridge.Natives mNativeMock;

    private WindowAndroid mWindowAndroid;
    private AutofillNameFixFlowBridge mBridge;
    private static final long NATIVE_POINTER = 12345L;

    @Before
    public void setUp() {
        AutofillNameFixFlowBridgeJni.setInstanceForTesting(mNativeMock);

        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        mWindowAndroid = new WindowAndroid(activity, false);

        mBridge =
                new AutofillNameFixFlowBridge(
                        NATIVE_POINTER,
                        /* title= */ "Title",
                        /* inferredName= */ "Inferred Name",
                        /* confirmButtonLabel= */ "Confirm",
                        /* iconId= */ 0,
                        mWindowAndroid);
    }

    @After
    public void tearDown() {
        mWindowAndroid.destroy();
    }

    @Test
    public void testOnUserDismiss_CallsJni() {
        mBridge.onUserDismiss();
        verify(mNativeMock).onUserDismiss(NATIVE_POINTER);
    }

    @Test
    public void testOnUserAcceptCardholderName_CallsJni() {
        mBridge.onUserAcceptCardholderName("New Name");
        verify(mNativeMock).onUserAccept(NATIVE_POINTER, "New Name");
    }

    @Test
    public void testOnPromptDismissed_CallsJniAndClearsPointer() {
        mBridge.onPromptDismissed();
        verify(mNativeMock).promptDismissed(NATIVE_POINTER);

        // Subsequent calls should not reach JNI.
        mBridge.onUserDismiss();
        mBridge.onUserAcceptCardholderName("New Name");
        mBridge.onPromptDismissed();

        // Verify no other calls were made to JNI after the first dismiss.
        org.mockito.Mockito.verifyNoMoreInteractions(mNativeMock);
    }
}
