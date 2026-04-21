// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.contextual_tasks.fusebox.ContextualTasksFuseboxManager;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link ContextualTasksUiServiceDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContextualTasksUiServiceDelegateUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private ContextualTasksUiServiceDelegate.Natives mMockJni;
    @Mock private HelpAndFeedbackLauncher mMockHelpAndFeedbackLauncher;
    @Mock private Activity mMockActivity;
    @Mock private ContextualTasksFuseboxManager mFuseboxManager;
    @Mock private WebContents mWebContents;

    private ContextualTasksUiServiceDelegate mDelegate;
    private final UnownedUserDataHost mUserDataHost = new UnownedUserDataHost();

    private static final String TEST_URL = "https://example.com";
    private static final String TEST_TASK_ID = "test-task-id";
    private static final long NATIVE_DELEGATE_PTR = 1234L;
    private static final long TEST_NATIVE_BROWSER_WINDOW_INTERFACE_PTR = 5678L;

    @Before
    public void setUp() {
        ContextualTasksUiServiceDelegateJni.setInstanceForTesting(mMockJni);
        mDelegate = ContextualTasksUiServiceDelegate.create(NATIVE_DELEGATE_PTR, mProfile);
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(mUserDataHost);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mMockActivity));
        HelpAndFeedbackLauncherFactory.setInstanceForTesting(mMockHelpAndFeedbackLauncher);
    }

    @Test
    public void testOnWebUIReady() {
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        ContextualTasksFuseboxManager.KEY.attachToHost(mUserDataHost, mFuseboxManager);

        mDelegate.onWebUIReady(TEST_TASK_ID, mWebContents);

        verify(mFuseboxManager).onWebUIReady(eq(TEST_TASK_ID), eq(mWebContents));
    }

    @Test
    public void testShowUndoSnackbar() {
        SnackbarManagerProvider.attach(mWindowAndroid, mSnackbarManager);

        mDelegate.showUndoSnackbar(mWindowAndroid, TEST_NATIVE_BROWSER_WINDOW_INTERFACE_PTR);

        verify(mSnackbarManager).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void testUndoActionCallsNative() {
        SnackbarManagerProvider.attach(mWindowAndroid, mSnackbarManager);

        mDelegate.showUndoSnackbar(mWindowAndroid, TEST_NATIVE_BROWSER_WINDOW_INTERFACE_PTR);

        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(snackbarCaptor.capture());

        Snackbar snackbar = snackbarCaptor.getValue();
        snackbar.getController().onAction(snackbar.getActionData());

        verify(mMockJni)
                .undoClose(eq(NATIVE_DELEGATE_PTR), eq(TEST_NATIVE_BROWSER_WINDOW_INTERFACE_PTR));
    }

    @Test
    public void testOpenFeedbackUi() {
        mDelegate.openFeedbackUi(mWindowAndroid, TEST_URL);

        verify(mMockHelpAndFeedbackLauncher)
                .showFeedback(eq(mMockActivity), eq(TEST_URL), eq("cobrowse"));
    }

    @Test
    public void testUndoActionClickedAfterDelegateDestroyed() {
        SnackbarManagerProvider.attach(mWindowAndroid, mSnackbarManager);
        mDelegate.showUndoSnackbar(mWindowAndroid, TEST_NATIVE_BROWSER_WINDOW_INTERFACE_PTR);

        mDelegate.clearNativePtr();

        // Trigger action after clearing.
        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(snackbarCaptor.capture());
        Snackbar snackbar = snackbarCaptor.getValue();
        snackbar.getController().onAction(snackbar.getActionData());

        // Should NOT call native.
        verify(mMockJni, org.mockito.Mockito.never()).undoClose(any(Long.class), any(Long.class));
    }
}
