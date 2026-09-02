// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appearance.settings;

import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.appearance.settings.AppearanceSettingsFragment.PREF_BOOKMARK_BAR;
import static org.chromium.chrome.browser.appearance.settings.AppearanceSettingsFragment.PREF_BOOKMARK_BAR_SWITCH;
import static org.chromium.chrome.browser.appearance.settings.AppearanceSettingsFragment.PREF_TOOLBAR_SHORTCUT;
import static org.chromium.chrome.browser.appearance.settings.AppearanceSettingsFragment.PREF_UI_THEME;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.UI_THEME_SETTING;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.NEW_TAB;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.NONE;

import androidx.preference.Preference;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils.BookmarkBarSettingChangeOrigin;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.night_mode.NightModeMetrics.ThemeSettingsEntry;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.ThemeType;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.chrome.browser.toolbar.adaptive.settings.AdaptiveToolbarSettingsFragment;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.bookmarks.BookmarkBarVisibilityState;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefChangeRegistrar.PrefObserver;
import org.chromium.components.prefs.PrefChangeRegistrarJni;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.HashSet;
import java.util.Set;

/** Tests for {@link AppearanceSettingsFragment}. */
@Batch(Batch.PER_CLASS)
@RunWith(ChromeJUnit4ClassRunner.class)
public class AppearanceSettingsFragmentTest {

    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsTestRule =
            new BlankUiTestActivitySettingsTestRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PrefChangeRegistrar.Natives mPrefChangeRegistrarJni;
    @Mock private PrefService mPrefService;
    @Mock private Profile mProfile;
    @Mock private UserPrefs.Natives mUserPrefsJni;

    private Set<PrefObserver> mBookmarkBarSettingObserverCache;
    private SettableNonNullObservableSupplier<Boolean> mBookmarkBarSettingSupplier;
    private SettableNonNullObservableSupplier<Integer> mBookmarkBarVisibilityStateSupplier;
    private AppearanceSettingsFragment mSettings;

    @Before
    @UiThreadTest
    public void setUp() {
        // Set up mocks.
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);

