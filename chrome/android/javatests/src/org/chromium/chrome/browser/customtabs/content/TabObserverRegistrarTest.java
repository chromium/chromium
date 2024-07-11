// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.junit.Assert.assertEquals;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link TabObserverRegistrar}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabObserverRegistrarTest {
    private static class LoadUrlTabObserver extends CustomTabTabObserver {
        private List<String> mUrlLoadRequests = new ArrayList<>();

        List<String> getLoadUrlRequests() {
            return mUrlLoadRequests;
        }

        @Override
        public void onLoadUrl(Tab tab, LoadUrlParams params, LoadUrlResult loadUrlResult) {
            mUrlLoadRequests.add(params.getUrl());
        }
    }

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    /**
     * Tests that the TabObserver registered by {@link
     * TabObserverRegistrar#registerActivityTabObserver()} switches when the active tab is switched.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug/341026733")
    public void testObserveActiveTab() throws Throwable {
        EmbeddedTestServer testServer = mCustomTabActivityTestRule.getTestServer();
        final String windowOpenUrl =
                testServer.getURL("/chrome/test/data/android/customtabs/test_window_open.html");
        final String url1 = testServer.getURL("/chrome/test/data/android/about.html");
        final String url2 = testServer.getURL("/chrome/test/data/android/simple.html");

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(), windowOpenUrl));

        // Register TabObserver via TabObserverRegistrar#registerActiveTabObserver()
        CustomTabActivity customTabActivity = mCustomTabActivityTestRule.getActivity();
        TabObserverRegistrar tabObserverRegistrar =
                customTabActivity.getComponent().resolveTabObserverRegistrar();
        LoadUrlTabObserver loadUrlTabObserver = new LoadUrlTabObserver();
        ThreadUtils.runOnUiThreadBlocking(
                () -> tabObserverRegistrar.registerActivityTabObserver(loadUrlTabObserver));

        final TabModelSelector tabSelector = customTabActivity.getTabModelSelector();
        final Tab initialActiveTab = tabSelector.getCurrentTab();

        // Open and wait for popup.
        final CallbackHelper openTabHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabSelector
                            .getModel(false)
                            .addObserver(
                                    new TabModelObserver() {
                                        @Override
                                        public void didSelectTab(
                                                Tab tab, @TabSelectionType int type, int lastId) {
                                            if (tab != initialActiveTab) {
                                                openTabHelper.notifyCalled();
                                            }
                                        }
                                    });
                });
        DOMUtils.clickNode(mCustomTabActivityTestRule.getWebContents(), "new_window");
        openTabHelper.waitForCallback(0, 1);

        assertEquals(2, tabSelector.getModel(false).getCount());
        final Tab activeTab = tabSelector.getCurrentTab();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    initialActiveTab.loadUrl(new LoadUrlParams(url1));
                    activeTab.loadUrl(new LoadUrlParams(url2));
                });

        List<String> urlRequests = loadUrlTabObserver.getLoadUrlRequests();
        assertEquals(1, urlRequests.size());
        assertEquals(url2, urlRequests.get(0));
    }
}
