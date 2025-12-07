// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.widget.Spinner;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.PublicTransitConfig;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.browsing_data.TimePeriodUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ChromeTriggers;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabGroupDialogFacility;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.quick_delete.QuickDeleteDialogFacility;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.SyncService;

import java.io.IOException;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.TimeoutException;

/** Tests for quick delete dialog view. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class QuickDeleteDialogDelegateTest {
    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.PRIVACY)
                    .build();

    @Mock private SyncService mMockSyncService;

    private ChromeTabbedActivity mActivity;
    private WebPageStation mPage;

    @Before
    public void setUp() {
        SyncServiceFactory.setInstanceForTesting(mMockSyncService);
        setSyncable(false);

        mPage = mCtaTestRule.startOnBlankPage();
        mActivity = mCtaTestRule.getActivity();
    }

    @After
    public void tearDown() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();

        // Clear history.
        runOnUiThreadBlocking(
                () -> {
                    BrowsingDataBridge.getForProfile(ProfileManager.getLastUsedRegularProfile())
                            .clearBrowsingData(
                                    callbackHelper::notifyCalled,
                                    new int[] {BrowsingDataType.HISTORY},
                                    TimePeriod.ALL_TIME);
                });

        callbackHelper.waitForOnly();
    }

    private void setSyncable(boolean syncable) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    when(mMockSyncService.getActiveDataTypes())
                            .thenReturn(
                                    syncable
                                            ? Set.of(DataType.HISTORY_DELETE_DIRECTIVES)
                                            : new HashSet<>());
                });
    }

    private int getNumberOfTabsInCurrentTabModel() {
        return ThreadUtils.runOnUiThreadBlocking(() -> mActivity.getCurrentTabModel().getCount());
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Feature({"RenderTest"})
    public void testQuickDeleteDialogView_WithSignInAndSync() throws IOException {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        setSyncable(true);

        mPage = mPage.loadWebPageProgrammatically("https://www.example.com/");
        mPage = mPage.openFakeLinkToWebPage("https://www.google.com/");
        assertEquals(2, getNumberOfTabsInCurrentTabModel());

        QuickDeleteDialogFacility dialog = mPage.openRegularTabAppMenu().clearBrowsingData();

        assertEquals("google.com + 1 site", dialog.historyInfoElement.value().getText().toString());
        dialog.expectMoreOnSyncedDevices(/* shown= */ true);
        assertTrue(dialog.tabsInfoElement.value().isEnabled());
        assertEquals("2 tabs on this device", dialog.tabsInfoElement.value().getText());
        dialog.expectSearchHistoryDisambiguation(/* shown= */ true);

        mRenderTestRule.render(
                dialog.customViewElement.value(), "quick_delete_dialog-signed-in-and-sync");

        dialog.clickCancel();
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Feature({"RenderTest"})
    public void testQuickDeleteDialogView_WithSignInOnly() throws IOException {
        mSigninTestRule.addTestAccountThenSignin();
        setSyncable(false);

        mPage = mPage.loadWebPageProgrammatically("https://www.google.com/");
        assertEquals(1, getNumberOfTabsInCurrentTabModel());

        QuickDeleteDialogFacility dialog = mPage.openRegularTabAppMenu().clearBrowsingData();

        assertEquals("google.com", dialog.historyInfoElement.value().getText().toString());
        dialog.expectMoreOnSyncedDevices(/* shown= */ false);
        assertTrue(dialog.tabsInfoElement.value().isEnabled());
        assertEquals("1 tab on this device", dialog.tabsInfoElement.value().getText());
        dialog.expectSearchHistoryDisambiguation(/* shown= */ true);

        mRenderTestRule.render(dialog.customViewElement.value(), "quick_delete_dialog-signed-in");

        dialog.clickCancel();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testQuickDeleteDialogView_WithoutTabsOrHistory() throws IOException {
        // Close all tabs, which goes to the tab switcher.
        RegularTabSwitcherStation tabSwitcher =
                ChromeTriggers.closeAllTabsProgrammaticallyTo(mPage)
                        .arriveAt(
                                new RegularTabSwitcherStation(
                                        /* regularTabsExist= */ false,
                                        /* incognitoTabsExist= */ false));
        assertEquals(0, getNumberOfTabsInCurrentTabModel());

        QuickDeleteDialogFacility dialog = tabSwitcher.openAppMenu().clearBrowsingData();

        assertEquals(
                "No sites from the last 15 minutes",
                dialog.historyInfoElement.value().getText().toString());
        dialog.expectMoreOnSyncedDevices(/* shown= */ false);
        assertTrue(dialog.tabsInfoElement.value().isEnabled());
        assertEquals("No tabs from the last 15 minutes", dialog.tabsInfoElement.value().getText());
        dialog.expectSearchHistoryDisambiguation(/* shown= */ false);

        mRenderTestRule.render(
                dialog.customViewElement.value(), "quick_delete_dialog-no-tabs-or-history");

        // Return to a page for InitialStateRule to reset state.
        dialog.clickCancel();
        tabSwitcher.openNewTab();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testQuickDeleteDialogView_MultiInstance() throws IOException {
        MultiWindowUtils.setInstanceCountForTesting(3);
        QuickDeleteDialogFacility dialog = mPage.openRegularTabAppMenu().clearBrowsingData();

        mRenderTestRule.render(
                dialog.customViewElement.value(), "quick_delete_dialog-tabs-disabled");

        dialog.clickCancel();
    }

    @Test
    @MediumTest
    public void testQuickDeleteDialogSpinnerViewContents() {
        QuickDeleteDialogFacility dialog = mPage.openRegularTabAppMenu().clearBrowsingData();
        Spinner spinnerView = dialog.spinnerElement.value();
        assertEquals(6, spinnerView.getAdapter().getCount());
        assertEquals(
                TimePeriod.LAST_15_MINUTES,
                ((TimePeriodUtils.TimePeriodSpinnerOption) spinnerView.getSelectedItem())
                        .getTimePeriod());
        assertEquals("Last 15 minutes", spinnerView.getItemAtPosition(0).toString());
        assertEquals("Last hour", spinnerView.getItemAtPosition(1).toString());
        assertEquals("Last 24 hours", spinnerView.getItemAtPosition(2).toString());
        assertEquals("Last 7 days", spinnerView.getItemAtPosition(3).toString());
        assertEquals("Last 4 weeks", spinnerView.getItemAtPosition(4).toString());
        assertEquals("All time", spinnerView.getItemAtPosition(5).toString());

        dialog.clickCancel();
        PublicTransitConfig.setOnExceptionCallback(
                TabGroupDialogFacility.TABS_LIST::printFromRoot, false);
    }
}
