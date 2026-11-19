// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.notNullValue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.widget.Button;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.AdditionalAnswers;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragment.DialogOption;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsFragmentRegistry;
import org.chromium.chrome.browser.settings.SettingsHostFragment;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.site_settings.AllSiteSettings;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Tests where {@link ClearBrowsingDataFragment} leaves to once a deletion completes, and what it
 * leaves behind, when settings is shown in a tab and navigated by URL.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB, ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV})
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
public class ClearBrowsingDataInTabTest {
    private static final String ALL_SITES_URL = "chrome://settings/allSites";
    private static final String CLEAR_BROWSING_DATA_URL = "chrome://settings/clearBrowsingData";
    private static final String[] IMPORTANT_ORIGINS = {
        "https://www.chrome.com", "https://www.google.com"
    };

    /**
     * How long to wait for the page to leave once the deletion is started. That includes deleting
     * all of the browsing history and committing the navigation that follows, which takes longer
     * than the default poll on a loaded test device.
     */
    private static final long DELETION_TIMEOUT_MS = 10000L;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        // Delete only browsing history, of all time, which deletes the navigation entries before
        // the page too. Deleting tabs, for one, would close the tab under test.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowsingDataBridge bridge =
                            BrowsingDataBridge.getForProfile(mActivityTestRule.getProfile(false));
                    for (@DialogOption Integer option : ClearBrowsingDataFragment.getAllOptions()) {
                        bridge.setBrowsingDataDeletionPreference(
                                ClearBrowsingDataFragment.getDataType(option),
                                option == DialogOption.CLEAR_HISTORY);
                    }
                    bridge.setBrowsingDataDeletionTimePeriod(TimePeriod.ALL_TIME);
                });
    }

    /** Regression test for b/572636107. */
    @Test
    @MediumTest
    public void testDeletionReturnsToAllSites() {
        openPage(ALL_SITES_URL, AllSiteSettings.class);
        openPage(CLEAR_BROWSING_DATA_URL, ClearBrowsingDataFragment.class);

        deleteBrowsingData();

        waitForPage(ALL_SITES_URL, AllSiteSettings.class, DELETION_TIMEOUT_MS);
    }

    /**
     * Clicking delete twice, before the important sites dialog appears, used to show two dialogs.
     * Confirming one deleted the data and left the page, but the other stayed up, and confirming it
     * crashed on the page it belonged to having gone. Regression test for b/580100671.
     */
    @Test
    @MediumTest
    public void testImportantSitesDialogDoesNotOutliveDeletion() {
        BrowsingDataBridge.Natives natives = setUpImportantSites();
        openPage(ALL_SITES_URL, AllSiteSettings.class);
        openPage(CLEAR_BROWSING_DATA_URL, ClearBrowsingDataFragment.class);

        // Click twice in one task, so that the second click lands before the first dialog shows.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    clickClearButton();
                    clickClearButton();
                });
        ConfirmImportantSitesDialogFragment dialog = waitForImportantSitesDialog();
        ThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(1, countImportantSitesDialogs()));

        // Confirm the dialog. testImportantSitesDialogClosesWithPage covers leaving it unanswered.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AlertDialog alertDialog = (AlertDialog) dialog.requireDialog();
                    alertDialog.getButton(AlertDialog.BUTTON_POSITIVE).performClick();
                });

        waitForPage(ALL_SITES_URL, AllSiteSettings.class, DELETION_TIMEOUT_MS);
        verify(natives).clearBrowsingData(any(), any(), any(), anyInt(), any(), any());
        ThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(0, countImportantSitesDialogs()));
    }

    /**
     * Tests that the important sites dialog goes away with the page that showed it, rather than
     * staying up over the next page and deleting data for a page that is gone.
     */
    @Test
    @MediumTest
    public void testImportantSitesDialogClosesWithPage() {
        BrowsingDataBridge.Natives natives = setUpImportantSites();
        openPage(ALL_SITES_URL, AllSiteSettings.class);
        openPage(CLEAR_BROWSING_DATA_URL, ClearBrowsingDataFragment.class);

        ThreadUtils.runOnUiThreadBlocking(this::clickClearButton);
        waitForImportantSitesDialog();

        // Leave the dialog unanswered. testImportantSitesDialogDoesNotOutliveDeletion covers
        // confirming it.
        openPage(ALL_SITES_URL, AllSiteSettings.class);

        ThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(0, countImportantSitesDialogs()));
        verify(natives, never()).clearBrowsingData(any(), any(), any(), anyInt(), any(), any());
    }

    /**
     * Makes the next page opened report important sites, and selects cache for deletion, so that
     * deleting asks about them. Returns the natives, which otherwise behave as usual.
     */
    private BrowsingDataBridge.Natives setUpImportantSites() {
        BrowsingDataBridge.Natives natives =
                mock(
                        BrowsingDataBridge.Natives.class,
                        AdditionalAnswers.delegatesTo(BrowsingDataBridgeJni.get()));
        doAnswer(
                        invocation -> {
                            BrowsingDataBridge.ImportantSitesCallback callback =
                                    invocation.getArgument(1);
                            callback.onImportantRegisterableDomainsReady(
                                    IMPORTANT_ORIGINS,
                                    IMPORTANT_ORIGINS,
                                    new int[IMPORTANT_ORIGINS.length],
                                    /* dialogDisabled= */ false);
                            return null;
                        })
                .when(natives)
                .fetchImportantSites(any(), any());
        BrowsingDataBridgeJni.setInstanceForTesting(natives);

        // The important sites dialog is only shown when deleting cache or cookies.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowsingDataBridge bridge =
                            BrowsingDataBridge.getForProfile(mActivityTestRule.getProfile(false));
                    bridge.setBrowsingDataDeletionPreference(BrowsingDataType.CACHE, true);
                });
        return natives;
    }

    private void clickClearButton() {
        Fragment page = getShownPage();
        Assert.assertNotNull(page);
        Button clearButton = page.requireView().findViewById(R.id.clear_button);
        Assert.assertTrue(clearButton.isEnabled());
        clearButton.callOnClick();
    }

    private ConfirmImportantSitesDialogFragment waitForImportantSitesDialog() {
        CriteriaHelper.pollUiThread(
                () -> {
                    ConfirmImportantSitesDialogFragment dialog = getImportantSitesDialog();
                    Criteria.checkThat(dialog, notNullValue());
                    Criteria.checkThat(dialog.getDialog(), notNullValue());
                    Criteria.checkThat(dialog.getDialog().isShowing(), is(true));
                });
        return ThreadUtils.runOnUiThreadBlocking(this::getImportantSitesDialog);
    }

    private ConfirmImportantSitesDialogFragment getImportantSitesDialog() {
        Fragment page = getShownPage();
        Criteria.checkThat(page, instanceOf(ClearBrowsingDataFragment.class));
        return ((ClearBrowsingDataFragment) page).getImportantSitesDialogFragment();
    }

    /** Counts the important sites dialogs anywhere in the activity. */
    private int countImportantSitesDialogs() {
        return countImportantSitesDialogs(
                mActivityTestRule.getActivity().getSupportFragmentManager());
    }

    private static int countImportantSitesDialogs(FragmentManager fragmentManager) {
        int count = 0;
        for (Fragment fragment : fragmentManager.getFragments()) {
            if (fragment instanceof ConfirmImportantSitesDialogFragment) ++count;
            if (fragment.isAdded()) {
                count += countImportantSitesDialogs(fragment.getChildFragmentManager());
            }
        }
        return count;
    }

    private void openPage(String url, Class<? extends Fragment> fragmentClass) {
        // loadUrl() waits for the page to load, so the default poll suffices.
        mActivityTestRule.loadUrl(url);
        waitForPage(url, fragmentClass, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
    }

    private void deleteBrowsingData() {
        ThreadUtils.runOnUiThreadBlocking(this::clickClearButton);
    }

    private Fragment getShownPage() {
        SettingsHostFragment hostFragment =
                SettingsHostFragment.get(mActivityTestRule.getActivity());
        return hostFragment == null ? null : hostFragment.getMainFragment();
    }

    /** Waits for the tab to show {@code url}, rendered by {@code fragmentClass}. */
    private void waitForPage(String url, Class<? extends Fragment> fragmentClass, long timeoutMs) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Fragment page = getShownPage();
                    Criteria.checkThat(page, instanceOf(fragmentClass));
                    Criteria.checkThat(page.isResumed(), is(true));
                    Tab tab = mActivityTestRule.getActivityTab();
                    Criteria.checkThat(tab, notNullValue());
                    String tabUrl = tab.getUrl().getSpec();
                    Criteria.checkThat(
                            tabUrl,
                            SettingsFragmentRegistry.isSameSettingsPage(tabUrl, url),
                            is(true));
                },
                timeoutMs,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }
}
