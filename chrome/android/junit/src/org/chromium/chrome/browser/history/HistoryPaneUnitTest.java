// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;

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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.incognito.IncognitoUtilsJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefChangeRegistrarJni;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.TestActivity;

import java.util.function.DoubleConsumer;

/** Unit tests for {@link HistoryPane}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.HISTORY_PANE_ANDROID,
    ChromeFeatureList.ENABLE_ESCAPE_HANDLING_FOR_SECONDARY_ACTIVITIES
})
public class HistoryPaneUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final OneshotSupplierImpl<ProfileProvider> mProfileProviderSupplier =
            new OneshotSupplierImpl<>();

    @Mock private DoubleConsumer mOnToolbarAlphaChange;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private Profile mProfile;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Tab mCurrentTab;
    @Mock private BrowsingHistoryBridge.Natives mBrowsingHistoryBridgeNatives;
    @Mock private PrefService mPrefService;
    @Mock private UserPrefs.Natives mUserPrefsNatives;
    @Mock private LargeIconBridge.Natives mLargeIconBridgeNatives;
    @Mock private SigninManager mSigninManager;
    @Mock private IdentityServicesProvider mIdentityService;
    @Mock private IdentityManager mIdentityManager;
    @Mock private SyncService mSyncService;
    @Mock private PrefChangeRegistrar.Natives mPrefChangeRegistrarNatives;
    @Mock private IncognitoUtils.Natives mIncognitoUtilsNatives;

    private HistoryPane mHistoryPane;

    @Before
    public void setUp() {
        doReturn(mProfile).when(mProfileProvider).getOriginalProfile();
        mProfileProviderSupplier.set(mProfileProvider);
        BrowsingHistoryBridgeJni.setInstanceForTesting(mBrowsingHistoryBridgeNatives);
        doReturn(mPrefService).when(mUserPrefsNatives).get(any());
        UserPrefsJni.setInstanceForTesting(mUserPrefsNatives);
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeNatives);
        IdentityServicesProvider.setInstanceForTests(mIdentityService);
        doReturn(mSigninManager).when(mIdentityService).getSigninManager(mProfile);
        doReturn(mIdentityManager).when(mIdentityService).getIdentityManager(mProfile);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        PrefChangeRegistrarJni.setInstanceForTesting(mPrefChangeRegistrarNatives);
        IncognitoUtilsJni.setInstanceForTesting(mIncognitoUtilsNatives);
        mHistoryPane =
                new HistoryPane(
                        mOnToolbarAlphaChange,
                        Robolectric.buildActivity(TestActivity.class).setup().get(),
                        mSnackbarManager,
                        mProfileProviderSupplier,
                        () -> mBottomSheetController,
                        () -> mCurrentTab);
    }

    @Test
    public void testNotifyLoadHint() {
        assertEquals(0, mHistoryPane.getRootView().getChildCount());

        mHistoryPane.notifyLoadHint(LoadHint.HOT);
        assertNotEquals(0, mHistoryPane.getRootView().getChildCount());

        mHistoryPane.notifyLoadHint(LoadHint.COLD);
        assertEquals(0, mHistoryPane.getRootView().getChildCount());
    }

    @Test
    public void testDestroy_WhileHot() {
        mHistoryPane.notifyLoadHint(LoadHint.HOT);
        mHistoryPane.destroy();
        assertEquals(0, mHistoryPane.getRootView().getChildCount());
    }

    @Test
    public void testDestroy_WhileCold() {
        mHistoryPane.notifyLoadHint(LoadHint.HOT);
        mHistoryPane.notifyLoadHint(LoadHint.COLD);
        mHistoryPane.destroy();
        assertEquals(0, mHistoryPane.getRootView().getChildCount());
    }

    @Test
    public void testDestroy_NoLoadHint() {
        mHistoryPane.destroy();
        assertEquals(0, mHistoryPane.getRootView().getChildCount());
    }
}
