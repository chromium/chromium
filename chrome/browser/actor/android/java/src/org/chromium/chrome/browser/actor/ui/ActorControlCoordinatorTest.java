// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ChromeImageButton;

/** Tests for {@link ActorControlCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ActorControlCoordinatorTest {
    private Activity mActivity;
    private FrameLayout mParentView;
    private ActorControlCoordinator mCoordinator;
    private PropertyModel mModel;
    private ActorControlMediator mMediator;

    @Mock private View.OnClickListener mPlayPauseListener;
    @Mock private View.OnClickListener mCloseListener;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mParentView = new FrameLayout(mActivity);
        mActivity.setContentView(mParentView);

        mPlayPauseListener = mock(View.OnClickListener.class);
        mCloseListener = mock(View.OnClickListener.class);

        mCoordinator =
                new ActorControlCoordinator(
                        mActivity, mParentView, mPlayPauseListener, mCloseListener);

        mModel = mCoordinator.getModelForTesting();
        mMediator = mCoordinator.getMediatorForTesting();

        ShadowLooper.idleMainLooper();
    }

    @Test
    public void testInitialization() {
        assertNotNull(mModel);
        assertEquals(mPlayPauseListener, mModel.get(ActorControlProperties.ON_PLAY_PAUSE_CLICKED));
        assertEquals(mCloseListener, mModel.get(ActorControlProperties.ON_CLOSE_CLICKED));
    }

    @Test
    public void testStatusIconUpdates_PausedToPlaying() {
        ActorControlView view = (ActorControlView) mParentView.getChildAt(0);
        ChromeImageButton statusButton = view.findViewById(R.id.actor_control_status_button);

        // Set to Paused (should show Play Arrow)
        mMediator.updateStatusIcon(true);

        assertEquals(
                R.drawable.ic_play_arrow_white_24dp,
                mModel.get(ActorControlProperties.STATUS_ICON_RESOURCE));

        assertEquals(
                R.drawable.ic_play_arrow_white_24dp,
                Shadows.shadowOf(statusButton.getDrawable()).getCreatedFromResId());

        // Set to Playing (should show Pause icon)
        mMediator.updateStatusIcon(false);

        assertEquals(
                R.drawable.ic_pause_white_24dp,
                mModel.get(ActorControlProperties.STATUS_ICON_RESOURCE));

        assertEquals(
                R.drawable.ic_pause_white_24dp,
                Shadows.shadowOf(statusButton.getDrawable()).getCreatedFromResId());
    }

    @Test
    public void testContentUpdates() {
        String testTitle = "Task in Progress";
        String testDesc = "Step 4 of 10";

        mMediator.setContent(testTitle, testDesc);

        ActorControlView view = (ActorControlView) mParentView.getChildAt(0);
        TextView titleView = view.findViewById(R.id.actor_control_title);
        TextView descView = view.findViewById(R.id.actor_control_description);

        assertEquals(testTitle, titleView.getText().toString());
        assertEquals(testDesc, descView.getText().toString());
    }

    @Test
    public void testPlayPauseClickTriggered() {
        ActorControlView view = (ActorControlView) mParentView.getChildAt(0);
        ChromeImageButton playPauseButton = view.findViewById(R.id.actor_control_status_button);

        playPauseButton.performClick();
        verify(mPlayPauseListener).onClick(playPauseButton);
    }

    @Test
    public void testCloseClickTriggered() {
        ActorControlView view = (ActorControlView) mParentView.getChildAt(0);
        ChromeImageButton closeButton = view.findViewById(R.id.actor_control_close_button);

        closeButton.performClick();
        verify(mCloseListener).onClick(closeButton);
    }

}
