// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search.module;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;

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

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchHooks;
import org.chromium.chrome.browser.auxiliary_search.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;

/** Unit tests for {@link AuxiliarySearchModuleCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AuxiliarySearchModuleCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private Runnable mOpenSettingsRunnable;
    @Mock private AuxiliarySearchHooks mHooks;

    private Context mContext;
    private AuxiliarySearchModuleCoordinator mCoordinator;

    @Before
    public void setup() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mHooks = Mockito.mock(AuxiliarySearchHooks.class);
        ServiceLoaderUtil.setInstanceForTesting(AuxiliarySearchHooks.class, mHooks);

        mCoordinator = new AuxiliarySearchModuleCoordinator(mModuleDelegate, mOpenSettingsRunnable);
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.AUXILIARY_SEARCH_HISTORY_DONATION})
    public void testGetModuleContextMenuHideText_Default() {
        assertEquals(
                mContext.getString(R.string.auxiliary_search_module_context_menu_hide),
                mCoordinator.getModuleContextMenuHideText(mContext));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.AUXILIARY_SEARCH_HISTORY_DONATION})
    public void testGetModuleContextMenuHideText_BrowsingDataDonation() {
        when(mHooks.isBrowsingDataDonationSupported()).thenReturn(true);
        assertEquals(
                mContext.getString(
                        R.string.auxiliary_search_browsing_data_module_context_menu_hide),
                mCoordinator.getModuleContextMenuHideText(mContext));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.AUXILIARY_SEARCH_HISTORY_DONATION})
    public void testGetModuleContextMenuHideText_BrowsingDataDonationNotSupported() {
        when(mHooks.isBrowsingDataDonationSupported()).thenReturn(false);
        assertEquals(
                mContext.getString(R.string.auxiliary_search_module_context_menu_hide),
                mCoordinator.getModuleContextMenuHideText(mContext));
    }
}
