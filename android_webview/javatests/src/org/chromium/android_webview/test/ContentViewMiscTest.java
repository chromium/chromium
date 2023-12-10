// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.BroadcastReceiver;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Proxy;
import android.os.Handler;
import android.os.Looper;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.ContentViewStatics;
import org.chromium.net.ProxyChangeListener;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for ContentView methods that don't fall into any other category. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class ContentViewMiscTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    public ContentViewMiscTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFindAddress() {
        Assert.assertNull(AwContentsStatics.findAddress("This is some random text"));

        String googleAddr = "1600 Amphitheatre Pkwy, Mountain View, CA 94043";
        String testString = "Address: " + googleAddr + "  in a string";
        Assert.assertEquals(googleAddr, AwContentsStatics.findAddress(testString));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testEnableDisablePlatformNotifications() {
        Looper.prepare();
        // Set up mock contexts to use with the listener
        final AtomicReference<BroadcastReceiver> receiverRef =
                new AtomicReference<BroadcastReceiver>();
        final AdvancedMockContext appContext =
                new AdvancedMockContext(
                        InstrumentationRegistry.getInstrumentation()
                                .getTargetContext()
                                .getApplicationContext()) {
                    @Override
                    public Intent registerReceiver(
                            BroadcastReceiver receiver,
                            IntentFilter filter,
                            String broadcastPermission,
                            Handler scheduler) {
                        receiverRef.set(receiver);
                        return null;
                    }

                    @Override
                    public Intent registerReceiver(
                            BroadcastReceiver receiver,
                            IntentFilter filter,
                            String broadcastPermission,
                            Handler scheduler,
                            int flags) {
                        receiverRef.set(receiver);
                        return null;
                    }
                };
        ContextUtils.initApplicationContextForTests(appContext);

        // Set up a delegate so we know when native code is about to get
        // informed of a proxy change.
        final AtomicBoolean proxyChanged = new AtomicBoolean();
        final ProxyChangeListener.Delegate delegate = () -> proxyChanged.set(true);
        Intent intent = new Intent();
        intent.setAction(Proxy.PROXY_CHANGE_ACTION);

        // Create the listener that's going to be used for the test
        ProxyChangeListener listener = ProxyChangeListener.create();
        listener.setDelegateForTesting(delegate);
        listener.start(0);

        // Start the actual tests

        // Make sure everything works by default
        proxyChanged.set(false);
        receiverRef.get().onReceive(appContext, intent);
        Assert.assertEquals(true, proxyChanged.get());

        // Now disable platform notifications and make sure we don't notify
        // native code.
        proxyChanged.set(false);
        ContentViewStatics.disablePlatformNotifications();
        receiverRef.get().onReceive(appContext, intent);
        Assert.assertEquals(false, proxyChanged.get());

        // Now re-enable notifications to make sure they work again.
        ContentViewStatics.enablePlatformNotifications();
        receiverRef.get().onReceive(appContext, intent);
        Assert.assertEquals(true, proxyChanged.get());
    }
}
