// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
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
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefChangeRegistrar.PrefObserver;
import org.chromium.components.prefs.PrefChangeRegistrarJni;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.HashSet;
import java.util.Set;

/** Unit tests for {@link BookmarkBarSettingProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkBarSettingProviderTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Callback<Boolean> mCallback;
    @Mock private PrefChangeRegistrarJni mPrefChangeRegistrarJni;
    @Mock private PrefService mPrefService;
    @Mock private Profile mProfile;
    @Mock private UserPrefsJni mUserPrefsJni;

    private final ObservableSupplierImpl<Profile> mProfileSupplier = new ObservableSupplierImpl<>();
    private final Set<PrefObserver> mSettingObserverCache = new HashSet<>();
    private final ObservableSupplierImpl<Boolean> mSettingSupplier = new ObservableSupplierImpl<>();

    private @Nullable BookmarkBarSettingProvider mProvider;

    @Before
    public void setUp() {
        mProfileSupplier.set(mProfile);

        // Set up mocks.
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);

        // Set up natives.
        PrefChangeRegistrarJni.setInstanceForTesting(mPrefChangeRegistrarJni);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);

        // Cache setting observers.
        BookmarkBarUtils.setSettingObserverCacheForTesting(mSettingObserverCache);

        // Update setting and notify observers when profile changes.
        mProfileSupplier.addSyncObserver(
                profile -> {
                    final boolean enabled = profile != null ? mSettingSupplier.get() : false;
                    BookmarkBarUtils.setSettingEnabledForTesting(enabled);
                    mSettingObserverCache.stream().forEach(PrefObserver::onPreferenceChange);
                });

        // Update setting and notify observers when supplier changes.
        mSettingSupplier.addObserver(
                enabled -> {
                    BookmarkBarUtils.setSettingEnabledForTesting(enabled);
                    mSettingObserverCache.stream().forEach(PrefObserver::onPreferenceChange);
                });

        // Update supplier when setting changes.
        doAnswer(runCallbackWithValueAtIndex(mSettingSupplier::set, 1))
                .when(mPrefService)
                .setBoolean(eq(Pref.SHOW_BOOKMARK_BAR), anyBoolean());
    }

    @After
    public void tearDown() {
        PrefChangeRegistrarJni.setInstanceForTesting(null);
        UserPrefsJni.setInstanceForTesting(null);
    }

    @Test
    @SmallTest
    public void testConstructAndDestroy() {
        // Case: Construct w/ setting disabled.
        mSettingSupplier.set(false);
        mProvider = new BookmarkBarSettingProvider(mProfileSupplier, mCallback);
        Robolectric.flushForegroundThreadScheduler();
        verify(mCallback).onResult(false);
        verifyNoMoreInteractions(mCallback);
        clearInvocations(mCallback);

        // Case: Destroy w/ setting disabled.
        mProvider.destroy();
        verifyNoMoreInteractions(mCallback);

        // Case: Construct w/ setting enabled.
        mSettingSupplier.set(true);
        mProvider = new BookmarkBarSettingProvider(mProfileSupplier, mCallback);
        Robolectric.flushForegroundThreadScheduler();
        verify(mCallback).onResult(true);
        verifyNoMoreInteractions(mCallback);
        clearInvocations(mCallback);

        // Case: Destroy w/ setting enabled.
        mProvider.destroy();
        verifyNoMoreInteractions(mCallback);
    }

    @Test
    @SmallTest
    public void testProfileChange() {
        // Set up.
        mSettingSupplier.set(true);
        mProvider = new BookmarkBarSettingProvider(mProfileSupplier, mCallback);
        Robolectric.flushForegroundThreadScheduler();
        verify(mCallback).onResult(true);
        verifyNoMoreInteractions(mCallback);
        clearInvocations(mCallback);

        // Case: Profile changed to `null`.
        mProfileSupplier.set(null);
        verify(mCallback).onResult(false);
        verifyNoMoreInteractions(mCallback);
        clearInvocations(mCallback);

        // Case: Profile changed from `null`.
        mProfileSupplier.set(mProfile);
        verify(mCallback).onResult(true);
        verifyNoMoreInteractions(mCallback);
    }

    @Test
    @SmallTest
    public void testSettingChange() {
        // Set up.
        mSettingSupplier.set(false);
        mProvider = new BookmarkBarSettingProvider(mProfileSupplier, mCallback);
        Robolectric.flushForegroundThreadScheduler();
        verify(mCallback).onResult(false);
        verifyNoMoreInteractions(mCallback);
        clearInvocations(mCallback);

        // Case: Setting changed to enabled.
        mSettingSupplier.set(true);
        verify(mCallback).onResult(true);
        verifyNoMoreInteractions(mCallback);
        clearInvocations(mCallback);

        // Case: Setting changed to disabled.
        mSettingSupplier.set(false);
        verify(mCallback).onResult(false);
        verifyNoMoreInteractions(mCallback);
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
