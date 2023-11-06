// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;
import android.widget.CompoundButton;
import android.widget.ImageView;
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
    @Mock CompoundButton.OnCheckedChangeListener mChangeListener;

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
        MenuItem item = mMenu.addItem(1, 0, "test item", Action.NONE);
        MenuItem recievedItem = mMenu.getItem(1);
        assertEquals(item, recievedItem);

        // check if itemId doesn't exist, get null
        assertEquals(mMenu.getItem(3), null);
    }

    @Test
    public void testExpandAction() {
        MenuItem item = mMenu.addItem(1, 0, "Expand action", Action.EXPAND);
        assertEquals(mMenu.getItem(1), item);
    }

    @Test
    public void testActionToggle() {
        // addItem and setValue
        MenuItem item = mMenu.addItem(1, 0, "Toggle action", Action.TOGGLE);
        item.setValue(true);
        SwitchCompat toggle = (SwitchCompat) item.findViewById(R.id.toggle_switch);
        assertTrue(toggle.isChecked());
        item.setValue(false);
        assertFalse(toggle.isChecked());

        // test setChangeListener()
        item.setChangeListener(mChangeListener);
        item.setValue(true);
        verify(mChangeListener).onCheckedChanged(any(CompoundButton.class), eq(true));
        item.setValue(false);
        verify(mChangeListener).onCheckedChanged(any(CompoundButton.class), eq(false));

        // test onClick()
        mMenu.setItemClickHandler(mHandler);
        item.setValue(false);
        assertTrue(item.getChildAt(0).performClick());
        assertTrue(toggle.isChecked());
        assertTrue(item.getChildAt(0).performClick());
        assertFalse(toggle.isChecked());

        verify(mHandler, times(2)).onResult(1);
    }

    @Test
    public void testActionRadio() {
        // addItem and setValue
        MenuItem item = mMenu.addItem(1, 0, "Radio action", Action.RADIO);
        item.setValue(true);
        RadioButton radioButton = (RadioButton) item.findViewById(R.id.readaloud_radio_button);
        assertTrue(radioButton.isChecked());
        item.setValue(false);
        assertFalse(radioButton.isChecked());

        // test setChangeListener()
        item.setChangeListener(mChangeListener);
        item.setValue(true);
        verify(mChangeListener, never()).onCheckedChanged(any(CompoundButton.class), eq(true));
        item.setValue(false);
        verify(mChangeListener, never()).onCheckedChanged(any(CompoundButton.class), eq(false));

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
        MenuItem item = mMenu.addItem(1, 0, "test item", Action.NONE);
        item.setItemEnabled(true);
        assertEquals(View.VISIBLE, item.getVisibility());
        item.setItemEnabled(false);
        assertEquals(View.GONE, item.getVisibility());
    }

    @Test
    public void testClearItems() {
        MenuItem item = mMenu.addItem(1, 0, "test item", Action.NONE);
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
        MenuItem item = mMenu.addItem(1, 0, "test item", Action.NONE);
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
        mMenu.onRadioButtonSelected(1);
        verify(mHandler).onResult(1);
    }
}
