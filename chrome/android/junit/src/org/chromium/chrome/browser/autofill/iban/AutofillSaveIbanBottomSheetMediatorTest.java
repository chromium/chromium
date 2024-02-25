// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
public final class AutofillSaveIbanBottomSheetMediatorTest {
    private static final String IBAN_LABEL = "CH56 0483 5012 3456 7800 9";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private AutofillSaveIbanBottomSheetMediator mMediator;

    @Mock private AutofillSaveIbanBottomSheetBridge mBridge;

    @Before
    public void setUp() {
        mMediator = new AutofillSaveIbanBottomSheetMediator(mBridge);
    }

    @Test
    public void testDestroy_doesNotCallOnUiIgnored_afterDestroy() {
        mMediator.destroy();
        verify(mBridge).onUiIgnored();

        mMediator.destroy();

        verifyNoMoreInteractions(mBridge);
    }
}
