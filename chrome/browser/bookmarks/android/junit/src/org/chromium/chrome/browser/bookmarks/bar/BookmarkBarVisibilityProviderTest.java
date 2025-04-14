// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Configuration;

import androidx.annotation.NonNull;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefChangeRegistrar.PrefObserver;
import org.chromium.components.prefs.PrefChangeRegistrarJni;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.HashSet;
import java.util.Set;

/** Unit tests for {@link BookmarkBarVisibilityProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkBarVisibilityProviderTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Activity mActivity;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private Callback<Boolean> mCallback;
    @Mock private Configuration mConfig;
    @Mock private PrefChangeRegistrarJni mPrefChangeRegistrarJni;
    @Mock private PrefService mPrefService;
    @Mock private Profile mProfile;
    @Mock private UserPrefsJni mUserPrefsJni;

    private final Set<ConfigurationChangedObserver> mConfigChangeObserverCache = new HashSet<>();
    private final ObservableSupplierImpl<Profile> mProfileSupplier = new ObservableSupplierImpl<>();
    private final Set<PrefObserver> mSettingObserverCache = new HashSet<>();

    @Before
    public void setUp() {
        mProfileSupplier.set(mProfile);

        // Set up mocks.
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);

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

    @After
    public void tearDown() {
        PrefChangeRegistrarJni.setInstanceForTesting(null);
        UserPrefsJni.setInstanceForTesting(null);
    }

    @Test
    @SmallTest
    public void testConstructAndDestroy() {
        // Case: Construct w/ feature disallowed and setting disabled.
        BookmarkBarUtils.setFeatureAllowedForTesting(false);
        BookmarkBarUtils.setSettingEnabledForTesting(false);
        BookmarkBarVisibilityProvider provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();
        verify(mCallback).onResult(false);
        verifyNoMoreInteractions(mCallback);
        clearInvocations(mCallback);

        // Clean up.
        provider.destroy();
        verifyNoMoreInteractions(mCallback);

        // Case: Construct w/ feature disallowed and setting enabled.
        BookmarkBarUtils.setSettingEnabledForTesting(true);
        provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();
        verify(mCallback).onResult(false);
        verifyNoMoreInteractions(mCallback);
        clearInvocations(mCallback);

        // Clean up.
        provider.destroy();
        verifyNoMoreInteractions(mCallback);

        // Case: Construct w/ feature allowed and setting disabled.
        BookmarkBarUtils.setFeatureAllowedForTesting(true);
        BookmarkBarUtils.setSettingEnabledForTesting(false);
        provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();
        verify(mCallback).onResult(false);
        verifyNoMoreInteractions(mCallback);
        clearInvocations(mCallback);

        // Clean up.
        provider.destroy();
        verifyNoMoreInteractions(mCallback);

        // Case: Construct w/ feature allowed and setting enabled.
        BookmarkBarUtils.setSettingEnabledForTesting(true);
        provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();
        verify(mCallback).onResult(true);
        verifyNoMoreInteractions(mCallback);
        clearInvocations(mCallback);

        // Clean up.
        provider.destroy();
        verifyNoMoreInteractions(mCallback);
    }

    @Test
    @SmallTest
    public void testConfigurationChange() {
        // Set up.
        BookmarkBarUtils.setFeatureAllowedForTesting(true);
        BookmarkBarUtils.setSettingEnabledForTesting(true);
        BookmarkBarVisibilityProvider provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();
        verify(mCallback).onResult(true);
        verifyNoMoreInteractions(mCallback);
        clearInvocations(mCallback);

        // Case: Configuration changed to disallow feature.
        BookmarkBarUtils.setFeatureAllowedForTesting(false);
        mConfigChangeObserverCache.stream().forEach(obs -> obs.onConfigurationChanged(mConfig));
        verify(mCallback).onResult(false);
        verifyNoMoreInteractions(mCallback);
        clearInvocations(mCallback);

        // Case: Configuration changed to allow feature.
        BookmarkBarUtils.setFeatureAllowedForTesting(true);
        mConfigChangeObserverCache.stream().forEach(obs -> obs.onConfigurationChanged(mConfig));
        verify(mCallback).onResult(true);
        verifyNoMoreInteractions(mCallback);
        clearInvocations(mCallback);

        // Clean up.
        provider.destroy();
        verifyNoMoreInteractions(mCallback);
    }

    @Test
    @SmallTest
    public void testSettingChange() {
        // Set up.
        BookmarkBarUtils.setFeatureAllowedForTesting(true);
        BookmarkBarUtils.setSettingEnabledForTesting(true);
        BookmarkBarVisibilityProvider provider = createProvider();
        Robolectric.flushForegroundThreadScheduler();
        verify(mCallback).onResult(true);
        verifyNoMoreInteractions(mCallback);
        clearInvocations(mCallback);

        // Case: Preference changed to disable setting.
        BookmarkBarUtils.setSettingEnabledForTesting(false);
        mSettingObserverCache.stream().forEach(PrefObserver::onPreferenceChange);
        verify(mCallback).onResult(false);
        verifyNoMoreInteractions(mCallback);
        clearInvocations(mCallback);

        // Case: Preference changed to enable setting.
        BookmarkBarUtils.setSettingEnabledForTesting(true);
        mSettingObserverCache.stream().forEach(PrefObserver::onPreferenceChange);
        verify(mCallback).onResult(true);
        verifyNoMoreInteractions(mCallback);
        clearInvocations(mCallback);

        // Clean up.
        provider.destroy();
        verifyNoMoreInteractions(mCallback);
    }

    private @NonNull <T> Answer<Void> addValueAtIndexToSet(@NonNull Set<T> set, int index) {
        return invocation -> {
            final T value = invocation.getArgument(index);
            set.add(value);
            return null;
        };
    }

    private @NonNull BookmarkBarVisibilityProvider createProvider() {
        return new BookmarkBarVisibilityProvider(
                mActivity, mActivityLifecycleDispatcher, mProfileSupplier, mCallback);
    }

    private @NonNull <T> Answer<Void> removeValueAtIndexFromSet(@NonNull Set<T> set, int index) {
        return invocation -> {
            final T value = invocation.getArgument(index);
            set.remove(value);
            return null;
        };
    }
}
