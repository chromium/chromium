// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider.EducationalTipCardType;

/** Unit tests for {@link EducationalTipCardProviderTriggerState}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EducationalTipCardProviderTriggerStateUnitTest {
    private EducationalTipCardProviderTriggerState mEducationalTipCardProviderTriggerState;

    @Before
    public void setUp() {
        mEducationalTipCardProviderTriggerState = new EducationalTipCardProviderTriggerState();
    }

    @Test
    @SmallTest
    public void testShouldNotifyCardShownPerSession() {
        assertTrue(
                mEducationalTipCardProviderTriggerState.shouldNotifyCardShownPerSession(
                        EducationalTipCardType.TAB_GROUP));
        assertFalse(
                mEducationalTipCardProviderTriggerState.shouldNotifyCardShownPerSession(
                        EducationalTipCardType.TAB_GROUP));

        assertTrue(
                mEducationalTipCardProviderTriggerState.shouldNotifyCardShownPerSession(
                        EducationalTipCardType.TAB_GROUP_SYNC));
        assertFalse(
                mEducationalTipCardProviderTriggerState.shouldNotifyCardShownPerSession(
                        EducationalTipCardType.TAB_GROUP_SYNC));

        assertTrue(
                mEducationalTipCardProviderTriggerState.shouldNotifyCardShownPerSession(
                        EducationalTipCardType.QUICK_DELETE));
        assertFalse(
                mEducationalTipCardProviderTriggerState.shouldNotifyCardShownPerSession(
                        EducationalTipCardType.QUICK_DELETE));
    }
}
