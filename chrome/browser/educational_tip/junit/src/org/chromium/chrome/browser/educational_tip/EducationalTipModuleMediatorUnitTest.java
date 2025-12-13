// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserPromoTriggerStateListener;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.shadows.ShadowAppCompatResources;

/** Unit tests for {@link EducationalTipModuleMediator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
public class EducationalTipModuleMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PropertyModel mModel;
    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private EducationTipModuleActionDelegate mActionDelegate;
    @Mock private Profile mProfile;
    @Mock private Tracker mTracker;
    @Mock private DefaultBrowserPromoUtils mMockDefaultBrowserPromoUtils;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private SyncService mSyncService;
    @Mock private IdentityManager mIdentityManager;

    @Captor
    private ArgumentCaptor<DefaultBrowserPromoTriggerStateListener>
            mDefaultBrowserPromoTriggerStateListener;

    ObservableSupplierImpl<Profile> mProfileSupplier;
    private Context mContext;
    private @ModuleType int mDefaultModuleTypeForTesting;
    private EducationalTipModuleMediator mEducationalTipModuleMediator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        when(mActionDelegate.getContext()).thenReturn(mContext);
        mDefaultModuleTypeForTesting = ModuleType.DEFAULT_BROWSER_PROMO;
        TrackerFactory.setTrackerForTests(mTracker);
        DefaultBrowserPromoUtils.setInstanceForTesting(mMockDefaultBrowserPromoUtils);

        // Setup for History sync promo
        mProfileSupplier = new ObservableSupplierImpl<>();
        mProfileSupplier.set(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mActionDelegate.getProfileSupplier()).thenReturn(mProfileSupplier);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        SyncServiceFactory.setInstanceForTesting(mSyncService);

        mEducationalTipModuleMediator =
                new EducationalTipModuleMediator(
                        mDefaultModuleTypeForTesting,
                        mModel,
                        mModuleDelegate,
                        mActionDelegate,
                        mProfile);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testShowModule() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());

        // Test showing default browser promo card.
        testShowModuleImpl(
                ModuleType.DEFAULT_BROWSER_PROMO,
                R.string.educational_tip_default_browser_title,
                R.string.educational_tip_default_browser_description,
                R.drawable.default_browser_promo_logo);

        // Test showing tab group promo card.
        testShowModuleImpl(
                ModuleType.TAB_GROUP_PROMO,
                R.string.educational_tip_tab_group_title,
                R.string.educational_tip_tab_group_description,
                R.drawable.tab_group_promo_logo);

        // Test showing tab group sync promo card.
        testShowModuleImpl(
                ModuleType.TAB_GROUP_SYNC_PROMO,
                R.string.educational_tip_tab_group_sync_title,
                R.string.educational_tip_tab_group_sync_description,
                R.drawable.tab_group_sync_promo_logo);

        // Test showing quick delete promo card.
        testShowModuleImpl(
                ModuleType.QUICK_DELETE_PROMO,
                R.string.educational_tip_quick_delete_title,
                R.string.educational_tip_quick_delete_description,
                R.drawable.quick_delete_promo_logo);

        // Test showing history sync promo card.
        testShowModuleImpl(
                ModuleType.HISTORY_SYNC_PROMO,
                R.string.educational_tip_history_sync_title,
                R.string.educational_tip_history_sync_description,
                R.drawable.history_sync_promo_logo);

        // Test showing tips notifications promo card.
        testShowModuleImpl(
                ModuleType.TIPS_NOTIFICATIONS_PROMO,
                R.string.educational_tip_tips_notifications_title,
                R.string.educational_tip_tips_notifications_description,
                R.drawable.tips_notifications_promo_logo);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testOnViewCreated_DefaultBrowserPromo_TrackerInitialized_ShouldDisplay() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());
        when(mTracker.isInitialized()).thenReturn(true);
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.DEFAULT_BROWSER_PROMO_MAGIC_STACK))
                .thenReturn(true);

        mEducationalTipModuleMediator.showModule();
        mEducationalTipModuleMediator.onViewCreated();
        verify(mMockDefaultBrowserPromoUtils)
                .removeListener(
                        mEducationalTipModuleMediator
                                .getDefaultBrowserPromoTriggerStateListenerForTesting());
        verify(mMockDefaultBrowserPromoUtils).notifyDefaultBrowserPromoVisible();
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testOnViewCreated_DefaultBrowserPromo_TrackerInitialized_ShouldNotDisplay() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());
        when(mTracker.isInitialized()).thenReturn(true);
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.DEFAULT_BROWSER_PROMO_MAGIC_STACK))
                .thenReturn(false);

        mEducationalTipModuleMediator.showModule();
        mEducationalTipModuleMediator.onViewCreated();
        verify(mMockDefaultBrowserPromoUtils, never()).removeListener(any());
        verify(mMockDefaultBrowserPromoUtils, never()).notifyDefaultBrowserPromoVisible();
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testOnViewCreated_DefaultBrowserPromo_TrackerNotInitialized() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());
        when(mTracker.isInitialized()).thenReturn(false);

        mEducationalTipModuleMediator.showModule();
        mEducationalTipModuleMediator.onViewCreated();
        verify(mMockDefaultBrowserPromoUtils)
                .removeListener(
                        mEducationalTipModuleMediator
                                .getDefaultBrowserPromoTriggerStateListenerForTesting());
        verify(mMockDefaultBrowserPromoUtils).notifyDefaultBrowserPromoVisible();
        verify(mTracker).addOnInitializedCallback(any());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testOnViewCreated_OtherPromoType() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());
        mEducationalTipModuleMediator.setModuleTypeForTesting(ModuleType.TAB_GROUP_PROMO);

        mEducationalTipModuleMediator.showModule();
        mEducationalTipModuleMediator.onViewCreated();
        verify(mMockDefaultBrowserPromoUtils, never()).removeListener(any());
        verify(mMockDefaultBrowserPromoUtils, never()).notifyDefaultBrowserPromoVisible();
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testRemoveModule() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());
        mEducationalTipModuleMediator.setModuleTypeForTesting(ModuleType.DEFAULT_BROWSER_PROMO);
        mEducationalTipModuleMediator.showModule();
        verify(mMockDefaultBrowserPromoUtils)
                .addListener(mDefaultBrowserPromoTriggerStateListener.capture());

        mDefaultBrowserPromoTriggerStateListener.getValue().onDefaultBrowserPromoTriggered();
        verify(mModuleDelegate).removeModule(mDefaultModuleTypeForTesting);
        verify(mMockDefaultBrowserPromoUtils)
                .removeListener(mDefaultBrowserPromoTriggerStateListener.capture());
    }

    private void testShowModuleImpl(
            @ModuleType int moduleType, int titleId, int descriptionId, int imageResource) {
        mEducationalTipModuleMediator.setModuleTypeForTesting(moduleType);
        mEducationalTipModuleMediator.showModule();

        verify(mModel)
                .set(
                        EducationalTipModuleProperties.MODULE_CONTENT_TITLE_STRING,
                        mContext.getString(titleId));
        verify(mModel)
                .set(
                        EducationalTipModuleProperties.MODULE_CONTENT_DESCRIPTION_STRING,
                        mContext.getString(descriptionId));
        verify(mModel).set(EducationalTipModuleProperties.MODULE_CONTENT_IMAGE, imageResource);
        verify(mModuleDelegate).onDataReady(moduleType, mModel);
        verify(mModuleDelegate, never()).onDataFetchFailed(moduleType);
    }
}
