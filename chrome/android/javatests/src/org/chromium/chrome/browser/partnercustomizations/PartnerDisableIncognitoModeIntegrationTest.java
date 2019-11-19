// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.PopupMenu;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsProvider;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Integration tests for the partner disabling incognito mode feature.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PartnerDisableIncognitoModeIntegrationTest {
    @Rule
    public BasePartnerBrowserCustomizationIntegrationTestRule mActivityTestRule =
            new BasePartnerBrowserCustomizationIntegrationTestRule();

    private void setParentalControlsEnabled(boolean enabled) {
        Uri uri = PartnerBrowserCustomizations.buildQueryUri(
                PartnerBrowserCustomizations.PARTNER_DISABLE_INCOGNITO_MODE_PATH);
        Bundle bundle = new Bundle();
        bundle.putBoolean(
                TestPartnerBrowserCustomizationsProvider.INCOGNITO_MODE_DISABLED_KEY, enabled);
        Context context = InstrumentationRegistry.getTargetContext();
        context.getContentResolver().call(uri, "setIncognitoModeDisabled", null, bundle);
    }

    private void assertIncognitoMenuItemEnabled(boolean enabled) throws ExecutionException {
        Menu menu = TestThreadUtils.runOnUiThreadBlocking(new Callable<Menu>() {
            @Override
            public Menu call() {
                // PopupMenu is a convenient way of building a temp menu.
                PopupMenu tempMenu = new PopupMenu(mActivityTestRule.getActivity(),
                        mActivityTestRule.getActivity().findViewById(R.id.menu_anchor_stub));
                tempMenu.inflate(R.menu.main_menu);
                Menu menu = tempMenu.getMenu();

                return menu;
            }
        });
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (item.getItemId() == R.id.new_incognito_tab_menu_id && item.isVisible()) {
                Assert.assertEquals(
                        "Menu item enabled state is not correct.", enabled, item.isEnabled());
            }
        }
    }

    private void waitForParentalControlsEnabledState(final boolean parentalControlsEnabled) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                // areParentalControlsEnabled is updated on a background thread, so we
                // also wait on the isIncognitoModeEnabled to ensure the updates on the
                // UI thread have also triggered.
                boolean retVal = parentalControlsEnabled
                        == PartnerBrowserCustomizations.isIncognitoDisabled();
                retVal &= parentalControlsEnabled != IncognitoUtils.isIncognitoModeEnabled();
                return retVal;
            }
        });
    }

    private void toggleActivityForegroundState() {
        TestThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.getActivity().onPause());
        TestThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.getActivity().onStop());
        TestThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.getActivity().onStart());
        TestThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.getActivity().onResume());
    }

    @Test
    @MediumTest
    @Feature({"DisableIncognitoMode"})
    @RetryOnFailure
    public void testIncognitoEnabledIfNoParentalControls() throws InterruptedException {
        setParentalControlsEnabled(false);
        mActivityTestRule.startMainActivityOnBlankPage();
        waitForParentalControlsEnabledState(false);
        mActivityTestRule.newIncognitoTabFromMenu();
    }

    @Test
    @MediumTest
    @Feature({"DisableIncognitoMode"})
    public void testIncognitoMenuItemEnabledBasedOnParentalControls()
            throws InterruptedException, ExecutionException {
        setParentalControlsEnabled(true);
        mActivityTestRule.startMainActivityOnBlankPage();
        waitForParentalControlsEnabledState(true);
        assertIncognitoMenuItemEnabled(false);

        setParentalControlsEnabled(false);
        toggleActivityForegroundState();
        waitForParentalControlsEnabledState(false);
        assertIncognitoMenuItemEnabled(true);
    }

    @Test
    @MediumTest
    @Feature({"DisableIncognitoMode"})
    public void testEnabledParentalControlsClosesIncognitoTabs() throws InterruptedException {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());

        try {
            String[] testUrls = {
                testServer.getURL("/chrome/test/data/android/about.html"),
                testServer.getURL("/chrome/test/data/android/ok.txt"),
                testServer.getURL("/chrome/test/data/android/test.html")
            };

            setParentalControlsEnabled(false);
            mActivityTestRule.startMainActivityOnBlankPage();
            waitForParentalControlsEnabledState(false);

            mActivityTestRule.loadUrlInNewTab(testUrls[0], true);
            mActivityTestRule.loadUrlInNewTab(testUrls[1], true);
            mActivityTestRule.loadUrlInNewTab(testUrls[2], true);
            mActivityTestRule.loadUrlInNewTab(testUrls[0], false);

            setParentalControlsEnabled(true);
            toggleActivityForegroundState();
            waitForParentalControlsEnabledState(true);

            CriteriaHelper.pollInstrumentationThread(
                    Criteria.equals(0, () -> mActivityTestRule.tabsCount(true /* incognito */)));
        } finally {
            testServer.stopAndDestroyServer();
        }
    }
}
