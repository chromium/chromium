// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Build;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.Button;
import android.widget.ListView;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;
import androidx.collection.ArraySet;
import androidx.fragment.app.Fragment;
import androidx.preference.CheckBoxPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.viewpager2.widget.ViewPager2;

import com.google.android.material.tabs.TabLayout;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge.OnClearBrowsingDataListener;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragment.DialogOption;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.FakeSyncServiceImpl;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.browser_ui.settings.SpinnerPreference;
import org.chromium.components.browsing_data.DeleteBrowsingDataAction;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.sync.DataType;
import org.chromium.ui.test.util.ViewUtils;

import java.util.Arrays;
import java.util.Set;

/** Tests for ClearBrowsingDataFragment interaction with underlying data model. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ClearBrowsingDataFragmentTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public SettingsActivityTestRule<ClearBrowsingDataFragmentAdvanced> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(ClearBrowsingDataFragmentAdvanced.class);

    @Rule
    public SettingsActivityTestRule<ClearBrowsingDataTabsFragment>
            mSettingsActivityTabFragmentTestRule =
                    new SettingsActivityTestRule<>(ClearBrowsingDataTabsFragment.class);

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private BrowsingDataBridge.Natives mBrowsingDataBridgeMock;

    @Mock private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;

    private final CallbackHelper mCallbackHelper = new CallbackHelper();

    @TimePeriod private static final int DEFAULT_TIME_PERIOD = TimePeriod.ALL_TIME;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(BrowsingDataBridgeJni.TEST_HOOKS, mBrowsingDataBridgeMock);
        // Ensure that whenever the mock is asked to clear browsing data, the callback is
        // immediately called.
        doAnswer(
                        (Answer<Void>)
                                invocation -> {
                                    ((OnClearBrowsingDataListener) invocation.getArgument(2))
                                            .onBrowsingDataCleared();
                                    mCallbackHelper.notifyCalled();
                                    return null;
                                })
                .when(mBrowsingDataBridgeMock)
                .clearBrowsingData(
                        any(), any(), any(), any(), anyInt(), any(), any(), any(), any());

        // Default to delete all history.
        when(mBrowsingDataBridgeMock.getBrowsingDataDeletionTimePeriod(any(), any(), anyInt()))
                .thenReturn(DEFAULT_TIME_PERIOD);

        mActivityTestRule.startMainActivityOnBlankPage();

        // There can be some left-over notification channels from other tests.
        // TODO(crbug.com/41452182): Find a general solution to avoid leaking channels between
        // tests.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        SiteChannelsManager manager = SiteChannelsManager.getInstance();
                        manager.deleteAllSiteChannels();
                    });
        }
    }

    /** Waits for the progress dialog to disappear from the given CBD preference. */
    private void waitForProgressToComplete(final ClearBrowsingDataFragment preferences) {
        // For pre-M, give it a bit more time to complete.
        final long kDelay = CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(preferences.getProgressDialog(), Matchers.nullValue());
                },
                kDelay,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private static void clickClearButton(Fragment preferences) {
        Button clearButton = preferences.getView().findViewById(R.id.clear_button);
        Assert.assertNotNull(clearButton);
        Assert.assertTrue(clearButton.isEnabled());
        clearButton.callOnClick();
    }

    private SettingsActivity startPreferences() {
        SettingsActivity settingsActivity =
                mSettingsActivityTestRule.startSettingsActivity(
                        ClearBrowsingDataFragment.createFragmentArgs(
                                mActivityTestRule.getActivity().getClass().getName(),
                                /* isFetcherSuppliedFromOutside= */ false));
        ClearBrowsingDataFragment fragment = mSettingsActivityTestRule.getFragment();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    fragment.getClearBrowsingDataFetcher()
                            .fetchImportantSites(fragment.getProfile());
                });
        return settingsActivity;
    }

    @Test
    @LargeTest
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testSigningOut_Legacy() {
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        final ClearBrowsingDataFragment preferences =
                (ClearBrowsingDataFragment) startPreferences().getMainFragment();
        CriteriaHelper.pollUiThread(
                () -> {
                    return mSettingsActivityTestRule
                                    .getActivity()
                                    .findViewById(R.id.menu_id_targeted_help)
                            != null;
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RecyclerView recyclerView =
                            preferences.getView().findViewById(R.id.recycler_view);
                    recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() - 1);
                });
        onView(withText(preferences.buildSignOutOfChromeText().toString()))
                .perform(clickOnSignOutLink());
        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());
        CriteriaHelper.pollUiThread(
                () ->
                        !IdentityServicesProvider.get()
                                .getIdentityManager(ProfileManager.getLastUsedRegularProfile())
                                .hasPrimaryAccount(ConsentLevel.SIGNIN),
                "Account should be signed out!");

        // Footer should be hidden after sign-out.
        onView(withText(preferences.buildSignOutOfChromeText().toString())).check(doesNotExist());
    }

    @Test
    @LargeTest
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testSigningOut() {
        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.TEST_ACCOUNT_1);
        final ClearBrowsingDataFragment preferences =
                (ClearBrowsingDataFragment) startPreferences().getMainFragment();
        ViewUtils.waitForVisibleView(withId(R.id.menu_id_targeted_help));

        onView(withId(R.id.recycler_view)).perform(RecyclerViewActions.scrollToLastPosition());
        onView(withText(preferences.buildSignOutOfChromeText().toString()))
                .perform(clickOnSignOutLink());

        // Title of the confirm sign out dialog.
        onView(withText(R.string.sign_out_title)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withText(R.string.sign_out)).inRoot(isDialog()).perform(click());

        CriteriaHelper.pollUiThread(
                () ->
                        !IdentityServicesProvider.get()
                                .getIdentityManager(ProfileManager.getLastUsedRegularProfile())
                                .hasPrimaryAccount(ConsentLevel.SIGNIN),
                "Account should be signed out!");
        // Footer should be hidden after sign-out.
        onView(withText(preferences.buildSignOutOfChromeText().toString())).check(doesNotExist());
    }

    @Test
    @LargeTest
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testSigningOut_UnsavedDataDialog() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FakeSyncServiceImpl fakeSyncService = new FakeSyncServiceImpl();
                    fakeSyncService.setTypesWithUnsyncedData(Set.of(DataType.BOOKMARKS));
                    SyncServiceFactory.setInstanceForTesting(fakeSyncService);
                });
        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.TEST_ACCOUNT_1);
        final ClearBrowsingDataFragment preferences =
                (ClearBrowsingDataFragment) startPreferences().getMainFragment();
        ViewUtils.waitForVisibleView(withId(R.id.menu_id_targeted_help));

        onView(withId(R.id.recycler_view)).perform(RecyclerViewActions.scrollToLastPosition());
        onView(withText(preferences.buildSignOutOfChromeText().toString()))
                .perform(clickOnSignOutLink());

        // Title of the confirm sign out dialog.
        onView(withText(R.string.sign_out_unsaved_data_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withText(R.string.sign_out_unsaved_data_primary_button))
                .inRoot(isDialog())
                .perform(click());

        CriteriaHelper.pollUiThread(
                () ->
                        !IdentityServicesProvider.get()
                                .getIdentityManager(ProfileManager.getLastUsedRegularProfile())
                                .hasPrimaryAccount(ConsentLevel.SIGNIN),
                "Account should be signed out!");
        // Footer should be hidden after sign-out.
        onView(withText(preferences.buildSignOutOfChromeText().toString())).check(doesNotExist());
    }

    /** Test that Clear Browsing Data offers two tabs and records a preference when switched. */
    @Test
    @MediumTest
    public void testTabsSwitcher() {
        setDataTypesToClear(ClearBrowsingDataFragment.getAllOptions().toArray(new Integer[0]));
        // Set "Advanced" as the user's cached preference.
        when(mBrowsingDataBridgeMock.getLastClearBrowsingDataTab(any(), any())).thenReturn(1);

        mSettingsActivityTabFragmentTestRule.startSettingsActivity(
                ClearBrowsingDataTabsFragment.createFragmentArgs(
                        mActivityTestRule.getActivity().getClass().getName()));
        final ClearBrowsingDataTabsFragment preferences =
                mSettingsActivityTabFragmentTestRule.getFragment();

        // Verify tab preference is loaded.
        verify(mBrowsingDataBridgeMock).getLastClearBrowsingDataTab(any(), any());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ViewPager2 viewPager =
                            (ViewPager2)
                                    preferences
                                            .getView()
                                            .findViewById(R.id.clear_browsing_data_viewpager);
                    RecyclerView.Adapter adapter = viewPager.getAdapter();
                    Assert.assertEquals(2, adapter.getItemCount());
                    Assert.assertEquals(1, viewPager.getCurrentItem());
                    TabLayout tabLayout =
                            preferences.getView().findViewById(R.id.clear_browsing_data_tabs);
                    Assert.assertEquals(
                            ApplicationProvider.getApplicationContext()
                                    .getString(R.string.clear_browsing_data_basic_tab_title),
                            tabLayout.getTabAt(0).getText());
                    Assert.assertEquals(
                            ApplicationProvider.getApplicationContext()
                                    .getString(R.string.prefs_section_advanced),
                            tabLayout.getTabAt(1).getText());
                    viewPager.setCurrentItem(0);
                });
        // Verify the tab preference is saved.
        verify(mBrowsingDataBridgeMock).setLastClearBrowsingDataTab(any(), any(), eq(0));
    }

    /**
     * Tests that a fragment with all options preselected indeed has all checkboxes checked on
     * startup, and that deletion with all checkboxes checked completes successfully.
     */
    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.QUICK_DELETE_FOR_ANDROID,
        ChromeFeatureList.QUICK_DELETE_ANDROID_FOLLOWUP
    })
    public void testClearingEverything() throws Exception {
        setDataTypesToClear(ClearBrowsingDataFragment.getAllOptions().toArray(new Integer[0]));

        final ClearBrowsingDataFragment preferences =
                (ClearBrowsingDataFragment) startPreferences().getMainFragment();
        final Profile expectedProfile = preferences.getProfile();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Privacy.DeleteBrowsingData.Action",
                        DeleteBrowsingDataAction.CLEAR_BROWSING_DATA_DIALOG);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PreferenceScreen screen = preferences.getPreferenceScreen();

                    for (int i = 0; i < screen.getPreferenceCount(); ++i) {
                        Preference pref = screen.getPreference(i);
                        if (!(pref instanceof CheckBoxPreference)) {
                            continue;
                        }
                        CheckBoxPreference checkbox = (CheckBoxPreference) pref;
                        Assert.assertTrue(checkbox.isChecked());
                    }
                    clickClearButton(preferences);
                });

        waitForProgressToComplete(preferences);
        mCallbackHelper.waitForOnly();

        // Verify DeleteBrowsingDataAction metric is recorded.
        histogramWatcher.assertExpected();

        // Verify that we got the appropriate call to clear all data.
        verify(mBrowsingDataBridgeMock)
                .clearBrowsingData(
                        any(),
                        eq(expectedProfile),
                        any(),
                        eq(getAllDataTypes()),
                        eq(DEFAULT_TIME_PERIOD),
                        any(),
                        any(),
                        any(),
                        any());
    }

    private static int[] getAllDataTypes() {
        Set<Integer> dialogTypes = ClearBrowsingDataFragment.getAllOptions();

        int[] datatypes = new int[dialogTypes.size()];
        for (int i = 0; i < datatypes.length; i++) {
            datatypes[i] = ClearBrowsingDataFragment.getDataType(i);
        }

        Arrays.sort(datatypes);
        return datatypes;
    }

    /** Tests that changing the time interval for deletion affects the delete request. */
    @Test
    @MediumTest
    public void testClearTimeInterval() throws Exception {
        setDataTypesToClear(DialogOption.CLEAR_CACHE);

        final ClearBrowsingDataFragment preferences =
                (ClearBrowsingDataFragment) startPreferences().getMainFragment();
        final Profile expectedProfile = preferences.getProfile();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    changeTimePeriodTo(preferences, TimePeriod.LAST_HOUR);
                    clickClearButton(preferences);
                });

        waitForProgressToComplete(preferences);
        mCallbackHelper.waitForOnly();

        // Verify that we got the appropriate call to clear all data.
        verify(mBrowsingDataBridgeMock)
                .clearBrowsingData(
                        any(),
                        eq(expectedProfile),
                        any(),
                        eq(new int[] {BrowsingDataType.CACHE}),
                        eq(TimePeriod.LAST_HOUR),
                        any(),
                        any(),
                        any(),
                        any());
    }

    /** Selects the specified time for browsing data removal. */
    private void changeTimePeriodTo(ClearBrowsingDataFragment preferences, @TimePeriod int time) {
        SpinnerPreference spinnerPref =
                (SpinnerPreference)
                        preferences.findPreference(ClearBrowsingDataFragment.PREF_TIME_RANGE);
        Spinner spinner = spinnerPref.getSpinnerForTesting();
        int itemCount = spinner.getAdapter().getCount();
        for (int i = 0; i < itemCount; i++) {
            var option = (TimePeriodUtils.TimePeriodSpinnerOption) spinner.getAdapter().getItem(i);
            if (option.getTimePeriod() == time) {
                spinner.setSelection(i);
                return;
            }
        }
        Assert.fail("Failed to find time period " + time);
    }

    @Test
    @MediumTest
    public void testHelpButtonClicked() {
        SettingsActivity activity = startPreferences();
        ClearBrowsingDataFragment fragment = mSettingsActivityTestRule.getFragment();

        HelpAndFeedbackLauncherFactory.setInstanceForTesting(mHelpAndFeedbackLauncher);
        onView(withId(R.id.menu_id_targeted_help)).perform(click());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    verify(mHelpAndFeedbackLauncher)
                            .show(
                                    activity,
                                    fragment.getString(R.string.help_context_clear_browsing_data),
                                    null);
                });
    }

    /**
     * A helper Runnable that opens the Settings activity containing a ClearBrowsingDataFragment
     * fragment and clicks the "Clear" button.
     */
    static class OpenPreferencesEnableDialogAndClickClearRunnable implements Runnable {
        final SettingsActivity mSettingsActivity;

        /**
         * Instantiates this OpenPreferencesEnableDialogAndClickClearRunnable.
         *
         * @param settingsActivity A Settings activity containing ClearBrowsingDataFragment
         *     fragment.
         */
        public OpenPreferencesEnableDialogAndClickClearRunnable(SettingsActivity settingsActivity) {
            mSettingsActivity = settingsActivity;
        }

        @Override
        public void run() {
            ClearBrowsingDataFragment fragment =
                    (ClearBrowsingDataFragment) mSettingsActivity.getMainFragment();

            // Enable the dialog and click the "Clear" button.
            ((ClearBrowsingDataFragment) mSettingsActivity.getMainFragment())
                    .getClearBrowsingDataFetcher()
                    .enableDialogAboutOtherFormsOfBrowsingHistory();
            clickClearButton(fragment);
        }
    }

    /**
     * A criterion that is satisfied when a ClearBrowsingDataFragment fragment in the given Settings
     * activity is closed.
     */
    static class PreferenceScreenClosedCriterion implements Runnable {
        final SettingsActivity mSettingsActivity;

        /**
         * Instantiates this PreferenceScreenClosedCriterion.
         *
         * @param settingsActivity A Settings activity containing ClearBrowsingDataFragment
         *     fragment.
         */
        public PreferenceScreenClosedCriterion(SettingsActivity settingsActivity) {
            mSettingsActivity = settingsActivity;
        }

        @Override
        public void run() {
            ClearBrowsingDataFragment fragment =
                    (ClearBrowsingDataFragment) mSettingsActivity.getMainFragment();
            if (fragment == null) return;
            Criteria.checkThat(fragment.isVisible(), Matchers.is(false));
        }
    }

    /**
     * Tests that if the dialog about other forms of browsing history is enabled, it will be shown
     * after the deletion completes, if and only if browsing history was checked for deletion and it
     * has not been shown before.
     */
    @Test
    @LargeTest
    public void testDialogAboutOtherFormsOfBrowsingHistory() throws Exception {
        // Sign in.
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        OtherFormsOfHistoryDialogFragment.clearShownPreferenceForTesting();

        // History is not selected. We still need to select some other datatype, otherwise the
        // "Clear" button won't be enabled.
        setDataTypesToClear(DialogOption.CLEAR_CACHE);
        final SettingsActivity settingsActivity1 = startPreferences();
        ThreadUtils.runOnUiThreadBlocking(
                new OpenPreferencesEnableDialogAndClickClearRunnable(settingsActivity1));
        mCallbackHelper.waitForCallback(0);

        assertDataTypesCleared(BrowsingDataType.CACHE);

        // The dialog about other forms of history is not shown. The Clear Browsing Data preferences
        // is closed as usual.
        CriteriaHelper.pollUiThread(new PreferenceScreenClosedCriterion(settingsActivity1));
        // Reopen Clear Browsing Data preferences, this time with history selected for clearing.
        setDataTypesToClear(DialogOption.CLEAR_HISTORY);
        final SettingsActivity settingsActivity2 = startPreferences();
        ThreadUtils.runOnUiThreadBlocking(
                new OpenPreferencesEnableDialogAndClickClearRunnable(settingsActivity2));

        // The dialog about other forms of history should now be shown.
        CriteriaHelper.pollUiThread(
                () -> {
                    ClearBrowsingDataFragment fragment =
                            (ClearBrowsingDataFragment) settingsActivity2.getMainFragment();
                    OtherFormsOfHistoryDialogFragment dialog =
                            fragment.getDialogAboutOtherFormsOfBrowsingHistory();
                    Criteria.checkThat(dialog, Matchers.notNullValue());
                    Criteria.checkThat(dialog.getActivity(), Matchers.notNullValue());
                });

        // Close that dialog.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ClearBrowsingDataFragment fragment =
                            (ClearBrowsingDataFragment) settingsActivity2.getMainFragment();
                    fragment.getDialogAboutOtherFormsOfBrowsingHistory()
                            .onClick(null, AlertDialog.BUTTON_POSITIVE);
                });

        // That should close the preference screen as well.
        CriteriaHelper.pollUiThread(new PreferenceScreenClosedCriterion(settingsActivity2));

        mCallbackHelper.waitForCallback(1);
        // Verify history cleared.
        assertDataTypesCleared(BrowsingDataType.HISTORY);

        // Reopen Clear Browsing Data preferences and clear history once again.
        setDataTypesToClear(DialogOption.CLEAR_HISTORY);
        final SettingsActivity settingsActivity3 = startPreferences();
        final Profile expectedProfile = mSettingsActivityTestRule.getFragment().getProfile();
        ThreadUtils.runOnUiThreadBlocking(
                new OpenPreferencesEnableDialogAndClickClearRunnable(settingsActivity3));

        // The dialog about other forms of browsing history is still enabled, and history has been
        // selected for deletion. However, the dialog has already been shown before, and therefore
        // we won't show it again. Expect that the preference screen closes.
        CriteriaHelper.pollUiThread(new PreferenceScreenClosedCriterion(settingsActivity3));

        int[] expectedTypes = new int[] {BrowsingDataType.HISTORY};
        // Should be cleared again.
        verify(mBrowsingDataBridgeMock, times(2))
                .clearBrowsingData(
                        any(),
                        eq(expectedProfile),
                        any(),
                        eq(expectedTypes),
                        anyInt(),
                        any(),
                        any(),
                        any(),
                        any());
    }

    /**
     * Verify that we got the appropriate call to clear browsing data with the given types.
     *
     * @param types Values of BrowsingDataType to remove.
     */
    private void assertDataTypesCleared(int... types) {
        // TODO(yfriedman): Add testing for time period.
        verify(mBrowsingDataBridgeMock)
                .clearBrowsingData(
                        any(),
                        any(),
                        any(),
                        eq(types),
                        eq(DEFAULT_TIME_PERIOD),
                        any(),
                        any(),
                        any(),
                        any());
    }

    /** This presses the 'clear' button on the root preference page. */
    private Runnable getPressClearRunnable(final ClearBrowsingDataFragment preferences) {
        return () -> clickClearButton(preferences);
    }

    /** This presses the clear button in the important sites dialog */
    private Runnable getPressButtonInImportantDialogRunnable(
            final ClearBrowsingDataFragment preferences, final int whichButton) {
        return () -> {
            Assert.assertNotNull(preferences);
            ConfirmImportantSitesDialogFragment dialog =
                    preferences.getImportantSitesDialogFragment();
            ((AlertDialog) dialog.getDialog()).getButton(whichButton).performClick();
        };
    }

    /**
     * This waits until the important dialog fragment & the given number of important sites are
     * shown.
     */
    private void waitForImportantDialogToShow(
            final ClearBrowsingDataFragment preferences, final int numImportantSites) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(preferences, Matchers.notNullValue());
                    Criteria.checkThat(
                            preferences.getImportantSitesDialogFragment(), Matchers.notNullValue());
                    Criteria.checkThat(
                            preferences.getImportantSitesDialogFragment().getDialog(),
                            Matchers.notNullValue());
                    Criteria.checkThat(
                            preferences.getImportantSitesDialogFragment().getDialog().isShowing(),
                            Matchers.is(true));

                    ListView sitesList =
                            preferences.getImportantSitesDialogFragment().getSitesList();
                    Criteria.checkThat(
                            sitesList.getAdapter().getCount(), Matchers.is(numImportantSites));
                    Criteria.checkThat(
                            sitesList.getChildCount(),
                            Matchers.greaterThanOrEqualTo(numImportantSites));
                });
    }

    private void markOriginsAsImportant(final String[] importantOrigins) {
        doAnswer(
                        invocation -> {
                            BrowsingDataBridge.ImportantSitesCallback callback =
                                    invocation.getArgument(1);
                            // Reason '0' is still a reason so just init the array.
                            int[] reasons = new int[importantOrigins.length];
                            callback.onImportantRegisterableDomainsReady(
                                    importantOrigins, importantOrigins, reasons, false);
                            return null;
                        })
                .when(mBrowsingDataBridgeMock)
                .fetchImportantSites(any(), any());

        // Return arbitrary number above 1.
        when(mBrowsingDataBridgeMock.getMaxImportantSites()).thenReturn(3);
    }

    /**
     * Tests that the important sites dialog is shown, and if we don't deselect anything we
     * correctly clear everything.
     */
    @Test
    @MediumTest
    @Feature({"SiteEngagement"})
    public void testImportantSitesDialogNoFiltering() throws Exception {
        // Sign in.
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();

        final String[] importantOrigins = {"http://www.facebook.com", "https://www.google.com"};
        // First mark our origins as important.
        markOriginsAsImportant(importantOrigins);
        setDataTypesToClear(DialogOption.CLEAR_HISTORY, DialogOption.CLEAR_CACHE);

        ClearBrowsingDataFragment preferences =
                (ClearBrowsingDataFragment) startPreferences().getMainFragment();

        // Clear in root preference.
        ThreadUtils.runOnUiThreadBlocking(getPressClearRunnable(preferences));
        // Check that the important sites dialog is shown, and the list is visible.
        waitForImportantDialogToShow(preferences, 2);
        // Clear in important dialog.
        ThreadUtils.runOnUiThreadBlocking(
                getPressButtonInImportantDialogRunnable(preferences, AlertDialog.BUTTON_POSITIVE));
        waitForProgressToComplete(preferences);
        mCallbackHelper.waitForOnly();

        // Verify history cleared.
        assertDataTypesCleared(BrowsingDataType.HISTORY, BrowsingDataType.CACHE);
    }

    /**
     * Tests that the important sites dialog is shown and if we cancel nothing happens.
     *
     * <p>http://crbug.com/727310
     */
    @Test
    @MediumTest
    @Feature({"SiteEngagement"})
    public void testImportantSitesDialogNoopOnCancel() throws Exception {
        // Sign in.
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();

        final String[] importantOrigins = {"http://www.facebook.com", "http://www.google.com"};
        // First mark our origins as important.
        markOriginsAsImportant(importantOrigins);
        setDataTypesToClear(DialogOption.CLEAR_HISTORY, DialogOption.CLEAR_CACHE);

        SettingsActivity settingsActivity = startPreferences();
        ClearBrowsingDataFragment fragment =
                (ClearBrowsingDataFragment) settingsActivity.getMainFragment();
        Profile expectedProfile = fragment.getProfile();
        ThreadUtils.runOnUiThreadBlocking(getPressClearRunnable(fragment));
        // Check that the important sites dialog is shown, and the list is visible.
        waitForImportantDialogToShow(fragment, 2);
        // Press the cancel button.
        ThreadUtils.runOnUiThreadBlocking(
                getPressButtonInImportantDialogRunnable(fragment, AlertDialog.BUTTON_NEGATIVE));
        settingsActivity.finish();

        // Nothing was cleared.
        verify(mBrowsingDataBridgeMock, never())
                .clearBrowsingData(
                        any(),
                        eq(expectedProfile),
                        any(),
                        any(),
                        anyInt(),
                        any(),
                        any(),
                        any(),
                        any());
    }

    /**
     * Tests that the important sites dialog is shown, we can successfully uncheck options, and
     * clicking clear doesn't clear the protected domain.
     */
    @Test
    @MediumTest
    @Feature({"SiteEngagement"})
    public void testImportantSitesDialog() throws Exception {
        // Sign in.
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();

        final String kKeepDomain = "https://www.chrome.com";
        final String kClearDomain = "https://www.google.com";

        final String[] importantOrigins = {kKeepDomain, kClearDomain};

        // First mark our origins as important.
        markOriginsAsImportant(importantOrigins);

        setDataTypesToClear(DialogOption.CLEAR_HISTORY, DialogOption.CLEAR_CACHE);

        final SettingsActivity settingsActivity = startPreferences();
        final ClearBrowsingDataFragment fragment =
                (ClearBrowsingDataFragment) settingsActivity.getMainFragment();
        final Profile expectedProfile = fragment.getProfile();

        // Uncheck the first item (our internal web server).
        ThreadUtils.runOnUiThreadBlocking(getPressClearRunnable(fragment));
        waitForImportantDialogToShow(fragment, 2);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ListView sitesList = fragment.getImportantSitesDialogFragment().getSitesList();
                    sitesList.performItemClick(
                            sitesList.getChildAt(0), 0, sitesList.getAdapter().getItemId(0));
                });

        // Check that our server origin is in the set of deselected domains.
        CriteriaHelper.pollUiThread(
                () -> {
                    ConfirmImportantSitesDialogFragment dialog =
                            fragment.getImportantSitesDialogFragment();
                    Criteria.checkThat(
                            dialog.getDeselectedDomains(), Matchers.hasItem(kKeepDomain));
                });

        // Click the clear button.
        ThreadUtils.runOnUiThreadBlocking(
                getPressButtonInImportantDialogRunnable(fragment, AlertDialog.BUTTON_POSITIVE));

        waitForProgressToComplete(fragment);
        mCallbackHelper.waitForOnly();

        int[] expectedTypes = new int[] {BrowsingDataType.HISTORY, BrowsingDataType.CACHE};
        String[] keepDomains = new String[] {kKeepDomain};
        String[] ignoredDomains = new String[] {kClearDomain};

        verify(mBrowsingDataBridgeMock)
                .clearBrowsingData(
                        any(),
                        eq(expectedProfile),
                        any(),
                        eq(expectedTypes),
                        eq(DEFAULT_TIME_PERIOD),
                        eq(keepDomains),
                        any(),
                        eq(ignoredDomains),
                        any());
    }

    @Test
    @MediumTest
    @Features.DisableFeatures(ChromeFeatureList.QUICK_DELETE_ANDROID_FOLLOWUP)
    @Features.EnableFeatures(ChromeFeatureList.QUICK_DELETE_FOR_ANDROID)
    public void testTabsCheckbox_withQuickDeleteV2Disabled() {
        ClearBrowsingDataFragment preferences =
                (ClearBrowsingDataFragment) startPreferences().getMainFragment();
        CheckBoxPreference checkboxPreference =
                preferences.findPreference(
                        ClearBrowsingDataFragment.getPreferenceKey(DialogOption.CLEAR_TABS));
        assertNull(checkboxPreference);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.QUICK_DELETE_FOR_ANDROID,
        ChromeFeatureList.QUICK_DELETE_ANDROID_FOLLOWUP
    })
    public void testTabsCheckbox_SingleInstance_withQuickDeleteV2Enabled() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Privacy.ClearBrowsingData.TabsEnabled", true);

        ClearBrowsingDataFragment preferences =
                (ClearBrowsingDataFragment) startPreferences().getMainFragment();
        CheckBoxPreference checkboxPreference =
                preferences.findPreference(
                        ClearBrowsingDataFragment.getPreferenceKey(DialogOption.CLEAR_TABS));

        assertNotNull(checkboxPreference);
        assertTrue(checkboxPreference.isEnabled());
        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.QUICK_DELETE_FOR_ANDROID,
        ChromeFeatureList.QUICK_DELETE_ANDROID_FOLLOWUP
    })
    public void testTabsCheckbox_MultiInstance_withQuickDeleteV2Enabled() {
        MultiWindowUtils.setInstanceCountForTesting(3);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Privacy.ClearBrowsingData.TabsEnabled", false);

        ClearBrowsingDataFragment preferences =
                (ClearBrowsingDataFragment) startPreferences().getMainFragment();
        CheckBoxPreference checkboxPreference =
                preferences.findPreference(
                        ClearBrowsingDataFragment.getPreferenceKey(DialogOption.CLEAR_TABS));

        assertNotNull(checkboxPreference);
        assertFalse(checkboxPreference.isEnabled());
        assertEquals(
                ApplicationProvider.getApplicationContext()
                        .getString(R.string.clear_tabs_disabled_summary),
                checkboxPreference.getSummary());
        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.QUICK_DELETE_FOR_ANDROID,
        ChromeFeatureList.QUICK_DELETE_ANDROID_FOLLOWUP
    })
    public void testSnackbarShown_defaultTimePeriod_withQuickDeleteV2Enabled() throws Exception {
        setDataTypesToClear(DialogOption.CLEAR_CACHE);

        final ClearBrowsingDataFragment preferences =
                (ClearBrowsingDataFragment) startPreferences().getMainFragment();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    clickClearButton(preferences);
                });

        waitForProgressToComplete(preferences);
        mCallbackHelper.waitForOnly();

        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        final String expectedSnackbarMessage =
                activity.getResources().getString(R.string.quick_delete_snackbar_all_time_message);
        waitForSnackbar(expectedSnackbarMessage);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.QUICK_DELETE_FOR_ANDROID,
        ChromeFeatureList.QUICK_DELETE_ANDROID_FOLLOWUP
    })
    public void testSnackbarShown_changeTimePeriod_withQuickDeleteV2Enabled() throws Exception {
        setDataTypesToClear(DialogOption.CLEAR_CACHE);

        final ClearBrowsingDataFragment preferences =
                (ClearBrowsingDataFragment) startPreferences().getMainFragment();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    changeTimePeriodTo(preferences, TimePeriod.LAST_HOUR);
                    clickClearButton(preferences);
                });

        waitForProgressToComplete(preferences);
        mCallbackHelper.waitForOnly();

        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        final String expectedSnackbarMessage =
                activity.getString(
                        R.string.quick_delete_snackbar_message,
                        TimePeriodUtils.getTimePeriodString(activity, TimePeriod.LAST_HOUR));
        waitForSnackbar(expectedSnackbarMessage);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.QUICK_DELETE_FOR_ANDROID,
        ChromeFeatureList.QUICK_DELETE_ANDROID_FOLLOWUP
    })
    public void testTabsCheckboxHidden_WhenLaunchedFromSearch() {
        mSettingsActivityTestRule.startSettingsActivity(
                ClearBrowsingDataFragment.createFragmentArgs(
                        SearchActivity.class.getName(), /* isFetcherSuppliedFromOutside= */ false));
        ClearBrowsingDataFragment fragment = mSettingsActivityTestRule.getFragment();

        CheckBoxPreference checkboxPreference =
                fragment.findPreference(
                        ClearBrowsingDataFragment.getPreferenceKey(DialogOption.CLEAR_TABS));

        assertNull(checkboxPreference);
    }

    /** Wait for the snackbar to show on the main activity post deletion. */
    private void waitForSnackbar(String expectedSnackbarMessage) {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    SnackbarManager snackbarManager = activity.getSnackbarManager();
                    Criteria.checkThat(snackbarManager.isShowing(), Matchers.is(true));
                    TextView snackbarMessage = activity.findViewById(R.id.snackbar_message);
                    Criteria.checkThat(snackbarMessage, Matchers.notNullValue());
                    Criteria.checkThat(
                            snackbarMessage.getText().toString(),
                            Matchers.is(expectedSnackbarMessage));
                });
    }

    private void setDataTypesToClear(final Integer... typesToClear) {
        Set<Integer> typesToClearSet = new ArraySet<Integer>(Arrays.asList(typesToClear));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (@DialogOption Integer option : ClearBrowsingDataFragment.getAllOptions()) {
                        boolean enabled = typesToClearSet.contains(option);
                        when(mBrowsingDataBridgeMock.getBrowsingDataDeletionPreference(
                                        any(),
                                        any(),
                                        eq(ClearBrowsingDataFragment.getDataType(option)),
                                        anyInt()))
                                .thenReturn(enabled);
                    }
                });
    }

    // TODO(crbug.com/40846557): Move this to a test util class.
    private ViewAction clickOnSignOutLink() {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return Matchers.instanceOf(TextView.class);
            }

            @Override
            public String getDescription() {
                return "Clicks on the sign out link in the clear browsing data footer";
            }

            @Override
            public void perform(UiController uiController, View view) {
                TextView textView = (TextView) view;
                Spanned spannedString = (Spanned) textView.getText();
                ClickableSpan[] spans =
                        spannedString.getSpans(0, spannedString.length(), ClickableSpan.class);
                if (spans.length != 1) {
                    throw new NoMatchingViewException.Builder()
                            .includeViewHierarchy(true)
                            .withRootView(textView)
                            .build();
                }
                spans[0].onClick(view);
            }
        };
    }
}
