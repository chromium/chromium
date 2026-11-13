// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.DeviceInfo;
import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.lens.LensOverlayInvocationSource;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for {@link ContextualTasksUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ContextualTasksUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private ContextualTasksUtils.Natives mNatives;

    @Test
    public void testIsContextualTasksUiEnabled_preNative_defaultDisabled() {
        DeviceInfo.setIsDesktopForTesting(true);
        assertFalse(FeatureList.isNativeInitialized());
        assertFalse(ContextualTasksUtils.isContextualTasksUiEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CONTEXTUAL_TASKS_SIDE_PANEL)
    public void testIsContextualTasksUiEnabled_preNative_sidePanelEnabled() {
        DeviceInfo.setIsDesktopForTesting(true);
        assertFalse(FeatureList.isNativeInitialized());
        assertTrue(ContextualTasksUtils.isContextualTasksUiEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CONTEXTUAL_TASKS_SIDE_PANEL)
    public void testIsContextualTasksUiEnabled_preNative_sidePanelEnabled_notDesktop() {
        DeviceInfo.setIsDesktopForTesting(false);
        assertFalse(FeatureList.isNativeInitialized());
        assertFalse(ContextualTasksUtils.isContextualTasksUiEnabled());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CONTEXTUAL_TASKS_SIDE_PANEL)
    public void testIsContextualTasksUiEnabled_preNative_sidePanelDisabled() {
        DeviceInfo.setIsDesktopForTesting(true);
        assertFalse(FeatureList.isNativeInitialized());
        assertFalse(ContextualTasksUtils.isContextualTasksUiEnabled());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CONTEXTUAL_TASKS_ENTRY_POINTS)
    public void testShouldShowAskGoogleEntryPoints_featureDisabled_skipsNative() {
        ContextualTasksUtilsJni.setInstanceForTesting(mNatives);
        doReturn(mWebContents).when(mTab).getWebContents();
        assertFalse(ContextualTasksUtils.shouldShowAskGoogleEntryPoints(mTab));
        verify(mNatives, never()).shouldShowAskGoogleEntryPoints(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CONTEXTUAL_TASKS_ENTRY_POINTS)
    public void testShouldShowAskGoogleEntryPoints() {
        ContextualTasksUtilsJni.setInstanceForTesting(mNatives);
        assertFalse(ContextualTasksUtils.shouldShowAskGoogleEntryPoints(null));

        doReturn(null).when(mTab).getWebContents();
        assertFalse(ContextualTasksUtils.shouldShowAskGoogleEntryPoints(mTab));

        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(true).when(mNatives).shouldShowAskGoogleEntryPoints(mWebContents);
        assertTrue(ContextualTasksUtils.shouldShowAskGoogleEntryPoints(mTab));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CONTEXTUAL_TASKS_ENTRY_POINTS + ":page_context_menu/false")
    public void testShouldShowAskGoogleEntryPoint_surfaceParamOverride() {
        ContextualTasksUtilsJni.setInstanceForTesting(mNatives);
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(true).when(mNatives).shouldShowAskGoogleEntryPoints(mWebContents);
        assertFalse(
                ContextualTasksUtils.shouldShowAskGoogleEntryPoint(
                        mTab, ChromeFeatureList.sContextualTasksEntryPointsPageContextMenu));
        assertTrue(
                ContextualTasksUtils.shouldShowAskGoogleEntryPoint(
                        mTab, ChromeFeatureList.sContextualTasksEntryPointsAppMenu));
    }

    @Test
    public void testOpenAskGoogleAboutThisPage() {
        ContextualTasksUtilsJni.setInstanceForTesting(mNatives);
        doReturn(mWebContents).when(mTab).getWebContents();

        ContextualTasksUtils.openAskGoogleAboutThisPage(
                mTab, LensOverlayInvocationSource.APP_MENU_ASK_GOOGLE);
        verify(mNatives)
                .openAskGoogleAboutThisPage(
                        mWebContents, LensOverlayInvocationSource.APP_MENU_ASK_GOOGLE);
    }

    @Test
    public void testToggleAskGoogleSidePanel() {
        ContextualTasksUtilsJni.setInstanceForTesting(mNatives);
        doReturn(mWebContents).when(mTab).getWebContents();

        ContextualTasksUtils.toggleAskGoogleSidePanel(
                mTab, LensOverlayInvocationSource.COBROWSE_TOOLBAR_BUTTON);
        verify(mNatives)
                .toggleAskGoogleSidePanel(
                        mWebContents, LensOverlayInvocationSource.COBROWSE_TOOLBAR_BUTTON);
    }
}
