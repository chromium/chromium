// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.ntp_cards;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.AUXILIARY_SEARCH;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.DEFAULT_BROWSER_PROMO;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.PRICE_CHANGE;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.QUICK_DELETE_PROMO;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.SAFETY_HUB;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.SINGLE_TAB;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.TAB_GROUP_PROMO;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.TAB_GROUP_SYNC_PROMO;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.BACK_PRESS_HANDLER;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LIST_CONTAINER_VIEW_DELEGATE;

import android.content.Context;
import android.view.View;
import android.widget.CompoundButton;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.magic_stack.HomeModulesUtils;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.ListContainerViewDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.function.Supplier;

/** Unit tests for {@link NtpCardsMediator} */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.HOME_MODULE_PREF_REFACTOR)
public class NtpCardsMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PropertyModel mContainerPropertyModel;
    @Mock private PropertyModel mBottomSheetPropertyModel;
    @Mock private PropertyModel mNtpCardsPropertyModel;
    @Mock private BottomSheetDelegate mDelegate;
    @Mock private Profile mProfile;
    @Mock private PrefService mPrefService;
    @Mock private HomeModulesConfigManager mHomeModulesConfigManager;
    @Mock private CompoundButton mCompoundButton;
    @Captor private ArgumentCaptor<View.OnClickListener> mBackPressHandlerCaptor;

    private Supplier<@Nullable Profile> mProfileSupplier;
    private NtpCardsMediator mNtpCardsMediator;
    private Context mContext;
    private ListContainerViewDelegate mListContainerViewDelegate;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mProfileSupplier = () -> mProfile;
        UserPrefs.setPrefServiceForTesting(mPrefService);
        HomeModulesConfigManager.setInstanceForTesting(mHomeModulesConfigManager);
        mNtpCardsMediator =
                new NtpCardsMediator(
                        mContainerPropertyModel,
                        mBottomSheetPropertyModel,
                        mNtpCardsPropertyModel,
                        mDelegate,
                        mProfileSupplier);
        mListContainerViewDelegate = mNtpCardsMediator.createListDelegate();
    }

    @Test
    public void testConstructor() {
        verify(mContainerPropertyModel)
                .set(eq(LIST_CONTAINER_VIEW_DELEGATE), any(ListContainerViewDelegate.class));
    }

    @Test
    public void testListContainerViewDelegate() {
        HomeModulesConfigManager homeModulesConfigManager = HomeModulesConfigManager.getInstance();

        // Verifies that the content of the delegate.getListItems() comes from
        // homeModulesConfigManager.
        List<Integer> content = mListContainerViewDelegate.getListItems();
        assertEquals(content, homeModulesConfigManager.getModuleListShownInSettings());

        // Verifies that the titles of list items come from HomeModulesUtils.
        List<Integer> types =
                List.of(
                        SINGLE_TAB,
                        PRICE_CHANGE,
                        SAFETY_HUB,
                        AUXILIARY_SEARCH,
                        DEFAULT_BROWSER_PROMO,
                        TAB_GROUP_PROMO,
                        TAB_GROUP_SYNC_PROMO,
                        QUICK_DELETE_PROMO);
        for (int type : types) {
            assertEquals(
                    HomeModulesUtils.getTitleForModuleType(type, mContext),
                    mListContainerViewDelegate.getListItemTitle(type, mContext));
        }
    }

    @Test
    public void testIsListItemChecked() {
        when(mHomeModulesConfigManager.getPrefModuleTypeEnabled(ModuleType.PRICE_CHANGE))
                .thenReturn(true);
        assertTrue(mListContainerViewDelegate.isListItemChecked(ModuleType.PRICE_CHANGE));

        when(mHomeModulesConfigManager.getPrefModuleTypeEnabled(ModuleType.PRICE_CHANGE))
                .thenReturn(false);
        assertFalse(mListContainerViewDelegate.isListItemChecked(ModuleType.PRICE_CHANGE));
    }

    @Test
    public void testGetOnCheckedChangeListener() {
        CompoundButton.OnCheckedChangeListener listener =
                mListContainerViewDelegate.getOnCheckedChangeListener(ModuleType.PRICE_CHANGE);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("NewTabPage.Customization.TurnOnModule", PRICE_CHANGE)
                        .build();
        listener.onCheckedChanged(mCompoundButton, true);
        verify(mHomeModulesConfigManager).setPrefModuleTypeEnabled(ModuleType.PRICE_CHANGE, true);
        watcher.assertExpected();

        watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("NewTabPage.Customization.TurnOffModule", PRICE_CHANGE)
                        .build();
        listener.onCheckedChanged(mCompoundButton, false);
        verify(mHomeModulesConfigManager).setPrefModuleTypeEnabled(ModuleType.PRICE_CHANGE, false);
        watcher.assertExpected();
    }

    @Test
    public void testBackPressHandler() {
        // Verifies that when the feed settings bottom sheet should show alone, the back press
        // handler should be set to null.
        when(mDelegate.shouldShowAlone()).thenReturn(true);
        new NtpCardsMediator(
                mContainerPropertyModel,
                mBottomSheetPropertyModel,
                mNtpCardsPropertyModel,
                mDelegate,
                mProfileSupplier);
        verify(mBottomSheetPropertyModel).set(BACK_PRESS_HANDLER, null);

        // Verifies that when the feed settings bottom sheet is part of the navigation flow starting
        // from the main bottom sheet, and the back press handler should be set to
        // backPressOnCurrentBottomSheet()
        View backButton = mock(View.class);
        clearInvocations(mBottomSheetPropertyModel);
        when(mDelegate.shouldShowAlone()).thenReturn(false);
        new NtpCardsMediator(
                mContainerPropertyModel,
                mBottomSheetPropertyModel,
                mNtpCardsPropertyModel,
                mDelegate,
                mProfileSupplier);
        verify(mBottomSheetPropertyModel)
                .set(eq(BACK_PRESS_HANDLER), mBackPressHandlerCaptor.capture());
        mBackPressHandlerCaptor.getValue().onClick(backButton);
        verify(mDelegate).backPressOnCurrentBottomSheet();
    }

    @Test
    public void testDestroy() {
        NtpCardsMediator spy = spy(mNtpCardsMediator);
        spy.destroy();

        verify(mBottomSheetPropertyModel).set(eq(BACK_PRESS_HANDLER), eq(null));
        verify(mContainerPropertyModel).set(eq(LIST_CONTAINER_VIEW_DELEGATE), eq(null));
        verify(spy).updateUserPrefs();
    }

    @Test
    public void updateUserPrefsTest() {
        // Set a preference value.
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        String prefKey = HomeModulesUtils.getSettingsPreferenceKey(ModuleType.SINGLE_TAB);
        sharedPreferencesManager.writeBoolean(prefKey, true);

        mNtpCardsMediator.updateUserPrefs();

        // Verify that UserPrefs.setBoolean is called.
        verify(mPrefService).setBoolean(Pref.TAB_RESUMPTION_HOME_MODULE_ENABLED, true);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.HOME_MODULE_PREF_REFACTOR)
    public void updateUserPrefsTest_FeatureDisabled() {
        mNtpCardsMediator.updateUserPrefs();

        // Verify that UserPrefs.setBoolean is not called.
        verify(mPrefService, never()).setBoolean(anyString(), anyBoolean());
    }

    @Test
    public void updateUserPrefsTest_ProfileNull() {
        mNtpCardsMediator.updateUserPrefs();

        // Verify that UserPrefs.setBoolean is not called.
        verify(mPrefService, never()).setBoolean(anyString(), anyBoolean());
    }

    @Test
    public void testOnAllCardsConfigChanged() {
        reset(mNtpCardsPropertyModel);
        mNtpCardsMediator.onAllCardsConfigChanged(true);
        verify(mNtpCardsPropertyModel)
                .set(eq(NtpCustomizationViewProperties.ARE_CARD_SWITCHES_ENABLED), eq(true));

        mNtpCardsMediator.onAllCardsConfigChanged(false);
        verify(mNtpCardsPropertyModel)
                .set(eq(NtpCustomizationViewProperties.ARE_CARD_SWITCHES_ENABLED), eq(false));
    }
}
