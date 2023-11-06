// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.TranslucentCustomTabActivity;

/** Unit tests for {@link SessionDataHolder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SessionDataHolderTest {

    private static final int TASK_ID_1 = 10;
    private static final int TASK_ID_2 = 20;

    private Intent mIntent1;
    private Intent mIntent2;
    private CustomTabsSessionToken mSession1;
    private CustomTabsSessionToken mSession2;

    @Mock CustomTabsConnection mConnection;
    @Mock SessionHandler mHandler1;
    @Mock SessionHandler mHandler2;
    @Mock Activity mActivityInTask1;
    @Mock Activity mActivityInTask2;
    @Captor ArgumentCaptor<Callback<CustomTabsSessionToken>> mDisconnectCallbackCaptor;

    private SessionDataHolder mHolder;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mIntent1 = createIntentWithSessionId(1);
        mSession1 = CustomTabsSessionToken.getSessionTokenFromIntent(mIntent1);
        mIntent2 = createIntentWithSessionId(2);
        mSession2 = CustomTabsSessionToken.getSessionTokenFromIntent(mIntent2);
        when(mHandler1.getSession()).thenReturn(mSession1);
        when(mHandler2.getSession()).thenReturn(mSession2);
        when(mHandler1.getActivityClass()).thenReturn((Class) CustomTabActivity.class);
        when(mHandler2.getActivityClass()).thenReturn((Class) TranslucentCustomTabActivity.class);
        when(mActivityInTask1.getTaskId()).thenReturn(TASK_ID_1);
        when(mActivityInTask2.getTaskId()).thenReturn(TASK_ID_2);
        doNothing().when(mConnection).setDisconnectCallback(mDisconnectCallbackCaptor.capture());
        mHolder = new SessionDataHolder(() -> mConnection);
    }

    private Intent createIntentWithSessionId(int id) {
        PendingIntent pi =
                PendingIntent.getActivity(RuntimeEnvironment.systemContext, id, new Intent(), 0);
        return new Intent().putExtra(CustomTabsIntent.EXTRA_SESSION_ID, pi);
    }

    @Test
    public void returnsActiveHandler_IfSessionsMatch() {
        startActivity1();
        assertEquals(mHandler1, mHolder.getActiveHandler(mSession1));
    }

    @Test
    public void doesntReturnActiveHandler_IfSessionsDontMatch() {
        startActivity2();
        assertNull(mHolder.getActiveHandler(mSession1));
    }

    @Test
    public void doesntReturnActiveHandler_IfItWasRemoved() {
        startActivity1();
        stopActivity1();
        assertNull(mHolder.getActiveHandler(mSession1));
    }

    @Test
    public void switchingFromOneCustomTabToAnother_MakesTheSecondOneTheActiveHandler() {
        startActivity1();

        // The order is important: onStart of foregrounded activity is called before onStop of the
        // backgrounded one.
        startActivity2();
        stopActivity1();
        assertEquals(mHandler2, mHolder.getActiveHandler(mSession2));
    }

    @Test
    public void returnsActivityClassOfActiveHandler_WithMatchingSession() {
        when(mHandler1.getTaskId()).thenReturn(TASK_ID_1);
        startActivity1();
        Class<? extends Activity> activity =
                mHolder.getActiveHandlerClassInCurrentTask(mIntent1, mActivityInTask1);
        assertEquals(CustomTabActivity.class, activity);
    }

    @Test
    public void returnsActivityClassOfActiveHandler_EvenIfItsNotFocused() {
        when(mHandler1.getTaskId()).thenReturn(TASK_ID_1);
        startActivity1();
        stopActivity1();

        // New intent arrives, bringing task 1 to foreground, but onStart was not yet called.
        Class<? extends Activity> activity =
                mHolder.getActiveHandlerClassInCurrentTask(mIntent1, mActivityInTask1);
        assertEquals(CustomTabActivity.class, activity);
    }

    @Test
    public void returnsNullActivityClass_IfSessionsDontMatch() {
        when(mHandler1.getTaskId()).thenReturn(TASK_ID_1);
        startActivity1();

        Class<? extends Activity> activity =
                mHolder.getActiveHandlerClassInCurrentTask(mIntent2, mActivityInTask1);
        assertNull(activity);
    }

    @Test
    public void returnsNullActivityClass_IfActivityWithMatchingSession_IsInAnotherTask() {
        when(mHandler1.getTaskId()).thenReturn(TASK_ID_1);
        when(mHandler2.getTaskId()).thenReturn(TASK_ID_2);

        startActivity1();
        startActivity2();

        Class<? extends Activity> activity =
                mHolder.getActiveHandlerClassInCurrentTask(mIntent2, mActivityInTask1);
        assertNull(activity);
    }

    @Test
    public void returnsNullActivityClass_IfActivityWithMatchingSession_IsNotTopmostInTask() {
        when(mHandler1.getTaskId()).thenReturn(TASK_ID_1);
        when(mHandler2.getTaskId()).thenReturn(TASK_ID_1); // Note: same task.

        startActivity1();
        startActivity2();

        Class<? extends Activity> activity =
                mHolder.getActiveHandlerClassInCurrentTask(mIntent1, mActivityInTask1);
        assertNull(activity);
    }

    @Test
    public void returnsNullActivityClass_IfSessionWasTerminated() {
        when(mHandler1.getTaskId()).thenReturn(TASK_ID_1);
        startActivity1();
        disconnect(mSession1);
        Class<? extends Activity> activity =
                mHolder.getActiveHandlerClassInCurrentTask(mIntent1, mActivityInTask1);
        assertNull(activity);
    }

    private void disconnect(CustomTabsSessionToken session) {
        Callback<CustomTabsSessionToken> callback = mDisconnectCallbackCaptor.getValue();
        if (callback != null) {
            callback.onResult(session);
        }
    }

    private void startActivity1() {
        mHolder.setActiveHandler(mHandler1);
    }

    private void startActivity2() {
        mHolder.setActiveHandler(mHandler2);
    }

    private void stopActivity1() {
        mHolder.removeActiveHandler(mHandler1);
    }
}
