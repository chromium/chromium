// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search.module;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchConfigManager;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchConfigManager.ShareTabsWithOsStateListener;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchControllerFactory;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchHooks;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchMetrics.ClickInfo;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchUtils;
import org.chromium.chrome.browser.auxiliary_search.R;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link AuxiliarySearchModuleMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AuxiliarySearchModuleMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AuxiliarySearchHooks mHooks;
    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private PropertyModel mPropertyModel;

    @Mock private Runnable mOpenSettingsRunnable;
    @Captor private ArgumentCaptor<OnClickListener> mFirstButtonClickListenerCaptor;
    @Captor private ArgumentCaptor<OnClickListener> mSecondButtonClickListenerCaptor;

    private Context mContext;
    private View mView;

    private AuxiliarySearchControllerFactory mFactory;

    private AuxiliarySearchModuleMediator mMediator;

    @Before
    public void setup() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        org.chromium.chrome.browser.auxiliary_search.R.style
                                .Theme_BrowserUI_DayNight);

        mFactory = AuxiliarySearchControllerFactory.getInstance();
        mHooks = Mockito.mock(AuxiliarySearchHooks.class);
        when(mHooks.isEnabled()).thenReturn(true);
        mFactory.setHooksForTesting(mHooks);
        assertTrue(mFactory.isEnabled());
    }

    @After
    public void tearDown() {
        AuxiliarySearchUtils.resetSharedPreferenceForTesting();
    }

    @Test
    @SmallTest
    public void testShowModule_DefaultOptIn_FirstButton() {
        when(mHooks.isSettingDefaultEnabledByOs()).thenReturn(true);
        assertTrue(mFactory.isSettingDefaultEnabledByOs());

        createMediator();
        inflateViewAndVerify(/* isDefaultOptIn= */ true);

        mMediator.showModule();
        verifyShowModuleComplete();

        // Verifies the case of clicking the "Go to settings" button.
        clickButtonAndVerify(mFirstButtonClickListenerCaptor.getValue(), ClickInfo.OPEN_SETTINGS);
        verify(mOpenSettingsRunnable).run();
    }

    @Test
    @SmallTest
    public void testShowModule_DefaultOptIn_SecondButton() {
        when(mHooks.isSettingDefaultEnabledByOs()).thenReturn(true);
        assertTrue(mFactory.isSettingDefaultEnabledByOs());

        createMediator();
        inflateViewAndVerify(/* isDefaultOptIn= */ true);

        mMediator.showModule();
        verifyShowModuleComplete();

        // Verifies the case of clicking the "Got it" button.
        clickButtonAndVerify(mSecondButtonClickListenerCaptor.getValue(), ClickInfo.OPT_IN);

        AuxiliarySearchUtils.resetSharedPreferenceForTesting();
    }

    @Test
    @SmallTest
    public void testShowModule_DefaultOptOut_FirstButton() {
        when(mHooks.isSettingDefaultEnabledByOs()).thenReturn(false);
        assertFalse(mFactory.isSettingDefaultEnabledByOs());

        createMediator();
        inflateViewAndVerify(/* isDefaultOptIn= */ false);

        mMediator.showModule();
        verifyShowModuleComplete();

        // Verifies the case of clicking the "No thanks" button.
        clickButtonAndVerify(mFirstButtonClickListenerCaptor.getValue(), ClickInfo.OPT_OUT);
    }

    @Test
    @SmallTest
    public void testShowModule_DefaultOptOut_SecondButton() {
        when(mHooks.isSettingDefaultEnabledByOs()).thenReturn(false);
        assertFalse(mFactory.isSettingDefaultEnabledByOs());

        createMediator();
        inflateViewAndVerify(/* isDefaultOptIn= */ false);
        ShareTabsWithOsStateListener listener = Mockito.mock(ShareTabsWithOsStateListener.class);
        AuxiliarySearchConfigManager.getInstance().addListener(listener);

        mMediator.showModule();
        verifyShowModuleComplete();

        // Verifies the case of clicking the "Turn on" button.
        clickButtonAndVerify(mSecondButtonClickListenerCaptor.getValue(), ClickInfo.TURN_ON);
        verify(listener).onConfigChanged(eq(true));

        AuxiliarySearchUtils.resetSharedPreferenceForTesting();
        AuxiliarySearchConfigManager.getInstance().removeListener(listener);
    }

    @Test
    @SmallTest
    public void testShowModule() {
        createMediator();

        mMediator.showModule();
        verify(mModuleDelegate).onDataReady(eq(ModuleType.AUXILIARY_SEARCH), eq(mPropertyModel));

        mMediator.showModule();
        // Verifies that the module won't show again if it is currently showing on the magic stack.
        verify(mModuleDelegate).onDataReady(eq(ModuleType.AUXILIARY_SEARCH), eq(mPropertyModel));

        mMediator.hideModule();
        mMediator.showModule();
        // Verifies the module will show again after hiding.
        verify(mModuleDelegate, times(2))
                .onDataReady(eq(ModuleType.AUXILIARY_SEARCH), eq(mPropertyModel));
    }

    @Test
    @SmallTest
    public void testGetModuleType() {
        createMediator();
        assertEquals(ModuleType.AUXILIARY_SEARCH, mMediator.getModuleType());
    }

    @Test
    @SmallTest
    public void testHideModule() {
        createMediator();
        mMediator.hideModule();

        verify(mPropertyModel)
                .set(
                        eq(AuxiliarySearchModuleProperties.MODULE_FIRST_BUTTON_ON_CLICK_LISTENER),
                        eq(null));
        verify(mPropertyModel)
                .set(
                        eq(AuxiliarySearchModuleProperties.MODULE_SECOND_BUTTON_ON_CLICK_LISTENER),
                        eq(null));
    }

    private void createMediator() {
        mMediator =
                new AuxiliarySearchModuleMediator(
                        mPropertyModel, mModuleDelegate, mOpenSettingsRunnable);
    }

    private void inflateViewAndVerify(boolean isDefaultOptIn) {
        mView =
                LayoutInflater.from(mContext)
                        .inflate(
                                org.chromium.chrome.browser.auxiliary_search.R.layout
                                        .auxiliary_search_module_layout,
                                null);

        int contentTextResId;
        int firstButtonTextResId;
        int secondButtonTextResId;

        if (isDefaultOptIn) {
            contentTextResId = R.string.auxiliary_search_module_content;
            firstButtonTextResId = R.string.auxiliary_search_module_button_go_to_settings;
            secondButtonTextResId = R.string.auxiliary_search_module_button_got_it;
        } else {
            contentTextResId = R.string.auxiliary_search_module_content_default_off;
            firstButtonTextResId = R.string.auxiliary_search_module_button_no_thanks;
            secondButtonTextResId = R.string.auxiliary_search_module_button_turn_on;
        }

        verify(mPropertyModel)
                .set(
                        eq(AuxiliarySearchModuleProperties.MODULE_CONTENT_TEXT_RES_ID),
                        eq(contentTextResId));
        verify(mPropertyModel)
                .set(
                        eq(AuxiliarySearchModuleProperties.MODULE_FIRST_BUTTON_TEXT_RES_ID),
                        eq(firstButtonTextResId));
        verify(mPropertyModel)
                .set(
                        eq(AuxiliarySearchModuleProperties.MODULE_SECOND_BUTTON_TEXT_RES_ID),
                        eq(secondButtonTextResId));
    }

    private void verifyShowModuleComplete() {
        verify(mPropertyModel)
                .set(
                        eq(AuxiliarySearchModuleProperties.MODULE_FIRST_BUTTON_ON_CLICK_LISTENER),
                        mFirstButtonClickListenerCaptor.capture());
        verify(mPropertyModel)
                .set(
                        eq(AuxiliarySearchModuleProperties.MODULE_SECOND_BUTTON_ON_CLICK_LISTENER),
                        mSecondButtonClickListenerCaptor.capture());
        verify(mModuleDelegate).onDataReady(eq(ModuleType.AUXILIARY_SEARCH), eq(mPropertyModel));
    }

    private void clickButtonAndVerify(OnClickListener clickListener, @ClickInfo int clickInfo) {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Search.AuxiliarySearch.Module.ClickInfo", clickInfo)
                        .build();
        clickListener.onClick(mView);

        verify(mModuleDelegate).onModuleClicked(eq(ModuleType.AUXILIARY_SEARCH));
        verify(mModuleDelegate).removeModule(eq(ModuleType.AUXILIARY_SEARCH));
        histogramWatcher.assertExpected();

        SharedPreferencesManager prefManager = ChromeSharedPreferences.getInstance();
        assertTrue(
                prefManager.readBoolean(
                        ChromePreferenceKeys.AUXILIARY_SEARCH_MODULE_USER_RESPONDED, false));
    }
}
