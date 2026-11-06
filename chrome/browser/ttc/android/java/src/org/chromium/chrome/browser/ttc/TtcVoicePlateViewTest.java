// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.view.PointerIcon;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageButton;

import androidx.core.view.ViewCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.floating_pill.FloatingPillView;

/** Unit tests for {@link TtcVoicePlateView}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TtcVoicePlateViewTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View.OnClickListener mOnClickListener;

    private Activity mActivity;
    private FrameLayout mParent;
    private TtcVoicePlateView mPlate;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mParent = new FrameLayout(mActivity);
        mActivity.setContentView(mParent);
        mPlate = TtcVoicePlateView.create(mParent);
    }

    @Test
    public void testCreateAndDestroy() {
        assertEquals(1, mParent.getChildCount());
        View root = mParent.getChildAt(0);
        assertEquals(View.GONE, root.getVisibility());

        mPlate.setVisible(true);
        assertEquals(View.VISIBLE, root.getVisibility());
        mPlate.setVisible(false);
        assertEquals(View.GONE, root.getVisibility());

        mPlate.destroy();
        assertEquals(0, mParent.getChildCount());
        assertNull(root.getParent());
    }

    @Test
    public void testPauseButton() {
        ImageButton pause = mPlate.getPauseButtonForTesting();
        String pauseLabel = mActivity.getString(R.string.ttc_voice_plate_pause);
        assertEquals(pauseLabel, pause.getContentDescription());
        assertEquals(pauseLabel, pause.getTooltipText());
        assertEquals(
                PointerIcon.getSystemIcon(mActivity, PointerIcon.TYPE_HAND),
                pause.getPointerIcon());

        mPlate.setOnPauseClickListener(mOnClickListener);
        pause.performClick();
        verify(mOnClickListener).onClick(pause);
    }

    @Test
    public void testMutedShowsResume() {
        ImageButton pause = mPlate.getPauseButtonForTesting();
        String resumeLabel = mActivity.getString(R.string.ttc_voice_plate_resume);

        mPlate.setMicMuted(true);
        assertEquals(resumeLabel, pause.getContentDescription());
        assertEquals(resumeLabel, pause.getTooltipText());
        assertEquals(
                R.drawable.ic_play_arrow_white_24dp,
                shadowOf(pause.getDrawable()).getCreatedFromResId());

        mPlate.setMicMuted(false);
        assertEquals(
                mActivity.getString(R.string.ttc_voice_plate_pause), pause.getContentDescription());
        assertEquals(
                R.drawable.ic_pause_white_24dp,
                shadowOf(pause.getDrawable()).getCreatedFromResId());
    }

    @Test
    public void testStatusReportedToAccessibility() {
        FloatingPillView pill = mPlate.getPillForTesting();
        View waveform = mPlate.getWaveformForTesting();
        String title = mActivity.getString(R.string.ttc_voice_plate_title);
        assertEquals(title, ViewCompat.getAccessibilityPaneTitle(pill));
        // Screen readers can focus the waveform, and hear the status as it changes. The pill,
        // which also holds the pause button, isn't a live region.
        assertEquals(title, waveform.getContentDescription());
        assertTrue(ViewCompat.isScreenReaderFocusable(waveform));
        assertEquals(
                ViewCompat.ACCESSIBILITY_LIVE_REGION_POLITE,
                ViewCompat.getAccessibilityLiveRegion(waveform));
        assertEquals(
                ViewCompat.ACCESSIBILITY_LIVE_REGION_NONE,
                ViewCompat.getAccessibilityLiveRegion(pill));

        mPlate.setStatusText(R.string.ttc_session_listening);
        assertEquals(
                mActivity.getString(R.string.ttc_session_listening),
                ViewCompat.getStateDescription(waveform));
    }
}
