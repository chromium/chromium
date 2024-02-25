// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.add_username_dialog;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

import java.lang.ref.WeakReference;

/** Tests for {@link AddUsernameDialogBridge} */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class AddUsernameDialogBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private AddUsernameDialogBridge.Natives mBridgeJniMock;
    @Mock private WindowAndroid mWindowAndroid;

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
        mBridge.onDialogDismissed();
        verify(mBridgeJniMock, times(1)).onDialogDismissed(sTestNativePointer);
    }

    @Test
    public void testOnDialogAccepted() {
        String username = "username";
        mBridge.onDialogAccepted(username);
        verify(mBridgeJniMock).onDialogAccepted(sTestNativePointer, username);
    }

    @Test
    public void testDialogIsDismissedFromNative() {
        when(mWindowAndroid.getModalDialogManager()).thenReturn(mModalDialogManager);
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<Context>(createActivity()));

        mBridge.showAddUsernameDialog("username");
        assertThat(mModalDialogManager.getShownDialogModel()).isNotNull();
        mBridge.dismiss();
        assertThat(mModalDialogManager.getShownDialogModel()).isNull();
    }

    private static AppCompatActivity createActivity() {
        ActivityController<AppCompatActivity> activityController =
                Robolectric.buildActivity(AppCompatActivity.class);
        // Need to setTheme to Activity in Robolectric test or will get exception: You need to use a
        // Theme.AppCompat theme (or descendant) with this activity.
        activityController.get().setTheme(R.style.Theme_AppCompat_Light);
        return activityController.create().start().resume().visible().get();
    }
}
