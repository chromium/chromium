// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import android.support.test.filters.MediumTest;
import android.support.test.rule.ActivityTestRule;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.test.ScreenShooter;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests for the appearance of Incognito Disclosure Dialog.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class IncognitoDisclosureDialogAppearanceTest {
    @Rule
    public ActivityTestRule<IncognitoDisclosureActivity> mActivityTestRule =
            new ActivityTestRule<>(IncognitoDisclosureActivity.class, true, true);

    @Rule
    public ScreenShooter mScreenShooter = new ScreenShooter();

    @Test
    @MediumTest
    @Feature({"CustomTabs"})
    public void shootAppearanceOfDialog() {
        Assert.assertNotNull(mActivityTestRule.getActivity());
        mScreenShooter.shoot("Incognito disclosure dialog");
    }
}
