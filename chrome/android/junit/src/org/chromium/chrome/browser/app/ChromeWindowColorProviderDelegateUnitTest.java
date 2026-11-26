// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link ChromeWindowColorProviderDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class ChromeWindowColorProviderDelegateUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ChromeWindowColorProviderDelegate.Natives mNativeMock;
    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private NightModeStateProvider mNightModeProvider;
    @Mock private Profile mProfile;
    @Mock private ProfileProvider mProfileProvider;

    private final OneshotSupplierImpl<ProfileProvider> mProfileProviderSupplier =
            new OneshotSupplierImpl<>();

    @Before
    public void setUp() {
        ChromeWindowColorProviderDelegateJni.setInstanceForTesting(mNativeMock);
    }

    @After
    public void tearDown() {
        ProfileManager.resetForTesting();
    }

    @Test
    public void testAttachesImmediatelyWhenProfileManagerInitialized() {
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        when(mNightModeProvider.isInNightMode()).thenReturn(true);
        ProfileManager.onProfileAdded(mProfile);
        mProfileProviderSupplier.set(mProfileProvider);

        ChromeWindowColorProviderDelegate.attach(
                mLifecycleDispatcher,
                mWindowAndroid,
                mProfileProviderSupplier,
                mNightModeProvider,
                /* shouldApplyBrowserTheme= */ true,
                /* isIncognitoWindow= */ false);

        verify(mNativeMock)
                .attach(
                        mWindowAndroid,
                        mProfile,
                        /* isIncognitoWindow= */ false,
                        /* isNightMode= */ true);
        verify(mLifecycleDispatcher, never()).register(any());
    }

    @Test
    public void testWaitsForProfileAddedThenAttachesSynchronously() {
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        when(mNightModeProvider.isInNightMode()).thenReturn(true);

        ChromeWindowColorProviderDelegate delegate =
                new ChromeWindowColorProviderDelegate(
                        mLifecycleDispatcher,
                        mWindowAndroid,
                        mProfileProviderSupplier,
                        mNightModeProvider,
                        /* shouldApplyBrowserTheme= */ true,
                        /* isIncognitoWindow= */ false);

        verify(mNativeMock, never()).attach(any(), any(), anyBoolean(), anyBoolean());
        verify(mLifecycleDispatcher).register(delegate);

        // Simulate ActivityProfileProvider populating the supplier before our observer runs.
        mProfileProviderSupplier.set(mProfileProvider);
        ProfileManager.onProfileAdded(mProfile);

        verify(mNativeMock)
                .attach(
                        mWindowAndroid,
                        mProfile,
                        /* isIncognitoWindow= */ false,
                        /* isNightMode= */ true);
        verify(mLifecycleDispatcher).unregister(delegate);
    }

    @Test
    public void testWaitsForDeferredProfileProviderSupplier() {
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        ProfileManager.onProfileAdded(mProfile);

        ChromeWindowColorProviderDelegate.attach(
                mLifecycleDispatcher,
                mWindowAndroid,
                mProfileProviderSupplier,
                mNightModeProvider,
                /* shouldApplyBrowserTheme= */ true,
                /* isIncognitoWindow= */ false);

        verify(mNativeMock, never()).attach(any(), any(), anyBoolean(), anyBoolean());
        verify(mLifecycleDispatcher, never()).register(any());

        mProfileProviderSupplier.set(mProfileProvider);
        ShadowLooper.runUiThreadTasks();

        verify(mNativeMock)
                .attach(
                        mWindowAndroid,
                        mProfile,
                        /* isIncognitoWindow= */ false,
                        /* isNightMode= */ false);
    }

    @Test
    public void testDestroyBeforeDeferredProfileProviderSkipsAttach() {
        ProfileManager.onProfileAdded(mProfile);

        ChromeWindowColorProviderDelegate.attach(
                mLifecycleDispatcher,
                mWindowAndroid,
                mProfileProviderSupplier,
                mNightModeProvider,
                /* shouldApplyBrowserTheme= */ true,
                /* isIncognitoWindow= */ false);

        when(mLifecycleDispatcher.isActivityFinishingOrDestroyed()).thenReturn(true);
        mProfileProviderSupplier.set(mProfileProvider);
        ShadowLooper.runUiThreadTasks();

        verify(mNativeMock, never()).attach(any(), any(), anyBoolean(), anyBoolean());
    }

    @Test
    public void testCustomTabAttachesWithoutProfile() {
        ProfileManager.onProfileAdded(mProfile);

        ChromeWindowColorProviderDelegate.attach(
                mLifecycleDispatcher,
                mWindowAndroid,
                mProfileProviderSupplier,
                mNightModeProvider,
                /* shouldApplyBrowserTheme= */ false,
                /* isIncognitoWindow= */ false);

        verify(mNativeMock)
                .attach(
                        mWindowAndroid,
                        /* profile= */ null,
                        /* isIncognitoWindow= */ false,
                        /* isNightMode= */ false);
    }

    @Test
    public void testIncognitoWindowAttachesWithoutProfile() {
        ProfileManager.onProfileAdded(mProfile);

        ChromeWindowColorProviderDelegate.attach(
                mLifecycleDispatcher,
                mWindowAndroid,
                mProfileProviderSupplier,
                mNightModeProvider,
                /* shouldApplyBrowserTheme= */ true,
                /* isIncognitoWindow= */ true);

        verify(mNativeMock)
                .attach(
                        mWindowAndroid,
                        /* profile= */ null,
                        /* isIncognitoWindow= */ true,
                        /* isNightMode= */ false);
    }

    @Test
    public void testDestroyBeforeProfileAddedSkipsAttach() {
        ChromeWindowColorProviderDelegate delegate =
                new ChromeWindowColorProviderDelegate(
                        mLifecycleDispatcher,
                        mWindowAndroid,
                        mProfileProviderSupplier,
                        mNightModeProvider,
                        /* shouldApplyBrowserTheme= */ true,
                        /* isIncognitoWindow= */ false);

        delegate.onDestroy();
        mProfileProviderSupplier.set(mProfileProvider);
        ProfileManager.onProfileAdded(mProfile);

        verify(mNativeMock, never()).attach(any(), any(), anyBoolean(), anyBoolean());
    }
}
