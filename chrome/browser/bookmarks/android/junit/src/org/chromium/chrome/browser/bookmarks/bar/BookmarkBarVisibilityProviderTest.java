// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

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
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;
import org.robolectric.Robolectric;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarVisibilityProvider.BookmarkBarVisibilityObserver;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
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
    private final ObservableSupplierImpl<Profile> mProfileSupplier = new ObservableSupplierImpl<>();
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

    @Test
    @SmallTest
    public void testConstructAndDestroy() {
        // Case: Construct w/ feature disallowed and setting disabled.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(false);
        BookmarkBarUtils.setSettingEnabledForTesting(false);
        BookmarkBarVisibilityProvider provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();
        verify(mObserver, times(1)).onVisibilityChanged(false);
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
        verify(mObserver, times(1)).onVisibilityChanged(false);
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
        verify(mObserver, times(1)).onVisibilityChanged(false);
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
        verify(mObserver, times(1)).onVisibilityChanged(true);
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

        // Case: Profile changed to `null`
        BookmarkBarUtils.setSettingEnabledForTesting(false);
        mProfileSupplier.set(null);
        verify(mObserver, times(1)).onVisibilityChanged(false);
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());
        clearInvocations(mObserver);

        // Case: Profile changed from `null`
        BookmarkBarUtils.setSettingEnabledForTesting(true);
        mProfileSupplier.set(mProfile);
        verify(mObserver, times(1)).onVisibilityChanged(true);
        verify(mObserver, never()).onItemWidthConstraintsChanged(anyInt(), anyInt());

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
                        mActivity, mActivityLifecycleDispatcher, mProfileSupplier);
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
