// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import android.annotation.SuppressLint;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.homepage.settings.HomepageEditor;
import org.chromium.chrome.browser.homepage.settings.HomepageSettings;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsProvider;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.UiUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/**
 * Integration test suite for partner homepage.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PartnerHomepageIntegrationTest {
    @Rule
    public BasePartnerBrowserCustomizationIntegrationTestRule mActivityTestRule =
            new BasePartnerBrowserCustomizationIntegrationTestRule();
    @Rule
    public SettingsActivityTestRule<HomepageSettings> mHomepageSettingsTestRule =
            new SettingsActivityTestRule<>(HomepageSettings.class);
    @Rule
    public SettingsActivityTestRule<HomepageEditor> mHomepageEditorTestRule =
            new SettingsActivityTestRule<>(HomepageEditor.class);

    private static final String TEST_PAGE = "/chrome/test/data/android/about.html";

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityFromLauncher();
    }

    /**
     * Homepage is loaded on startup.
     */
    @Test
    @MediumTest
    @Feature({"Homepage"})
    public void testHomepageInitialLoading() {
        Assert.assertEquals(Uri.parse(TestPartnerBrowserCustomizationsProvider.HOMEPAGE_URI),
                Uri.parse(ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab())));
    }

    /**
     * Clicking the homepage button should load homepage in the current tab.
     */
    @Test
    @MediumTest
    @Feature({"Homepage"})
    public void testHomepageButtonClick() throws InterruptedException {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        try {
            // Load non-homepage URL.
            mActivityTestRule.loadUrl(testServer.getURL(TEST_PAGE));
            UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
            Assert.assertNotSame(Uri.parse(TestPartnerBrowserCustomizationsProvider.HOMEPAGE_URI),
                    Uri.parse(ChromeTabUtils.getUrlStringOnUiThread(
                            mActivityTestRule.getActivity().getActivityTab())));

            // Click homepage button.
            ChromeTabUtils.waitForTabPageLoaded(mActivityTestRule.getActivity().getActivityTab(),
                    TestPartnerBrowserCustomizationsProvider.HOMEPAGE_URI, new Runnable() {
                        @Override
                        public void run() {
                            View homeButton =
                                    mActivityTestRule.getActivity().findViewById(R.id.home_button);
                            Assert.assertEquals("Homepage button is not shown", View.VISIBLE,
                                    homeButton.getVisibility());
                            TouchCommon.singleClickView(homeButton);
                        }
                    });
            Assert.assertEquals(Uri.parse(TestPartnerBrowserCustomizationsProvider.HOMEPAGE_URI),
                    Uri.parse(ChromeTabUtils.getUrlStringOnUiThread(
                            mActivityTestRule.getActivity().getActivityTab())));
        } finally {
            testServer.stopAndDestroyServer();
        }
    }

    /**
     * Homepage button visibility should be updated by enabling and disabling homepage in settings.
     */
    @Test
    @MediumTest
    @Feature({"Homepage"})
    public void testHomepageButtonEnableDisable() {
        // Disable homepage.
        toggleHomepageSwitchPreference(false);

        // Assert no homepage button.
        Assert.assertFalse(HomepageManager.isHomepageEnabled());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Homepage button is shown", View.GONE,
                    mActivityTestRule.getActivity().findViewById(R.id.home_button).getVisibility());
        });

        // Enable homepage.
        toggleHomepageSwitchPreference(true);

        // Assert homepage button.
        Assert.assertTrue(HomepageManager.isHomepageEnabled());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Homepage button is shown", View.VISIBLE,
                    mActivityTestRule.getActivity().findViewById(R.id.home_button).getVisibility());
        });
    }

    /**
     * Custom homepage URI should be fixed (e.g., "chrome.com" -> "http://chrome.com/")
     * when the URI is saved from the home page edit screen.
     */
    @Test
    @MediumTest
    @Feature({"Homepage"})
    public void testPreferenceCustomUriFixup() {
        // Change home page custom URI on hompage edit screen.
        final SettingsActivity editHomepagePreferenceActivity =
                mHomepageEditorTestRule.startSettingsActivity();
        TestThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            // TODO(crbug.com/635567): Fix this properly.
            @SuppressLint("SetTextI18n")
            public void run() {
                ((EditText) editHomepagePreferenceActivity.findViewById(R.id.homepage_url_edit))
                        .setText("chrome.com");
            }
        });
        Button saveButton =
                (Button) editHomepagePreferenceActivity.findViewById(R.id.homepage_save);
        TouchCommon.singleClickView(saveButton);

        CriteriaHelper.pollUiThread(() -> editHomepagePreferenceActivity.isDestroyed());

        Assert.assertEquals("http://chrome.com/", HomepageManager.getHomepageUri());
    }

    /**
     * Closing the last tab should also close Chrome on Tabbed mode.
     */
    @Test
    @MediumTest
    @Feature({"Homepage"})
    public void testLastTabClosed() {
        ChromeTabUtils.closeCurrentTab(InstrumentationRegistry.getInstrumentation(),
                (ChromeTabbedActivity) mActivityTestRule.getActivity());
        Assert.assertTrue("Activity was not closed.",
                mActivityTestRule.getActivity().isFinishing()
                        || mActivityTestRule.getActivity().isDestroyed());
    }

    /**
     * Closing all tabs should finalize all tab closures and close Chrome on Tabbed mode.
     */
    @Test
    @MediumTest
    @Feature({"Homepage"})
    public void testCloseAllTabs() {
        final CallbackHelper tabClosed = new CallbackHelper();
        final TabModel tabModel = mActivityTestRule.getActivity().getCurrentTabModel();
        mActivityTestRule.getActivity().getCurrentTabModel().addObserver(new TabModelObserver() {
            @Override
            public void didCloseTab(int tabId, boolean incognito) {
                if (tabModel.getCount() == 0) tabClosed.notifyCalled();
            }
        });
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getActivity().getTabModelSelector().closeAllTabs();
            }
        });

        try {
            tabClosed.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail("Never closed all of the tabs");
        }
        Assert.assertEquals("Expected no tabs to be present", 0,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());
        TabList fullModel =
                mActivityTestRule.getActivity().getCurrentTabModel().getComprehensiveModel();
        // By the time TAB_CLOSED event is received, all tab closures should be finalized
        Assert.assertEquals("Expected no tabs to be present in the comprehensive model", 0,
                fullModel.getCount());

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertTrue("Activity was not closed.",
                mActivityTestRule.getActivity().isFinishing()
                        || mActivityTestRule.getActivity().isDestroyed());
    }

    /**
     * Toggle the state of the homepage switch preference in settings by performing a click on it.
     *
     * @param expected Expected checked state of the preference switch after clicking.
     */
    private void toggleHomepageSwitchPreference(boolean expected) {
        // Launch preference activity with Homepage settings fragment.
        SettingsActivity homepagePreferenceActivity =
                mHomepageSettingsTestRule.startSettingsActivity();
        HomepageSettings fragment = mHomepageSettingsTestRule.getFragment();
        ChromeSwitchPreference preference = (ChromeSwitchPreference) fragment.findPreference(
                HomepageSettings.PREF_HOMEPAGE_SWITCH);
        Assert.assertNotNull(preference);

        // Click toggle and verify that checked state matches expectation.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            preference.performClick();
            Assert.assertEquals(preference.isChecked(), expected);
        });

        homepagePreferenceActivity.finish();
    }
}
