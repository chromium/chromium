// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appearance.settings;

import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.appearance.settings.BookmarkBarSettingsFragment.PREF_BOOKMARK_BAR;

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
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;
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

/** Tests for {@link BookmarkBarSettingsFragment}. */
@Batch(Batch.PER_CLASS)
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.BOOKMARKS_BAR_NTP})
public class BookmarkBarSettingsFragmentTest {

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
    private BookmarkBarSettingsFragment mSettings;

    @Before
    @UiThreadTest
    public void setUp() {
        // Set up mocks.
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);

        // Set up natives.
        PrefChangeRegistrarJni.setInstanceForTesting(mPrefChangeRegistrarJni);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);
        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);

        // Cache bookmark bar setting observers.
        mBookmarkBarSettingObserverCache = new HashSet<>();
        BookmarkBarUtils.setSettingObserverCacheForTesting(mBookmarkBarSettingObserverCache);

        // Update bookmark bar setting and notify observers when supplier changes.
        mBookmarkBarSettingSupplier = ObservableSuppliers.createNonNull(false);
        mBookmarkBarSettingSupplier.addSyncObserverAndPostIfNonNull(
                enabled -> {
                    BookmarkBarUtils.setSettingEnabledForTesting(enabled);
                    mBookmarkBarSettingObserverCache.stream()
                            .filter(observer -> observer != null)
                            .forEach(PrefObserver::onPreferenceChange);
                });

        // Update supplier when bookmark bar setting changes.
        doAnswer(runCallbackWithValueAtIndex(mBookmarkBarSettingSupplier::set, 1))
                .when(mPrefService)
                .setBoolean(eq(Pref.SHOW_BOOKMARK_BAR), anyBoolean());
    }

    @AfterClass
    public static void tearDown() {
        PrefChangeRegistrarJni.setInstanceForTesting(null);
        UserPrefsJni.setInstanceForTesting(null);
        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(null);
    }

    @Test
    @SmallTest
    public void testBookmarkBarPreferenceIsPresent() {
        launchSettings();
        assertRadioButtonGroupExists(PREF_BOOKMARK_BAR);
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.DESKTOP)
    public void testBookmarkBarPreferenceUpdatesSettingWhenChanged_Desktop() {
        ThreadUtils.runOnUiThreadBlocking(() -> mBookmarkBarSettingSupplier.set(true));
        launchSettings();

        final var bookmarkBarPref = assertRadioButtonGroupExists(PREF_BOOKMARK_BAR);
        Assert.assertTrue(bookmarkBarPref.getAlwaysShowButtonForTesting().isChecked());
        Assert.assertFalse(bookmarkBarPref.getAlwaysHideButtonForTesting().isChecked());

        ThreadUtils.runOnUiThreadBlocking(
                bookmarkBarPref.getAlwaysHideButtonForTesting()::performClick);
        Assert.assertTrue(bookmarkBarPref.getAlwaysHideButtonForTesting().isChecked());
        Assert.assertFalse(bookmarkBarPref.getAlwaysShowButtonForTesting().isChecked());
        Assert.assertFalse(mBookmarkBarSettingSupplier.get());

        ThreadUtils.runOnUiThreadBlocking(
                bookmarkBarPref.getAlwaysShowButtonForTesting()::performClick);
        Assert.assertTrue(bookmarkBarPref.getAlwaysShowButtonForTesting().isChecked());
        Assert.assertFalse(bookmarkBarPref.getAlwaysHideButtonForTesting().isChecked());
        Assert.assertTrue(mBookmarkBarSettingSupplier.get());
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.DESKTOP)
    public void testBookmarkBarPreferenceIsUpdatedWhenSettingChanges_Desktop() {
        ThreadUtils.runOnUiThreadBlocking(() -> mBookmarkBarSettingSupplier.set(true));
        launchSettings();

        final var bookmarkBarPref = assertRadioButtonGroupExists(PREF_BOOKMARK_BAR);
        Assert.assertTrue(bookmarkBarPref.getAlwaysShowButtonForTesting().isChecked());

        ThreadUtils.runOnUiThreadBlocking(() -> mBookmarkBarSettingSupplier.set(false));
        Assert.assertTrue(bookmarkBarPref.getAlwaysHideButtonForTesting().isChecked());

        ThreadUtils.runOnUiThreadBlocking(() -> mBookmarkBarSettingSupplier.set(true));
        Assert.assertTrue(bookmarkBarPref.getAlwaysShowButtonForTesting().isChecked());
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    public void testBookmarkBarPreferenceUpdatesSettingWhenChanged_NonDesktop() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BookmarkBarUtils.setDevicePrefShowBookmarksBar(
                            mProfile, true, /* fromKeyboardShortcut= */ false);
                });
        launchSettings();

        final var bookmarkBarPref = assertRadioButtonGroupExists(PREF_BOOKMARK_BAR);
        CriteriaHelper.pollUiThread(() -> bookmarkBarPref.getAlwaysShowButtonForTesting() != null);
        CriteriaHelper.pollUiThread(() -> bookmarkBarPref.getAlwaysHideButtonForTesting() != null);
        Assert.assertTrue(bookmarkBarPref.getAlwaysShowButtonForTesting().isChecked());

        ThreadUtils.runOnUiThreadBlocking(
                bookmarkBarPref.getAlwaysHideButtonForTesting()::performClick);
        Assert.assertTrue(bookmarkBarPref.getAlwaysHideButtonForTesting().isChecked());
        Assert.assertFalse(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));
        Assert.assertTrue(BookmarkBarUtils.hasUserSetDevicePrefShowBookmarksBar());

        ThreadUtils.runOnUiThreadBlocking(
                bookmarkBarPref.getAlwaysShowButtonForTesting()::performClick);
        Assert.assertTrue(bookmarkBarPref.getAlwaysShowButtonForTesting().isChecked());
        Assert.assertTrue(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));
        Assert.assertTrue(BookmarkBarUtils.hasUserSetDevicePrefShowBookmarksBar());
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    public void testBookmarkBarPreferenceIsUpdatedWhenSettingChanges_NonDesktop() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BookmarkBarUtils.setDevicePrefShowBookmarksBar(
                            mProfile, true, /* fromKeyboardShortcut= */ false);
                });
        launchSettings();

        final var bookmarkBarPref = assertRadioButtonGroupExists(PREF_BOOKMARK_BAR);
        CriteriaHelper.pollUiThread(() -> bookmarkBarPref.getAlwaysShowButtonForTesting() != null);
        CriteriaHelper.pollUiThread(() -> bookmarkBarPref.getAlwaysHideButtonForTesting() != null);
        Assert.assertTrue(bookmarkBarPref.getAlwaysShowButtonForTesting().isChecked());

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        BookmarkBarUtils.setDevicePrefShowBookmarksBar(
                                mProfile, false, /* fromKeyboardShortcut= */ true));
        Assert.assertTrue(bookmarkBarPref.getAlwaysHideButtonForTesting().isChecked());

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        BookmarkBarUtils.setDevicePrefShowBookmarksBar(
                                mProfile, true, /* fromKeyboardShortcut= */ false));
        Assert.assertTrue(bookmarkBarPref.getAlwaysShowButtonForTesting().isChecked());
    }

    @Test
    @SmallTest
    public void testSearchIndex_BookmarkBarCompatible() {
        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);
        SettingsIndexData indexData = mock(SettingsIndexData.class);
        var context = mSettingsTestRule.getActivity();
        String prefFragment = BookmarkBarSettingsFragment.class.getName();

        BookmarkBarSettingsFragment.SEARCH_INDEX_DATA_PROVIDER.updateDynamicPreferences(
                context, indexData);

        verify(indexData)
                .updateEntrySummaryForKey(
                        prefFragment, PREF_BOOKMARK_BAR, R.string.bookmark_bar_setting_subtitle);
    }

    @Test
    @SmallTest
    public void testSearchIndex_BookmarkBarNotCompatible() {
        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(false);
        SettingsIndexData indexData = mock(SettingsIndexData.class);
        var context = mSettingsTestRule.getActivity();
        String prefFragment = BookmarkBarSettingsFragment.class.getName();

        BookmarkBarSettingsFragment.SEARCH_INDEX_DATA_PROVIDER.updateDynamicPreferences(
                context, indexData);

        verify(indexData).removeEntryForKey(prefFragment, PREF_BOOKMARK_BAR);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testSearchIndex_FlagDisabled() {
        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);
        SettingsIndexData indexData = mock(SettingsIndexData.class);
        var context = mSettingsTestRule.getActivity();
        String prefFragment = BookmarkBarSettingsFragment.class.getName();

        BookmarkBarSettingsFragment.SEARCH_INDEX_DATA_PROVIDER.updateDynamicPreferences(
                context, indexData);

        verify(indexData).removeEntryForKey(prefFragment, PREF_BOOKMARK_BAR);
    }

    private RadioButtonGroupBookmarkBarPreference assertRadioButtonGroupExists(String prefKey) {
        final Preference pref = mSettings.findPreference(prefKey);
        Assert.assertNotNull(pref);
        Assert.assertTrue(pref instanceof RadioButtonGroupBookmarkBarPreference);
        return (RadioButtonGroupBookmarkBarPreference) pref;
    }

    private void launchSettings() {
        mSettingsTestRule.launchPreference(
                BookmarkBarSettingsFragment.class,
                /* fragmentArgs= */ null,
                (fragment) -> ((BookmarkBarSettingsFragment) fragment).setProfile(mProfile));
        mSettings = (BookmarkBarSettingsFragment) mSettingsTestRule.getPreferenceFragment();
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
