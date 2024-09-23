// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.PopupMenu;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsProvider;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/** Integration tests for the partner disabling incognito mode feature. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PartnerDisableIncognitoModeIntegrationTest {
    @Rule
    public BasePartnerBrowserCustomizationIntegrationTestRule mActivityTestRule =
            new BasePartnerBrowserCustomizationIntegrationTestRule();

    private void setParentalControlsEnabled(boolean enabled) {
        Uri uri =
                CustomizationProviderDelegateUpstreamImpl.buildQueryUri(
                        PartnerBrowserCustomizations.PARTNER_DISABLE_INCOGNITO_MODE_PATH);
        Bundle bundle = new Bundle();
        bundle.putBoolean(
                TestPartnerBrowserCustomizationsProvider.INCOGNITO_MODE_DISABLED_KEY, enabled);
        Context context = ApplicationProvider.getApplicationContext();
        context.getContentResolver().call(uri, "setIncognitoModeDisabled", null, bundle);
    }

    private void assertIncognitoMenuItemEnabled(boolean enabled) throws ExecutionException {
        Menu menu =
                ThreadUtils.runOnUiThreadBlocking(
                        new Callable<Menu>() {
                            @Override
                            public Menu call() {
                                // PopupMenu is a convenient way of building a temp menu.
                                PopupMenu tempMenu =
                                        new PopupMenu(
                                                mActivityTestRule.getActivity(),
                                                mActivityTestRule
                                                        .getActivity()
                                                        .findViewById(R.id.menu_anchor_stub));
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
        CriteriaHelper.pollUiThread(
                () -> {
                    // areParentalControlsEnabled is updated on a background thread, so we
                    // also wait on the isIncognitoModeEnabled to ensure the updates on the
                    // UI thread have also triggered.
                    Criteria.checkThat(
                            PartnerBrowserCustomizations.isIncognitoDisabled(),
                            Matchers.is(parentalControlsEnabled));
                    Criteria.checkThat(
                            IncognitoUtils.isIncognitoModeEnabled(
                                    ProfileManager.getLastUsedRegularProfile()),
                            Matchers.not(parentalControlsEnabled));
                });
    }

    private void toggleActivityForegroundState() {
        ThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.getActivity().onPause());
        ThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.getActivity().onStop());
        ThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.getActivity().onStart());
        ThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.getActivity().onResume());
    }

    @Test
    @MediumTest
    @Feature({"DisableIncognitoMode"})
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
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());

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
                () -> {
                    Criteria.checkThat(
                            mActivityTestRule.tabsCount(/* incognito= */ true), Matchers.is(0));
                });
    }
}
