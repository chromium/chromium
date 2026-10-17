// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.startup;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.SessionStartupPref;
import org.chromium.chrome.browser.multiwindow.TabbedStartupCoordinator;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.containment.ContainedLinearLayout;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.listmenu.ListMenuButton;

import java.util.List;

/** Instrumentation tests for {@link OnStartupSettingsFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@EnableFeatures(ChromeFeatureList.ON_STARTUP_SETTINGS_UI)
public class OnStartupSettingsFragmentTest {
    private static final String URL_ESPN = "https://www.espn.com/";
    private static final String URL_WIKIPEDIA = "https://www.wikipedia.org/";
    private static final String URL_DOCS = "https://docs.google.com/document/u/0/";

    @Rule
    public final SettingsTestRule<OnStartupSettingsFragment> mSettingsTestRule =
            new SettingsTestRule<>(OnStartupSettingsFragment.class);

    private Profile mProfile;

    @Before
    public void setUp() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ true);
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mProfile = ProfileManager.getLastUsedRegularProfile());
        setRestoreOnStartupUrls(List.of());
    }

    @After
    public void tearDown() {
        setRestoreOnStartupUrls(List.of());
        setRestoreOnStartupPref(SessionStartupPref.LAST);
    }

    @Test
    @SmallTest
    public void testInitialSelection_matchesStoredStartupPref() {
        // Setup stored startup preference as URLS.
        setRestoreOnStartupPref(SessionStartupPref.URLS);

        // Act.
        RadioButtonGroupOnStartupPreference pref = launchAndGetRadioGroupPreference();

        // Verify initial selection matches URLS and the other radio buttons are unchecked.
        assertFalse(pref.getNewTabPrefForTesting().isChecked());
        assertFalse(pref.getLastPrefForTesting().isChecked());
        assertTrue(pref.getUrlsPrefForTesting().isChecked());
    }

    @Test
    @SmallTest
    public void testSelectingRadioButtons_updatesPrefService() {
        // Setup stored startup preference as NEW_TAB.
        setRestoreOnStartupPref(SessionStartupPref.NEW_TAB);
        RadioButtonGroupOnStartupPreference pref = launchAndGetRadioGroupPreference();
        assertTrue(pref.getNewTabPrefForTesting().isChecked());
        assertEquals(View.GONE, pref.getStartupPagesSectionForTesting().getVisibility());
        assertTrue(pref.getUrlsPrefForTesting().getParent() instanceof ContainedLinearLayout);
        assertEquals(
                pref.getUrlsPrefForTesting().getParent(),
                pref.getStartupPagesSectionForTesting().getParent());

        // Act & Verify selecting LAST updates PrefService and hides startup pages section.
        ThreadUtils.runOnUiThreadBlocking(() -> pref.getLastPrefForTesting().performClick());
        assertEquals(View.GONE, pref.getStartupPagesSectionForTesting().getVisibility());
        assertEquals(SessionStartupPref.LAST, getRestoreOnStartupPref());

        // Act & Verify selecting URLS updates PrefService and reveals startup pages section.
        ThreadUtils.runOnUiThreadBlocking(() -> pref.getUrlsPrefForTesting().performClick());
        assertEquals(View.VISIBLE, pref.getStartupPagesSectionForTesting().getVisibility());
        assertEquals(SessionStartupPref.URLS, getRestoreOnStartupPref());
    }

    @Test
    @SmallTest
    public void testStartupPagesList_displaysConfiguredUrls() {
        // Setup configured startup URLs.
        setRestoreOnStartupPref(SessionStartupPref.URLS);
        setRestoreOnStartupUrls(List.of(URL_ESPN, URL_WIKIPEDIA, URL_DOCS));

        // Act.
        RadioButtonGroupOnStartupPreference pref = launchAndGetRadioGroupPreference();
        LinearLayout container = pref.getStartupPagesContainerForTesting();

        // Verify fallback display origin when unvisited, full URL with scheme and path, and menu
        // content description.
        assertNotNull(container);
        assertEquals(3, container.getChildCount());
        View firstItem = container.getChildAt(0);
        TextView firstTitle = firstItem.findViewById(R.id.startup_page_title);
        TextView firstUrl = firstItem.findViewById(R.id.startup_page_url);
        ListMenuButton firstMoreButton = firstItem.findViewById(R.id.startup_page_more_button);
        assertEquals("https://www.espn.com", firstTitle.getText().toString());
        assertEquals("https://www.espn.com", firstUrl.getText().toString());
        assertTrue(
                firstMoreButton
                        .getContentDescription()
                        .toString()
                        .contains("https://www.espn.com"));

        View secondItem = container.getChildAt(1);
        TextView secondTitle = secondItem.findViewById(R.id.startup_page_title);
        TextView secondUrl = secondItem.findViewById(R.id.startup_page_url);
        assertEquals("https://www.wikipedia.org", secondTitle.getText().toString());
        assertEquals("https://www.wikipedia.org", secondUrl.getText().toString());

        View thirdItem = container.getChildAt(2);
        TextView thirdTitle = thirdItem.findViewById(R.id.startup_page_title);
        TextView thirdUrl = thirdItem.findViewById(R.id.startup_page_url);
        assertEquals("https://docs.google.com", thirdTitle.getText().toString());
        assertEquals("https://docs.google.com/document/u/0/", thirdUrl.getText().toString());
    }

    @Test
    @SmallTest
    public void testStartupPagesList_displaysWebsiteTitlesAndFavicons() {
        // Setup preference in URLS mode with custom title and favicon loaders.
        Drawable espnFavicon = new ColorDrawable(Color.RED);
        setRestoreOnStartupPref(SessionStartupPref.URLS);
        RadioButtonGroupOnStartupPreference pref = launchAndGetRadioGroupPreference();

        // Act.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        pref.initialize(
                                SessionStartupPref.URLS,
                                List.of(URL_ESPN, URL_WIKIPEDIA),
                                startupType -> {},
                                new RadioButtonGroupOnStartupPreference
                                        .StartupPageItemActionListener() {
                                    @Override
                                    public void onEditStartupPage(int index, String url) {}

                                    @Override
                                    public void onRemoveStartupPage(int index, String url) {}
                                },
                                (url, callback) -> {
                                    if (URL_ESPN.equals(url)) {
                                        callback.onResult(espnFavicon);
                                    }
                                },
                                (url, callback) -> {
                                    callback.onResult(
                                            URL_ESPN.equals(url)
                                                    ? "ESPN - Serving Sports Fans"
                                                    : "");
                                }));
        LinearLayout container = pref.getStartupPagesContainerForTesting();

        // Verify resolved website title and favicon on first row and origin fallback on second row.
        assertNotNull(container);
        assertEquals(2, container.getChildCount());
        View firstItem = container.getChildAt(0);
        ImageView firstFavicon = firstItem.findViewById(R.id.startup_page_favicon);
        TextView firstTitle = firstItem.findViewById(R.id.startup_page_title);
        TextView firstUrl = firstItem.findViewById(R.id.startup_page_url);
        assertEquals(espnFavicon, firstFavicon.getDrawable());
        assertEquals("ESPN - Serving Sports Fans", firstTitle.getText().toString());
        assertEquals("https://www.espn.com", firstUrl.getText().toString());

        View secondItem = container.getChildAt(1);
        TextView secondTitle = secondItem.findViewById(R.id.startup_page_title);
        TextView secondUrl = secondItem.findViewById(R.id.startup_page_url);
        assertEquals("https://www.wikipedia.org", secondTitle.getText().toString());
        assertEquals("https://www.wikipedia.org", secondUrl.getText().toString());
    }

    @Test
    @SmallTest
    public void testRemoveStartupPage_removesRowAndUpdatesPrefService() {
        // Setup two configured startup URLs.
        setRestoreOnStartupPref(SessionStartupPref.URLS);
        setRestoreOnStartupUrls(List.of(URL_ESPN, URL_WIKIPEDIA));
        RadioButtonGroupOnStartupPreference pref = launchAndGetRadioGroupPreference();
        LinearLayout container = pref.getStartupPagesContainerForTesting();
        assertNotNull(container);
        assertEquals(2, container.getChildCount());

        // Act: Open the overflow menu on the first row and click "Remove".
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ListMenuButton moreButton =
                            container.getChildAt(0).findViewById(R.id.startup_page_more_button);
                    moreButton.callOnClick();
                });
        onView(withText(R.string.remove)).perform(click());

        // Verify the first URL is removed from both the UI list and PrefService.
        assertEquals(1, container.getChildCount());
        TextView remainingUrl = container.getChildAt(0).findViewById(R.id.startup_page_url);
        assertEquals("https://www.wikipedia.org", remainingUrl.getText().toString());
        List<String> savedUrls =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                TabbedStartupCoordinator.getSessionStartupUrls(
                                        UserPrefs.get(mProfile)));
        assertEquals(List.of(URL_WIKIPEDIA), savedUrls);
    }

    @Test
    @SmallTest
    public void testExternalPrefChanges_updatesRadioSelectionAndStartupPagesList() {
        // Setup initial startup preference as LAST.
        setRestoreOnStartupPref(SessionStartupPref.LAST);
        RadioButtonGroupOnStartupPreference pref = launchAndGetRadioGroupPreference();
        LinearLayout container = pref.getStartupPagesContainerForTesting();
        assertTrue(pref.getLastPrefForTesting().isChecked());
        assertEquals(View.GONE, pref.getStartupPagesSectionForTesting().getVisibility());

        // Act: Update startup type to URLS and populate startup URLs after launch.
        setRestoreOnStartupUrls(List.of(URL_DOCS));
        setRestoreOnStartupPref(SessionStartupPref.URLS);

        // Verify radio selection and startup pages list update dynamically.
        assertTrue(pref.getUrlsPrefForTesting().isChecked());
        assertEquals(View.VISIBLE, pref.getStartupPagesSectionForTesting().getVisibility());
        assertNotNull(container);
        assertEquals(1, container.getChildCount());
        TextView urlView = container.getChildAt(0).findViewById(R.id.startup_page_url);
        assertEquals("https://docs.google.com/document/u/0/", urlView.getText().toString());
    }

    private RadioButtonGroupOnStartupPreference launchAndGetRadioGroupPreference() {
        mSettingsTestRule.startSettingsActivity();
        OnStartupSettingsFragment fragment = mSettingsTestRule.getFragment();
        RadioButtonGroupOnStartupPreference pref =
                fragment.findPreference(OnStartupSettingsFragment.PREF_RADIO_BUTTON_GROUP);
        CriteriaHelper.pollUiThread(
                () -> {
                    LinearLayout container = pref.getStartupPagesContainerForTesting();
                    return fragment.getListView() != null
                            && fragment.getListView().getItemDecorationCount() > 0
                            && container != null
                            && container.isAttachedToWindow();
                });
        return pref;
    }

    private void setRestoreOnStartupPref(@SessionStartupPref int startupType) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> TabbedStartupCoordinator.setSessionStartupType(mProfile, startupType));
    }

    private void setRestoreOnStartupUrls(List<String> urls) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        TabbedStartupCoordinator.setSessionStartupUrls(
                                UserPrefs.get(mProfile), urls));
    }

    private @SessionStartupPref int getRestoreOnStartupPref() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> UserPrefs.get(mProfile).getInteger(Pref.RESTORE_ON_STARTUP));
    }
}
