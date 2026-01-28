// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

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

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.setup_list.SetupListManager;
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
    @Mock private SetupListManager mSetupListManager;
    private SharedPreferencesManager mPrefsManager;

    @Captor
    private ArgumentCaptor<DefaultBrowserPromoTriggerStateListener>
            mDefaultBrowserPromoTriggerStateListener;

    SettableMonotonicObservableSupplier<Profile> mProfileSupplier;
    private Context mContext;
    private @ModuleType int mDefaultModuleTypeForTesting;
    private EducationalTipModuleMediator mEducationalTipModuleMediator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        when(mActionDelegate.getContext()).thenReturn(mContext);
        mDefaultModuleTypeForTesting = ModuleType.DEFAULT_BROWSER_PROMO;
        TrackerFactory.setTrackerForTests(mTracker);
        mPrefsManager = ChromeSharedPreferences.getInstance();
        mPrefsManager.removeKey(
                ChromePreferenceKeys.SETUP_LIST_ENHANCED_SAFE_BROWSING_PROMO_COMPLETED);
        mPrefsManager.removeKey(ChromePreferenceKeys.SETUP_LIST_ADDRESS_BAR_PROMO_COMPLETED);
        DefaultBrowserPromoUtils.setInstanceForTesting(mMockDefaultBrowserPromoUtils);
        SetupListManager.setInstanceForTesting(mSetupListManager);

        // Setup for History sync promo
        mProfileSupplier = ObservableSuppliers.createMonotonic();
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
    public void testShowModule() {
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
    @EnableFeatures({ChromeFeatureList.ANDROID_SETUP_LIST})
    public void testShowSetupList_EnhancedSafeBrowsingPromo() {
        // Test showing enhance safe browsing promo card.
        testShowModuleImpl(
                ModuleType.ENHANCED_SAFE_BROWSING_PROMO,
                R.string.educational_tip_enhanced_safe_browsing_title,
                R.string.educational_tip_enhanced_safe_browsing_description,
                R.drawable.enhanced_safe_browsing_promo_logo);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ANDROID_SETUP_LIST})
    public void testShowSetupList_AddressBarPlacementPromo() {
        // Test showing address bar placement promo card.
        testShowModuleImpl(
                ModuleType.ADDRESS_BAR_PLACEMENT_PROMO,
                R.string.educational_tip_address_bar_placement_title,
                R.string.educational_tip_address_bar_placement_description,
                R.drawable.address_bar_placement_promo_logo);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ANDROID_SETUP_LIST})
    public void testShowSetupList_SignInPromo() {
        // Test showing sign in promo card.
        testShowModuleImpl(
                ModuleType.SIGN_IN_PROMO,
                R.string.educational_tip_sign_in_promo_title,
                R.string.educational_tip_sign_in_promo_description,
                R.drawable.sign_in_promo_logo);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ANDROID_SETUP_LIST})
    public void testShowSetupList_SavePasswordsPromo() {
        // Test showing save passwords promo card.
        testShowModuleImpl(
                ModuleType.SAVE_PASSWORDS_PROMO,
                R.string.educational_tip_save_passwords_title,
                R.string.educational_tip_save_passwords_description,
                R.drawable.save_passwords_promo_logo);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ANDROID_SETUP_LIST})
    public void testShowSetupList_PasswordCheckupPromo() {
        // Test showing password checkup promo card.
        testShowModuleImpl(
                ModuleType.PASSWORD_CHECKUP_PROMO,
                R.string.educational_tip_password_checkup_title,
                R.string.educational_tip_password_checkup_description,
                R.drawable.password_checkup_promo_logo);
    }

    @Test
    @SmallTest
    public void testShowSetupList_Completed() {
        when(mSetupListManager.isSetupListActive()).thenReturn(true);
        mPrefsManager.writeBoolean(
                ChromePreferenceKeys.SETUP_LIST_ENHANCED_SAFE_BROWSING_PROMO_COMPLETED, true);

        testShowModuleImpl(
                ModuleType.ENHANCED_SAFE_BROWSING_PROMO,
                R.string.educational_tip_enhanced_safe_browsing_title,
                R.string.educational_tip_enhanced_safe_browsing_description,
                R.drawable.setup_list_completed_background_wavy_circle,
                true);
    }

    @Test
    @SmallTest
    public void testShowSetupList_NotCompleted() {
        when(mSetupListManager.isSetupListActive()).thenReturn(true);
        mPrefsManager.writeBoolean(
                ChromePreferenceKeys.SETUP_LIST_ENHANCED_SAFE_BROWSING_PROMO_COMPLETED, false);

        testShowModuleImpl(
                ModuleType.ENHANCED_SAFE_BROWSING_PROMO,
                R.string.educational_tip_enhanced_safe_browsing_title,
                R.string.educational_tip_enhanced_safe_browsing_description,
                R.drawable.enhanced_safe_browsing_promo_logo,
                false);
    }

    @Test
    @SmallTest
    public void testShowModule_NonSetupList_IsCompletedNull() {
        when(mSetupListManager.isSetupListActive()).thenReturn(true);
        // ModuleType.TAB_GROUP_PROMO is not a Setup List module.
        testShowModuleImpl(
                ModuleType.TAB_GROUP_PROMO,
                R.string.educational_tip_tab_group_title,
                R.string.educational_tip_tab_group_description,
                R.drawable.tab_group_promo_logo);
    }

    @Test
    @SmallTest
    public void testOnViewCreated_DefaultBrowserPromo_TrackerInitialized_ShouldDisplay() {
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
    public void testOnViewCreated_DefaultBrowserPromo_TrackerInitialized_ShouldNotDisplay() {
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
    public void testOnViewCreated_DefaultBrowserPromo_TrackerNotInitialized() {
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
    public void testOnViewCreated_OtherPromoType() {
        mEducationalTipModuleMediator.setModuleTypeForTesting(ModuleType.TAB_GROUP_PROMO);

        mEducationalTipModuleMediator.showModule();
        mEducationalTipModuleMediator.onViewCreated();
        verify(mMockDefaultBrowserPromoUtils, never()).removeListener(any());
        verify(mMockDefaultBrowserPromoUtils, never()).notifyDefaultBrowserPromoVisible();
    }

    @Test
    @SmallTest
    public void testRemoveModule() {
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

    private void testShowModuleImpl(
            @ModuleType int moduleType,
            int titleId,
            int descriptionId,
            int imageResource,
            boolean isCompleted) {
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
        verify(mModel).set(EducationalTipModuleProperties.MARK_COMPLETED, isCompleted);
        verify(mModuleDelegate).onDataReady(moduleType, mModel);
        verify(mModuleDelegate, never()).onDataFetchFailed(moduleType);
    }
}
