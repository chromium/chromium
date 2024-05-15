// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;
import android.view.View.AccessibilityDelegate;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RadioButton;

import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.SwitchCompat;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.chrome.browser.readaloud.player.expanded.MenuItem.Action;

/** Unit tests for {@link Menu}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MenuUnitTest {
    private final Activity mActivity;
    private Menu mMenu;
    @Mock Callback<Integer> mHandler;
    @Mock Callback<Boolean> mToggleHandler;

    public MenuUnitTest() {
        mActivity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        // Need to set theme before inflating layout.
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mMenu = (Menu) mActivity.getLayoutInflater().inflate(R.layout.readaloud_menu, null);
        assertNotNull(mMenu);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testAddGetItem() {
        MenuItem item = mMenu.addItem(1, 0, "test item", /* header= */ null, Action.NONE);
        MenuItem recievedItem = mMenu.getItem(1);
        assertEquals(item, recievedItem);

        // check if itemId doesn't exist, get null
        assertEquals(mMenu.getItem(3), null);
    }

    @Test
    public void testExpandAction() {
        MenuItem item = mMenu.addItem(1, 0, "Expand action", /* header= */ null, Action.EXPAND);
        assertEquals(mMenu.getItem(1), item);
    }

    @Test
    public void testActionToggle() {
        // addItem and setValue
        MenuItem item = mMenu.addItem(1, 0, "Toggle action", /* header= */ null, Action.TOGGLE);
        item.setValue(true);
        SwitchCompat toggle = (SwitchCompat) item.findViewById(R.id.toggle_switch);
        assertTrue(toggle.isChecked());
        item.setValue(false);
        assertFalse(toggle.isChecked());

        // test toggle listener
        item.setToggleHandler(mToggleHandler);
        item.setValue(false);
        // change the value
        assertTrue(item.getChildAt(0).performClick());
        verify(mToggleHandler).onResult(true);
    }

    @Test
    public void testActionRadio() {
        // addItem and setValue
        MenuItem item = mMenu.addItem(1, 0, "Radio action", /* header= */ null, Action.RADIO);
        item.setValue(true);
        RadioButton radioButton = (RadioButton) item.findViewById(R.id.readaloud_radio_button);
        assertTrue(radioButton.isChecked());
        item.setValue(false);
        assertFalse(radioButton.isChecked());

        // test onClick()
        mMenu.setItemClickHandler(mHandler);
        item.setValue(false);
        assertTrue(item.getChildAt(0).performClick());
        assertTrue(radioButton.isChecked());
        // clicking a checked radio button should leave it checked.
        assertTrue(item.getChildAt(0).performClick());
        assertTrue(radioButton.isChecked());

        verify(mHandler, times(2)).onResult(1);
    }

    @Test
    public void testSetItemEnabled() {
        MenuItem item = mMenu.addItem(1, 0, "test item", /* header= */ null, Action.TOGGLE);
        item.setToggleHandler(mToggleHandler);
        item.setItemEnabled(false);

        item.performClick();
        verify(mToggleHandler, never()).onResult(anyBoolean());
    }

    @Test
    public void testClearItems() {
        MenuItem item = mMenu.addItem(1, 0, "test item", /* header= */ null, Action.NONE);
        assertEquals(item, mMenu.getItem(1));
        mMenu.clearItems();
        assertEquals(null, mMenu.getItem(1));
    }

    @Test
    public void testOnItemClicked() {
        mMenu.setItemClickHandler(mHandler);
        mMenu.onItemClicked(1);
        verify(mHandler).onResult(1);
    }

    @Test
    public void testAddPlayButton_OnPlayButtonClicked() {
        mMenu.setPlayButtonClickHandler(mHandler);
        MenuItem item = mMenu.addItem(1, 0, "test item", /* header= */ null, Action.NONE);
        item.addPlayButton();
        ImageView playButton = (ImageView) item.findViewById(R.id.play_button);
        assertEquals(View.VISIBLE, playButton.getVisibility());

        assertTrue(playButton.performClick());
        verify(mHandler, times(1)).onResult(1);

        reset(mHandler);

        mMenu.onPlayButtonClicked(1);
        verify(mHandler, times(1)).onResult(1);
    }

    @Test
    public void testOnRadioButtonSelected() {
        mMenu.setRadioTrueHandler(mHandler);
        for (int i = 0; i < 3; i++) {
            mMenu.addItem(i, 0, "test item", /* header= */ null, Action.RADIO);
        }
        mMenu.onRadioButtonSelected(0);
        mMenu.onRadioButtonSelected(1);
        verify(mHandler).onResult(1);
        assertFalse(
                ((RadioButton) mMenu.getItem(0).findViewById(R.id.readaloud_radio_button))
                        .isChecked());
        assertFalse(
                ((RadioButton) mMenu.getItem(2).findViewById(R.id.readaloud_radio_button))
                        .isChecked());
    }

    @Test
    public void testMenuItemLayoutInflated() {
        MenuItem item = mMenu.addItem(1, 0, "testToggle", /* header= */ null, Action.TOGGLE);
        LinearLayout layout =
                (LinearLayout)
                        mActivity.getLayoutInflater().inflate(R.layout.readaloud_menu_item, null);
        item.getLayoutSupplier().set(layout);
        SwitchCompat button = (SwitchCompat) item.findViewById(R.id.toggle_switch);
        assertNotNull(button);

        // tests if onInitializeAccessibilityEvent is properly setting the event's checked state to
        // match the button's checked state
        AccessibilityDelegate accessibilityDelegate = layout.getAccessibilityDelegate();
        assertNotNull(accessibilityDelegate);

        AccessibilityEvent event = AccessibilityEvent.obtain();
        event.setAction(AccessibilityEvent.TYPE_VIEW_CLICKED);

        item.setValue(true);
        accessibilityDelegate.onInitializeAccessibilityEvent(layout, event);
        assertEquals(button.isChecked(), event.isChecked());

        item.setValue(false);
        accessibilityDelegate.onInitializeAccessibilityEvent(layout, event);
        assertEquals(button.isChecked(), event.isChecked());

        // tests if onInitializeAccessibilityNodeInfo is properly setting the event's checked state
        // to match the button's checked state and the same for checkable state
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();

        item.setValue(true);
        item.setItemEnabled(true);
        accessibilityDelegate.onInitializeAccessibilityNodeInfo(layout, info);
        assertEquals(button.isChecked(), info.isChecked());
        assertEquals(button.isEnabled(), info.isEnabled());

        item.setValue(false);
        item.setItemEnabled(false);
        accessibilityDelegate.onInitializeAccessibilityNodeInfo(layout, info);
        assertEquals(button.isChecked(), info.isChecked());
        assertEquals(button.isEnabled(), info.isEnabled());
    }

    @Test
    public void testMenuItemLayoutInflated_NothingForActionExpand() {
        MenuItem item = mMenu.addItem(1, 0, "testExpand", /* header= */ null, Action.EXPAND);
        LinearLayout layout =
                (LinearLayout)
                        mActivity.getLayoutInflater().inflate(R.layout.readaloud_menu_item, null);
        item.getLayoutSupplier().set(layout);

        // accessibility delegate will be null for action items without buttons
        AccessibilityDelegate accessibilityDelegate = layout.getAccessibilityDelegate();
        assertNull(accessibilityDelegate);
    }
}
