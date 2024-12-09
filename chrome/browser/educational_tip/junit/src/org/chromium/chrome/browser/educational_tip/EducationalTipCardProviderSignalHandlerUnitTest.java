// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider.EducationalTipCardType;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.segmentation_platform.InputContext;

/** Unit tests for {@link EducationalTipCardProviderSignalHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EducationalTipCardProviderSignalHandlerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private EducationTipModuleActionDelegate mActionDelegate;
    @Mock private DefaultBrowserPromoUtils mMockDefaultBrowserPromoUtils;
    @Mock private Tracker mTracker;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabGroupModelFilter mNormalFilter;
    @Mock private TabGroupModelFilter mIncognitoFilter;
    @Mock private TabModel mNormalModel;
    @Mock private TabModel mIncognitoModel;
    @Mock private TabGroupModelFilterProvider mProvider;

    private EducationalTipCardProviderSignalHandler mEducationalTipCardProviderSignalHandler;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mEducationalTipCardProviderSignalHandler = new EducationalTipCardProviderSignalHandler();
        when(mActionDelegate.getContext()).thenReturn(mContext);
        when(mActionDelegate.getTabModelSelector()).thenReturn(mTabModelSelector);
        when(mTabModelSelector.getTabGroupModelFilterProvider()).thenReturn(mProvider);
        when(mProvider.getTabGroupModelFilter(/* isIncognito= */ false)).thenReturn(mNormalFilter);
        when(mProvider.getTabGroupModelFilter(/* isIncognito= */ true))
                .thenReturn(mIncognitoFilter);
        when(mTabModelSelector.getModel(/* incognito= */ false)).thenReturn(mNormalModel);
        when(mTabModelSelector.getModel(/* incognito= */ true)).thenReturn(mIncognitoModel);
        DefaultBrowserPromoUtils.setInstanceForTesting(mMockDefaultBrowserPromoUtils);
        TrackerFactory.setTrackerForTests(mTracker);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testCreateInputContext() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());

        InputContext inputContext =
                mEducationalTipCardProviderSignalHandler.createInputContext(
                        mActionDelegate, mTracker);
        assertEquals(4, inputContext.getSizeForTesting());

        // Test signal "should_show_non_role_manager_default_browser_promo".
        when(mMockDefaultBrowserPromoUtils.shouldShowNonRoleManagerPromo(mContext))
                .thenReturn(true);
        inputContext =
                mEducationalTipCardProviderSignalHandler.createInputContext(
                        mActionDelegate, mTracker);
        assertEquals(
                1,
                inputContext.getEntryForTesting(
                                "should_show_non_role_manager_default_browser_promo")
                        .floatValue,
                0.01);

        when(mMockDefaultBrowserPromoUtils.shouldShowNonRoleManagerPromo(mContext))
                .thenReturn(false);
        inputContext =
                mEducationalTipCardProviderSignalHandler.createInputContext(
                        mActionDelegate, mTracker);
        assertEquals(
                0,
                inputContext.getEntryForTesting(
                                "should_show_non_role_manager_default_browser_promo")
                        .floatValue,
                0.01);

        // Test signal "has_default_browser_promo_shown_in_other_surface".
        when(mTracker.wouldTriggerHelpUi(FeatureConstants.DEFAULT_BROWSER_PROMO_MAGIC_STACK))
                .thenReturn(true);
        inputContext =
                mEducationalTipCardProviderSignalHandler.createInputContext(
                        mActionDelegate, mTracker);
        assertEquals(
                0,
                inputContext.getEntryForTesting("has_default_browser_promo_shown_in_other_surface")
                        .floatValue,
                0.01);

        when(mTracker.wouldTriggerHelpUi(FeatureConstants.DEFAULT_BROWSER_PROMO_MAGIC_STACK))
                .thenReturn(false);
        inputContext =
                mEducationalTipCardProviderSignalHandler.createInputContext(
                        mActionDelegate, mTracker);
        assertEquals(
                1,
                inputContext.getEntryForTesting("has_default_browser_promo_shown_in_other_surface")
                        .floatValue,
                0.01);

        // Test signal "tab_group_exists".
        when(mNormalFilter.getTabGroupCount()).thenReturn(0);
        when(mIncognitoFilter.getTabGroupCount()).thenReturn(0);
        inputContext =
                mEducationalTipCardProviderSignalHandler.createInputContext(
                        mActionDelegate, mTracker);
        assertEquals(0, inputContext.getEntryForTesting("tab_group_exists").floatValue, 0.01);

        when(mNormalFilter.getTabGroupCount()).thenReturn(5);
        when(mIncognitoFilter.getTabGroupCount()).thenReturn(6);
        inputContext =
                mEducationalTipCardProviderSignalHandler.createInputContext(
                        mActionDelegate, mTracker);
        assertEquals(1, inputContext.getEntryForTesting("tab_group_exists").floatValue, 0.01);

        // Test signal "number_of_tabs".
        when(mNormalModel.getCount()).thenReturn(0);
        when(mIncognitoModel.getCount()).thenReturn(0);
        inputContext =
                mEducationalTipCardProviderSignalHandler.createInputContext(
                        mActionDelegate, mTracker);
        assertEquals(0, inputContext.getEntryForTesting("number_of_tabs").floatValue, 0.01);

        when(mNormalModel.getCount()).thenReturn(5);
        when(mIncognitoModel.getCount()).thenReturn(0);
        inputContext =
                mEducationalTipCardProviderSignalHandler.createInputContext(
                        mActionDelegate, mTracker);
        assertEquals(5, inputContext.getEntryForTesting("number_of_tabs").floatValue, 0.01);

        when(mNormalModel.getCount()).thenReturn(0);
        when(mIncognitoModel.getCount()).thenReturn(10);
        inputContext =
                mEducationalTipCardProviderSignalHandler.createInputContext(
                        mActionDelegate, mTracker);
        assertEquals(10, inputContext.getEntryForTesting("number_of_tabs").floatValue, 0.01);

        when(mNormalModel.getCount()).thenReturn(10);
        when(mIncognitoModel.getCount()).thenReturn(10);
        inputContext =
                mEducationalTipCardProviderSignalHandler.createInputContext(
                        mActionDelegate, mTracker);
        assertEquals(20, inputContext.getEntryForTesting("number_of_tabs").floatValue, 0.01);
    }

    @Test
    @SmallTest
    public void testShouldNotifyCardShownPerSession() {
        for (int cardType = 1; cardType < EducationalTipCardType.NUM_ENTRIES; cardType++) {
            assertTrue(
                    mEducationalTipCardProviderSignalHandler.shouldNotifyCardShownPerSession(
                            cardType));

            assertFalse(
                    mEducationalTipCardProviderSignalHandler.shouldNotifyCardShownPerSession(
                            cardType));
            assertFalse(
                    mEducationalTipCardProviderSignalHandler.shouldNotifyCardShownPerSession(
                            cardType));
        }
    }
}
