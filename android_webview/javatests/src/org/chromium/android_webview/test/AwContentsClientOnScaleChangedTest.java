// Copyright 2012 The Chromium Authors
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

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CriteriaHelper;

/** Tests for the WebViewClient.onScaleChanged. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwContentsClientOnScaleChangedTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    public AwContentsClientOnScaleChangedTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
    }

    @Test
    @SmallTest
    public void testScaleUp() throws Throwable {
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setSupportZoom(true);
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageFrom(
                        "<meta name=\"viewport\" content=\"initial-scale=1.0, "
                                + " minimum-scale=0.5, maximum-scale=2, user-scalable=yes\" />",
                        "testScaleUp test page body"),
                "text/html",
                false);
        CriteriaHelper.pollUiThread(() -> mAwContents.canZoomIn());
        int callCount = mContentsClient.getOnScaleChangedHelper().getCallCount();
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT, () -> Assert.assertTrue(mAwContents.zoomIn()));
        mContentsClient.getOnScaleChangedHelper().waitForCallback(callCount);
        Assert.assertTrue(
                "Scale ratio:" + mContentsClient.getOnScaleChangedHelper().getLastScaleRatio(),
                mContentsClient.getOnScaleChangedHelper().getLastScaleRatio() > 1);
    }
}
