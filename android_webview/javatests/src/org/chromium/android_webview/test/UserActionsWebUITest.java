// Copyright 2026 The Chromium Authors
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
import org.chromium.android_webview.test.AwActivityTestRule.TestDependencyFactory;
import org.chromium.android_webview.test.TestWebMessageListener.Data;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;

@Batch(Batch.PER_CLASS)
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class UserActionsWebUITest extends AwParameterizedTest {

    @Rule public AwActivityTestRule mActivityTestRule;

    private AwTestContainerView mTestContainerView;
    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    public UserActionsWebUITest(AwSettingsMutation param) {
        mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(
                        mContentsClient, false, new TestDependencyFactory());
        mAwContents = mTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
    }

    private void loadUserActionsWebUI() throws Exception {
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), "chrome://user-actions/");
        mActivityTestRule.waitForVisualStateCallback(mAwContents);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void userActionsTableExists() throws Exception {
        loadUserActionsWebUI();

        String pageHasUserActionsTable =
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents,
                        mContentsClient,
                        """
                            !!document.getElementById("user-actions-table")
                        """);
        Assert.assertEquals("true", pageHasUserActionsTable);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void userActionShows() throws Exception {
        TestWebMessageListener listener = new TestWebMessageListener();
        TestWebMessageListener.addWebMessageListenerOnUiThread(
                mAwContents, "listener", new String[] {"*"}, listener);

        loadUserActionsWebUI();

        mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents,
                mContentsClient,
                """
                    const target = document.getElementById("user-actions-table")

                    const config = {childList: true};

                    const observer = new MutationObserver((mutationList, observer) => {
                        for (mutation of mutationList) {
                            for (node of mutation.addedNodes) {
                                if (node.children[0].innerHTML === "Test action") {
                                    listener.postMessage("test-action")
                                    break
                                }
                            }
                        }
                    })


                    observer.observe(target, config);
                """);

        ThreadUtils.runOnUiThreadBlocking(() -> RecordUserAction.record("Test action"));

        Data action = listener.waitForOnPostMessage();
        Assert.assertEquals("test-action", action.getAsString());
    }
}
