// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.browser_window.AndroidBrowserWindowObserver;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeatureKey;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.util.List;
import java.util.function.Supplier;

/** Unit tests for {@link ContextualTasksBridgeRegistrar}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ContextualTasksBridgeRegistrarUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ChromeAndroidTask mChromeAndroidTask;
    @Mock private ActivityWindowAndroid mWindowAndroid;
    @Mock private ActivityWindowAndroid mOtherWindowAndroid;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private Profile mRegularProfile;
    @Mock private Profile mIncognitoProfile;
    @Mock private ContextualTasksUtils.Natives mMockJni;
    @Captor private ArgumentCaptor<Supplier<ContextualTasksBridge>> mBridgeSupplierCaptor;
    @Captor private ArgumentCaptor<AndroidBrowserWindowObserver> mObserverCaptor;

    private ContextualTasksBridgeRegistrar mRegistrar;

    @Before
    public void setUp() {
        ContextualTasksUtilsJni.setInstanceForTesting(mMockJni);
        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);
        when(mTabModelSelector.getModel(/* incognito= */ true)).thenReturn(mIncognitoTabModel);

        mRegistrar =
                new ContextualTasksBridgeRegistrar(
                        mChromeAndroidTask, mWindowAndroid, mTabModelSelector);
    }

    @Test
    public void testInit_registersBridgeForRegularProfile() {
        setAllowSignedOutUserInDesktopAndroid(false);
        setValidProfiles(mRegularProfile);

        mRegistrar.init();

        verify(mChromeAndroidTask).addFeature(eq(regularKey()), mBridgeSupplierCaptor.capture());
        assertNotNull(mBridgeSupplierCaptor.getValue().get());
    }

    @Test
    public void testInit_flagOn_registersBridgeForExistingIncognitoWindow() {
        setAllowSignedOutUserInDesktopAndroid(true);
        setValidProfiles(mRegularProfile, mIncognitoProfile);

        mRegistrar.init();

        verify(mChromeAndroidTask).addFeature(eq(regularKey()), any());
        verify(mChromeAndroidTask).addFeature(eq(incognitoKey()), any());
    }

    @Test
    public void testInit_flagOff_doesNotRegisterBridgeForIncognitoWindow() {
        setAllowSignedOutUserInDesktopAndroid(false);
        setValidProfiles(mRegularProfile, mIncognitoProfile);

        mRegistrar.init();

        verify(mChromeAndroidTask).addFeature(eq(regularKey()), any());
        verifyNoBridgeForIncognito();
    }

    @Test
    public void testHandleBrowserWindowAdded_flagOn_registersBridgeForIncognitoWindow() {
        setAllowSignedOutUserInDesktopAndroid(true);
        setValidProfiles(mRegularProfile);
        mRegistrar.init();

        mRegistrar.handleBrowserWindowAdded(mIncognitoProfile, mWindowAndroid);

        verify(mChromeAndroidTask).addFeature(eq(incognitoKey()), any());
    }

    @Test
    public void testHandleBrowserWindowAdded_flagOff_doesNotRegisterBridgeForIncognitoWindow() {
        setAllowSignedOutUserInDesktopAndroid(false);
        setValidProfiles(mRegularProfile);
        mRegistrar.init();

        mRegistrar.handleBrowserWindowAdded(mIncognitoProfile, mWindowAndroid);

        verifyNoBridgeForIncognito();
    }

    @Test
    public void testHandleBrowserWindowAdded_otherActivity_isIgnored() {
        setAllowSignedOutUserInDesktopAndroid(true);
        setValidProfiles(mRegularProfile);
        mRegistrar.init();

        mRegistrar.handleBrowserWindowAdded(mIncognitoProfile, mOtherWindowAndroid);

        verifyNoBridgeForIncognito();
    }

    @Test
    public void testDestroy_removesObserver() {
        setValidProfiles(mRegularProfile);
        mRegistrar.init();
        AndroidBrowserWindowObserver observer = captureObserver();

        mRegistrar.destroy();

        verify(mChromeAndroidTask).removeAndroidBrowserWindowObserver(observer);
    }

    private void setAllowSignedOutUserInDesktopAndroid(boolean enabled) {
        when(mMockJni.isAllowSignedOutUserInDesktopAndroidEnabled()).thenReturn(enabled);
    }

    private void setValidProfiles(Profile... profiles) {
        when(mChromeAndroidTask.getValidProfilesForActivity(mWindowAndroid))
                .thenReturn(List.of(profiles));
    }

    private AndroidBrowserWindowObserver captureObserver() {
        verify(mChromeAndroidTask).addAndroidBrowserWindowObserver(mObserverCaptor.capture());
        return mObserverCaptor.getValue();
    }

    private void verifyNoBridgeForIncognito() {
        verify(mChromeAndroidTask, never())
                .addFeature(argThat(key -> key.mProfile == mIncognitoProfile), any());
    }

    private ChromeAndroidTaskFeatureKey regularKey() {
        return new ChromeAndroidTaskFeatureKey(
                ContextualTasksBridge.class, mRegularProfile, mWindowAndroid);
    }

    private ChromeAndroidTaskFeatureKey incognitoKey() {
        return new ChromeAndroidTaskFeatureKey(
                ContextualTasksBridge.class, mIncognitoProfile, mWindowAndroid, mIncognitoTabModel);
    }
}
