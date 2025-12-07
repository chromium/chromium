// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.safe_browsing;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.password_manager.PasswordManagerDialogContents;
import org.chromium.chrome.browser.password_manager.PasswordManagerDialogCoordinator;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for SafeBrowsingPasswordReuseDialogBridge. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SafeBrowsingPasswordReuseDialogBridgeTest {
    private static final String TITLE = "title";
    private static final String DETAILS = "details";
    private static final String PRIMARY_BUTTON_TEXT = "primaryButtonText";
    private static final String SECONDARY_BUTTON_TEXT = "secondaryButtonText";

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private PasswordManagerDialogCoordinator mDialogCoordinator;
    @Mock private ChromeActivity mActivity;
    private WeakReference<Activity> mActivityRef;
    private SafeBrowsingPasswordReuseDialogBridge mDialog;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Before
    public void setUp() {
        mActivityRef = new WeakReference<>(mActivity);
        when(mWindowAndroid.getActivity()).thenReturn(mActivityRef);
        mDialog =
                SafeBrowsingPasswordReuseDialogBridge.createForTests(
                        mWindowAndroid,
                        /* nativePasswordReuseDialogViewAndroid= */ 1L,
                        mDialogCoordinator);
    }

    @Test
    public void testTextIsSavedToDialogContents_ensureAllTextIsPresent() {
        mDialog.showDialog(TITLE, DETAILS, PRIMARY_BUTTON_TEXT, SECONDARY_BUTTON_TEXT);

        PasswordManagerDialogContents contents = mDialog.getPasswordManagerDialogContentsForTests();
        Assert.assertEquals(TITLE, contents.getTitle());
        Assert.assertEquals(DETAILS, contents.getDetails());
        Assert.assertEquals(PRIMARY_BUTTON_TEXT, contents.getPrimaryButtonText());
        Assert.assertEquals(SECONDARY_BUTTON_TEXT, contents.getSecondaryButtonText());

        verify(mDialogCoordinator).initialize(any(), any());
        verify(mDialogCoordinator).showDialog();
    }

    @Test
    public void testTextIsSavedToDialogContents_ensureSeconaryButtonTextIsNull() {
        mDialog.showDialog(TITLE, DETAILS, PRIMARY_BUTTON_TEXT, "");

        PasswordManagerDialogContents contents = mDialog.getPasswordManagerDialogContentsForTests();
        Assert.assertEquals(TITLE, contents.getTitle());
        Assert.assertEquals(DETAILS, contents.getDetails());
        Assert.assertEquals(PRIMARY_BUTTON_TEXT, contents.getPrimaryButtonText());
        Assert.assertNull(contents.getSecondaryButtonText());

        verify(mDialogCoordinator).initialize(any(), any());
        verify(mDialogCoordinator).showDialog();
    }
}
