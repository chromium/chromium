// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.login;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.atomic.AtomicReference;

/** Tests for the Android specific HTTP auth UI. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ChromeHttpAuthHandlerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
    }

    @Test
    @MediumTest
    public void authDialogShows() throws Exception {
        ChromeHttpAuthHandler handler = triggerAuth();
        verifyAuthDialogVisibility(handler, true);
    }

    @Test
    @MediumTest
    public void authDialogDismissOnNavigation() throws Exception {
        ChromeHttpAuthHandler handler = triggerAuth();
        verifyAuthDialogVisibility(handler, true);
        ChromeTabUtils.loadUrlOnUiThread(
                mActivityTestRule.getActivity().getActivityTab(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        verifyAuthDialogVisibility(handler, false);
    }

    @Test
    @MediumTest
    public void authDialogDismissOnTabSwitched() throws Exception {
        ChromeHttpAuthHandler handler = triggerAuth();
        verifyAuthDialogVisibility(handler, true);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mActivityTestRule
                                .getActivity()
                                .getTabCreator(false)
                                .launchUrl("about:blank", TabLaunchType.FROM_CHROME_UI));
        verifyAuthDialogVisibility(handler, false);
    }

    @Test
    @MediumTest
    public void authDialogDismissOnTabClosed() throws Exception {
        ChromeHttpAuthHandler handler = triggerAuth();
        verifyAuthDialogVisibility(handler, true);
        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        verifyAuthDialogVisibility(handler, false);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @DisabledTest(message = "https://crbug.com/1218039")
    public void authDialogSuppressedOnBackgroundTab() throws Exception {
        Tab firstTab = mActivityTestRule.getActivity().getActivityTab();
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        // If the first tab was closed due to OOM, then just exit the test.
        if (ThreadUtils.runOnUiThreadBlocking(
                () -> firstTab.isClosing() || SadTab.isShowing(firstTab))) {
            return;
        }
        ChromeHttpAuthHandler handler = triggerAuthForTab(firstTab);
        verifyAuthDialogVisibility(handler, false);
    }

    private ChromeHttpAuthHandler triggerAuth() throws Exception {
        return triggerAuthForTab(mActivityTestRule.getActivity().getActivityTab());
    }

    private ChromeHttpAuthHandler triggerAuthForTab(Tab tab) throws Exception {
        AtomicReference<ChromeHttpAuthHandler> handlerRef = new AtomicReference<>();
        CallbackHelper handlerCallback = new CallbackHelper();
        Callback<ChromeHttpAuthHandler> callback =
                (handler) -> {
                    handlerRef.set(handler);
                    handlerCallback.notifyCalled();
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeHttpAuthHandler.setTestCreationCallback(callback);
                });

        String url = mTestServer.getURL("/auth-basic");
        ChromeTabUtils.loadUrlOnUiThread(tab, url);
        handlerCallback.waitForOnly();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeHttpAuthHandler.setTestCreationCallback(null);
                });
        return handlerRef.get();
    }

    private void verifyAuthDialogVisibility(ChromeHttpAuthHandler handler, boolean isVisible) {
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(handler.isShowingAuthDialog(), Matchers.is(isVisible)));
    }
}
