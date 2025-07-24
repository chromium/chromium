// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProviderJni;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.AsyncTabLauncher;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.prefs.PrefChangeRegistrarJni;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.GURL;

/** Unit tests for {@link HistoryContentManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DisableFeatures(SigninFeatures.HISTORY_PAGE_HISTORY_SYNC_PROMO)
public class HistoryContentManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    @Mock private HistoryContentManager.Observer mObserver;
    @Mock private Profile mProfile;
    @Mock private SelectionDelegate<HistoryItem> mSelectionDelegate;
    @Mock private Supplier<BottomSheetController> mBottomSheetController;
    @Mock private Tab mTab;
    @Mock private Supplier<Tab> mTabSupplier;
    @Mock private HistoryUmaRecorder mUmaRecorder;
    @Mock private HistoryProvider mHistoryProvider;
    @Mock private AsyncTabLauncher mAsyncTabLauncher;
    @Mock private LargeIconBridgeJni mLargeIconBridgeJni;
    @Mock private IdentityServicesProviderJni mIdentityServicesProviderJni;
    @Mock private SigninManager mSigninManager;
    @Mock private UserPrefsJni mUserPrefsJni;
    @Mock private PrefChangeRegistrarJni mPrefChangeRegistrarJni;

    private HistoryContentManager mHistoryContentManager;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeJni);
        when(mIdentityServicesProviderJni.getSigninManager(any())).thenReturn(mSigninManager);
        IdentityServicesProviderJni.setInstanceForTesting(mIdentityServicesProviderJni);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);
        PrefChangeRegistrarJni.setInstanceForTesting(mPrefChangeRegistrarJni);

        mHistoryContentManager =
                new HistoryContentManager(
                        mActivity,
                        mObserver,
                        /* isSeparateActivity= */ false,
                        mProfile,
                        /* shouldShowPrivacyDisclaimers= */ false,
                        /* shouldShowClearDataIfAvailable= */ false,
                        null,
                        mSelectionDelegate,
                        mBottomSheetController,
                        mTabSupplier,
                        () -> {},
                        mUmaRecorder,
                        mHistoryProvider,
                        null,
                        false,
                        false,
                        null,
                        mAsyncTabLauncher);
    }

    @Test
    public void testOpenUrlInSameTab() {
        when(mTabSupplier.get()).thenReturn(mTab);
        mHistoryContentManager.openUrl(
                new GURL("https://test.com"),
                /* isIncognito= */ false,
                /* createNewTab= */ false,
                /* runCallback= */ false);
        verify(mTab).loadUrl(any());
    }

    @Test
    public void testOpenUrlInSameTab_CurrentTabIsNullFallsBackToNewTab() {
        when(mTabSupplier.get()).thenReturn(null);
        mHistoryContentManager.openUrl(
                new GURL("https://test.com"),
                /* isIncognito= */ false,
                /* createNewTab= */ false,
                /* runCallback= */ false);
        verify(mAsyncTabLauncher).launchNewTab(any(), anyInt(), any());
    }
}
