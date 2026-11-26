// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.KeyEvent;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ttc.R;
import org.chromium.ui.base.TestActivity;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for {@link TtcShortcutRecorderDialog}. The C++ tests of ttc::ValidateShortcut() cover
 * which key presses make a valid shortcut.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class TtcShortcutRecorderDialogTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private TtcShortcutUtils.Natives mShortcutUtilsJni;

    private TestActivity mActivity;
    private final List<String> mSaved = new ArrayList<>();
    private int mDismissCount;
    private TtcShortcutRecorderDialog mRecorder;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        TtcShortcutUtilsJni.setInstanceForTesting(mShortcutUtilsJni);
        // A key press names no shortcut until a test calls setPressedShortcut().
        when(mShortcutUtilsJni.fromKeyEvent(any())).thenReturn("");
        when(mShortcutUtilsJni.getStatus("")).thenReturn(TtcShortcutStatus.INVALID);
        when(mShortcutUtilsJni.getKeyLabels(anyString())).thenReturn(new String[] {"Key"});
        // TTC's other shortcut, saved with its modifiers in another order than a key press gives.
        when(mShortcutUtilsJni.canonicalize("Shift+Alt+Space")).thenReturn("Alt+Shift+Space");
        mRecorder =
                new TtcShortcutRecorderDialog(
                        mActivity,
                        "Keyboard shortcut",
                        /* unavailableShortcut= */ "Shift+Alt+Space",
                        mSaved::add,
                        /* onDismiss= */ () -> mDismissCount++);
        mRecorder.show();
    }

    /** Makes key presses name {@code shortcut}, a shortcut pref value with {@code status}. */
    private void setPressedShortcut(String shortcut, @TtcShortcutStatus int status) {
        when(mShortcutUtilsJni.fromKeyEvent(any())).thenReturn(shortcut);
        when(mShortcutUtilsJni.getStatus(shortcut)).thenReturn(status);
    }

    private boolean press(int keyCode, int metaState) {
        AlertDialog dialog = mRecorder.getDialogForTesting();
        KeyEvent down = new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, keyCode, 0, metaState);
        boolean handled = mRecorder.onKey(dialog, keyCode, down);
        // Holding the key down repeats it.
        KeyEvent repeat = new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, keyCode, 1, metaState);
        assertEquals(handled, mRecorder.onKey(dialog, keyCode, repeat));
        KeyEvent up = new KeyEvent(0, 0, KeyEvent.ACTION_UP, keyCode, 0, metaState);
        mRecorder.onKey(dialog, keyCode, up);
        return handled;
    }

    private boolean isSaveEnabled() {
        return mRecorder.getDialogForTesting().getButton(AlertDialog.BUTTON_POSITIVE).isEnabled();
    }

    private String getMessage() {
        TextView message =
                mRecorder.getDialogForTesting().findViewById(R.id.ttc_shortcut_recorder_message);
        return message.getText().toString();
    }

    @Test
    public void testSaveDisabledUntilShortcutPressed() {
        assertFalse(isSaveEnabled());
        assertEquals(
                mActivity.getString(R.string.ttc_settings_shortcut_dialog_message), getMessage());
    }

    @Test
    public void testRecordsAndSavesShortcut() {
        setPressedShortcut("Ctrl+Shift+J", TtcShortcutStatus.VALID);
        when(mShortcutUtilsJni.getKeyLabels("Ctrl+Shift+J"))
                .thenReturn(new String[] {"Ctrl", "Shift", "J"});
        assertTrue(press(KeyEvent.KEYCODE_J, KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON));
        assertEquals("Ctrl+Shift+J", mRecorder.getRecordedShortcutForTesting());
        assertTrue(isSaveEnabled());
        // The message names the keys, for screen readers.
        assertEquals(
                mActivity.getString(R.string.ttc_settings_shortcut_dialog_recorded, "Ctrl+Shift+J"),
                getMessage());
        // The key press is only recorded once, however long the key is held.
        verify(mShortcutUtilsJni).fromKeyEvent(any());

        mRecorder.getDialogForTesting().getButton(AlertDialog.BUTTON_POSITIVE).performClick();
        ShadowLooper.idleMainLooper();
        assertEquals(List.of("Ctrl+Shift+J"), mSaved);
        assertFalse(mRecorder.getDialogForTesting().isShowing());
        assertEquals(1, mDismissCount);
    }

    @Test
    public void testDismissDoesNotSave() {
        setPressedShortcut("Ctrl+Shift+J", TtcShortcutStatus.VALID);
        press(KeyEvent.KEYCODE_J, KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON);

        mRecorder.dismiss();
        ShadowLooper.idleMainLooper();
        assertFalse(mRecorder.getDialogForTesting().isShowing());
        assertEquals(1, mDismissCount);
        assertTrue(mSaved.isEmpty());
    }

    @Test
    public void testKeyPressThatNamesNoShortcutDisablesSave() {
        setPressedShortcut("Ctrl+J", TtcShortcutStatus.VALID);
        press(KeyEvent.KEYCODE_J, KeyEvent.META_CTRL_ON);
        assertTrue(isSaveEnabled());

        // E.g. a key without Ctrl or Alt.
        when(mShortcutUtilsJni.fromKeyEvent(any())).thenReturn("");
        assertTrue(press(KeyEvent.KEYCODE_J, 0));
        assertNull(mRecorder.getRecordedShortcutForTesting());
        assertFalse(isSaveEnabled());
        assertEquals(
                mActivity.getString(R.string.ttc_settings_shortcut_dialog_invalid), getMessage());
    }

    @Test
    public void testInvalidShortcutIsRejected() {
        setPressedShortcut("Ctrl+Alt+J", TtcShortcutStatus.INVALID);
        assertTrue(press(KeyEvent.KEYCODE_J, KeyEvent.META_CTRL_ON | KeyEvent.META_ALT_ON));
        assertNull(mRecorder.getRecordedShortcutForTesting());
        assertFalse(isSaveEnabled());
        assertEquals(
                mActivity.getString(R.string.ttc_settings_shortcut_dialog_invalid), getMessage());
    }

    @Test
    public void testBrowserShortcutIsRejected() {
        setPressedShortcut("Ctrl+T", TtcShortcutStatus.CONFLICTS_WITH_BROWSER_SHORTCUT);
        assertTrue(press(KeyEvent.KEYCODE_T, KeyEvent.META_CTRL_ON));
        assertNull(mRecorder.getRecordedShortcutForTesting());
        assertFalse(isSaveEnabled());
        assertEquals(
                mActivity.getString(R.string.ttc_settings_shortcut_dialog_browser_shortcut),
                getMessage());
    }

    @Test
    public void testOtherTtcShortcutIsRejected() {
        // Though it was saved with its modifiers in another order.
        setPressedShortcut("Alt+Shift+Space", TtcShortcutStatus.VALID);
        press(KeyEvent.KEYCODE_SPACE, KeyEvent.META_ALT_ON | KeyEvent.META_SHIFT_ON);
        assertNull(mRecorder.getRecordedShortcutForTesting());
        assertFalse(isSaveEnabled());
        assertEquals(
                mActivity.getString(R.string.ttc_settings_shortcut_dialog_in_use), getMessage());
    }

    @Test
    public void testModifierPressAloneChangesNothing() {
        assertTrue(press(KeyEvent.KEYCODE_ALT_LEFT, KeyEvent.META_ALT_ON));
        verify(mShortcutUtilsJni, never()).fromKeyEvent(any());
        assertNull(mRecorder.getRecordedShortcutForTesting());
        assertEquals(
                mActivity.getString(R.string.ttc_settings_shortcut_dialog_message), getMessage());
    }

    @Test
    public void testNavigationKeysAreLeftToTheDialog() {
        assertFalse(press(KeyEvent.KEYCODE_BACK, 0));
        assertFalse(press(KeyEvent.KEYCODE_ESCAPE, 0));
        assertFalse(press(KeyEvent.KEYCODE_TAB, 0));
        assertFalse(press(KeyEvent.KEYCODE_TAB, KeyEvent.META_SHIFT_ON));
        assertFalse(press(KeyEvent.KEYCODE_ENTER, 0));
        assertFalse(press(KeyEvent.KEYCODE_SPACE, 0));
        verify(mShortcutUtilsJni, never()).fromKeyEvent(any());

        // With Ctrl or Alt, Tab is part of a shortcut.
        setPressedShortcut("Alt+Tab", TtcShortcutStatus.VALID);
        assertTrue(press(KeyEvent.KEYCODE_TAB, KeyEvent.META_ALT_ON));
        assertEquals("Alt+Tab", mRecorder.getRecordedShortcutForTesting());
    }

    @Test
    public void testNavigationKeysWithOtherModifiersAreRecorded() {
        // Only Shift+Tab is left to the dialog. With Shift or Meta, the other navigation keys and
        // Meta+Tab make an invalid shortcut.
        int[][] presses = {
            {KeyEvent.KEYCODE_SPACE, KeyEvent.META_SHIFT_ON},
            {KeyEvent.KEYCODE_ENTER, KeyEvent.META_SHIFT_ON},
            {KeyEvent.KEYCODE_DPAD_DOWN, KeyEvent.META_SHIFT_ON},
            {KeyEvent.KEYCODE_SPACE, KeyEvent.META_META_ON},
            {KeyEvent.KEYCODE_TAB, KeyEvent.META_META_ON},
        };
        for (int[] keyPress : presses) {
            assertTrue(press(keyPress[0], keyPress[1]));
            assertFalse(isSaveEnabled());
            assertEquals(
                    mActivity.getString(R.string.ttc_settings_shortcut_dialog_invalid),
                    getMessage());
        }
    }
}
