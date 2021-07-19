// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quickactionsearchwidget;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.Intent;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.IntentUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.ui.quickactionsearchwidget.QuickActionSearchWidgetReceiverDelegate;

/**
 * Tests for (@link QuickActionSearchWidgetReceiver}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class QuickActionSearchWidgetReceiverTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private Context mContextMock;
    @Mock
    private QuickActionSearchWidgetReceiverDelegate mDelegateMock;

    private QuickActionSearchWidgetReceiver mWidgetReceiver;

    @Before
    public void setUp() {
        FirstRunStatus.setFirstRunFlowComplete(true);

        mWidgetReceiver = Mockito.spy(new QuickActionSearchWidgetReceiver());
        mWidgetReceiver.setDelegateForTesting(mDelegateMock);
    }

    @After
    public void tearDown() {
        IntentUtils.setForceIsTrustedIntentForTesting(false);
    }

    @Test
    @SmallTest
    public void testNonChromeIntentsDoNotInvokeHandleAction() {
        Intent nonChromeIntent = new Intent("SOME_NON_CHROME_INTENT");

        mWidgetReceiver.onReceive(mContextMock, nonChromeIntent);

        verify(mDelegateMock, never()).handleAction(any(), any());
    }

    @Test
    @SmallTest
    public void testTrustedChromeIntentsInvokesHandleAction() {
        Intent trustedChromeIntent = new Intent("SOME_CHROME_INTENT");
        IntentUtils.setForceIsTrustedIntentForTesting(true);

        mWidgetReceiver.onReceive(mContextMock, trustedChromeIntent);

        verify(mDelegateMock, times(1)).handleAction(mContextMock, trustedChromeIntent);
        verify(mDelegateMock, times(1)).handleAction(any(), any());
    }
}
