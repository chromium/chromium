// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.add_username_dialog;

import static org.mockito.Mockito.verify;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/** Tests for {@link AddUsernameDialogBridge} */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class AddUsernameDialogBridgeTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private AddUsernameDialogBridge.Natives mBridgeJniMock;
    @Mock
    private WindowAndroid mWindowAndroid;

    private static final long sTestNativePointer = 1;

    private FakeModalDialogManager mModalDialogManager = new FakeModalDialogManager(0);
    private AddUsernameDialogBridge mBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mJniMocker.mock(AddUsernameDialogBridgeJni.TEST_HOOKS, mBridgeJniMock);

        mBridge = new AddUsernameDialogBridge(sTestNativePointer, mWindowAndroid);
    }

    @Test
    public void testOnDialogDismissed() {
        mBridge.onDialogDismissed();
        verify(mBridgeJniMock).onDialogDismissed(sTestNativePointer);

        AssertionError exception = null;
        try {
            mBridge.onDialogDismissed();
        } catch (AssertionError e) {
            exception = e;
        }
        Assert.assertNotNull(exception);
        Assert.assertEquals(
                "mNativeAddUsernameDialogBridge must not be null", exception.getMessage());
    }

    @Test
    public void testOnDialogAccepted() {
        String username = "username";
        mBridge.onDialogAccepted(username);
        verify(mBridgeJniMock).onDialogAccepted(sTestNativePointer, username);
    }
}
