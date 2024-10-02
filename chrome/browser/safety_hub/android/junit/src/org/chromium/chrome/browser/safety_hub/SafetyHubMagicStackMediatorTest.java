// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.PendingIntent;
import android.content.Context;
import android.view.View;
import android.view.View.OnClickListener;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar.PrefObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.prefs.PrefService;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for the Safety Hub Magic Stack mediator. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class SafetyHubMagicStackMediatorTest {
    private static final String DESCRIPTION = "description";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public SafetyHubTestRule mSafetyHubTestRule = new SafetyHubTestRule();

    @Mock private MagicStackBridge mMagicStackBridge;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private SettingsNavigation mSettingsNavigation;
    @Mock private PrefChangeRegistrar mPrefChangeRegistrar;
    @Mock private Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    @Mock private View mView;
    @Mock private Callback<String> mShowSurveyCallback;

    private Context mContext;
    private Profile mProfile;
    private PrefService mPrefService;
    private PendingIntent mPasswordCheckIntentForAccountCheckup;
    private PropertyModel mModel;
    private SafetyHubMagicStackMediator mMediator;
    private ModalDialogManager mModalDialogManager;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        mProfile = mSafetyHubTestRule.getProfile();
        mPrefService = mSafetyHubTestRule.getPrefService();
        mPasswordCheckIntentForAccountCheckup =
                mSafetyHubTestRule.getIntentForAccountPasswordCheckup();
        mModel = new PropertyModel(SafetyHubMagicStackViewProperties.ALL_KEYS);
        mMediator =
                new SafetyHubMagicStackMediator(
                        mContext,
                        mProfile,
                        mPrefService,
                        mModel,
                        mMagicStackBridge,
                        mTabModelSelector,
                        mModuleDelegate,
                        mPrefChangeRegistrar,
                        mModalDialogManagerSupplier,
                        mShowSurveyCallback);
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);

        mModalDialogManager =
                new ModalDialogManager(
                        mock(ModalDialogManager.Presenter.class),
                        ModalDialogManager.ModalDialogType.APP);
        when(mModalDialogManagerSupplier.get()).thenReturn(mModalDialogManager);

        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
    }

    @Test
    public void testNothingToDisplay() {
        doReturn(null).when(mMagicStackBridge).getModuleToShow();
        mMediator.showModule();
        verify(mModuleDelegate).onDataFetchFailed(ModuleType.SAFETY_HUB);
    }

    @Test
    public void testSafeBrowsingDisplayed() {
        MagicStackEntry entry =
                MagicStackEntry.create(DESCRIPTION, MagicStackEntry.ModuleType.SAFE_BROWSING);
        doReturn(entry).when(mMagicStackBridge).getModuleToShow();

        mMediator.showModule();

        verify(mModuleDelegate).onDataReady(eq(ModuleType.SAFETY_HUB), eq(mModel));
        assertEquals(
                mModel.get(SafetyHubMagicStackViewProperties.HEADER),
                mContext.getResources().getString(R.string.safety_hub_magic_stack_module_name));
        assertEquals(
                mModel.get(SafetyHubMagicStackViewProperties.TITLE),
                mContext.getResources()
                        .getString(R.string.safety_hub_magic_stack_safe_browsing_title));
        assertEquals(mModel.get(SafetyHubMagicStackViewProperties.SUMMARY), DESCRIPTION);
        assertEquals(
                mModel.get(SafetyHubMagicStackViewProperties.BUTTON_TEXT),
                mContext.getResources()
                        .getString(R.string.safety_hub_magic_stack_safe_browsing_button_text));
        assertEquals(
                shadowOf(mModel.get(SafetyHubMagicStackViewProperties.ICON_DRAWABLE))
                        .getCreatedFromResId(),
                R.drawable.ic_gshield_24);

        OnClickListener onClickListener =
                mModel.get(SafetyHubMagicStackViewProperties.BUTTON_ON_CLICK_LISTENER);
        onClickListener.onClick(mView);
        verify(mSettingsNavigation)
                .startSettings(eq(mContext), eq(SafeBrowsingSettingsFragment.class));
        verify(mModuleDelegate, times(1)).removeModule(ModuleType.SAFETY_HUB);
        verify(mMagicStackBridge, times(1)).dismissSafeBrowsingModule();
        verify(mShowSurveyCallback).onResult(MagicStackEntry.ModuleType.SAFE_BROWSING);
    }

    @Test
    public void testRevokedPermisisonsDisplayed() {
        MagicStackEntry entry =
                MagicStackEntry.create(DESCRIPTION, MagicStackEntry.ModuleType.REVOKED_PERMISSIONS);
        doReturn(entry).when(mMagicStackBridge).getModuleToShow();
        mMediator.showModule();

        verify(mModuleDelegate).onDataReady(eq(ModuleType.SAFETY_HUB), eq(mModel));
        assertEquals(
                mModel.get(SafetyHubMagicStackViewProperties.HEADER),
                mContext.getResources().getString(R.string.safety_hub_magic_stack_module_name));
        assertEquals(mModel.get(SafetyHubMagicStackViewProperties.TITLE), DESCRIPTION);
        assertNull(mModel.get(SafetyHubMagicStackViewProperties.SUMMARY));
        assertEquals(
                mModel.get(SafetyHubMagicStackViewProperties.BUTTON_TEXT),
                mContext.getResources()
                        .getString(R.string.safety_hub_magic_stack_safe_state_button_text));
        assertEquals(
                shadowOf(mModel.get(SafetyHubMagicStackViewProperties.ICON_DRAWABLE))
                        .getCreatedFromResId(),
                R.drawable.ic_check_circle_filled_green_24dp);

        OnClickListener onClickListener =
                mModel.get(SafetyHubMagicStackViewProperties.BUTTON_ON_CLICK_LISTENER);
        onClickListener.onClick(mView);
        verify(mSettingsNavigation).startSettings(eq(mContext), eq(SafetyHubFragment.class));
        verify(mShowSurveyCallback).onResult(MagicStackEntry.ModuleType.REVOKED_PERMISSIONS);
    }

    @Test
    public void testNotificationPermissionsDisplayed() {
        MagicStackEntry entry =
                MagicStackEntry.create(
                        DESCRIPTION, MagicStackEntry.ModuleType.NOTIFICATION_PERMISSIONS);
        doReturn(entry).when(mMagicStackBridge).getModuleToShow();
        mMediator.showModule();

        verify(mModuleDelegate).onDataReady(eq(ModuleType.SAFETY_HUB), eq(mModel));
        assertEquals(
                mModel.get(SafetyHubMagicStackViewProperties.HEADER),
                mContext.getResources().getString(R.string.safety_hub_magic_stack_module_name));
        assertEquals(
                mModel.get(SafetyHubMagicStackViewProperties.TITLE),
                mContext.getResources()
                        .getString(R.string.safety_hub_magic_stack_notifications_title));
        assertEquals(mModel.get(SafetyHubMagicStackViewProperties.SUMMARY), DESCRIPTION);
        assertEquals(
                mModel.get(SafetyHubMagicStackViewProperties.BUTTON_TEXT),
                mContext.getResources()
                        .getString(R.string.safety_hub_magic_stack_safe_state_button_text));
        assertEquals(
                shadowOf(mModel.get(SafetyHubMagicStackViewProperties.ICON_DRAWABLE))
                        .getCreatedFromResId(),
                R.drawable.safety_hub_notifications_icon);

        OnClickListener onClickListener =
                mModel.get(SafetyHubMagicStackViewProperties.BUTTON_ON_CLICK_LISTENER);
        onClickListener.onClick(mView);
        verify(mSettingsNavigation).startSettings(eq(mContext), eq(SafetyHubFragment.class));
        verify(mShowSurveyCallback).onResult(MagicStackEntry.ModuleType.NOTIFICATION_PERMISSIONS);
    }

    @Test
    public void testCompromisedPasswordsDisplayed() throws PendingIntent.CanceledException {
        MagicStackEntry entry =
                MagicStackEntry.create(DESCRIPTION, MagicStackEntry.ModuleType.PASSWORDS);
        doReturn(entry).when(mMagicStackBridge).getModuleToShow();
        mMediator.showModule();

        verify(mModuleDelegate).onDataReady(eq(ModuleType.SAFETY_HUB), eq(mModel));
        assertEquals(
                mModel.get(SafetyHubMagicStackViewProperties.HEADER),
                mContext.getResources().getString(R.string.safety_hub_magic_stack_module_name));
        assertEquals(
                mModel.get(SafetyHubMagicStackViewProperties.TITLE),
                mContext.getResources()
                        .getString(R.string.safety_hub_magic_stack_compromised_passwords_title));
        assertEquals(mModel.get(SafetyHubMagicStackViewProperties.SUMMARY), DESCRIPTION);
        assertEquals(
                mModel.get(SafetyHubMagicStackViewProperties.BUTTON_TEXT),
                mContext.getResources()
                        .getString(R.string.safety_hub_magic_stack_compromised_passwords_title));
        assertEquals(
                shadowOf(mModel.get(SafetyHubMagicStackViewProperties.ICON_DRAWABLE))
                        .getCreatedFromResId(),
                R.drawable.ic_password_manager_key);

        OnClickListener onClickListener =
                mModel.get(SafetyHubMagicStackViewProperties.BUTTON_ON_CLICK_LISTENER);
        onClickListener.onClick(mView);
        verify(mPasswordCheckIntentForAccountCheckup, times(1)).send();
        verify(mModuleDelegate, times(1)).removeModule(ModuleType.SAFETY_HUB);
        verify(mMagicStackBridge, times(1)).dismissCompromisedPasswordsModule();
        verify(mShowSurveyCallback).onResult(MagicStackEntry.ModuleType.PASSWORDS);
    }

    @Test
    public void testDismissSafeBrowsing() {
        MagicStackEntry entry =
                MagicStackEntry.create(DESCRIPTION, MagicStackEntry.ModuleType.SAFE_BROWSING);
        doReturn(entry).when(mMagicStackBridge).getModuleToShow();
        mMediator.showModule();

        // Capture the callback
        ArgumentCaptor<PrefObserver> captor = ArgumentCaptor.forClass(PrefObserver.class);
        verify(mPrefChangeRegistrar).addObserver(eq(Pref.SAFE_BROWSING_ENABLED), captor.capture());
        PrefObserver observer = captor.getValue();

        // Test that the module is not dismissed when Safe Browsing is disabled.
        doReturn(false).when(mPrefService).getBoolean(Pref.SAFE_BROWSING_ENABLED);
        observer.onPreferenceChange();
        verify(mModuleDelegate, times(0)).removeModule(ModuleType.SAFETY_HUB);
        verify(mMagicStackBridge, times(0)).dismissSafeBrowsingModule();

        // Dismiss the module when Safe Browsing is enabled.
        doReturn(true).when(mPrefService).getBoolean(Pref.SAFE_BROWSING_ENABLED);
        observer.onPreferenceChange();
        verify(mModuleDelegate, times(1)).removeModule(ModuleType.SAFETY_HUB);
        verify(mMagicStackBridge, times(1)).dismissSafeBrowsingModule();

        // Ensure that dismissal is idempotent.
        observer.onPreferenceChange();
        verify(mModuleDelegate, times(1)).removeModule(eq(ModuleType.SAFETY_HUB));
        verify(mMagicStackBridge, times(1)).dismissSafeBrowsingModule();

        // Ensure that the module cannot be shown anymore.
        mMediator.showModule();
        verify(mModuleDelegate, times(1)).onDataReady(eq(ModuleType.SAFETY_HUB), any());
    }

    @Test
    public void testDismissSafeState() {
        MagicStackEntry entry =
                MagicStackEntry.create(DESCRIPTION, MagicStackEntry.ModuleType.REVOKED_PERMISSIONS);
        doReturn(entry).when(mMagicStackBridge).getModuleToShow();
        mMediator.showModule();

        // Verify that the pref change registrar is not used.
        verify(mPrefChangeRegistrar, times(0)).addObserver(any(), any());

        // Dismiss the module with the dismiss callback.
        mMediator.activeModuleDismissed();
        verify(mModuleDelegate, times(1)).removeModule(ModuleType.SAFETY_HUB);

        // Verify that dismissal is idempotent.
        mMediator.activeModuleDismissed();
        verify(mModuleDelegate, times(1)).removeModule(ModuleType.SAFETY_HUB);

        // Ensure that the module cannot be shown anymore.
        mMediator.showModule();
        verify(mModuleDelegate, times(1)).onDataReady(eq(ModuleType.SAFETY_HUB), any());
    }

    @Test
    public void testDismissCompromisedPasswords() {
        MagicStackEntry entry =
                MagicStackEntry.create(DESCRIPTION, MagicStackEntry.ModuleType.PASSWORDS);
        doReturn(entry).when(mMagicStackBridge).getModuleToShow();
        mMediator.showModule();

        // Capture the callback
        ArgumentCaptor<PrefObserver> captor = ArgumentCaptor.forClass(PrefObserver.class);
        verify(mPrefChangeRegistrar)
                .addObserver(eq(Pref.BREACHED_CREDENTIALS_COUNT), captor.capture());
        PrefObserver observer = captor.getValue();

        // Test that the module is not dismissed when compromised passwords exist.
        doReturn(5).when(mPrefService).getInteger(Pref.BREACHED_CREDENTIALS_COUNT);
        observer.onPreferenceChange();
        verify(mModuleDelegate, times(0)).removeModule(ModuleType.SAFETY_HUB);
        verify(mMagicStackBridge, times(0)).dismissCompromisedPasswordsModule();

        // Dismiss the module when there is no compromised passwords.
        doReturn(0).when(mPrefService).getInteger(Pref.BREACHED_CREDENTIALS_COUNT);
        observer.onPreferenceChange();
        verify(mModuleDelegate, times(1)).removeModule(ModuleType.SAFETY_HUB);
        verify(mMagicStackBridge, times(1)).dismissCompromisedPasswordsModule();

        // Ensure that dismissal is idempotent.
        observer.onPreferenceChange();
        verify(mModuleDelegate, times(1)).removeModule(eq(ModuleType.SAFETY_HUB));
        verify(mMagicStackBridge, times(1)).dismissCompromisedPasswordsModule();

        // Ensure that the module cannot be shown anymore.
        mMediator.showModule();
        verify(mModuleDelegate, times(1)).onDataReady(eq(ModuleType.SAFETY_HUB), any());
    }
}
