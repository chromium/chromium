// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;

/** Tests for the {@link ChromeNextTabPolicySupplier}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
// TODO(b/555414915): Update Android tests with WebUI NTP enabled on AL.
@DisableFeatures(ChromeFeatureList.USE_WEB_UI_NTP_ANDROID)
public class ChromeNextTabPolicySupplierTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Test
    @SmallTest
    public void verifyOverviewModeBehaviorIsNotNull() {
        RegularNewTabPageStation ntp = mActivityTestRule.startOnNtp();
        Assert.assertNotNull(ntp.getActivity().getNextTabPolicySupplier().getLayoutStateProvider());
    }
}
