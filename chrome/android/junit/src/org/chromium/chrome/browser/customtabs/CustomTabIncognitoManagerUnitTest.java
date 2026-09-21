// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.Window;
import android.view.WindowManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.flags.CustomTabProfileType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileManagerJni;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHost;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostRegistry;

/** Unit tests for {@link CustomTabIncognitoManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CustomTabIncognitoManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Activity mActivity;
    @Mock private Window mWindow;
    @Mock private CustomTabActivityNavigationController mNavigationController;
    @Mock private BrowserServicesIntentDataProvider mIntentDataProvider;
    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private Profile mOtrProfile;
    @Mock private ProfileManager.Natives mProfileManagerJni;

    private OneshotSupplierImpl<ProfileProvider> mProfileProviderSupplier;
    private CustomTabIncognitoManager mIncognitoManager;

    @Before
    public void setUp() {
        ProfileManagerJni.setInstanceForTesting(mProfileManagerJni);
        IncognitoTabHostRegistry.getInstance().getHosts().clear();

        WindowManager.LayoutParams layoutParams = new WindowManager.LayoutParams();
        doReturn(mWindow).when(mActivity).getWindow();
        doReturn(layoutParams).when(mWindow).getAttributes();
        doReturn(true).when(mActivity).isFinishing();

        mProfileProviderSupplier = new OneshotSupplierImpl<>();
        mProfileProviderSupplier.set(mProfileProvider);
        doReturn(mOtrProfile).when(mProfileProvider).getOffTheRecordProfile();
        doReturn(mOtrProfile).when(mProfileProvider).getOffTheRecordProfile(any(Boolean.class));

        mIncognitoManager =
                new CustomTabIncognitoManager(
                        mActivity,
                        mNavigationController,
                        mIntentDataProvider,
                        mProfileProviderSupplier,
                        mLifecycleDispatcher);
    }

    @After
    public void tearDown() {
        IncognitoTabHostRegistry.getInstance().getHosts().clear();
    }

    @Test
    public void testOnDestroy_uniqueOtrProfile_destroysProfile() {
        doReturn(false).when(mOtrProfile).isPrimaryOtrProfile();

        mIncognitoManager.onDestroy();

        verify(mProfileManagerJni).destroyWhenAppropriate(mOtrProfile);
    }

    @Test
    public void testOnDestroy_primaryOtrProfile_withOtherIncognitoTabs_doesNotDestroyProfile() {
        doReturn(true).when(mOtrProfile).isPrimaryOtrProfile();

        IncognitoTabHost otherHost = mock(IncognitoTabHost.class);
        doReturn(true).when(otherHost).hasIncognitoTabs();
        IncognitoTabHostRegistry.getInstance().register(otherHost);

        mIncognitoManager.onDestroy();

        verify(mProfileManagerJni, never()).destroyWhenAppropriate(any());
    }

    @Test
    public void testOnDestroy_primaryOtrProfile_noOtherIncognitoTabs_destroysProfile() {
        doReturn(true).when(mOtrProfile).isPrimaryOtrProfile();

        mIncognitoManager.onDestroy();

        verify(mProfileManagerJni).destroyWhenAppropriate(mOtrProfile);
    }

    @Test
    public void testOnFinishNativeInitialization_openedByChrome_registersHost() {
        doReturn(CustomTabProfileType.INCOGNITO).when(mIntentDataProvider).getCustomTabMode();
        doReturn(true).when(mIntentDataProvider).isOpenedByChrome();
        doReturn(true).when(mOtrProfile).isPrimaryOtrProfile();

        mIncognitoManager.onFinishNativeInitialization();

        assertEquals(1, IncognitoTabHostRegistry.getInstance().getHosts().size());

        mIncognitoManager.onDestroy();

        assertEquals(0, IncognitoTabHostRegistry.getInstance().getHosts().size());
    }

    @Test
    public void testOnFinishNativeInitialization_isolatedOtrProfile_doesNotRegisterHost() {
        doReturn(CustomTabProfileType.INCOGNITO).when(mIntentDataProvider).getCustomTabMode();
        doReturn(true).when(mIntentDataProvider).isOpenedByChrome();
        doReturn(false).when(mOtrProfile).isPrimaryOtrProfile();

        mIncognitoManager.onFinishNativeInitialization();

        assertEquals(0, IncognitoTabHostRegistry.getInstance().getHosts().size());
    }

    @Test
    public void testOnDestroy_activityNotFinishing_doesNotDestroyProfile() {
        doReturn(false).when(mActivity).isFinishing();
        doReturn(false).when(mOtrProfile).isPrimaryOtrProfile();

        mIncognitoManager.onDestroy();

        verify(mProfileManagerJni, never()).destroyWhenAppropriate(any());
    }
}
