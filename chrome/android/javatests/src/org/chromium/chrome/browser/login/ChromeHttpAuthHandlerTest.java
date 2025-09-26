// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.login;

import android.view.View;

import androidx.appcompat.app.AlertDialog;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.hamcrest.TypeSafeMatcher;
import org.junit.After;
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
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.autofill.AndroidAutofillAvailabilityStatus;
import org.chromium.chrome.browser.autofill.AutofillClientProviderUtils;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.browser_ui.http_auth.LoginPrompt;
import org.chromium.components.browser_ui.http_auth.R;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.atomic.AtomicReference;

/** Tests for the Android specific HTTP auth UI. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ChromeHttpAuthHandlerTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private EmbeddedTestServer mTestServer;
    private WebPageStation mPage;

    @Before
    public void setUp() throws Exception {
        mPage = mActivityTestRule.startOnBlankPage();
        mTestServer = mActivityTestRule.getTestServer();
    }

    @After
    public void tearDown() throws Exception {
        AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(null);
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
                mActivityTestRule.getActivityTab(), ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
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
        Tab firstTab = mActivityTestRule.getActivityTab();
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
        return triggerAuthForTab(mActivityTestRule.getActivityTab());
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

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ANDROID_AUTOFILL_SUPPORT_FOR_HTTP_AUTH)
    public void testAutofillUrlProvidedWhenAvailable() throws Exception {
        AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                AndroidAutofillAvailabilityStatus.AVAILABLE);

        ChromeHttpAuthHandler handler = triggerAuth();
        verifyAuthDialogVisibility(handler, true);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            handler, hasAutofillImportance(View.IMPORTANT_FOR_AUTOFILL_YES));
                });
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ANDROID_AUTOFILL_SUPPORT_FOR_HTTP_AUTH)
    public void testAutofillUrlNotProvidedWhenNotAvailable() throws Exception {
        AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                AndroidAutofillAvailabilityStatus.SETTING_TURNED_OFF);

        ChromeHttpAuthHandler handler = triggerAuth();
        verifyAuthDialogVisibility(handler, true);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            handler, hasAutofillImportance(View.IMPORTANT_FOR_AUTOFILL_NO));
                });
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.ANDROID_AUTOFILL_SUPPORT_FOR_HTTP_AUTH)
    public void testAutofillUrlNotProvidedWhenFeatureDisabled() throws Exception {
        AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                AndroidAutofillAvailabilityStatus.AVAILABLE);

        ChromeHttpAuthHandler handler = triggerAuth();
        verifyAuthDialogVisibility(handler, true);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            handler, hasAutofillImportance(View.IMPORTANT_FOR_AUTOFILL_NO));
                });
    }

    private static Matcher<ChromeHttpAuthHandler> hasAutofillImportance(int expectedImportance) {
        return new TypeSafeMatcher<ChromeHttpAuthHandler>() {
            @Override
            protected boolean matchesSafely(ChromeHttpAuthHandler handler) {
                LoginPrompt prompt = handler.getLoginPromptForTesting();
                if (prompt == null) return false;
                AlertDialog dialog = prompt.getDialogForTesting();
                if (dialog == null) return false;
                View usernameView = dialog.findViewById(R.id.username);
                if (usernameView == null
                        || usernameView.getImportantForAutofill() != expectedImportance) {
                    return false;
                }
                View passwordView = dialog.findViewById(R.id.password);
                return passwordView != null
                        && passwordView.getImportantForAutofill() == expectedImportance;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("has autofill importance of " + expectedImportance);
            }
        };
    }
}
