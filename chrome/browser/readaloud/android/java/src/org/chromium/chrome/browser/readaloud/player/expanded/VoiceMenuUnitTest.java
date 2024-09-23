// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;
import android.widget.RadioButton;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.readaloud.player.InteractionHandler;
import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackVoice;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Unit tests for {@link VoiceMenuSheetContent}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class VoiceMenuUnitTest {
    @Mock private InteractionHandler mInteractionHandler;
    @Mock private Runnable mHideSelfRunnable;

    @Captor private ArgumentCaptor<PlaybackVoice> mVoiceCaptor;

    private Activity mActivity;
    private PropertyModel mModel;
    private VoiceMenu mContent;
    private Menu mMenu;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        // Need to set theme before inflating layout.
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mModel =
                new PropertyModel.Builder(PlayerProperties.ALL_KEYS)
                        .with(PlayerProperties.INTERACTION_HANDLER, mInteractionHandler)
                        .with(
                                PlayerProperties.VOICES_LIST,
                                List.of(
                                        createVoice("a", "voice a"),
                                        createVoice("b", "voice b"),
                                        createVoice("c", "voice c")))
                        .with(PlayerProperties.SELECTED_VOICE_ID, "a")
                        .build();
        mContent = new VoiceMenu(mActivity, mModel, mHideSelfRunnable, mActivity.getLayoutInflater());
        mMenu = mContent.getMenu();
    }

    @Test
    public void testTitle() {
        assertEquals("Voice", getText(mMenu, R.id.readaloud_menu_title));
    }

    @Test
    public void testSetVoices() {
        // First check the voices from setUp().
        MenuItem item0 = mMenu.getItem(0);
        assertEquals("voice a", getText(item0, R.id.item_label));
        assertNotNull(item0.findViewById(R.id.readaloud_radio_button));

        MenuItem item1 = mMenu.getItem(1);
        assertEquals("voice b", getText(item1, R.id.item_label));
        assertNotNull(item1.findViewById(R.id.readaloud_radio_button));

        MenuItem item2 = mMenu.getItem(2);
        assertEquals("voice c", getText(item2, R.id.item_label));
        assertNotNull(item2.findViewById(R.id.readaloud_radio_button));
        assertNull(mMenu.getItem(3));

        mContent.setVoices(
                List.of(
                        createVoice(
                                "a", "voice d", PlaybackVoice.Pitch.LOW, PlaybackVoice.Tone.SMOOTH),
                        createVoice(
                                "e", "voice e", PlaybackVoice.Pitch.MID, PlaybackVoice.Tone.CALM)));

        assertEquals("voice d", getText(mMenu.getItem(0), R.id.item_label));
        assertEquals("Low-pitch, Smooth", getText(mMenu.getItem(0), R.id.item_sublabel));
        assertEquals("voice e", getText(mMenu.getItem(1), R.id.item_label));
        assertEquals("Mid-pitch, Calm", getText(mMenu.getItem(1), R.id.item_sublabel));
        assertNull(mMenu.getItem(2));
    }

    @Test
    public void testSetVoiceSelection() {
        // Initial setting from setUp()
        assertTrue(getRadioButton(mMenu.getItem(0)).isChecked());

        mContent.setVoiceSelection("b");
        assertFalse(getRadioButton(mMenu.getItem(0)).isChecked());
        assertTrue(getRadioButton(mMenu.getItem(1)).isChecked());
        assertFalse(getRadioButton(mMenu.getItem(2)).isChecked());

        mContent.setVoiceSelection("c");
        assertFalse(getRadioButton(mMenu.getItem(0)).isChecked());
        assertFalse(getRadioButton(mMenu.getItem(1)).isChecked());
        assertTrue(getRadioButton(mMenu.getItem(2)).isChecked());
    }

    @Test
    public void testUserSelectsVoice() {
        mMenu.getItem(1).getChildAt(0).performClick();
        verify(mInteractionHandler, times(1)).onVoiceSelected(mVoiceCaptor.capture());

        PlaybackVoice voice = mVoiceCaptor.getValue();
        assertNotNull(voice);
        assertEquals("en", voice.getLanguage());
        assertEquals("b", voice.getVoiceId());
        assertEquals("voice b", voice.getDisplayName());
    }

    @Test
    public void testMenuItemInitialState() {
        assertEquals(View.GONE, mMenu.getItem(0).findViewById(R.id.spinner).getVisibility());
        assertEquals(View.VISIBLE, mMenu.getItem(0).findViewById(R.id.play_button).getVisibility());
    }

    @Test
    public void testShowPreviewSpinner() {
        mContent.updatePreviewButtons("a", PlaybackListener.State.BUFFERING);
        assertEquals(View.VISIBLE, mMenu.getItem(0).findViewById(R.id.spinner).getVisibility());
        assertEquals(View.GONE, mMenu.getItem(0).findViewById(R.id.play_button).getVisibility());
    }

    @Test
    public void testPlayPreview() {
        mContent.updatePreviewButtons("a", PlaybackListener.State.PLAYING);
        assertEquals(View.GONE, mMenu.getItem(0).findViewById(R.id.spinner).getVisibility());
        assertEquals(View.VISIBLE, mMenu.getItem(0).findViewById(R.id.play_button).getVisibility());
    }

    @Test
    public void testStopPreview() {
        mContent.updatePreviewButtons("a", PlaybackListener.State.STOPPED);
        assertEquals(View.GONE, mMenu.getItem(0).findViewById(R.id.spinner).getVisibility());
        assertEquals(View.VISIBLE, mMenu.getItem(0).findViewById(R.id.play_button).getVisibility());
    }

    @Test
    public void testClickPreviewButton() {
        mMenu.getItem(0).findViewById(R.id.play_button).performClick();
        verify(mInteractionHandler)
                .onPreviewVoiceClick(eq(mModel.get(PlayerProperties.VOICES_LIST).get(0)));
    }

    private static CharSequence getText(View ancestor, int viewId) {
        return ((TextView) ancestor.findViewById(viewId)).getText();
    }

    private static RadioButton getRadioButton(MenuItem item) {
        return (RadioButton) item.findViewById(R.id.readaloud_radio_button);
    }


    @Test
    public void testEmptyVoiceList() {
        mModel =
                new PropertyModel.Builder(PlayerProperties.ALL_KEYS)
                        .with(PlayerProperties.INTERACTION_HANDLER, mInteractionHandler)
                        .with(PlayerProperties.VOICES_LIST, List.of())
                        .with(PlayerProperties.SELECTED_VOICE_ID, "a")
                        .build();
        mContent = new VoiceMenu(mActivity, mModel, mHideSelfRunnable, mActivity.getLayoutInflater());
        mMenu = mContent.getMenu();

        assertNull(mMenu.getItem(0));
        // test there's no crash
        mContent.setVoiceSelection("fr");

        // now set voices and check that they're updated
        mContent.setVoices(
                List.of(
                        createVoice(
                                "d", "voice d", PlaybackVoice.Pitch.LOW, PlaybackVoice.Tone.SMOOTH),
                        createVoice(
                                "e", "voice e", PlaybackVoice.Pitch.MID, PlaybackVoice.Tone.CALM)));

        assertEquals("voice d", getText(mMenu.getItem(0), R.id.item_label));
        assertEquals("Low-pitch, Smooth", getText(mMenu.getItem(0), R.id.item_sublabel));
        assertEquals("voice e", getText(mMenu.getItem(1), R.id.item_label));
        assertEquals("Mid-pitch, Calm", getText(mMenu.getItem(1), R.id.item_sublabel));
    }

    @Test
    public void testLocaleLabel() {
        mContent.setVoices(
                List.of(
                        createVoice(
                                "a", "voice d", PlaybackVoice.Pitch.LOW, PlaybackVoice.Tone.SMOOTH),
                        createVoice(
                                "e", "voice e", PlaybackVoice.Pitch.MID, PlaybackVoice.Tone.CALM),
                        new PlaybackVoice(
                                "en",
                                "IN",
                                "in",
                                "voice in",
                                PlaybackVoice.Pitch.MID,
                                PlaybackVoice.Tone.CALM)));

        assertEquals("English (United States)", getText(mMenu.getItem(0), R.id.item_header));
        assertEquals("", getText(mMenu.getItem(1), R.id.item_header));
        assertEquals("English (India)", getText(mMenu.getItem(2), R.id.item_header));
    }

    private static PlaybackVoice createVoice(String id, String displayName) {
        return createVoice(id, displayName, PlaybackVoice.Pitch.NONE, PlaybackVoice.Tone.NONE);
    }

    private static PlaybackVoice createVoice(
            String id,
            String displayName,
            @PlaybackVoice.Pitch int pitch,
            @PlaybackVoice.Tone int tone) {
        return new PlaybackVoice("en", "US", id, displayName, pitch, tone);
    }
}
