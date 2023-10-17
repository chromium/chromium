// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.smoke.utilities;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;
import org.chromium.chrome.test.pagecontroller.utils.UiAutomatorUtils;
import org.chromium.chrome.test.pagecontroller.utils.UiLocatorHelper;

/** FirstRunNavigator is used to Navigate through FRE page. */
public class FirstRunNavigator {
    public static final String TAG = "FirstRunNavigator";

    public FirstRunNavigator() {}

    public void navigateThroughFRE() {
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
        IUi2Locator[] frePageDetectors =
                new IUi2Locator[] {
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
            UiAutomatorUtils.getInstance().waitUntilAnyVisible(frePageDetectors);
            // Different FRE versions show up randomly and in different order,
            // figure out which one we are on and proceed.
            if (uiLocatorHelper.isOnScreen(urlBar)) {
                Log.i(TAG, "FRE is done (Found URL bar).");
                // FRE is over.
                break;
            } else if (uiLocatorHelper.isOnScreen(playServicesUpdateText)) {
                // If the update play services alert is a modal, dismiss it.
                // Otherwise its just a toast/notification that should not
                // interfere with the test.
                if (uiLocatorHelper.isOnScreen(updatePlayServicesPanel)) {
                    Log.i(TAG, "Dismissing Play Services dialog");
                    UiAutomatorUtils.getInstance().clickOutsideOf(updatePlayServicesPanel);
                } else {
                    Log.i(TAG, "Ignoring Play Services toast");
                }
            } else if (uiLocatorHelper.isOnScreen(noAddAccountButton)) {
                // Do not add an account.
                Log.i(TAG, "Clicking through add account dialog");
                UiAutomatorUtils.getInstance().click(noAddAccountButton);
            } else if (uiLocatorHelper.isOnScreen(signinSkipButton)) {
                // Do not sign in with an account.
                Log.i(TAG, "Clicking through sign in dialog via \"skip\"");
                UiAutomatorUtils.getInstance().click(signinSkipButton);
            } else if (uiLocatorHelper.isOnScreen(signinContinueButton)) {
                // Sometimes there is only the continue button (eg: when signin is
                // disabled.)
                Log.i(TAG, "Clicking through sign in dialog via \"continue\"");
                UiAutomatorUtils.getInstance().click(signinContinueButton);
            } else if (uiLocatorHelper.isOnScreen(signinProgressSpinner)) {
                // Do nothing and wait.
                Log.i(TAG, "Waiting for progress spinner");
            } else if (uiLocatorHelper.isOnScreen(defaultSearchEngineNextButton)) {
                // Just press next on choosing the default SE.
                Log.i(TAG, "Clicking through search engine selection");
                UiAutomatorUtils.getInstance().click(defaultSearchEngineNextButton);
            } else {
                throw new RuntimeException("Unexpected FRE or Start page detected.");
            }
        }
    }
}
