// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageStateHandler;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content_public.browser.test.util.TestWebContentsObserver;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Tests for making sure the distillability service is communicating correctly. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class DistillabilityServiceTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TEST_PAGE = "/chrome/test/data/dom_distiller/simple_article.html";

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    /** Make sure that Reader Mode appears after navigating from a native page. */
    @Test
    @Feature({"Distillability-Service"})
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    @DisabledTest(message = "Flaky - crbug/1455454")
    public void testServiceAliveAfterNativePage() throws TimeoutException, ExecutionException {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());

        final CallbackHelper readerShownCallbackHelper = new CallbackHelper();

        TestWebContentsObserver observer =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new TestWebContentsObserver(mActivityTestRule.getWebContents()));
        OnPageFinishedHelper finishHelper = observer.getOnPageFinishedHelper();

        // Navigate to a native page.
        int curCallCount = finishHelper.getCallCount();
        mActivityTestRule.loadUrl("chrome://history/");
        finishHelper.waitForCallback(curCallCount, 1);
        verifyReaderModeMessageShown(readerShownCallbackHelper);
        Assert.assertEquals(0, readerShownCallbackHelper.getCallCount());

        // Navigate to a normal page.
        curCallCount = readerShownCallbackHelper.getCallCount();
        mActivityTestRule.loadUrl(testServer.getURL(TEST_PAGE));
        verifyReaderModeMessageShown(readerShownCallbackHelper);
        readerShownCallbackHelper.waitForCallback(curCallCount, 1);
        Assert.assertEquals(1, readerShownCallbackHelper.getCallCount());
    }

    private void verifyReaderModeMessageShown(CallbackHelper readerShownCallbackHelper)
            throws ExecutionException {
        MessageDispatcher messageDispatcher =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                MessageDispatcherProvider.from(
                                        mActivityTestRule.getActivity().getWindowAndroid()));
        List<MessageStateHandler> messages =
                MessagesTestHelper.getEnqueuedMessages(
                        messageDispatcher, MessageIdentifier.READER_MODE);
        if (messages.size() > 0 && MessagesTestHelper.getCurrentMessage(messages.get(0)) != null) {
            readerShownCallbackHelper.notifyCalled();
        }
    }
}
