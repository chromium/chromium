// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;

import android.graphics.Color;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.test.util.DeviceRestriction;

/** Tests for the Settings menu. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Tests cannot run batched because they launch a Settings activity.")
public class SettingsActivityTest {
    @Rule
    public SettingsActivityTestRule<MainSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(MainSettings.class);

    @After
    public void tearDown() {
        mSettingsActivityTestRule.getActivity().finish();
    }

    /** Test status bar is always black in Automotive devices. */
    @Test
    @SmallTest
    @Feature({"StatusBar, Automotive Toolbar"})
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_AUTO)
    public void testStatusBarBlackInAutomotive() {
        mSettingsActivityTestRule.startSettingsActivity();
        assertEquals(
                "Status bar should always be black in automotive devices.",
                Color.BLACK,
                mSettingsActivityTestRule.getActivity().getWindow().getStatusBarColor());
    }
}
