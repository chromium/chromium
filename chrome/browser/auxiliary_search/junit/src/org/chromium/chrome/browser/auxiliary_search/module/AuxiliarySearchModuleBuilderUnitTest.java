// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search.module;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchControllerFactory;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchHooks;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchUtils;
import org.chromium.chrome.browser.auxiliary_search.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.segmentation_platform.InputContext;

/** Unit tests for {@link AuxiliarySearchModuleBuilder}. */
@EnableFeatures({ChromeFeatureList.ANDROID_APP_INTEGRATION_MODULE})
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AuxiliarySearchModuleBuilderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AuxiliarySearchHooks mHooks;
    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private Callback<ModuleProvider> mOnModuleBuiltCallback;
    private ViewGroup mParentView;
    @Mock private Runnable mOpenSettingsRunnable;

    private Context mContext;
    private AuxiliarySearchControllerFactory mFactory;
    private AuxiliarySearchModuleBuilder mBuilder;

    @Before
    public void setup() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mParentView =
                (ViewGroup)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.auxiliary_search_module_layout, null);

        mFactory = AuxiliarySearchControllerFactory.getInstance();
        mHooks = Mockito.mock(AuxiliarySearchHooks.class);
        when(mHooks.isEnabled()).thenReturn(true);
        when(mHooks.isSettingDefaultEnabledByOs()).thenReturn(true);
        mFactory.setHooksForTesting(mHooks);
        assertTrue(mFactory.isEnabled());
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.AUXILIARY_SEARCH_CONSUMER_SCHEMA_FOUND, true);

        mBuilder = new AuxiliarySearchModuleBuilder(mContext, mOpenSettingsRunnable);
    }

    @Test
    @SmallTest
    public void testIsEnabled() {
        assertTrue(mFactory.isEnabled());
        assertTrue(mBuilder.isEligible());

        when(mHooks.isEnabled()).thenReturn(false);
        assertFalse(mFactory.isEnabled());
        assertFalse(mBuilder.isEligible());

        // Verifies that if the device doesn't provide a consumer schema, we no longer build the
        // opt in card module.
        when(mHooks.isEnabled()).thenReturn(true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.AUXILIARY_SEARCH_CONSUMER_SCHEMA_FOUND, false);
        assertTrue(mFactory.isEnabled());
        assertFalse(mBuilder.isEligible());
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.ANDROID_APP_INTEGRATION_MODULE})
    public void testIsDisabled() {
        assertFalse(ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_APP_INTEGRATION_MODULE));

        assertTrue(mFactory.isEnabled());
        assertFalse(mBuilder.isEligible());
    }

    @Test
    @SmallTest
    public void testBuild() {
        assertTrue(mBuilder.build(mModuleDelegate, mOnModuleBuiltCallback));
        verify(mOnModuleBuiltCallback).onResult(any(AuxiliarySearchModuleCoordinator.class));

        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        assertEquals(
                0,
                prefsManager.readInt(ChromePreferenceKeys.AUXILIARY_SEARCH_MODULE_IMPRESSION, 0));

        // Verifies that the impression count increases after the view is created.
        mBuilder.createView(mParentView);
        assertEquals(
                1,
                prefsManager.readInt(ChromePreferenceKeys.AUXILIARY_SEARCH_MODULE_IMPRESSION, 0));

        // Verifies that calling build() will return false after the module has been shown.
        assertFalse(mBuilder.build(mModuleDelegate, mOnModuleBuiltCallback));

        AuxiliarySearchUtils.resetSharedPreferenceForTesting();
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.ANDROID_APP_INTEGRATION_MODULE})
    public void testCreateInputContext() {
        InputContext inputContext = mBuilder.createInputContext();
        assertEquals(0f, inputContext.getEntryValue("auxiliary_search_available").floatValue, 0.01);
    }

    @Test
    @SmallTest
    public void testCreateInputContext_Enabled() {
        AuxiliarySearchModuleBuilder.resetShownInThisSessionForTesting();
        InputContext inputContext = mBuilder.createInputContext();
        assertEquals(1f, inputContext.getEntryValue("auxiliary_search_available").floatValue, 0.01);
    }
}
