// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Configuration;
import android.content.res.Resources;

import androidx.annotation.NonNull;
import androidx.test.filters.SmallTest;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;
import org.robolectric.Robolectric;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarVisibilityProvider.BookmarkBarVisibilityObserver;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
import org.chromium.components.bookmarks.BookmarkBarVisibilityState;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefChangeRegistrar.PrefObserver;
import org.chromium.components.prefs.PrefChangeRegistrarJni;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.HashSet;
import java.util.Set;

/** Unit tests for {@link BookmarkBarVisibilityProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
public class BookmarkBarVisibilityProviderTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public OverrideContextWrapperTestRule mOverrideContextRule =
            new OverrideContextWrapperTestRule();

    @Mock private Activity mActivity;
    @Mock private Resources mResources;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private Configuration mConfig;
    @Mock private PrefChangeRegistrar.Natives mPrefChangeRegistrarJni;
    @Mock private PrefService mPrefService;
    @Mock private Profile mProfile;
    @Mock private UserPrefs.Natives mUserPrefsJni;
    @Mock private BookmarkBarVisibilityObserver mObserver;

    private final Set<ConfigurationChangedObserver> mConfigChangeObserverCache = new HashSet<>();
    private final SettableMonotonicObservableSupplier<Profile> mProfileSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableNonNullObservableSupplier<Boolean> mXrSpaceModeSupplier =
            ObservableSuppliers.createNonNull(false);
    private final Set<PrefObserver> mSettingObserverCache = new HashSet<>();

    @Before
    public void setUp() {
        mProfileSupplier.set(mProfile);
        mOverrideContextRule.setIsDesktop(true);

        // Set up mocks.
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);
        when(mActivity.getResources()).thenReturn(mResources);
        when(mResources.getDimensionPixelSize(anyInt())).thenReturn(12);

        // Set up natives.
        PrefChangeRegistrarJni.setInstanceForTesting(mPrefChangeRegistrarJni);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);

        // Cache configuration change observers.
        doAnswer(addValueAtIndexToSet(mConfigChangeObserverCache, /* index= */ 0))
                .when(mActivityLifecycleDispatcher)
                .register(any(ConfigurationChangedObserver.class));
        doAnswer(removeValueAtIndexFromSet(mConfigChangeObserverCache, /* index= */ 0))
                .when(mActivityLifecycleDispatcher)
                .unregister(any(ConfigurationChangedObserver.class));

        // Cache setting observers.
        BookmarkBarUtils.setSettingObserverCacheForTesting(mSettingObserverCache);
    }

    @AfterClass
    public static void tearDown() {
        PrefChangeRegistrarJni.setInstanceForTesting(null);
        UserPrefsJni.setInstanceForTesting(null);
    }

    // ---------------------------------------------------------------------------------------------
    // Group 1: V1 (Boolean)
    // ---------------------------------------------------------------------------------------------

    @Test
    @SmallTest
    public void testConstructAndDestroy() {
        // Case: Construct w/ feature disallowed and setting disabled.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(false);
        BookmarkBarUtils.setSettingEnabledForTesting(false);
        BookmarkBarVisibilityProvider provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();
        verify(mObserver, times(2)).onVisibilityChanged(false);
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());
        clearInvocations(mObserver);

        // Clean up.
        provider.destroy();
        verify(mObserver, never()).onVisibilityChanged(anyBoolean());
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());

        // Case: Construct w/ feature disallowed and setting enabled.
        BookmarkBarUtils.setSettingEnabledForTesting(true);
        provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();
        // Called 2 times because mXrSpaceModeSupplier will also call it when initialized.
        verify(mObserver, times(2)).onVisibilityChanged(false);
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());
        clearInvocations(mObserver);

        // Clean up.
        provider.destroy();
        verify(mObserver, never()).onVisibilityChanged(anyBoolean());
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());

        // Case: Construct w/ feature allowed and setting disabled.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
        BookmarkBarUtils.setSettingEnabledForTesting(false);
        provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();
        // Called 2 times because mXrSpaceModeSupplier will also call it when initialized.
        verify(mObserver, times(2)).onVisibilityChanged(false);
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());
        clearInvocations(mObserver);

        // Clean up.
        provider.destroy();
        verify(mObserver, never()).onVisibilityChanged(anyBoolean());
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());

        // Case: Construct w/ feature allowed and setting enabled.
        BookmarkBarUtils.setSettingEnabledForTesting(true);
        provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();
        verify(mObserver, times(2)).onVisibilityChanged(true);
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());
        clearInvocations(mObserver);

        // Clean up.
        provider.destroy();
        verify(mObserver, never()).onVisibilityChanged(anyBoolean());
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());
    }

    @Test
    @SmallTest
    public void testConfigurationChange() {
        // Set up.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
        BookmarkBarUtils.setSettingEnabledForTesting(true);
        BookmarkBarVisibilityProvider provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();

        // Case: Configuration changed to disallow feature.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(false);
        mConfigChangeObserverCache.stream().forEach(obs -> obs.onConfigurationChanged(mConfig));
        verify(mObserver, times(1)).onVisibilityChanged(false);
        verify(mObserver, times(1)).onItemWidthConstraintsChanged(12, 12);
        clearInvocations(mObserver);

        // Case: Configuration changed to allow feature.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
        mConfigChangeObserverCache.stream().forEach(obs -> obs.onConfigurationChanged(mConfig));
        verify(mObserver, times(1)).onVisibilityChanged(true);
        verify(mObserver, times(1)).onItemWidthConstraintsChanged(12, 12);

        // Clean up.
        provider.destroy();
    }

    @Test
    @SmallTest
    public void testPrefChange() {
        // Set up.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
        BookmarkBarUtils.setSettingEnabledForTesting(true);
        BookmarkBarVisibilityProvider provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();

        // Case: Preference changed to disable setting.
        BookmarkBarUtils.setSettingEnabledForTesting(false);
        mSettingObserverCache.stream().forEach(PrefObserver::onPreferenceChange);
        verify(mObserver, times(1)).onVisibilityChanged(false);
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());
        clearInvocations(mObserver);

        // Case: Preference changed to enable setting.
        BookmarkBarUtils.setSettingEnabledForTesting(true);
        mSettingObserverCache.stream().forEach(PrefObserver::onPreferenceChange);
        verify(mObserver, times(1)).onVisibilityChanged(true);
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());

        // Clean up.
        provider.destroy();
    }

    @Test
    @SmallTest
    public void testProfileChange() {
        // Set up.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
        BookmarkBarUtils.setSettingEnabledForTesting(true);
        BookmarkBarVisibilityProvider provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();

        // Case: Profile changed
        clearInvocations(mObserver);
        BookmarkBarUtils.setSettingEnabledForTesting(true);
        mProfileSupplier.set(Mockito.mock(Profile.class));
        verify(mObserver, times(1)).onVisibilityChanged(true);
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());

        // Clean up.
        provider.destroy();
    }

    @Test
    @SmallTest
    public void testXrSpaceModeChange() {
        // Set up.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
        BookmarkBarUtils.setSettingEnabledForTesting(true);
        BookmarkBarVisibilityProvider provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();

        // Verify initial state.
        verify(mObserver, times(2)).onVisibilityChanged(true);
        clearInvocations(mObserver);

        // Case: XR space mode changed to true.
        mXrSpaceModeSupplier.set(true);
        verify(mObserver, times(1)).onVisibilityChanged(false);
        assertFalse(
                BookmarkBarUtils.isBookmarkBarVisible(
                        mActivity, mProfile, /* isXrFullSpaceMode= */ true));
        clearInvocations(mObserver);

        // Case: XR space mode changed to false.
        mXrSpaceModeSupplier.set(false);
        verify(mObserver, times(1)).onVisibilityChanged(true);
        assertTrue(
                BookmarkBarUtils.isBookmarkBarVisible(
                        mActivity, mProfile, /* isXrFullSpaceMode= */ false));
        clearInvocations(mObserver);

        // Clean up.
        provider.destroy();
    }

    // ---------------------------------------------------------------------------------------------
    // Group 2: V2 (Tri-State Integers)
    // ---------------------------------------------------------------------------------------------

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testConstructAndDestroy_TriState() {
        // Case: Construct w/ feature disallowed and state ALWAYS_HIDE.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(false);
        when(mPrefService.getInteger(Pref.BOOKMARK_BAR_VISIBILITY_STATE))
                .thenReturn(BookmarkBarVisibilityState.ALWAYS_HIDE);
        BookmarkBarVisibilityProvider provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();
        verify(mObserver, times(2))
                .onVisibilityChanged_TriState(BookmarkBarVisibilityState.ALWAYS_HIDE);
        verify(mObserver, never()).onVisibilityChanged(anyBoolean());
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());
        clearInvocations(mObserver);

        // Clean up.
        provider.destroy();
        verify(mObserver, never()).onVisibilityChanged_TriState(anyInt());
        verify(mObserver, never()).onVisibilityChanged(anyBoolean());
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());

        // Case: Construct w/ feature disallowed and state ALWAYS_SHOW.
        when(mPrefService.getInteger(Pref.BOOKMARK_BAR_VISIBILITY_STATE))
                .thenReturn(BookmarkBarVisibilityState.ALWAYS_SHOW);
        provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();
        // Called 2 times because mXrSpaceModeSupplier will also call it when initialized.
        verify(mObserver, times(2))
                .onVisibilityChanged_TriState(BookmarkBarVisibilityState.ALWAYS_HIDE);
        verify(mObserver, never()).onVisibilityChanged(anyBoolean());
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());
        clearInvocations(mObserver);

        // Clean up.
        provider.destroy();
        verify(mObserver, never()).onVisibilityChanged_TriState(anyInt());
        verify(mObserver, never()).onVisibilityChanged(anyBoolean());
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());

        // Case: Construct w/ feature allowed and state ALWAYS_HIDE.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
        when(mPrefService.getInteger(Pref.BOOKMARK_BAR_VISIBILITY_STATE))
                .thenReturn(BookmarkBarVisibilityState.ALWAYS_HIDE);
        provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();
        verify(mObserver, times(2))
                .onVisibilityChanged_TriState(BookmarkBarVisibilityState.ALWAYS_HIDE);
        verify(mObserver, never()).onVisibilityChanged(anyBoolean());
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());
        clearInvocations(mObserver);

        // Clean up.
        provider.destroy();
        verify(mObserver, never()).onVisibilityChanged_TriState(anyInt());
        verify(mObserver, never()).onVisibilityChanged(anyBoolean());
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());

        // Case: Construct w/ feature allowed and state ALWAYS_SHOW.
        when(mPrefService.getInteger(Pref.BOOKMARK_BAR_VISIBILITY_STATE))
                .thenReturn(BookmarkBarVisibilityState.ALWAYS_SHOW);
        provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();
        verify(mObserver, times(2))
                .onVisibilityChanged_TriState(BookmarkBarVisibilityState.ALWAYS_SHOW);
        verify(mObserver, never()).onVisibilityChanged(anyBoolean());
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());
        clearInvocations(mObserver);

        // Clean up.
        provider.destroy();
        verify(mObserver, never()).onVisibilityChanged_TriState(anyInt());
        verify(mObserver, never()).onVisibilityChanged(anyBoolean());
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());

        // Case: Construct w/ feature allowed and state ONLY_SHOW_ON_NTP.
        when(mPrefService.getInteger(Pref.BOOKMARK_BAR_VISIBILITY_STATE))
                .thenReturn(BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP);
        provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();
        verify(mObserver, times(2))
                .onVisibilityChanged_TriState(BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP);
        verify(mObserver, never()).onVisibilityChanged(anyBoolean());
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());
        clearInvocations(mObserver);

        // Clean up.
        provider.destroy();
        verify(mObserver, never()).onVisibilityChanged_TriState(anyInt());
        verify(mObserver, never()).onVisibilityChanged(anyBoolean());
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testConfigurationChange_TriState() {
        // Set up.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
        when(mPrefService.getInteger(Pref.BOOKMARK_BAR_VISIBILITY_STATE))
                .thenReturn(BookmarkBarVisibilityState.ALWAYS_SHOW);
        BookmarkBarVisibilityProvider provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();

        // Case: Configuration changed to disallow feature.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(false);
        mConfigChangeObserverCache.stream().forEach(obs -> obs.onConfigurationChanged(mConfig));
        verify(mObserver, times(1))
                .onVisibilityChanged_TriState(BookmarkBarVisibilityState.ALWAYS_HIDE);
        verify(mObserver, times(1)).onItemWidthConstraintsChanged(12, 12);
        clearInvocations(mObserver);

        // Case: Configuration changed to allow feature.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
        mConfigChangeObserverCache.stream().forEach(obs -> obs.onConfigurationChanged(mConfig));
        verify(mObserver, times(1))
                .onVisibilityChanged_TriState(BookmarkBarVisibilityState.ALWAYS_SHOW);
        verify(mObserver, times(1)).onItemWidthConstraintsChanged(12, 12);

        // Clean up.
        provider.destroy();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testPrefChange_TriState_Desktop() {
        // Set up.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
        when(mPrefService.getInteger(Pref.BOOKMARK_BAR_VISIBILITY_STATE))
                .thenReturn(BookmarkBarVisibilityState.ALWAYS_SHOW);
        BookmarkBarVisibilityProvider provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();

        // Case: Preference changed to ALWAYS_HIDE.
        when(mPrefService.getInteger(Pref.BOOKMARK_BAR_VISIBILITY_STATE))
                .thenReturn(BookmarkBarVisibilityState.ALWAYS_HIDE);
        mSettingObserverCache.stream().forEach(PrefObserver::onPreferenceChange);
        verify(mObserver, times(1))
                .onVisibilityChanged_TriState(BookmarkBarVisibilityState.ALWAYS_HIDE);
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());
        clearInvocations(mObserver);

        // Case: Preference changed to ONLY_SHOW_ON_NTP.
        when(mPrefService.getInteger(Pref.BOOKMARK_BAR_VISIBILITY_STATE))
                .thenReturn(BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP);
        mSettingObserverCache.stream().forEach(PrefObserver::onPreferenceChange);
        verify(mObserver, times(1))
                .onVisibilityChanged_TriState(BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP);
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());
        clearInvocations(mObserver);

        // Case: Preference changed to ALWAYS_SHOW.
        when(mPrefService.getInteger(Pref.BOOKMARK_BAR_VISIBILITY_STATE))
                .thenReturn(BookmarkBarVisibilityState.ALWAYS_SHOW);
        mSettingObserverCache.stream().forEach(PrefObserver::onPreferenceChange);
        verify(mObserver, times(1))
                .onVisibilityChanged_TriState(BookmarkBarVisibilityState.ALWAYS_SHOW);
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());

        // Clean up.
        provider.destroy();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testPrefChange_TriState_Tablet() {
        // Set up tablet context.
        mOverrideContextRule.setIsDesktop(false);
        ContextUtils.getAppSharedPreferences().edit().clear().apply();
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);

        BookmarkBarVisibilityProvider provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();

        // Case: Tablet shared preference changed to ALWAYS_SHOW.
        clearInvocations(mObserver);
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putInt(
                        BookmarkBarConstants.BOOKMARK_BAR_BOOKMARK_BAR_VISIBILITY_STATE,
                        BookmarkBarVisibilityState.ALWAYS_SHOW)
                .apply();
        Robolectric.flushForegroundThreadScheduler();
        verify(mObserver, times(1))
                .onVisibilityChanged_TriState(BookmarkBarVisibilityState.ALWAYS_SHOW);
        clearInvocations(mObserver);

        // Case: Tablet shared preference changed to ONLY_SHOW_ON_NTP.
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putInt(
                        BookmarkBarConstants.BOOKMARK_BAR_BOOKMARK_BAR_VISIBILITY_STATE,
                        BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP)
                .apply();
        Robolectric.flushForegroundThreadScheduler();
        verify(mObserver, times(1))
                .onVisibilityChanged_TriState(BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP);
        clearInvocations(mObserver);

        // Case: Legacy boolean pref change does NOT trigger observer when tri-state flag is
        // enabled.
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR, true)
                .apply();
        Robolectric.flushForegroundThreadScheduler();
        verify(mObserver, never()).onVisibilityChanged_TriState(anyInt());

        // Clean up.
        provider.destroy();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testProfileChange_TriState() {
        // Set up.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
        when(mPrefService.getInteger(Pref.BOOKMARK_BAR_VISIBILITY_STATE))
                .thenReturn(BookmarkBarVisibilityState.ALWAYS_SHOW);
        BookmarkBarVisibilityProvider provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();

        // Case: Profile changed.
        clearInvocations(mObserver);
        Profile newProfile = Mockito.mock(Profile.class);
        when(newProfile.getOriginalProfile()).thenReturn(newProfile);
        PrefService newPrefService = Mockito.mock(PrefService.class);
        when(mUserPrefsJni.get(newProfile)).thenReturn(newPrefService);
        when(newPrefService.getInteger(Pref.BOOKMARK_BAR_VISIBILITY_STATE))
                .thenReturn(BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP);

        mProfileSupplier.set(newProfile);
        verify(mObserver, times(1))
                .onVisibilityChanged_TriState(BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP);
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());

        // Clean up.
        provider.destroy();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testXrSpaceModeChange_TriState() {
        // Set up.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
        when(mPrefService.getInteger(Pref.BOOKMARK_BAR_VISIBILITY_STATE))
                .thenReturn(BookmarkBarVisibilityState.ALWAYS_SHOW);
        BookmarkBarVisibilityProvider provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();

        // Verify initial state.
        verify(mObserver, times(2))
                .onVisibilityChanged_TriState(BookmarkBarVisibilityState.ALWAYS_SHOW);
        clearInvocations(mObserver);

        // Case: XR space mode changed to true.
        mXrSpaceModeSupplier.set(true);
        verify(mObserver, times(1))
                .onVisibilityChanged_TriState(BookmarkBarVisibilityState.ALWAYS_HIDE);
        clearInvocations(mObserver);

        // Case: XR space mode changed to false.
        mXrSpaceModeSupplier.set(false);
        verify(mObserver, times(1))
                .onVisibilityChanged_TriState(BookmarkBarVisibilityState.ALWAYS_SHOW);
        clearInvocations(mObserver);

        // Clean up.
        provider.destroy();
    }

    private @NonNull <T> Answer<Void> addValueAtIndexToSet(@NonNull Set<T> set, int index) {
        return invocation -> {
            final T value = invocation.getArgument(index);
            set.add(value);
            return null;
        };
    }

    private @NonNull BookmarkBarVisibilityProvider createProvider() {
        BookmarkBarVisibilityProvider provider =
                new BookmarkBarVisibilityProvider(
                        mActivity,
                        mActivityLifecycleDispatcher,
                        mProfileSupplier,
                        mXrSpaceModeSupplier);
        provider.addObserver(mObserver);
        mSettingObserverCache.add(provider.getPrefObserverForTesting());
        return provider;
    }

    private @NonNull <T> Answer<Void> removeValueAtIndexFromSet(@NonNull Set<T> set, int index) {
        return invocation -> {
            final T value = invocation.getArgument(index);
            set.remove(value);
            return null;
        };
    }
}
