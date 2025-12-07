// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.appcompat.app.AlertDialog;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for the DialogControllerImpl class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DialogControllerImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Mock private WebContents mWebContents;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Activity mActivity;

    @Mock private DialogControllerImpl.AlertDialogFactory mAlertDialogFactory;
    @Mock private AlertDialog.Builder mAlertDialog;
    @Mock private Callback<String> mDenyCallback;
    @Mock private Runnable mApproveCallback;

    private DialogControllerImpl mDialogController;

    @Before
    public void setUp() {
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getVisibility()).thenReturn(Visibility.VISIBLE);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference(mActivity));

        when(mAlertDialogFactory.createAlertDialogBuilder(any(), anyInt()))
                .thenReturn(mAlertDialog);
        when(mAlertDialog.setMessage(any(String.class))).thenReturn(mAlertDialog);
        when(mAlertDialog.setMessage(anyInt())).thenReturn(mAlertDialog);
        when(mAlertDialog.setTitle(anyInt())).thenReturn(mAlertDialog);
        when(mAlertDialog.setPositiveButton(anyInt(), any())).thenReturn(mAlertDialog);
        when(mAlertDialog.setNegativeButton(anyInt(), any())).thenReturn(mAlertDialog);
        when(mAlertDialog.setOnCancelListener(any())).thenReturn(mAlertDialog);

        mDialogController = new DialogControllerImpl(mWebContents, mAlertDialogFactory);
    }

    /** The dialog controller can show an IS_READY_TO_PAY debug information dialog. */
    @Test
    public void testShowDebugDialog() {
        mDialogController.showReadyToPayDebugInfo("Debug info");

        verify(mAlertDialogFactory).createAlertDialogBuilder(eq(mActivity), anyInt());
        verify(mAlertDialog).setMessage(eq("Debug info"));
        verify(mAlertDialog).show();
    }

    /** Cannot show the debug dialog if the web contents are destroyed. */
    @Test
    public void testCannotShowDebugDialogInDestroyedWebContents() {
        when(mWebContents.isDestroyed()).thenReturn(true);

        mDialogController.showReadyToPayDebugInfo("Debug info");

        verify(mAlertDialogFactory, never()).createAlertDialogBuilder(any(), anyInt());
    }

    /** Cannot show the debug dialog if the web contents are hidden. */
    @Test
    public void testCannotShowDebugDialogInHiddenWebContents() {
        when(mWebContents.getVisibility()).thenReturn(Visibility.HIDDEN);

        mDialogController.showReadyToPayDebugInfo("Debug info");

        verify(mAlertDialogFactory, never()).createAlertDialogBuilder(any(), anyInt());
    }

    /** Cannot show the debug dialog if the web contents are not attached to an Android window. */
    @Test
    public void testCannotShowDebugDialogInWebContentsWithoutTopLevelNativeWindow() {
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(null);

        mDialogController.showReadyToPayDebugInfo("Debug info");

        verify(mAlertDialogFactory, never()).createAlertDialogBuilder(any(), anyInt());
    }

    /** Cannot show the debug dialog if the web contents are not attached an Android activity. */
    @Test
    public void testCannotShowDebugDialogInWebContentsWithoutActivity() {
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference(null));

        mDialogController.showReadyToPayDebugInfo("Debug info");

        verify(mAlertDialogFactory, never()).createAlertDialogBuilder(any(), anyInt());
    }

    /** The dialog controller can show a warning dialog for leaving incognito mode. */
    @Test
    public void testShowIncognitoWarning() {
        mDialogController.showLeavingIncognitoWarning(mDenyCallback, mApproveCallback);

        verify(mAlertDialogFactory).createAlertDialogBuilder(eq(mActivity), anyInt());
        verify(mAlertDialog).setTitle(anyInt());
        verify(mAlertDialog).setMessage(anyInt());
        verify(mAlertDialog).show();
    }

    /** Cannot show the incognito warning dialog if the web contents are destroyed. */
    @Test
    public void testCannotShowIncognitoWarningInDestroyedWebContents() {
        when(mWebContents.isDestroyed()).thenReturn(true);

        mDialogController.showLeavingIncognitoWarning(mDenyCallback, mApproveCallback);

        verify(mDenyCallback).onResult("Unable to find Chrome activity.");
        verify(mAlertDialogFactory, never()).createAlertDialogBuilder(any(), anyInt());
    }

    /** Cannot show the incognito warning dialog if the web contents are hidden. */
    @Test
    public void testCannotShowIncognitoWarningInHiddenWebContents() {
        when(mWebContents.getVisibility()).thenReturn(Visibility.HIDDEN);

        mDialogController.showLeavingIncognitoWarning(mDenyCallback, mApproveCallback);

        verify(mDenyCallback).onResult("Unable to find Chrome activity.");
        verify(mAlertDialogFactory, never()).createAlertDialogBuilder(any(), anyInt());
    }

    /**
     * Cannot show the incognito warning dialog if the web contents are not attached to an Android
     * window.
     */
    @Test
    public void testCannotShowIncognitoWarningInWebContentsWithoutTopLevelNativeWindow() {
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(null);

        mDialogController.showLeavingIncognitoWarning(mDenyCallback, mApproveCallback);

        verify(mDenyCallback).onResult("Unable to find Chrome activity.");
        verify(mAlertDialogFactory, never()).createAlertDialogBuilder(any(), anyInt());
    }

    /**
     * Cannot show the incognito warning dialog if the web contents are not attached to an Android
     * activity.
     */
    @Test
    public void testCannotShowIncognitoWarningInWebContentsWithoutActivity() {
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference(null));

        mDialogController.showLeavingIncognitoWarning(mDenyCallback, mApproveCallback);

        verify(mDenyCallback).onResult("Unable to find Chrome activity.");
        verify(mAlertDialogFactory, never()).createAlertDialogBuilder(any(), anyInt());
    }
}
