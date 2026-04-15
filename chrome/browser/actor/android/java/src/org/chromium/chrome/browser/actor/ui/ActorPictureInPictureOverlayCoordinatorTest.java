// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.actor.ActorTaskState;

/** Tests for {@link ActorPictureInPictureOverlayCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ActorPictureInPictureOverlayCoordinatorTest {
    private Activity mActivity;
    private FrameLayout mParentView;
    private ActorPictureInPictureOverlayCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mParentView = new FrameLayout(mActivity);
        mCoordinator = new ActorPictureInPictureOverlayCoordinator(mActivity, mParentView);
    }

    @Test
    public void testInitialization_AttachesView() {
        assertEquals(1, mParentView.getChildCount());
        View view = mParentView.getChildAt(0);
        assertTrue(view instanceof ActorPictureInPictureOverlayView);
    }

    @Test
    public void testUpdateTitle_UpdatesTextView() {
        String testTitle = "Booking a flight";
        mCoordinator.updateTitle(testTitle);

        TextView titleTextView = mParentView.findViewById(R.id.pip_title);
        assertEquals(testTitle, titleTextView.getText().toString());
    }

    @Test
    public void testUpdateStatus_WorkingState() {
        mCoordinator.updateStatus(ActorTaskState.ACTING);

        TextView statusTextView = mParentView.findViewById(R.id.pip_status);
        String expectedStatus = mActivity.getString(R.string.actor_pip_working_status);
        assertEquals(expectedStatus, statusTextView.getText().toString());
    }

    @Test
    public void testUpdateStatus_WaitingOnUserState() {
        mCoordinator.updateStatus(ActorTaskState.WAITING_ON_USER);

        TextView statusTextView = mParentView.findViewById(R.id.pip_status);
        String expectedStatus = mActivity.getString(R.string.actor_pip_needs_attention_status);
        assertEquals(expectedStatus, statusTextView.getText().toString());
    }

    @Test
    public void testUpdateStatus_PausedState() {
        mCoordinator.updateStatus(ActorTaskState.PAUSED_BY_USER);

        TextView statusTextView = mParentView.findViewById(R.id.pip_status);
        String expectedStatus = mActivity.getString(R.string.actor_pip_paused_status);
        assertEquals(expectedStatus, statusTextView.getText().toString());
    }

    @Test
    public void testUpdateStatus_CompleteState() {
        mCoordinator.updateStatus(ActorTaskState.FINISHED);

        TextView statusTextView = mParentView.findViewById(R.id.pip_status);
        String expectedStatus = mActivity.getString(R.string.actor_pip_complete_status);
        assertEquals(expectedStatus, statusTextView.getText().toString());
    }

    @Test
    public void testUpdateStatus_InterruptedState() {
        mCoordinator.updateStatus(ActorTaskState.CANCELLED);

        TextView statusTextView = mParentView.findViewById(R.id.pip_status);
        String expectedStatus =
                mActivity.getString(R.string.actor_notification_title_task_interrupted);
        assertEquals(expectedStatus, statusTextView.getText().toString());
    }

    @Test
    public void testDestroy_RemovesView() {
        assertEquals(1, mParentView.getChildCount());
        mCoordinator.destroy();
        assertEquals(0, mParentView.getChildCount());
    }
}
