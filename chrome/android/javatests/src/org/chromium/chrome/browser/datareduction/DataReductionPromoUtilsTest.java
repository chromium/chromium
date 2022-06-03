// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.datareduction;

import android.content.Context;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.FieldTrialList;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.about_settings.AboutSettingsBridge;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.test.ChromeBrowserTestRule;

/**
 * Unit test suite for DataReductionPromoUtils.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class DataReductionPromoUtilsTest {
    @Rule
    public final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    private static final String SHARED_PREF_DISPLAYED_INFOBAR_PROMO_VERSION =
            "displayed_data_reduction_infobar_promo_version";

    private Context mContext;

    @Before
    public void setUp() {
        // Using an AdvancedMockContext allows us to use a fresh in-memory SharedPreference.
        mContext = new AdvancedMockContext(InstrumentationRegistry.getInstrumentation()
                                                   .getTargetContext()
                                                   .getApplicationContext());
        ContextUtils.initApplicationContextForTests(mContext);
    }

    /**
     * Tests that promos cannot be shown if the data reduction proxy is enabled.
     */
    @Test
    @SmallTest
    @UiThreadTest
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
            "force-fieldtrials=DataCompressionProxyPromoVisibility/Enabled"})
    @Feature({"DataReduction"})
    public void
    testCanShowPromos() {
        if (DataReductionProxySettings.getInstance().isDataReductionProxyManaged()) return;
        Assert.assertFalse(DataReductionProxySettings.getInstance().isDataReductionProxyEnabled());

        // In some unknown cases, the force-fieldtrials flag may not be effective. This may possibly
        // be because this test runs on UiThread.
        if (!FieldTrialList.findFullName("DataCompressionProxyPromoVisibility").equals("Enabled")) {
            return;
        }
        Assert.assertTrue(
                DataReductionProxySettings.getInstance().isDataReductionProxyPromoAllowed());
        Assert.assertTrue(DataReductionPromoUtils.canShowPromos());
        DataReductionProxySettings.getInstance().setDataReductionProxyEnabled(mContext, true);
        Assert.assertFalse(DataReductionPromoUtils.canShowPromos());
    }

    /**
     * Tests that saving the first run experience and second run promo state updates the pref and
     * also stores the application version. Tests that the first run experience opt out pref is
     * updated.
     */
    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"DataReduction"})
    public void testFreOrSecondRunPromoDisplayed() {
        // The first run experience or second run promo should not have been shown yet.
        Assert.assertFalse(DataReductionPromoUtils.getDisplayedFreOrSecondRunPromo());

        // Save that the first run experience or second run promo has been displayed.
        DataReductionPromoUtils.saveFreOrSecondRunPromoDisplayed();
        Assert.assertTrue(DataReductionPromoUtils.getDisplayedFreOrSecondRunPromo());
        Assert.assertFalse(DataReductionPromoUtils.getDisplayedInfoBarPromo());
        Assert.assertFalse(DataReductionPromoUtils.getOptedOutOnFrePromo());
        Assert.assertEquals(AboutSettingsBridge.getApplicationVersion(),
                DataReductionPromoUtils.getDisplayedFreOrSecondRunPromoVersion());
    }

    /**
     * Tests that the first run experience opt out pref is updated.
     */
    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"DataReduction"})
    public void testFrePromoOptOut() {
        // Save that the user opted out of the first run experience.
        DataReductionPromoUtils.saveFrePromoOptOut(true);
        Assert.assertTrue(DataReductionPromoUtils.getOptedOutOnFrePromo());

        // Save that the user did not opt out of the first run experience.
        DataReductionPromoUtils.saveFrePromoOptOut(false);
        Assert.assertFalse(DataReductionPromoUtils.getOptedOutOnFrePromo());
    }

    /**
     * Tests that saving the infobar promo state updates the pref and also stores the application
     * version.
     */
    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"DataReduction"})
    public void testInfoBarPromoDisplayed() {
        // The infobar should not have been shown yet.
        Assert.assertFalse(DataReductionPromoUtils.getDisplayedInfoBarPromo());

        // Save that the infobar promo has been displayed.
        DataReductionPromoUtils.saveInfoBarPromoDisplayed();
        Assert.assertFalse(DataReductionPromoUtils.getDisplayedFreOrSecondRunPromo());
        Assert.assertTrue(DataReductionPromoUtils.getDisplayedInfoBarPromo());
        Assert.assertFalse(DataReductionPromoUtils.getOptedOutOnFrePromo());
        Assert.assertEquals(AboutSettingsBridge.getApplicationVersion(),
                ContextUtils.getAppSharedPreferences().getString(
                        SHARED_PREF_DISPLAYED_INFOBAR_PROMO_VERSION, ""));
    }
}
