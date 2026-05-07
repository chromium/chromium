// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
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
import org.chromium.ui.base.ActivityWindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link ContextualTasksBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContextualTasksBridgeUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private ActivityWindowAndroid mWindowAndroid;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private ContextualTasksBridge.Natives mMockJni;
    @Mock private HelpAndFeedbackLauncher mMockHelpAndFeedbackLauncher;
    @Mock private Activity mMockActivity;
    @Mock private ContextualTasksFuseboxManager mFuseboxManager;
    @Mock private WebContents mWebContents;

    private ContextualTasksBridge mBridge;
    private final UnownedUserDataHost mUserDataHost = new UnownedUserDataHost();

    private static final String TEST_URL = "https://example.com";
    private static final String TEST_TASK_ID = "test-task-id";
    private static final long TEST_NATIVE_BROWSER_WINDOW_INTERFACE_PTR = 5678L;
    private static final long TEST_NATIVE_BRIDGE_PTR = 1234L;

    @Before
    public void setUp() {
        ContextualTasksBridgeJni.setInstanceForTesting(mMockJni);
        when(mMockJni.init(any(), eq(TEST_NATIVE_BROWSER_WINDOW_INTERFACE_PTR), eq(mProfile)))
                .thenReturn(TEST_NATIVE_BRIDGE_PTR);
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(mUserDataHost);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mMockActivity));

        mBridge = new ContextualTasksBridge(mProfile, mWindowAndroid);
        mBridge.onAddedToTask(TEST_NATIVE_BROWSER_WINDOW_INTERFACE_PTR);

        HelpAndFeedbackLauncherFactory.setInstanceForTesting(mMockHelpAndFeedbackLauncher);
    }

    @Test
    public void testOnWebUIReady() {
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        ContextualTasksFuseboxManager.KEY.attachToHost(mUserDataHost, mFuseboxManager);

        mBridge.onWebUIReady(TEST_TASK_ID, mWebContents);

        verify(mFuseboxManager).onWebUIReady(eq(TEST_TASK_ID), eq(mWebContents));
    }

    @Test
    public void testOnWebUIDestroyed() {
        ContextualTasksFuseboxManager.KEY.attachToHost(mUserDataHost, mFuseboxManager);

        mBridge.onWebUIDestroyed(TEST_TASK_ID);

        verify(mFuseboxManager).onWebUIDestroyed(eq(TEST_TASK_ID));
    }

    @Test
    public void testOnTaskChanged() {
        ContextualTasksFuseboxManager.KEY.attachToHost(mUserDataHost, mFuseboxManager);
        String oldTaskId = "old-task-id";
        String newTaskId = "new-task-id";

        mBridge.onTaskChanged(oldTaskId, newTaskId);

        verify(mFuseboxManager).onTaskChanged(eq(oldTaskId), eq(newTaskId));
    }

    @Test
    public void testShowUndoSnackbar() {
        SnackbarManagerProvider.attach(mWindowAndroid, mSnackbarManager);

        mBridge.showUndoSnackbar();

        verify(mSnackbarManager).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void testUndoActionCallsNative() {
        SnackbarManagerProvider.attach(mWindowAndroid, mSnackbarManager);

        mBridge.showUndoSnackbar();

        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(snackbarCaptor.capture());

        Snackbar snackbar = snackbarCaptor.getValue();
        snackbar.getController().onAction(snackbar.getActionData());

        verify(mMockJni).undoClose(eq(TEST_NATIVE_BRIDGE_PTR));
    }

    @Test
    public void testOpenFeedbackUi() {
        mBridge.openFeedbackUi(TEST_URL);

        verify(mMockHelpAndFeedbackLauncher)
                .showFeedback(eq(mMockActivity), eq(TEST_URL), eq("cobrowse"));
    }

    @Test
    public void testUndoActionClickedAfterBridgeDestroyed() {
        SnackbarManagerProvider.attach(mWindowAndroid, mSnackbarManager);
        mBridge.showUndoSnackbar();

        mBridge.onFeatureRemoved();

        // Trigger action after clearing.
        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(snackbarCaptor.capture());
        Snackbar snackbar = snackbarCaptor.getValue();
        snackbar.getController().onAction(snackbar.getActionData());

        // Should NOT call native.
        verify(mMockJni, never()).undoClose(anyLong());
    }
}
