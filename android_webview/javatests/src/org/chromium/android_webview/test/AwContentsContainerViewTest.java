// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.base.test.util.Feature;

/** AwContents container view tests. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwContentsContainerViewTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mContainerView;

    public AwContentsContainerViewTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        mContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testContainerViewClickable() {
        Assert.assertTrue(mContainerView.isClickable());
    }
}
