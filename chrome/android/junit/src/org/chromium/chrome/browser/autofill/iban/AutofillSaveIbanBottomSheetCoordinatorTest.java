// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
public final class AutofillSaveIbanBottomSheetCoordinatorTest {
    private static final String IBAN_LABEL = "CH56 0483 5012 3456 7800 9";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private AutofillSaveIbanBottomSheetCoordinator mCoordinator;

    @Mock private AutofillSaveIbanBottomSheetMediator mMediator;

    @Before
    public void setUp() {
        mCoordinator = new AutofillSaveIbanBottomSheetCoordinator(mMediator);
    }

    @Test
    public void testRequestShowContent_callsMediatorRequestShow() {
        mCoordinator.requestShowContent(IBAN_LABEL);

        verify(mMediator).requestShowContent(IBAN_LABEL);
    }

    @Test
    public void testRequestShowContent_requestsShowEmptyString() {
        IllegalArgumentException e =
                assertThrows(
                        IllegalArgumentException.class, () -> mCoordinator.requestShowContent(""));
        assertThat(e)
                .hasMessageThat()
                .isEqualTo("IBAN label passed from C++ should not be NULL or empty.");
    }

    @Test
    public void testDestroy_callsMediatorDestroy() {
        mCoordinator.requestShowContent(IBAN_LABEL);
        mCoordinator.destroy();

        verify(mMediator).destroy();
    }
}
