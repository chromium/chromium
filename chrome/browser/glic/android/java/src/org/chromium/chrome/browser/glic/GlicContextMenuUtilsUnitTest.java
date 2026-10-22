// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;

/** Unit tests for {@link GlicContextMenuUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.CLANK_GLIC_CONTEXT_MENU,
    ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL,
    ChromeFeatureList.TAB_BOTTOM_SHEET,
    ChromeFeatureList.GLIC_SHARE_IMAGE,
})
public class GlicContextMenuUtilsUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfileMock;
    @Mock private Profile mIncognitoProfileMock;
    @Mock private GlicEnabling.Natives mGlicEnablingJniMock;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        DeviceInfo.setIsDesktopForTesting(true);
        GlicEnablingJni.setInstanceForTesting(mGlicEnablingJniMock);

        when(mProfileMock.isOffTheRecord()).thenReturn(false);
        when(mIncognitoProfileMock.isOffTheRecord()).thenReturn(true);
        when(mGlicEnablingJniMock.isReadyForProfile(mProfileMock)).thenReturn(true);
        when(mGlicEnablingJniMock.isReadyForProfile(mIncognitoProfileMock)).thenReturn(false);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CLANK_GLIC_CONTEXT_MENU)
    public void testIsGlicContextMenuEligible_featureDisabled() {
        assertFalse(GlicContextMenuUtils.isGlicContextMenuEligible(mProfileMock));
    }

    @Test
    public void testIsGlicContextMenuEligible() {
        assertTrue(GlicContextMenuUtils.isGlicContextMenuEligible(mProfileMock));

        when(mGlicEnablingJniMock.isReadyForProfile(mProfileMock)).thenReturn(false);
        assertFalse(GlicContextMenuUtils.isGlicContextMenuEligible(mProfileMock));

        assertFalse(GlicContextMenuUtils.isGlicContextMenuEligible(null));
        assertFalse(GlicContextMenuUtils.isGlicContextMenuEligible(mIncognitoProfileMock));

        when(mGlicEnablingJniMock.isReadyForProfile(mProfileMock)).thenReturn(true);
        DeviceInfo.setIsAutomotiveForTesting(true);
        assertFalse(GlicContextMenuUtils.isGlicContextMenuEligible(mProfileMock));
    }

    @Test
    public void testShouldShowAskGeminiForLink_readyProfile() {
        assertTrue(GlicContextMenuUtils.shouldShowAskGeminiForLink(mProfileMock, mContext));
    }

    @Test
    public void testShouldShowAskGeminiForLink_signedOutOrIneligible() {
        when(mGlicEnablingJniMock.isReadyForProfile(mProfileMock)).thenReturn(false);
        assertFalse(GlicContextMenuUtils.shouldShowAskGeminiForLink(mProfileMock, mContext));
    }

    @Test
    public void testShouldShowAskGeminiForLink_nullOrIncognitoProfile() {
        assertFalse(GlicContextMenuUtils.shouldShowAskGeminiForLink(null, mContext));
        assertFalse(
                GlicContextMenuUtils.shouldShowAskGeminiForLink(mIncognitoProfileMock, mContext));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CLANK_GLIC_CONTEXT_MENU)
    public void testShouldShowAskGeminiForLink_featureDisabled() {
        assertFalse(GlicContextMenuUtils.shouldShowAskGeminiForLink(mProfileMock, mContext));
    }

    @Test
    public void testShouldShowAskGeminiForSelection_readyProfile() {
        assertTrue(GlicContextMenuUtils.shouldShowAskGeminiForSelection(mProfileMock, mContext));
    }

    @Test
    public void testShouldShowAskGeminiForSelection_signedOutOrIneligible() {
        when(mGlicEnablingJniMock.isReadyForProfile(mProfileMock)).thenReturn(false);
        assertFalse(GlicContextMenuUtils.shouldShowAskGeminiForSelection(mProfileMock, mContext));
    }

    @Test
    public void testShouldShowAskGeminiForSelection_nullOrIncognitoProfile() {
        assertFalse(GlicContextMenuUtils.shouldShowAskGeminiForSelection(null, mContext));
        assertFalse(
                GlicContextMenuUtils.shouldShowAskGeminiForSelection(
                        mIncognitoProfileMock, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_GLIC_CONTEXT_MENU + ":show_on_page/true")
    public void testShouldShowAskGeminiForPage_readyProfile() {
        assertTrue(GlicContextMenuUtils.shouldShowAskGeminiForPage(mProfileMock, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_GLIC_CONTEXT_MENU + ":show_on_page/true")
    public void testShouldShowAskGeminiForPage_signedOutOrIneligible() {
        when(mGlicEnablingJniMock.isReadyForProfile(mProfileMock)).thenReturn(false);
        assertFalse(GlicContextMenuUtils.shouldShowAskGeminiForPage(mProfileMock, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_GLIC_CONTEXT_MENU + ":show_on_image_mobile/true")
    public void testShouldShowAskGeminiForImage_readyProfile() {
        assertTrue(GlicContextMenuUtils.shouldShowAskGeminiForImage(mProfileMock));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_GLIC_CONTEXT_MENU + ":show_on_image_mobile/true")
    public void testShouldShowAskGeminiForImage_signedOutOrIneligible() {
        when(mGlicEnablingJniMock.isReadyForProfile(mProfileMock)).thenReturn(false);
        assertFalse(GlicContextMenuUtils.shouldShowAskGeminiForImage(mProfileMock));
    }
}