        // Set up natives.
        PrefChangeRegistrarJni.setInstanceForTesting(mPrefChangeRegistrarJni);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);

        // Cache bookmark bar setting observers.
        mBookmarkBarSettingObserverCache = new HashSet<>();
        BookmarkBarUtils.setSettingObserverCacheForTesting(mBookmarkBarSettingObserverCache);

        // Update bookmark bar setting and notify observers when supplier changes.
        mBookmarkBarSettingSupplier = ObservableSuppliers.createNonNull(false);
        mBookmarkBarSettingSupplier.addSyncObserverAndPostIfNonNull(
                enabled -> {
                    BookmarkBarUtils.setSettingEnabledForTesting(enabled);
                    // Safely call onPreferenceChange only on non-null observers (since tablets
                    // don't call #initBookmarkBarPrefForUserPrefs).
                    mBookmarkBarSettingObserverCache.stream()
                            .filter(observer -> observer != null)
                            .forEach(PrefObserver::onPreferenceChange);
                });

        // Update supplier when bookmark bar setting changes.
        doAnswer(runCallbackWithValueAtIndex(mBookmarkBarSettingSupplier::set, 1))
                .when(mPrefService)
                .setBoolean(eq(Pref.SHOW_BOOKMARK_BAR), anyBoolean());

        // Update bookmark bar visibility state setting and notify observers when supplier changes.
        mBookmarkBarVisibilityStateSupplier =
                ObservableSuppliers.createNonNull(BookmarkBarVisibilityState.ALWAYS_HIDE);
        mBookmarkBarVisibilityStateSupplier.addSyncObserverAndPostIfNonNull(
                state -> {
                    mBookmarkBarSettingObserverCache.stream()
                            .filter(observer -> observer != null)
                            .forEach(PrefObserver::onPreferenceChange);
                });

        // Update supplier when bookmark bar visibility state changes.
        doAnswer(runCallbackWithValueAtIndex(mBookmarkBarVisibilityStateSupplier::set, 1))
                .when(mPrefService)
                .setInteger(eq(Pref.BOOKMARK_BAR_VISIBILITY_STATE), anyInt());

        doAnswer(i -> mBookmarkBarVisibilityStateSupplier.get())
                .when(mPrefService)
                .getInteger(eq(Pref.BOOKMARK_BAR_VISIBILITY_STATE));
    }

    @AfterClass
    public static void tearDown() {
        PrefChangeRegistrarJni.setInstanceForTesting(null);
        UserPrefsJni.setInstanceForTesting(null);
    }

    @Test
    @SmallTest
    public void testBookmarkBarPreferenceIsAbsentWhenDisabled() {
        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(false);
        launchSettings();
        Assert.assertNull(mSettings.findPreference(PREF_BOOKMARK_BAR));
        Assert.assertNull(mSettings.findPreference(PREF_BOOKMARK_BAR_SWITCH));
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testBookmarkBarPreferenceIsPresentWhenEnabled_FlagDisabled() {
        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);
        launchSettings();
        assertSwitchExists(PREF_BOOKMARK_BAR_SWITCH);
        Assert.assertNull(mSettings.findPreference(PREF_BOOKMARK_BAR));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testBookmarkBarPreferenceIsPresentWhenEnabled_FlagEnabled()
            throws ClassNotFoundException {
        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);
        launchSettings();
        assertSettingsExists(PREF_BOOKMARK_BAR, BookmarkBarSettingsFragment.class);
        Assert.assertNull(mSettings.findPreference(PREF_BOOKMARK_BAR_SWITCH));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    @Restriction(DeviceFormFactor.DESKTOP)
    public void testBookmarkBarPreferenceSummary_Desktop() throws ClassNotFoundException {
        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mBookmarkBarVisibilityStateSupplier.set(
                                BookmarkBarVisibilityState.ALWAYS_SHOW));
        launchSettings();

        final var bookmarkBarPref =
                assertSettingsExists(PREF_BOOKMARK_BAR, BookmarkBarSettingsFragment.class);
        final var context = mSettings.getContext();
        Assert.assertEquals(
                context.getString(R.string.bookmark_bar_setting_always_show),
                bookmarkBarPref.getSummary());

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mBookmarkBarVisibilityStateSupplier.set(
                                BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP));
        Assert.assertEquals(
                context.getString(R.string.bookmark_bar_setting_only_show_bookmarks_bar_on_ntp),
                bookmarkBarPref.getSummary());

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mBookmarkBarVisibilityStateSupplier.set(
                                BookmarkBarVisibilityState.ALWAYS_HIDE));
        Assert.assertEquals(
                context.getString(R.string.bookmark_bar_setting_always_hide),
                bookmarkBarPref.getSummary());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    public void testBookmarkBarPreferenceSummary_NonDesktop() throws ClassNotFoundException {
        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BookmarkBarUtils.setDevicePrefBookmarkBarVisibilityState(
                            BookmarkBarVisibilityState.ALWAYS_SHOW,
                            BookmarkBarSettingChangeOrigin.APPEARANCE_SETTINGS);
                });
        launchSettings();

        final var bookmarkBarPref =
                assertSettingsExists(PREF_BOOKMARK_BAR, BookmarkBarSettingsFragment.class);
        final var context = mSettings.getContext();
        Assert.assertEquals(
                context.getString(R.string.bookmark_bar_setting_always_show),
                bookmarkBarPref.getSummary());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BookmarkBarUtils.setDevicePrefBookmarkBarVisibilityState(
                            BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP,
                            BookmarkBarSettingChangeOrigin.APPEARANCE_SETTINGS);
                });
        Assert.assertEquals(
                context.getString(R.string.bookmark_bar_setting_only_show_bookmarks_bar_on_ntp),
                bookmarkBarPref.getSummary());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BookmarkBarUtils.setDevicePrefBookmarkBarVisibilityState(
                            BookmarkBarVisibilityState.ALWAYS_HIDE,
                            BookmarkBarSettingChangeOrigin.APPEARANCE_SETTINGS);
                });
        Assert.assertEquals(
                context.getString(R.string.bookmark_bar_setting_always_hide),
                bookmarkBarPref.getSummary());
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.DESKTOP)
    @DisableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testBookmarkBarPreferenceUpdatesSettingWhenChanged_Desktop() {
        ThreadUtils.runOnUiThreadBlocking(() -> mBookmarkBarSettingSupplier.set(true));
        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);
        launchSettings();

        final var bookmarkBarPref = assertSwitchExists(PREF_BOOKMARK_BAR_SWITCH);
        Assert.assertTrue(bookmarkBarPref.isChecked());

        ThreadUtils.runOnUiThreadBlocking(bookmarkBarPref::performClick);
        Assert.assertFalse(bookmarkBarPref.isChecked());
        Assert.assertFalse(mBookmarkBarSettingSupplier.get());

        ThreadUtils.runOnUiThreadBlocking(bookmarkBarPref::performClick);
        Assert.assertTrue(bookmarkBarPref.isChecked());
        Assert.assertTrue(mBookmarkBarSettingSupplier.get());
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.DESKTOP)
    @DisableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testBookmarkBarPreferenceIsUpdatedWhenSettingChanges_Desktop() {
        ThreadUtils.runOnUiThreadBlocking(() -> mBookmarkBarSettingSupplier.set(true));
        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);
        launchSettings();

        final var bookmarkBarPref = assertSwitchExists(PREF_BOOKMARK_BAR_SWITCH);
        Assert.assertTrue(bookmarkBarPref.isChecked());

        ThreadUtils.runOnUiThreadBlocking(() -> mBookmarkBarSettingSupplier.set(false));
        Assert.assertFalse(bookmarkBarPref.isChecked());

        ThreadUtils.runOnUiThreadBlocking(() -> mBookmarkBarSettingSupplier.set(true));
        Assert.assertTrue(bookmarkBarPref.isChecked());
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    @DisableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testBookmarkBarPreferenceUpdatesSettingWhenChanged_NonDesktop() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BookmarkBarUtils.setDevicePrefShowBookmarksBar(
                            true, /* fromKeyboardShortcut= */ false);
                });
        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);
        launchSettings();

        final var bookmarkBarPref = assertSwitchExists(PREF_BOOKMARK_BAR_SWITCH);
        Assert.assertTrue(bookmarkBarPref.isChecked());

        ThreadUtils.runOnUiThreadBlocking(bookmarkBarPref::performClick);
        Assert.assertFalse(bookmarkBarPref.isChecked());
        Assert.assertFalse(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));
        Assert.assertTrue(BookmarkBarUtils.hasUserSetDevicePrefShowBookmarksBar());

        ThreadUtils.runOnUiThreadBlocking(bookmarkBarPref::performClick);
        Assert.assertTrue(bookmarkBarPref.isChecked());
        Assert.assertTrue(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));
        Assert.assertTrue(BookmarkBarUtils.hasUserSetDevicePrefShowBookmarksBar());
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    @DisableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testBookmarkBarPreferenceIsUpdatedWhenSettingChanges_NonDesktop() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BookmarkBarUtils.setDevicePrefShowBookmarksBar(
                            true, /* fromKeyboardShortcut= */ false);
                });

        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);
        launchSettings();

        final var bookmarkBarPref = assertSwitchExists(PREF_BOOKMARK_BAR_SWITCH);
        Assert.assertTrue(bookmarkBarPref.isChecked());

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        BookmarkBarUtils.setDevicePrefShowBookmarksBar(
                                false, /* fromKeyboardShortcut= */ true));
        Assert.assertFalse(bookmarkBarPref.isChecked());

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        BookmarkBarUtils.setDevicePrefShowBookmarksBar(
                                true, /* fromKeyboardShortcut= */ false));
        Assert.assertTrue(bookmarkBarPref.isChecked());
    }

    @Test
    @SmallTest
    public void testToolbarShortcutPreferenceIsAbsentWhenDisabled() {
        AdaptiveToolbarStatePredictor.setToolbarStateForTesting(NONE);
        launchSettings();
        Assert.assertNull(mSettings.findPreference(PREF_TOOLBAR_SHORTCUT));
    }

    @Test
    @SmallTest
    public void testToolbarShortcutPreferenceIsPresentWhenEnabled() throws ClassNotFoundException {
        AdaptiveToolbarStatePredictor.setToolbarStateForTesting(NEW_TAB);
        launchSettings();
        assertSettingsExists(PREF_TOOLBAR_SHORTCUT, AdaptiveToolbarSettingsFragment.class);
    }

    @Test
    @SmallTest
    public void testUiThemePreference() throws ClassNotFoundException {
        launchSettings();

        final var uiThemePref = assertSettingsExists(PREF_UI_THEME, ThemeSettingsFragment.class);
        Assert.assertEquals(
                ThemeSettingsEntry.SETTINGS,
                uiThemePref.getExtras().getInt(ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY));

        final var context = mSettings.getContext();
        Assert.assertEquals(
                NightModeUtils.getThemeSettingTitle(context, NightModeUtils.getThemeSetting()),
                uiThemePref.getSummary());

        final var prefs = ChromeSharedPreferences.getInstance();
        for (int theme = 0; theme < ThemeType.NUM_ENTRIES; theme++) {
            ThreadUtils.runOnUiThreadBlocking(mSettings::onPause);
            ThreadUtils.runOnUiThreadBlocking(mSettings::onStop);
            prefs.writeInt(UI_THEME_SETTING, theme);
            ThreadUtils.runOnUiThreadBlocking(mSettings::onStart);
            ThreadUtils.runOnUiThreadBlocking(mSettings::onResume);
            Assert.assertEquals(
                    NightModeUtils.getThemeSettingTitle(context, theme), uiThemePref.getSummary());
        }
    }

    @Test
    @SmallTest
    public void testSearchIndex_BookmarkBarNotCompatible() {
        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(false);
        SettingsIndexData indexData = mock(SettingsIndexData.class);
        var context = ContextUtils.getApplicationContext();
        String prefFragment = AppearanceSettingsFragment.class.getName();

        AppearanceSettingsFragment.SEARCH_INDEX_DATA_PROVIDER.updateDynamicPreferences(
                context, indexData, mProfile);

        verify(indexData).removeEntryForKey(prefFragment, PREF_BOOKMARK_BAR);
        verify(indexData).removeEntryForKey(prefFragment, PREF_BOOKMARK_BAR_SWITCH);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testSearchIndex_BookmarkBarCompatible_SubpageEnabled() {
        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);
        SettingsIndexData indexData = mock(SettingsIndexData.class);
        var context = ContextUtils.getApplicationContext();
        String prefFragment = AppearanceSettingsFragment.class.getName();

        AppearanceSettingsFragment.SEARCH_INDEX_DATA_PROVIDER.updateDynamicPreferences(
                context, indexData, mProfile);

        verify(indexData).removeEntryForKey(prefFragment, PREF_BOOKMARK_BAR_SWITCH);
        verify(indexData, never()).removeEntryForKey(prefFragment, PREF_BOOKMARK_BAR);
        verify(indexData)
                .updateEntrySummaryForKey(
                        prefFragment, PREF_BOOKMARK_BAR, R.string.bookmark_bar_setting_always_hide);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testSearchIndex_BookmarkBarCompatible_SubpageDisabled() {
        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);
        SettingsIndexData indexData = mock(SettingsIndexData.class);
        var context = ContextUtils.getApplicationContext();
        String prefFragment = AppearanceSettingsFragment.class.getName();

        AppearanceSettingsFragment.SEARCH_INDEX_DATA_PROVIDER.updateDynamicPreferences(
                context, indexData, mProfile);

        verify(indexData).removeEntryForKey(prefFragment, PREF_BOOKMARK_BAR);
        verify(indexData, never()).removeEntryForKey(prefFragment, PREF_BOOKMARK_BAR_SWITCH);
    }

    private Preference assertSettingsExists(String prefKey, Class settingsFragmentClass)
            throws ClassNotFoundException {
        final Preference pref = mSettings.findPreference(prefKey);
        Assert.assertNotNull(pref);
        Assert.assertNotNull(pref.getFragment());
        Assert.assertEquals(settingsFragmentClass, Class.forName(pref.getFragment()));
        return pref;
    }

    private ChromeSwitchPreference assertSwitchExists(String prefKey) {
        final Preference pref = mSettings.findPreference(prefKey);
        Assert.assertNotNull(pref);
        Assert.assertTrue(pref instanceof ChromeSwitchPreference);
        return (ChromeSwitchPreference) pref;
    }

    private void launchSettings() {
        mSettingsTestRule.launchPreference(
                AppearanceSettingsFragment.class,
                /* fragmentArgs= */ null,
                (fragment) -> ((AppearanceSettingsFragment) fragment).setProfile(mProfile));
        mSettings = (AppearanceSettingsFragment) mSettingsTestRule.getPreferenceFragment();
        mBookmarkBarSettingObserverCache.add(mSettings.getPrefObserverForTesting());
    }

    private <T> Answer<Void> runCallbackWithValueAtIndex(Callback<T> callback, int index) {
        return invocation -> {
            final T value = invocation.getArgument(index);
            callback.onResult(value);
            return null;
        };
    }
}
