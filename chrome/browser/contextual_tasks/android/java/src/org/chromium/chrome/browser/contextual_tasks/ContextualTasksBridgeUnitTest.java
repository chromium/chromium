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
import android.graphics.Rect;
import android.view.Display;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feedback.FeedbackPolicyManager;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature.InitInfo;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.lang.ref.WeakReference;

@RunWith(BaseRobolectricTestRunner.class)
public class ContextualTasksBridgeUnitTest {
    private static final String TEST_URL = "https://example.com";
    private static final long TEST_NATIVE_BROWSER_WINDOW_INTERFACE_PTR = 5678L;
    private static final long TEST_NATIVE_BRIDGE_PTR = 1234L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private ActivityWindowAndroid mWindowAndroid;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private ContextualTasksBridge.Natives mMockJni;
    @Mock private HelpAndFeedbackLauncher mMockHelpAndFeedbackLauncher;
    @Mock private Activity mMockActivity;
    @Mock private WebContents mWebContents;
    @Mock private FeedbackPolicyManager mFeedbackPolicyManager;
    @Mock private Tab mTab;
    @Mock private Callback<String> mCallback;
    @Captor private ArgumentCaptor<Snackbar> mSnackbarCaptor;

    private ContextualTasksBridge mBridge;
    private final UnownedUserDataHost mUserDataHost = new UnownedUserDataHost();

    @Before
    public void setUp() {
        ContextualTasksBridgeJni.setInstanceForTesting(mMockJni);
        when(mMockJni.init(any(), eq(TEST_NATIVE_BROWSER_WINDOW_INTERFACE_PTR), eq(mProfile)))
                .thenReturn(TEST_NATIVE_BRIDGE_PTR);
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(mUserDataHost);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mMockActivity));

        mBridge = new ContextualTasksBridge(mProfile, mWindowAndroid);
        mBridge.onAddedToTask(
                new InitInfo(
                        TEST_NATIVE_BROWSER_WINDOW_INTERFACE_PTR,
                        /* isVisible= */ true,
                        new Rect(),
                        new Rect(),
                        Display.DEFAULT_DISPLAY));

        HelpAndFeedbackLauncherFactory.setInstanceForTesting(mMockHelpAndFeedbackLauncher);
        FeedbackPolicyManager.setInstanceForTesting(mFeedbackPolicyManager);
        when(mFeedbackPolicyManager.isUserFeedbackAllowed()).thenReturn(true);
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

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());

        Snackbar snackbar = mSnackbarCaptor.getValue();
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
    public void testOpenFeedbackUi_PolicyDisabled() {
        when(mFeedbackPolicyManager.isUserFeedbackAllowed()).thenReturn(false);
        mBridge.openFeedbackUi(TEST_URL);

        verify(mMockHelpAndFeedbackLauncher, never()).showFeedback(any(), any(), any());
    }

    @Test
    public void testUndoActionClickedAfterBridgeDestroyed() {
        SnackbarManagerProvider.attach(mWindowAndroid, mSnackbarManager);
        mBridge.showUndoSnackbar();

        mBridge.onFeatureRemoved();

        // Trigger action after clearing.
        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        snackbar.getController().onAction(snackbar.getActionData());

        // Should NOT call native.
        verify(mMockJni, never()).undoClose(anyLong());
    }

    @Test
    public void testGetTaskTitleForTab() {
        when(mTab.getWebContents()).thenReturn(mWebContents);

        ContextualTasksBridge.getTaskTitleForTab(mTab, mCallback);

        verify(mMockJni).getTaskTitleForTab(eq(mWebContents), eq(mCallback));
    }
}
