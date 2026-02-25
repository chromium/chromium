// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.educational_tip.cards.HistorySyncPromoCoordinator;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.setup_list.SetupListManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserPromoTriggerStateListener;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.shadows.ShadowAppCompatResources;

import java.util.Set;

/** Unit tests for {@link EducationalTipModuleMediator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
@EnableFeatures({
    SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
    SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
})
public class EducationalTipModuleMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTime = new FakeTimeTestRule();

    private PropertyModel mModel;
    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private EducationTipModuleActionDelegate mActionDelegate;
    @Mock private Profile mProfile;
    @Mock private Tracker mTracker;
    @Mock private DefaultBrowserPromoUtils mMockDefaultBrowserPromoUtils;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private SyncService mSyncService;
    @Mock private IdentityManager mIdentityManager;
    @Mock private SetupListManager mSetupListManager;
    @Mock private BottomSheetController mBottomSheetController;

    @Captor
    private ArgumentCaptor<DefaultBrowserPromoTriggerStateListener>
            mDefaultBrowserPromoTriggerStateListener;

    @Captor private ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor;

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
        DefaultBrowserPromoUtils.setInstanceForTesting(mMockDefaultBrowserPromoUtils);
        SetupListManager.setInstanceForTesting(mSetupListManager);
        when(mActionDelegate.getBottomSheetController()).thenReturn(mBottomSheetController);

        // Setup for History sync promo
        mProfileSupplier = ObservableSuppliers.createMonotonic();
        mProfileSupplier.set(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mActionDelegate.getProfileSupplier()).thenReturn(mProfileSupplier);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        SyncServiceFactory.setInstanceForTesting(mSyncService);

        mModel = new PropertyModel.Builder(EducationalTipModuleProperties.ALL_KEYS).build();
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
                R.string.use_chrome_by_default,
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
    public void testShowSetupList_CelebratoryPromo() {
        // Test showing celebratory promo card.
        testShowModuleImpl(
                ModuleType.SETUP_LIST_CELEBRATORY_PROMO,
                R.string.setup_list_celebratory_promo_title,
                R.string.setup_list_celebratory_promo_description,
                R.drawable.setup_list_celebratory_promo_logo);
    }

    @Test
    @SmallTest
    public void testShowSetupList_Completed() {
        when(mSetupListManager.isSetupListActive()).thenReturn(true);
        when(mSetupListManager.isSetupListModule(ModuleType.ENHANCED_SAFE_BROWSING_PROMO))
                .thenReturn(true);
        when(mSetupListManager.isModuleCompleted(ModuleType.ENHANCED_SAFE_BROWSING_PROMO))
                .thenReturn(true);

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
        when(mSetupListManager.isSetupListModule(ModuleType.ENHANCED_SAFE_BROWSING_PROMO))
                .thenReturn(true);
        when(mSetupListManager.isModuleCompleted(ModuleType.ENHANCED_SAFE_BROWSING_PROMO))
                .thenReturn(false);

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
    public void testHistorySyncPromo_SetupList_AnimationFlow() {
        mEducationalTipModuleMediator.setModuleTypeForTesting(ModuleType.HISTORY_SYNC_PROMO);
        when(mSetupListManager.isSetupListModule(ModuleType.HISTORY_SYNC_PROMO)).thenReturn(true);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        mEducationalTipModuleMediator.showModule();

        // 1. Simulate sync completion via system state (e.g. Settings).
        when(mSyncService.getSelectedTypes())
                .thenReturn(Set.of(UserSelectableType.HISTORY, UserSelectableType.TABS));

        HistorySyncPromoCoordinator coordinator =
                (HistorySyncPromoCoordinator)
                        mEducationalTipModuleMediator.getCardProviderForTesting();

        // This records completion but should NOT trigger the animation yet.
        coordinator.syncStateChanged();
        verify(mModuleDelegate, never()).removeModule(anyInt());
        assertEquals(false, mModel.get(EducationalTipModuleProperties.MARK_COMPLETED));

        // 2. Simulate user returning to NTP (calling updateModule).
        when(mSetupListManager.isModuleAwaitingCompletionAnimation(ModuleType.HISTORY_SYNC_PROMO))
                .thenReturn(true);
        mEducationalTipModuleMediator.updateModule();

        // Now the animation sequence should run. Strikethrough should be applied immediately.
        assertEquals(true, mModel.get(EducationalTipModuleProperties.MARK_COMPLETED));
    }

    @Test
    @SmallTest
    public void testUpdateModule_TriggersAnimation() {
        mEducationalTipModuleMediator.setModuleTypeForTesting(ModuleType.SIGN_IN_PROMO);
        when(mSetupListManager.isSetupListModule(ModuleType.SIGN_IN_PROMO)).thenReturn(true);
        when(mSetupListManager.isModuleAwaitingCompletionAnimation(ModuleType.SIGN_IN_PROMO))
                .thenReturn(true);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        mEducationalTipModuleMediator.showModule();
        mEducationalTipModuleMediator.updateModule();

        // Verify priming was called immediately.
        verify(mSetupListManager).maybePrimeCompletionStatus(mProfile);

        // Completion image should be set immediately to trigger the icon animation.
        assertEquals(
                R.drawable.setup_list_completed_background_wavy_circle,
                mModel.get(EducationalTipModuleProperties.MODULE_CONTENT_COMPLETED_IMAGE));

        // Strikethrough should be applied immediately.
        assertEquals(true, mModel.get(EducationalTipModuleProperties.MARK_COMPLETED));

        // Verify reordering has NOT happened yet.
        verify(mModuleDelegate, never()).maybeMoveModuleToTheEnd(anyInt());

        // 1. Advance to the combined duration.
        mFakeTime.advanceMillis(
                SetupListManager.STRIKETHROUGH_DURATION_MS + SetupListManager.HIDE_DURATION_MS);
        ShadowLooper.runMainLooperOneTask();

        // Final verification of completion signal and reordering trigger.
        verify(mSetupListManager).onCompletionAnimationFinished(ModuleType.SIGN_IN_PROMO);
        verify(mModuleDelegate).maybeMoveModuleToTheEnd(ModuleType.SIGN_IN_PROMO);
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

    @Test
    @SmallTest
    public void testBottomSheetObserver_AddedAndRemoved() {
        verify(mBottomSheetController).addObserver(any());
        mEducationalTipModuleMediator.destroy();
        verify(mBottomSheetController).removeObserver(any());
    }

    @Test
    @SmallTest
    public void testBottomSheetObserver_TriggersUpdate() {
        mEducationalTipModuleMediator.setModuleTypeForTesting(ModuleType.SAVE_PASSWORDS_PROMO);
        verify(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());

        // Simulate sheet dismissal.
        mBottomSheetObserverCaptor
                .getValue()
                .onSheetStateChanged(BottomSheetController.SheetState.HIDDEN, 0);

        // verify it triggers a profile check as part of updateModule().
        verify(mActionDelegate).getProfileSupplier();
    }

    @Test
    @SmallTest
    public void testBottomSheetObserver_DefaultBrowser_SkipsOnInteractionComplete() {
        mEducationalTipModuleMediator.setModuleTypeForTesting(ModuleType.DEFAULT_BROWSER_PROMO);
        verify(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());

        // Simulate sheet dismissal with interaction complete (clicked button).
        mBottomSheetObserverCaptor
                .getValue()
                .onSheetStateChanged(
                        BottomSheetController.SheetState.HIDDEN,
                        BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);

        // verify updateModule was NOT called (profile supplier never accessed).
        verify(mActionDelegate, never()).getProfileSupplier();
    }

    private void testShowModuleImpl(
            @ModuleType int moduleType, int titleId, int descriptionId, int imageResource) {
        mEducationalTipModuleMediator.setModuleTypeForTesting(moduleType);
        mEducationalTipModuleMediator.showModule();

        assertEquals(
                mContext.getString(titleId),
                mModel.get(EducationalTipModuleProperties.MODULE_CONTENT_TITLE_STRING));
        assertEquals(
                mContext.getString(descriptionId),
                mModel.get(EducationalTipModuleProperties.MODULE_CONTENT_DESCRIPTION_STRING));
        assertEquals(
                imageResource, mModel.get(EducationalTipModuleProperties.MODULE_CONTENT_IMAGE));

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

        assertEquals(
                mContext.getString(titleId),
                mModel.get(EducationalTipModuleProperties.MODULE_CONTENT_TITLE_STRING));
        assertEquals(
                mContext.getString(descriptionId),
                mModel.get(EducationalTipModuleProperties.MODULE_CONTENT_DESCRIPTION_STRING));
        assertEquals(
                imageResource, mModel.get(EducationalTipModuleProperties.MODULE_CONTENT_IMAGE));
        assertEquals(
                isCompleted,
                mModel.get(EducationalTipModuleProperties.MARK_COMPLETED).booleanValue());

        verify(mModuleDelegate).onDataReady(moduleType, mModel);
        verify(mModuleDelegate, never()).onDataFetchFailed(moduleType);
    }
}
