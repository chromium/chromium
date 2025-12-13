// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appearance.settings;

import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.appearance.settings.AppearanceSettingsFragment.PREF_BOOKMARK_BAR;
import static org.chromium.chrome.browser.appearance.settings.AppearanceSettingsFragment.PREF_TOOLBAR_SHORTCUT;
import static org.chromium.chrome.browser.appearance.settings.AppearanceSettingsFragment.PREF_UI_THEME;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.UI_THEME_SETTING;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.NEW_TAB;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.NONE;

import androidx.annotation.NonNull;
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
import org.chromium.base.FeatureOverrides;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.night_mode.NightModeMetrics.ThemeSettingsEntry;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.ThemeType;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.chrome.browser.toolbar.adaptive.settings.AdaptiveToolbarSettingsFragment;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefChangeRegistrar.PrefObserver;
import org.chromium.components.prefs.PrefChangeRegistrarJni;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.HashSet;
import java.util.Set;

/** Tests for {@link AppearanceSettingsFragment}. */
@Batch(Batch.PER_CLASS)
@RunWith(BaseJUnit4ClassRunner.class)
public class AppearanceSettingsFragmentTest {

    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsTestRule =
            new BlankUiTestActivitySettingsTestRule();

    @Rule
    public OverrideContextWrapperTestRule mOverrideContextRule =
            new OverrideContextWrapperTestRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PrefChangeRegistrar.Natives mPrefChangeRegistrarJni;
    @Mock private PrefService mPrefService;
    @Mock private Profile mProfile;
    @Mock private UserPrefs.Natives mUserPrefsJni;
    @Mock private Tracker mTracker;

    private Set<PrefObserver> mBookmarkBarSettingObserverCache;
    private ObservableSupplierImpl<Boolean> mBookmarkBarSettingSupplier;
    private AppearanceSettingsFragment mSettings;

    @Before
    @UiThreadTest
    public void setUp() {
        // Set up mocks.
        TrackerFactory.setTrackerForTests(mTracker);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);

        // Set up natives.
        PrefChangeRegistrarJni.setInstanceForTesting(mPrefChangeRegistrarJni);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);

        // Cache bookmark bar setting observers.
        mBookmarkBarSettingObserverCache = new HashSet<>();
        BookmarkBarUtils.setSettingObserverCacheForTesting(mBookmarkBarSettingObserverCache);

        // Update bookmark bar setting and notify observers when supplier changes.
        mBookmarkBarSettingSupplier = new ObservableSupplierImpl<>();
        mBookmarkBarSettingSupplier.addObserver(
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

        // Explicitly override FeatureParam for consistency.
        FeatureOverrides.Builder overrides = FeatureOverrides.newBuilder();
        overrides =
                overrides.param(ChromeFeatureList.ANDROID_BOOKMARK_BAR, "show_bookmark_bar", true);
        overrides.apply();
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
    }

    @Test
    @SmallTest
    public void testBookmarkBarPreferenceIsPresentWhenEnabled() {
        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);
        launchSettings();
        assertSwitchExists(PREF_BOOKMARK_BAR);
    }

    @Test
    @SmallTest
    public void testBookmarkBarPreferenceUpdatesSettingWhenChanged_Desktop() {
        mOverrideContextRule.setIsDesktop(true);

        ThreadUtils.runOnUiThreadBlocking(() -> mBookmarkBarSettingSupplier.set(true));
        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);
        launchSettings();

        final var bookmarkBarPref = assertSwitchExists(PREF_BOOKMARK_BAR);
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
    public void testBookmarkBarPreferenceIsUpdatedWhenSettingChanges_Desktop() {
        mOverrideContextRule.setIsDesktop(true);

        ThreadUtils.runOnUiThreadBlocking(() -> mBookmarkBarSettingSupplier.set(true));
        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);
        launchSettings();

        final var bookmarkBarPref = assertSwitchExists(PREF_BOOKMARK_BAR);
        Assert.assertTrue(bookmarkBarPref.isChecked());

        ThreadUtils.runOnUiThreadBlocking(() -> mBookmarkBarSettingSupplier.set(false));
        Assert.assertFalse(bookmarkBarPref.isChecked());

        ThreadUtils.runOnUiThreadBlocking(() -> mBookmarkBarSettingSupplier.set(true));
        Assert.assertTrue(bookmarkBarPref.isChecked());
    }

    @Test
    @SmallTest
    public void testBookmarkBarPreferenceUpdatesSettingWhenChanged_Tablet() {
        mOverrideContextRule.setIsDesktop(false);

        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);
        launchSettings();

        final var bookmarkBarPref = assertSwitchExists(PREF_BOOKMARK_BAR);
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
    public void testBookmarkBarPreferenceIsUpdatedWhenSettingChanges_Tablet() {
        mOverrideContextRule.setIsDesktop(false);

        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);
        launchSettings();

        final var bookmarkBarPref = assertSwitchExists(PREF_BOOKMARK_BAR);
        Assert.assertTrue(bookmarkBarPref.isChecked());

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        BookmarkBarUtils.setDevicePrefShowBookmarksBar(
                                mProfile, false, /* fromKeyboardShortcut= */ true));
        Assert.assertFalse(bookmarkBarPref.isChecked());

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        BookmarkBarUtils.setDevicePrefShowBookmarksBar(
                                mProfile, true, /* fromKeyboardShortcut= */ false));
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

    private @NonNull Preference assertSettingsExists(
            @NonNull String prefKey, @NonNull Class settingsFragmentClass)
            throws ClassNotFoundException {
        final Preference pref = mSettings.findPreference(prefKey);
        Assert.assertNotNull(pref);
        Assert.assertNotNull(pref.getFragment());
        Assert.assertEquals(settingsFragmentClass, Class.forName(pref.getFragment()));
        return pref;
    }

    private @NonNull ChromeSwitchPreference assertSwitchExists(@NonNull String prefKey) {
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

    private @NonNull <T> Answer<Void> runCallbackWithValueAtIndex(
            @NonNull Callback<T> callback, int index) {
        return invocation -> {
            final T value = invocation.getArgument(index);
            callback.onResult(value);
            return null;
        };
    }
}
