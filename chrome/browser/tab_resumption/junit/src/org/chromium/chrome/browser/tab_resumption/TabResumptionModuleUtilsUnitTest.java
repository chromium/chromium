// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.res.Resources;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.JUnitProcessor;

import java.util.concurrent.TimeUnit;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabResumptionModuleUtilsUnitTest {
    @Rule public JUnitProcessor mFeaturesProcessor = new JUnitProcessor();

    @Test
    @SmallTest
    public void testRecencyString() {
        Resources res = ApplicationProvider.getApplicationContext().getResources();
        long dayInMs = TimeUnit.DAYS.toMillis(1);
        Assert.assertEquals("1 min ago", TabResumptionModuleUtils.getRecencyString(res, -1000000L));
        Assert.assertEquals("1 min ago", TabResumptionModuleUtils.getRecencyString(res, 0L));
        Assert.assertEquals("1 min ago", TabResumptionModuleUtils.getRecencyString(res, 59999L));
        Assert.assertEquals("1 min ago", TabResumptionModuleUtils.getRecencyString(res, 60000L));
        Assert.assertEquals("1 min ago", TabResumptionModuleUtils.getRecencyString(res, 119999L));
        Assert.assertEquals("2 min ago", TabResumptionModuleUtils.getRecencyString(res, 120000L));
        Assert.assertEquals("59 min ago", TabResumptionModuleUtils.getRecencyString(res, 3599999L));
        Assert.assertEquals("1 hr ago", TabResumptionModuleUtils.getRecencyString(res, 3600000L));
        Assert.assertEquals("1 hr ago", TabResumptionModuleUtils.getRecencyString(res, 7199999L));
        Assert.assertEquals("2 hr ago", TabResumptionModuleUtils.getRecencyString(res, 7200000L));
        Assert.assertEquals(
                "23 hr ago", TabResumptionModuleUtils.getRecencyString(res, dayInMs - 1));
        Assert.assertEquals("1 day ago", TabResumptionModuleUtils.getRecencyString(res, dayInMs));
        Assert.assertEquals(
                "1 day ago", TabResumptionModuleUtils.getRecencyString(res, dayInMs * 2 - 1));
        Assert.assertEquals(
                "2 days ago", TabResumptionModuleUtils.getRecencyString(res, dayInMs * 2));
        Assert.assertEquals(
                "100 days ago", TabResumptionModuleUtils.getRecencyString(res, dayInMs * 100));
    }
}
