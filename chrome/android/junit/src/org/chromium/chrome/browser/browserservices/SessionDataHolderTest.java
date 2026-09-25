// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;

import androidx.browser.customtabs.CustomTabsIntent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.TranslucentCustomTabActivity;

/** Unit tests for {@link SessionDataHolder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SessionDataHolderTest {

    private static final int TASK_ID_1 = 10;
    private static final int TASK_ID_2 = 20;

    private Intent mIntent1;
    private Intent mIntent2;
    private SessionHolder mSession1;
    private SessionHolder mSession2;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock CustomTabsConnection mConnection;
    @Mock SessionHandler mHandler1;
    @Mock SessionHandler mHandler2;
    @Mock Activity mActivityInTask1;
    @Mock Activity mActivityInTask2;
    @Captor ArgumentCaptor<Callback<SessionHolder>> mDisconnectCallbackCaptor;

    @Before
    public void setUp() {
        CustomTabsConnection.setInstanceForTesting(mConnection);
        mIntent1 = createIntentWithSessionId(1);
        mSession1 = SessionHolder.getSessionHolderFromIntent(mIntent1);
        mIntent2 = createIntentWithSessionId(2);
        mSession2 = SessionHolder.getSessionHolderFromIntent(mIntent2);
        doReturn(mSession1).when(mHandler1).getSession();
        doReturn(mSession2).when(mHandler2).getSession();
        doReturn(CustomTabActivity.class).when(mHandler1).getActivityClass();
        doReturn(TranslucentCustomTabActivity.class).when(mHandler2).getActivityClass();
        when(mActivityInTask1.getTaskId()).thenReturn(TASK_ID_1);
        when(mActivityInTask2.getTaskId()).thenReturn(TASK_ID_2);
        doNothing().when(mConnection).setDisconnectCallback(mDisconnectCallbackCaptor.capture());
        SessionDataHolder.setInstanceForTesting(new SessionDataHolder());
    }

    private Intent createIntentWithSessionId(int id) {
        PendingIntent pi =
                PendingIntent.getActivity(RuntimeEnvironment.systemContext, id, new Intent(), 0);
        return new Intent().putExtra(CustomTabsIntent.EXTRA_SESSION_ID, pi);
    }

    @Test
    public void returnsActiveHandler_IfSessionsMatch() {
        startActivity1();
        assertEquals(mHandler1, SessionDataHolder.getInstance().getActiveHandler(mSession1));
    }

    @Test
    public void doesntReturnActiveHandler_IfSessionsDontMatch() {
        startActivity2();
        assertNull(SessionDataHolder.getInstance().getActiveHandler(mSession1));
    }

    @Test
    public void doesntReturnActiveHandler_IfItWasRemoved() {
        startActivity1();
        stopActivity1();
        assertNull(SessionDataHolder.getInstance().getActiveHandler(mSession1));
    }

    @Test
    public void switchingFromOneCustomTabToAnother_MakesTheSecondOneTheActiveHandler() {
        startActivity1();

        // The order is important: onStart of foregrounded activity is called before onStop of the
        // backgrounded one.
        startActivity2();
        stopActivity1();
        assertEquals(mHandler2, SessionDataHolder.getInstance().getActiveHandler(mSession2));
    }

    @Test
    public void returnsActivityClassOfActiveHandler_WithMatchingSession() {
        when(mHandler1.getTaskId()).thenReturn(TASK_ID_1);
        startActivity1();
        Class<? extends Activity> activity =
                SessionDataHolder.getInstance()
                        .getActiveHandlerClassInCurrentTask(mIntent1, mActivityInTask1);
        assertEquals(CustomTabActivity.class, activity);
    }

    @Test
    public void returnsActivityClassOfActiveHandler_EvenIfItsNotFocused() {
        when(mHandler1.getTaskId()).thenReturn(TASK_ID_1);
        startActivity1();
        stopActivity1();

        // New intent arrives, bringing task 1 to foreground, but onStart was not yet called.
        Class<? extends Activity> activity =
                SessionDataHolder.getInstance()
                        .getActiveHandlerClassInCurrentTask(mIntent1, mActivityInTask1);
        assertEquals(CustomTabActivity.class, activity);
    }

    @Test
    public void returnsNullActivityClass_IfSessionsDontMatch() {
        when(mHandler1.getTaskId()).thenReturn(TASK_ID_1);
        startActivity1();

        Class<? extends Activity> activity =
                SessionDataHolder.getInstance()
                        .getActiveHandlerClassInCurrentTask(mIntent2, mActivityInTask1);
        assertNull(activity);
    }

    @Test
    public void returnsNullActivityClass_IfActivityWithMatchingSession_IsInAnotherTask() {
        when(mHandler1.getTaskId()).thenReturn(TASK_ID_1);
        when(mHandler2.getTaskId()).thenReturn(TASK_ID_2);

        startActivity1();
        startActivity2();

        Class<? extends Activity> activity =
                SessionDataHolder.getInstance()
                        .getActiveHandlerClassInCurrentTask(mIntent2, mActivityInTask1);
        assertNull(activity);
    }

    @Test
    public void returnsNullActivityClass_IfActivityWithMatchingSession_IsNotTopmostInTask() {
        when(mHandler1.getTaskId()).thenReturn(TASK_ID_1);
        when(mHandler2.getTaskId()).thenReturn(TASK_ID_1); // Note: same task.

        startActivity1();
        startActivity2();

        Class<? extends Activity> activity =
                SessionDataHolder.getInstance()
                        .getActiveHandlerClassInCurrentTask(mIntent1, mActivityInTask1);
        assertNull(activity);
    }

    @Test
    public void returnsNullActivityClass_IfSessionWasTerminated() {
        when(mHandler1.getTaskId()).thenReturn(TASK_ID_1);
        startActivity1();
        disconnect(mSession1);
        Class<? extends Activity> activity =
                SessionDataHolder.getInstance()
                        .getActiveHandlerClassInCurrentTask(mIntent1, mActivityInTask1);
        assertNull(activity);
    }

    @Test
    public void getHandlerForIntent_ReturnsHandlerAcrossTasks_EvenWhenUnfocused() {
        when(mHandler1.getTaskId()).thenReturn(TASK_ID_1);
        when(mHandler2.getTaskId()).thenReturn(TASK_ID_2);

        startActivity1();
        startActivity2();
        stopActivity1();

        assertNull(SessionDataHolder.getInstance().getActiveHandlerForIntent(mIntent1));
        assertEquals(mHandler1, SessionDataHolder.getInstance().getHandlerForIntent(mIntent1));
        assertEquals(mHandler2, SessionDataHolder.getInstance().getHandlerForIntent(mIntent2));
    }

    @Test
    public void getHandlerForSession_ReturnsNull_WhenHandlerRemovedFromTask() {
        when(mHandler1.getTaskId()).thenReturn(TASK_ID_1);
        startActivity1();
        SessionDataHolder.getInstance().removeHandlerFromTask(mHandler1);

        assertNull(SessionDataHolder.getInstance().getHandlerForSession(mSession1));
    }

    @Test
    public void removeHandlerFromTask_KeepsActivityClassForClearTopRouting() {
        // E.g. "Don't keep activities": the activity is destroyed but its task survives, so new
        // intents must still be routed into it with CLEAR_TOP.
        when(mHandler1.getTaskId()).thenReturn(TASK_ID_1);
        startActivity1();
        stopActivity1();
        SessionDataHolder.getInstance().removeHandlerFromTask(mHandler1);

        assertEquals(
                CustomTabActivity.class,
                SessionDataHolder.getInstance()
                        .getActiveHandlerClassInCurrentTask(mIntent1, mActivityInTask1));
    }

    @Test
    public void removeHandlerFromTask_DoesNotAffectNewerHandlerInSameTask() {
        // A relaunch with CLEAR_TOP registers the new instance before the old one is destroyed.
        when(mHandler1.getTaskId()).thenReturn(TASK_ID_1);
        when(mHandler2.getTaskId()).thenReturn(TASK_ID_1);
        doReturn(mSession1).when(mHandler2).getSession();
        startActivity1();
        startActivity2();
        SessionDataHolder.getInstance().removeActiveHandler(mHandler2);
        SessionDataHolder.getInstance().removeHandlerFromTask(mHandler1);

        assertEquals(mHandler2, SessionDataHolder.getInstance().getHandlerForSession(mSession1));
        assertEquals(
                TranslucentCustomTabActivity.class,
                SessionDataHolder.getInstance()
                        .getActiveHandlerClassInCurrentTask(mIntent1, mActivityInTask1));
    }

    @Test
    public void getHandlerForSession_ReturnsNull_WhenSessionDisconnected() {
        when(mHandler1.getTaskId()).thenReturn(TASK_ID_1);
        startActivity1();
        stopActivity1();
        disconnect(mSession1);

        assertNull(SessionDataHolder.getInstance().getHandlerForSession(mSession1));
    }

    @Test
    public void disconnect_RemovesAllTaskEntriesForSession() {
        // Same session in two adjacent tasks (TASK_ID_1 < TASK_ID_2). A forward loop using
        // removeAt(i) would skip the second entry after removing the first one.
        when(mHandler1.getTaskId()).thenReturn(TASK_ID_1);
        when(mHandler2.getTaskId()).thenReturn(TASK_ID_2);
        doReturn(mSession1).when(mHandler2).getSession();
        SessionDataHolder holder = SessionDataHolder.getInstance();

        startActivity1();
        startActivity2();
        holder.removeActiveHandler(mHandler2);
        assertEquals(
                CustomTabActivity.class,
                holder.getActiveHandlerClassInCurrentTask(mIntent1, mActivityInTask1));
        assertEquals(
                TranslucentCustomTabActivity.class,
                holder.getActiveHandlerClassInCurrentTask(mIntent1, mActivityInTask2));

        disconnect(mSession1);

        assertNull(holder.getActiveHandlerClassInCurrentTask(mIntent1, mActivityInTask1));
        assertNull(holder.getActiveHandlerClassInCurrentTask(mIntent1, mActivityInTask2));
        assertNull(holder.getHandlerForSession(mSession1));
    }

    private void disconnect(SessionHolder session) {
        Callback<SessionHolder> callback = mDisconnectCallbackCaptor.getValue();
        if (callback != null) {
            callback.onResult(session);
        }
    }

    private void startActivity1() {
        SessionDataHolder.getInstance().setActiveHandler(mHandler1);
    }

    private void startActivity2() {
        SessionDataHolder.getInstance().setActiveHandler(mHandler2);
    }

    private void stopActivity1() {
        SessionDataHolder.getInstance().removeActiveHandler(mHandler1);
    }
}
