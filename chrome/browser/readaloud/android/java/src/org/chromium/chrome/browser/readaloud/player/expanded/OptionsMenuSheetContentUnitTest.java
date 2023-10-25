// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.Context;

import androidx.appcompat.app.AppCompatActivity;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.chrome.browser.readaloud.player.expanded.OptionsMenuSheetContent.Item;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link OptionsMenuSheetContent}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OptionsMenuSheetContentUnitTest {
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private ExpandedPlayerSheetContent mBottomSheetContent;
    @Mock private PropertyModel mModel;
    private Activity mActivity;
    private Context mContext;
    private Menu mMenu;
    private OptionsMenuSheetContent mContent;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = ApplicationProvider.getApplicationContext();
        mActivity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        // Need to set theme before inflating layout.
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mMenu = (Menu) mActivity.getLayoutInflater().inflate(R.layout.readaloud_menu, null);
        mContent =
                new OptionsMenuSheetContent(
                        mContext, mBottomSheetContent, mBottomSheetController, mMenu, mModel);
    }

    @Test
    public void testSetup() {
        assertTrue(mMenu.getItem(Item.VOICE) != null);
        assertTrue(mMenu.getItem(Item.TRANSLATE) != null);
        assertTrue(mMenu.getItem(Item.HIGHLIGHT) != null);
    }
}
