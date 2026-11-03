// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for {@link SigninFirstRunFragment}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SigninFirstRunFragmentUnitTest {
    @Test
    @DisableFeatures(ChromeFeatureList.FRE_STRING_CHANGES)
    public void testFreStrings_flagDisabled() {
        assertEquals(R.string.signin_fre_title, SigninFirstRunFragment.getTitleStringId());
        assertEquals(R.string.signin_fre_subtitle, SigninFirstRunFragment.getSubtitleStringId());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.FRE_STRING_CHANGES)
    public void testFreStrings_flagEnabledWithoutParams() {
        assertEquals(R.string.signin_fre_title, SigninFirstRunFragment.getTitleStringId());
        assertEquals(R.string.signin_fre_subtitle, SigninFirstRunFragment.getSubtitleStringId());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.FRE_STRING_CHANGES
                    + ":"
                    + ChromeFeatureList.FRE_STRING_CHANGES_PERSONALIZED
                    + "/true")
    public void testFreStrings_personalizedArm() {
        assertEquals(
                R.string.signin_fre_personalized_title, SigninFirstRunFragment.getTitleStringId());
        assertEquals(
                R.string.signin_fre_personalized_subtitle,
                SigninFirstRunFragment.getSubtitleStringId());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.FRE_STRING_CHANGES
                    + ":"
                    + ChromeFeatureList.FRE_STRING_CHANGES_SAFETY
                    + "/true")
    public void testFreStrings_safetyArm() {
        assertEquals(R.string.signin_fre_safety_title, SigninFirstRunFragment.getTitleStringId());
        assertEquals(
                R.string.signin_fre_safety_subtitle, SigninFirstRunFragment.getSubtitleStringId());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.FRE_STRING_CHANGES
                    + ":"
                    + ChromeFeatureList.FRE_STRING_CHANGES_SEAMLESSNESS
                    + "/true")
    public void testFreStrings_seamlessnessArm() {
        assertEquals(
                R.string.signin_fre_seamlessness_title, SigninFirstRunFragment.getTitleStringId());
        assertEquals(
                R.string.signin_fre_seamlessness_subtitle,
                SigninFirstRunFragment.getSubtitleStringId());
    }
}
