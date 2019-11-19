// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.AppHooksModule;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.dynamicmodule.CustomTabsDynamicModuleTestUtils.AppHooksModuleForTest;
import org.chromium.chrome.browser.customtabs.dynamicmodule.CustomTabsDynamicModuleTestUtils.IntentBuilder;
import org.chromium.chrome.browser.dependency_injection.ModuleOverridesRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.net.test.util.TestWebServer;

import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.PostMessageBackend;

/**
 * Instrumentation tests for the CCT Dynamic Module post message API.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class CustomTabsDynamicModulePostMessageTest {

    private final TestRule mModuleOverridesRule = new ModuleOverridesRule()
            .setOverride(AppHooksModule.Factory.class, AppHooksModuleForTest::new);

    private final CustomTabActivityTestRule mActivityRule = new CustomTabActivityTestRule();

    @Rule
    public final TestRule mOverrideModulesThenLaunchRule =
            RuleChain.outerRule(mModuleOverridesRule).around(mActivityRule);

    private TestWebServer mServer;

    private static final String JS_MESSAGE = "from_js";
    private static final String TITLE_FROM_POSTMESSAGE_TO_CHANNEL =
            "<!DOCTYPE html><html><body>"
                    + "    <script>"
                    + "        var received = '';"
                    + "        onmessage = function (e) {"
                    + "            var myport = e.ports[0];"
                    + "            myport.onmessage = function (f) {"
                    + "                received += f.data;"
                    + "                document.title = received;"
                    + "            }"
                    + "        }"
                    + "   </script>"
                    + "</body></html>";

    private static final String MESSAGE_FROM_PAGE_TO_CHANNEL =
            "<!DOCTYPE html><html><body>"
                    + "    <script>"
                    + "        onmessage = function (e) {"
                    + "            if (e.ports != null && e.ports.length > 0) {"
                    + "               e.ports[0].postMessage(\"" + JS_MESSAGE + "\");"
                    + "            }"
                    + "        }"
                    + "   </script>"
                    + "</body></html>";

    private static final Uri FAKE_ORIGIN_URI = Uri.parse("android-app://com.google.test");


    @Before
    public void setUp() throws Exception {
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
        mServer = TestWebServer.start();
    }

    @After
    public void tearDown() {
        mServer.shutdown();
    }

    /**
     * Tests the sent postMessage requests not only return success, but is also received by page.
     */
    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.CCT_MODULE_POST_MESSAGE)
    public void testPostMessageFromDynamicModuleReceivedInPage() {
        final String url =
                mServer.setResponse("/test.html", TITLE_FROM_POSTMESSAGE_TO_CHANNEL, null);

        Intent intent = new IntentBuilder(url).build();
        mActivityRule.startCustomTabActivityWithIntent(intent);

        ChromeTabUtils.waitForTabPageLoaded(getActivity().getActivityTab(), url);
        DynamicModuleCoordinator coordinator = getModuleCoordinator();
        coordinator.maybeInitialiseDynamicModulePostMessageHandler(new PostMessageBackend() {
            @Override
            public boolean onPostMessage(String message, Bundle extras) {
                return true;
            }

            @Override
            public boolean onNotifyMessageChannelReady(Bundle extras) {
                // Now attempt to post a message.
                assertEquals(coordinator.postMessage("New title"),
                        CustomTabsService.RESULT_SUCCESS);
                return true;
            }

            @Override
            public void onDisconnectChannel(Context appContext) {}
        });

        assertTrue(coordinator.requestPostMessageChannel(FAKE_ORIGIN_URI));
        // The callback registered above will post a message once the requested channel is ready.
        ChromeTabUtils.waitForTitle(getActivity().getActivityTab(), "New title");
    }

    /**
     * Tests the postMessage requests sent from the page is received on the client side.
     */
    @Test
    @SmallTest
    @Features.EnableFeatures(
            {ChromeFeatureList.CCT_MODULE, ChromeFeatureList.CCT_MODULE_POST_MESSAGE})
    public void testPostMessageReceivedFromPageByDynamicModule() throws Exception {
        final CallbackHelper messageChannelHelper = new CallbackHelper();
        final CallbackHelper onPostMessageHelper = new CallbackHelper();
        final String url = mServer.setResponse("/test.html", MESSAGE_FROM_PAGE_TO_CHANNEL, null);

        Intent intent = new IntentBuilder(url).build();
        mActivityRule.startCustomTabActivityWithIntent(intent);

        ChromeTabUtils.waitForTabPageLoaded(getActivity().getActivityTab(), url);

        DynamicModuleCoordinator coordinator = getModuleCoordinator();

        coordinator.maybeInitialiseDynamicModulePostMessageHandler(new PostMessageBackend() {
            @Override
            public boolean onPostMessage(String message, Bundle extras) {
                onPostMessageHelper.notifyCalled();
                return true;
            }

            @Override
            public boolean onNotifyMessageChannelReady(Bundle extras) {
                messageChannelHelper.notifyCalled();
                return true;
            }

            @Override
            public void onDisconnectChannel(Context appContext) {}
        });

        assertTrue(coordinator.requestPostMessageChannel(FAKE_ORIGIN_URI));
        messageChannelHelper.waitForFirst();
        onPostMessageHelper.waitForFirst();
    }

    /**
     * Tests the postMessage requests sent from the page is received on the client side.
     */
    @Test
    @SmallTest
    @Features
            .EnableFeatures(ChromeFeatureList.CCT_MODULE_POST_MESSAGE)
            @Features.DisableFeatures(ChromeFeatureList.CCT_MODULE)
            public void testPostMessageFromDynamicModuleDisallowedBeforeModuleLoaded() {
        final CallbackHelper messageChannelHelper = new CallbackHelper();
        final CallbackHelper onPostMessageHelper = new CallbackHelper();
        final String url = mServer.setResponse("/test.html", MESSAGE_FROM_PAGE_TO_CHANNEL, null);

        Intent intent = new IntentBuilder(url).build();
        mActivityRule.startCustomTabActivityWithIntent(intent);

        ChromeTabUtils.waitForTabPageLoaded(getActivity().getActivityTab(), url);

        // Dynamic module is not instantiated because CCT_MODULE feature is disabled,
        // resolve it manually. Module loading is started in onFinishNativeInitialization()
        // onFinishNativeInitialization() call happens before therefore if we instantiate
        // DynamicModuleCoordinator now, the module will not be loaded.
        DynamicModuleCoordinator coordinator = getModuleCoordinator();
        assertFalse(coordinator.isModuleLoading() || coordinator.isModuleLoaded()
                || coordinator.hasModuleFailedToLoad());

        // We shouldn't be able to open a channel or post messages yet.
        assertFalse(coordinator.requestPostMessageChannel(FAKE_ORIGIN_URI));
        assertEquals(coordinator.postMessage("Message"),
                CustomTabsService.RESULT_FAILURE_DISALLOWED);

        // Now fake initialisation of the dynamic module.
        coordinator.maybeInitialiseDynamicModulePostMessageHandler(
                new PostMessageBackend() {
                    @Override
                    public boolean onPostMessage(String message, Bundle extras) {
                        onPostMessageHelper.notifyCalled();
                        return true;
                    }

                    @Override
                    public boolean onNotifyMessageChannelReady(Bundle extras) {
                        messageChannelHelper.notifyCalled();
                        return true;
                    }

                    @Override
                    public void onDisconnectChannel(Context appContext) {}
                });

        // We can now request a postMessage channel.
        assertTrue(coordinator.requestPostMessageChannel(FAKE_ORIGIN_URI));
    }

    @Test
    @SmallTest
    @Features
            .EnableFeatures(ChromeFeatureList.CCT_MODULE)
            @Features.DisableFeatures(ChromeFeatureList.CCT_MODULE_POST_MESSAGE)
            public void testPostMessageFromDynamicModuleDisallowedWhenFeatureDisabled() {
        final String url = mServer.setResponse("/test.html", MESSAGE_FROM_PAGE_TO_CHANNEL, null);

        Intent intent = new IntentBuilder(url).build();
        mActivityRule.startCustomTabActivityWithIntent(intent);

        ChromeTabUtils.waitForTabPageLoaded(getActivity().getActivityTab(), url);
        DynamicModuleCoordinator coordinator = getModuleCoordinator();

        // We shouldn't be able to open a channel or post messages yet.
        assertFalse(coordinator.requestPostMessageChannel(FAKE_ORIGIN_URI));
        assertEquals(coordinator.postMessage("Message"),
                CustomTabsService.RESULT_FAILURE_DISALLOWED);
    }

    private CustomTabActivity getActivity() {
        return mActivityRule.getActivity();
    }

    private DynamicModuleCoordinator getModuleCoordinator() {
        return getActivity().getComponent().resolveDynamicModuleCoordinator();
    }
}
