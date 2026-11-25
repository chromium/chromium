// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doCallRealMethod;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
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
public class CustomTabIncognitoManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private CustomTabActivityNavigationController mNavigationController;
    @Mock private BrowserServicesIntentDataProvider mIntentDataProvider;
    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private Profile mOtrProfile;
    @Mock private ProfileManager.Natives mProfileManagerJni;

    private Activity mActivity;
    private OneshotSupplierImpl<ProfileProvider> mProfileProviderSupplier;
    private CustomTabIncognitoManager mIncognitoManager;

    @Before
    public void setUp() {
        ProfileManagerJni.setInstanceForTesting(mProfileManagerJni);
        IncognitoTabHostRegistry.getInstance().clearForTesting();

        mActivity = Robolectric.buildActivity(Activity.class).get();

        mProfileProviderSupplier = new OneshotSupplierImpl<>();
        mProfileProviderSupplier.set(mProfileProvider);
        doReturn(mOtrProfile).when(mProfileProvider).getOffTheRecordProfile();
        doReturn(mOtrProfile).when(mProfileProvider).getOffTheRecordProfile(any(Boolean.class));
        doReturn(true).when(mOtrProfile).isOffTheRecord();
        // Derive usesPrimaryOtrProfile() from the stubbed profile type, UI type and opener.
        doCallRealMethod().when(mIntentDataProvider).usesPrimaryOtrProfile();

        mIncognitoManager = createIncognitoManager();
    }

    @After
    public void tearDown() {
        IncognitoTabHostRegistry.getInstance().clearForTesting();
    }

    private CustomTabIncognitoManager createIncognitoManager() {
        return new CustomTabIncognitoManager(
                mActivity,
                mNavigationController,
                mIntentDataProvider,
                mProfileProviderSupplier,
                mLifecycleDispatcher);
    }

    /** Stubs the intent data provider as an incognito popup opened by Chrome (shared OTR). */
    private void stubIncognitoPopupOpenedByChrome() {
        doReturn(CustomTabProfileType.INCOGNITO).when(mIntentDataProvider).getCustomTabMode();
        doReturn(CustomTabsUiType.POPUP).when(mIntentDataProvider).getUiType();
        doReturn(true).when(mIntentDataProvider).isOpenedByChrome();
        doReturn(true).when(mOtrProfile).isPrimaryOtrProfile();
    }

    @Test
    public void testOnDestroy_uniqueOtrProfile_destroysProfile() {
        doReturn(false).when(mOtrProfile).isPrimaryOtrProfile();

        mActivity.finish();
        mIncognitoManager.onDestroy();

        verify(mProfileManagerJni).destroyWhenAppropriate(mOtrProfile);
    }

    @Test
    public void testOnDestroy_primaryOtrProfile_withOtherIncognitoTabs_doesNotDestroyProfile() {
        doReturn(true).when(mOtrProfile).isPrimaryOtrProfile();

        IncognitoTabHost otherHost = mock(IncognitoTabHost.class);
        doReturn(true).when(otherHost).hasIncognitoTabs(mOtrProfile);
        IncognitoTabHostRegistry.getInstance().register(otherHost);

        mActivity.finish();
        mIncognitoManager.onDestroy();

        verify(mProfileManagerJni, never()).destroyWhenAppropriate(any());
    }

    @Test
    public void testOnDestroy_primaryOtrProfile_withOtherActiveIncognitoModel_doesNotDestroy() {
        doReturn(true).when(mOtrProfile).isPrimaryOtrProfile();

        IncognitoTabHost otherHost = mock(IncognitoTabHost.class);
        doReturn(false).when(otherHost).hasIncognitoTabs(mOtrProfile);
        doReturn(true).when(otherHost).isActiveModel(mOtrProfile);
        IncognitoTabHostRegistry.getInstance().register(otherHost);

        mActivity.finish();
        mIncognitoManager.onDestroy();

        verify(mProfileManagerJni, never()).destroyWhenAppropriate(any());
    }

    @Test
    public void testOnDestroy_primaryOtrProfile_noOtherIncognitoTabs_destroysProfile() {
        doReturn(true).when(mOtrProfile).isPrimaryOtrProfile();

        mActivity.finish();
        mIncognitoManager.onDestroy();

        verify(mProfileManagerJni).destroyWhenAppropriate(mOtrProfile);
    }

    @Test
    public void testConstructor_incognitoPopupOpenedByChrome_registersHostBeforeNativeInit() {
        stubIncognitoPopupOpenedByChrome();

        // The manager created in setUp() was built before the stubs; create the real subject now.
        CustomTabIncognitoManager manager = createIncognitoManager();

        // Registered without onFinishNativeInitialization() having run.
        assertEquals(1, IncognitoTabHostRegistry.getInstance().getHosts().size());
        IncognitoTabHost host = IncognitoTabHostRegistry.getInstance().getHosts().get(0);
        assertTrue(host.hasIncognitoTabs(/* otrProfile= */ null));
        assertTrue(host.hasIncognitoTabs(mOtrProfile));
        assertTrue(host.isActiveModel(mOtrProfile));

        mActivity.finish();
        manager.onDestroy();

        assertEquals(0, IncognitoTabHostRegistry.getInstance().getHosts().size());
    }

    @Test
    public void testConstructor_incognitoNonPopup_doesNotRegisterHostBeforeNativeInit() {
        doReturn(CustomTabProfileType.INCOGNITO).when(mIntentDataProvider).getCustomTabMode();
        doReturn(CustomTabsUiType.DEFAULT).when(mIntentDataProvider).getUiType();
        doReturn(true).when(mIntentDataProvider).isOpenedByChrome();

        createIncognitoManager();

        assertEquals(0, IncognitoTabHostRegistry.getInstance().getHosts().size());
    }

    @Test
    public void testConstructor_popupNotOpenedByChrome_doesNotRegisterHostBeforeNativeInit() {
        doReturn(CustomTabProfileType.INCOGNITO).when(mIntentDataProvider).getCustomTabMode();
        doReturn(CustomTabsUiType.POPUP).when(mIntentDataProvider).getUiType();
        doReturn(false).when(mIntentDataProvider).isOpenedByChrome();

        createIncognitoManager();

        assertEquals(0, IncognitoTabHostRegistry.getInstance().getHosts().size());
    }

    @Test
    public void testHost_whileFinishing_reportsNoTabsButActiveModelUntilDestroyed() {
        stubIncognitoPopupOpenedByChrome();
        CustomTabIncognitoManager manager = createIncognitoManager();
        IncognitoTabHost host = IncognitoTabHostRegistry.getInstance().getHosts().get(0);

        // A finishing popup has no tabs but keeps the incognito model active until destroyed.
        mActivity.finish();
        assertTrue(mActivity.isFinishing());
        assertFalse(host.hasIncognitoTabs(/* otrProfile= */ null));
        assertFalse(host.hasIncognitoTabs(mOtrProfile));
        assertTrue(host.isActiveModel(mOtrProfile));

        manager.onDestroy();
        assertEquals(0, IncognitoTabHostRegistry.getInstance().getHosts().size());
        // The popup was the last holder of the primary OTR profile.
        verify(mProfileManagerJni).destroyWhenAppropriate(mOtrProfile);
    }

    @Test
    public void testOnDestroy_notFinishing_unregistersHostWithoutDestroyingProfile() {
        stubIncognitoPopupOpenedByChrome();
        CustomTabIncognitoManager manager = createIncognitoManager();
        assertEquals(1, IncognitoTabHostRegistry.getInstance().getHosts().size());

        // Recreation, e.g. on configuration change.
        manager.onDestroy();

        assertEquals(0, IncognitoTabHostRegistry.getInstance().getHosts().size());
        verify(mProfileManagerJni, never()).destroyWhenAppropriate(any());
    }

    @Test
    public void testOnDestroy_activityNotFinishing_doesNotDestroyProfile() {
        doReturn(false).when(mOtrProfile).isPrimaryOtrProfile();

        mIncognitoManager.onDestroy();

        verify(mProfileManagerJni, never()).destroyWhenAppropriate(any());
    }

    @Test
    public void testOnDestroy_shutdownStarted_doesNotDestroyProfile() {
        doReturn(false).when(mOtrProfile).isPrimaryOtrProfile();
        doReturn(true).when(mOtrProfile).shutdownStarted();

        mActivity.finish();
        mIncognitoManager.onDestroy();

        verify(mProfileManagerJni, never()).destroyWhenAppropriate(any());
    }

    @Test
    public void testMultiProfileIsolation() {
        stubIncognitoPopupOpenedByChrome();
        Profile otherOtrProfile = mock(Profile.class);
        doReturn(true).when(otherOtrProfile).isOffTheRecord();
        doReturn(true).when(otherOtrProfile).isPrimaryOtrProfile();
        Profile nonPrimaryOtr = mock(Profile.class);
        doReturn(true).when(nonPrimaryOtr).isOffTheRecord();

        CustomTabIncognitoManager manager = createIncognitoManager();
        IncognitoTabHost host = IncognitoTabHostRegistry.getInstance().getHosts().get(0);
        assertTrue(host.hasIncognitoTabs(mOtrProfile));
        assertTrue(host.isActiveModel(mOtrProfile));
        assertFalse(host.hasIncognitoTabs(otherOtrProfile));
        assertFalse(host.isActiveModel(otherOtrProfile));

        // Fail closed for primary OTR queries while ProfileProvider is still pending.
        OneshotSupplierImpl<ProfileProvider> delayedSupplier = new OneshotSupplierImpl<>();
        CustomTabIncognitoManager delayedManager =
                new CustomTabIncognitoManager(
                        mActivity,
                        mNavigationController,
                        mIntentDataProvider,
                        delayedSupplier,
                        mLifecycleDispatcher);
        IncognitoTabHost delayedHost = IncognitoTabHostRegistry.getInstance().getHosts().get(1);
        assertTrue(delayedHost.hasIncognitoTabs(mOtrProfile));
        assertTrue(delayedHost.isActiveModel(mOtrProfile));
        assertFalse(delayedHost.hasIncognitoTabs(nonPrimaryOtr));
        assertFalse(delayedHost.isActiveModel(nonPrimaryOtr));
        delayedManager.onDestroy();

        // onDestroy destroys mOtrProfile even when another host is active for a different profile.
        IncognitoTabHost otherHost = mock(IncognitoTabHost.class);
        doReturn(true).when(otherHost).hasIncognitoTabs(/* otrProfile= */ null);
        doReturn(true).when(otherHost).hasIncognitoTabs(otherOtrProfile);
        doReturn(true).when(otherHost).isActiveModel(otherOtrProfile);
        IncognitoTabHostRegistry.getInstance().register(otherHost);

        mActivity.finish();
        manager.onDestroy();
        verify(mProfileManagerJni).destroyWhenAppropriate(mOtrProfile);
    }
}
