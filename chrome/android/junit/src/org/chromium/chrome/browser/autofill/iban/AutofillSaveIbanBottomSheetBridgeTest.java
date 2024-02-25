// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;

/** Unit tests for {@link AutofillSaveIbanBottomSheetBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@SmallTest
public final class AutofillSaveIbanBottomSheetBridgeTest {
    private static final long MOCK_POINTER = 0xb00fb00f;

    private static final String IBAN_LABEL = "CH56 0483 5012 3456 7800 9";

    private static final String USER_PROVIDED_NICKNAME = "My Doctor's IBAN";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public JniMocker mJniMocker = new JniMocker();

    private AutofillSaveIbanBottomSheetBridge mAutofillSaveIbanBottomSheetBridge;

    @Mock private AutofillSaveIbanBottomSheetBridge.Natives mBridgeNatives;

    @Mock private AutofillSaveIbanBottomSheetBridge.CoordinatorFactory mCoordinatorFactory;

    @Mock private AutofillSaveIbanBottomSheetCoordinator mCoordinator;

    @Before
    public void setUp() {
        mJniMocker.mock(AutofillSaveIbanBottomSheetBridgeJni.TEST_HOOKS, mBridgeNatives);
        mAutofillSaveIbanBottomSheetBridge =
                new AutofillSaveIbanBottomSheetBridge(MOCK_POINTER, mCoordinatorFactory);
    }

    private void setUpCoordinatorFactory() {
        when(mCoordinatorFactory.create(mAutofillSaveIbanBottomSheetBridge))
                .thenReturn(mCoordinator);
    }

    @Test
    public void testRequestShowContent_requestsShowOnCoordinator() {
        setUpCoordinatorFactory();

        mAutofillSaveIbanBottomSheetBridge.requestShowContent(IBAN_LABEL);

        verify(mCoordinator).requestShowContent(IBAN_LABEL);
    }

    @Test
    public void testDestroy_callsCoordinatorDestroy() {
        setUpCoordinatorFactory();
        mAutofillSaveIbanBottomSheetBridge.requestShowContent(IBAN_LABEL);

        mAutofillSaveIbanBottomSheetBridge.destroy();

        verify(mCoordinator).destroy();
    }

    @Test
    public void testDestroy_whenCoordinatorHasNotBeenCreated() {
        mAutofillSaveIbanBottomSheetBridge.destroy();

        verifyNoInteractions(mCoordinator);
    }

    @Test
    public void testDestroyTwice_destroysCoordinatorOnce() {
        setUpCoordinatorFactory();
        mAutofillSaveIbanBottomSheetBridge.requestShowContent(IBAN_LABEL);

        mAutofillSaveIbanBottomSheetBridge.destroy();
        mAutofillSaveIbanBottomSheetBridge.destroy();

        verify(mCoordinator).destroy();
    }

    @Test
    public void testOnUiAccepted_callsNativeOnUiAccepted() {
        mAutofillSaveIbanBottomSheetBridge.onUiAccepted(USER_PROVIDED_NICKNAME);

        verify(mBridgeNatives).onUiAccepted(MOCK_POINTER, USER_PROVIDED_NICKNAME);
    }

    @Test
    public void testOnUiAccepted_doesNotCallNative_afterDestroy() {
        mAutofillSaveIbanBottomSheetBridge.destroy();

        mAutofillSaveIbanBottomSheetBridge.onUiAccepted(USER_PROVIDED_NICKNAME);

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testOnUiCanceled_callsNativeOnUiCanceled() {
        mAutofillSaveIbanBottomSheetBridge.onUiCanceled();

        verify(mBridgeNatives).onUiCanceled(MOCK_POINTER);
    }

    @Test
    public void testOnUiCanceled_doesNotCallNative_afterDestroy() {
        mAutofillSaveIbanBottomSheetBridge.destroy();

        mAutofillSaveIbanBottomSheetBridge.onUiCanceled();

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testOnUiIgnored_callsNativeOnUiIgnored() {
        mAutofillSaveIbanBottomSheetBridge.onUiIgnored();

        verify(mBridgeNatives).onUiIgnored(MOCK_POINTER);
    }

    @Test
    public void testOnUiIgnored_doesNotCallNative_afterDestroy() {
        mAutofillSaveIbanBottomSheetBridge.destroy();

        mAutofillSaveIbanBottomSheetBridge.onUiIgnored();

        verifyNoInteractions(mBridgeNatives);
    }
}
