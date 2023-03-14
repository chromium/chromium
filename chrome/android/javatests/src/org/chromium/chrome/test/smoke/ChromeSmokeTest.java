// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.smoke;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiApplicationTestRule;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiAutomatorTestRule;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;
import org.chromium.chrome.test.pagecontroller.utils.UiAutomatorUtils;
import org.chromium.chrome.test.pagecontroller.utils.UiLocatorHelper;

import java.util.concurrent.Callable;

/**
 * Smoke Test for Chrome Android.
 */
@LargeTest
@RunWith(BaseJUnit4ClassRunner.class)
public class ChromeSmokeTest {
    private static final String DATA_URL = "data:,Hello";
    private static final String ACTIVITY_NAME = "org.chromium.chrome.browser.ChromeTabbedActivity";

    public static final long TIMEOUT_MS = 20000L;
    public static final long UI_CHECK_INTERVAL = 1000L;
    private String mPackageName;
    public ChromeUiAutomatorTestRule mRule = new ChromeUiAutomatorTestRule();
    public ChromeUiApplicationTestRule mChromeUiRule = new ChromeUiApplicationTestRule();
    @Rule
    public final TestRule mChain = RuleChain.outerRule(mChromeUiRule).around(mRule);

    private static Runnable toNotSatisfiedRunnable(
            Callable<Boolean> criteria, String failureReason) {
        return () -> {
            try {
                boolean isSatisfied = criteria.call();
                Criteria.checkThat(failureReason, isSatisfied, Matchers.is(true));
            } catch (RuntimeException e) {
                throw e;
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        };
    }

    private void waitUntilAnyVisible(IUi2Locator... locators) {
        CriteriaHelper.pollInstrumentationThread(
                toNotSatisfiedRunnable(
                        ()
                                -> {
                            for (IUi2Locator locator : locators) {
                                if (UiAutomatorUtils.getInstance().getLocatorHelper().isOnScreen(
                                            locator)) {
                                    return true;
                                }
                            }
                            return false;
                        },
                        "No Chrome views on screen. (i.e. Chrome has crashed "
                                + "on startup). Look at earlier logs for the actual "
                                + "crash stacktrace."),
                TIMEOUT_MS, UI_CHECK_INTERVAL);
    }

    private void navigateThroughFRE() {
        // Used in SyncConsentFirstRunFragment FRE page.
        IUi2Locator noAddAccountButton = Ui2Locators.withAnyResEntry(R.id.negative_button);

        // Used in SigninFirstRunFragment FRE page.
        IUi2Locator signinSkipButton = Ui2Locators.withAnyResEntry(R.id.signin_fre_dismiss_button);
        IUi2Locator signinContinueButton =
                Ui2Locators.withAnyResEntry(R.id.signin_fre_continue_button);
        IUi2Locator signinProgressSpinner =
                Ui2Locators.withAnyResEntry(R.id.fre_native_and_policy_load_progress_spinner);

        // Used in DefaultSearchEngineFirstRunFragment FRE page.
        IUi2Locator defaultSearchEngineNextButton =
                Ui2Locators.withAnyResEntry(R.id.button_primary);

        // Url bar shown after the FRE is over.
        IUi2Locator urlBar = Ui2Locators.withAnyResEntry(R.id.url_bar);

        // When Play services is too old, android shows an alert.
        IUi2Locator updatePlayServicesPanel = Ui2Locators.withResName("android:id/parentPanel");
        IUi2Locator playServicesUpdateText =
                Ui2Locators.withTextContaining("update Google Play services");

        UiLocatorHelper uiLocatorHelper = UiAutomatorUtils.getInstance().getLocatorHelper();

        // These locators show up in one FRE page or another
        IUi2Locator[] frePageDetectors = new IUi2Locator[] {
                playServicesUpdateText,
                signinSkipButton,
                signinContinueButton,
                signinProgressSpinner,
                noAddAccountButton,
                defaultSearchEngineNextButton,
                urlBar,
        };

        // Manually go through FRE.
        while (true) {
            // Wait for an FRE page to show up.
            waitUntilAnyVisible(frePageDetectors);
            // Different FRE versions show up randomly and in different order,
            // figure out which one we are on and proceed.
            if (uiLocatorHelper.isOnScreen(urlBar)) {
                // FRE is over.
                break;
            } else if (uiLocatorHelper.isOnScreen(playServicesUpdateText)) {
                // If the update play services alert is a modal, dismiss it.
                // Otherwise its just a toast/notification that should not
                // interfere with the test.
                if (uiLocatorHelper.isOnScreen(updatePlayServicesPanel)) {
                    UiAutomatorUtils.getInstance().clickOutsideOf(updatePlayServicesPanel);
                }
            } else if (uiLocatorHelper.isOnScreen(noAddAccountButton)) {
                // Do not add an account.
                UiAutomatorUtils.getInstance().click(noAddAccountButton);
            } else if (uiLocatorHelper.isOnScreen(signinSkipButton)) {
                // Do not sign in with an account.
                UiAutomatorUtils.getInstance().click(signinSkipButton);
            } else if (uiLocatorHelper.isOnScreen(signinContinueButton)) {
                // Sometimes there is only the continue button (eg: when signin is
                // disabled.)
                UiAutomatorUtils.getInstance().click(signinContinueButton);
            } else if (uiLocatorHelper.isOnScreen(signinProgressSpinner)) {
                // Do nothing and wait.
            } else if (uiLocatorHelper.isOnScreen(defaultSearchEngineNextButton)) {
                // Just press next on choosing the default SE.
                UiAutomatorUtils.getInstance().click(defaultSearchEngineNextButton);
            } else {
                throw new RuntimeException("Unexpected FRE or Start page detected.");
            }
        }
    }

    @Before
    public void setUp() throws Exception {
        mPackageName = InstrumentationRegistry.getArguments().getString(
                ChromeUiApplicationTestRule.PACKAGE_NAME_ARG, "org.chromium.chrome");
    }

    @Test
    public void testHello() {
        Context context = InstrumentationRegistry.getContext();
        final Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(DATA_URL));
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK | Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setComponent(new ComponentName(mPackageName, ACTIVITY_NAME));
        context.startActivity(intent);

        // Looks for the any view/layout with the chrome package name.
        IUi2Locator locatorChrome = Ui2Locators.withPackageName(mPackageName);
        // Wait until chrome shows up
        waitUntilAnyVisible(locatorChrome);

        // Go through the FRE until you see ChromeTabbedActivity urlbar.
        navigateThroughFRE();

        // FRE should be over and we should be shown the url we requested.
        IUi2Locator dataUrlText = Ui2Locators.withText(DATA_URL);
        UiAutomatorUtils.getInstance().getLocatorHelper().verifyOnScreen(dataUrlText);
    }
}
