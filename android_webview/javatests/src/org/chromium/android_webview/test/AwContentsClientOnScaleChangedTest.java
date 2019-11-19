// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

/**
 * Tests for the WebViewClient.onScaleChanged.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwContentsClientOnScaleChangedTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

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
        mActivityTestRule.loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageFrom(
                        "<meta name=\"viewport\" content=\"initial-scale=1.0, "
                                + " minimum-scale=0.5, maximum-scale=2, user-scalable=yes\" />",
                        "testScaleUp test page body"),
                "text/html", false);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mAwContents.canZoomIn();
            }
        });
        int callCount = mContentsClient.getOnScaleChangedHelper().getCallCount();
        PostTask.runOrPostTask(
                UiThreadTaskTraits.DEFAULT, () -> Assert.assertTrue(mAwContents.zoomIn()));
        mContentsClient.getOnScaleChangedHelper().waitForCallback(callCount);
        Assert.assertTrue(
                "Scale ratio:" + mContentsClient.getOnScaleChangedHelper().getLastScaleRatio(),
                mContentsClient.getOnScaleChangedHelper().getLastScaleRatio() > 1);
    }
}
