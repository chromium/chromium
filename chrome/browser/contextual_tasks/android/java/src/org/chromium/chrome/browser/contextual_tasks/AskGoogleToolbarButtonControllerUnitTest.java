// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.graphics.drawable.ColorDrawable;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.ui.lens.LensOverlayInvocationSource;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for {@link AskGoogleToolbarButtonController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({
    ChromeFeatureList.CONTEXTUAL_TASKS_ENTRY_POINTS,
    ChromeFeatureList.CONTEXTUAL_TASKS_SIDE_PANEL
})
public class AskGoogleToolbarButtonControllerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private ContextualTasksUtils.Natives mUtilsJni;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;

    private Tab mActiveTab;
    private AskGoogleToolbarButtonController mController;

    @Before
    public void setUp() {
        DeviceInfo.setIsDesktopForTesting(true);
        ContextualTasksUtilsJni.setInstanceForTesting(mUtilsJni);
        mActiveTab = mTab;
        mController =
                new AskGoogleToolbarButtonController(
                        () -> mActiveTab, new ColorDrawable(), "Ask Google about this page");
    }

    private void setTabEligible(boolean eligible) {
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mUtilsJni.shouldShowAskGoogleEntryPoints(mWebContents)).thenReturn(eligible);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testVariant() {
        setTabEligible(true);
        assertEquals(
                AdaptiveToolbarButtonVariant.ASK_GOOGLE,
                mController.get(mTab).getButtonSpec().getButtonVariant());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CONTEXTUAL_TASKS_ENTRY_POINTS)
    public void testShouldShowButton_FlagDisabled() {
        assertFalse(mController.shouldShowButton(mTab));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CONTEXTUAL_TASKS_ENTRY_POINTS + ":toolbar_button/false")
    public void testShouldShowButton_ParamDisabled() {
        assertFalse(mController.shouldShowButton(mTab));
    }

    @Test
    public void testShouldShowButton_NullOrIneligibleTab() {
        assertFalse(mController.shouldShowButton(null));

        setTabEligible(false);
        assertFalse(mController.shouldShowButton(mTab));
    }

    @Test
    public void testShouldShowButton_Eligible() {
        setTabEligible(true);
        assertTrue(mController.shouldShowButton(mTab));
    }

    @Test
    public void testOnClick_PinnedInvocationSource() {
        when(mTab.getWebContents()).thenReturn(mWebContents);
        mController.onClick(new View(ContextUtils.getApplicationContext()));
        verify(mUtilsJni)
                .toggleAskGoogleSidePanel(
                        mWebContents, LensOverlayInvocationSource.COBROWSE_PINNED_TOOLBAR_BUTTON);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CONTEXTUAL_TASKS_ENTRY_POINTS + ":force_toolbar_button/true")
    public void testOnClick_ForcedInvocationSource() {
        when(mTab.getWebContents()).thenReturn(mWebContents);
        mController.onClick(new View(ContextUtils.getApplicationContext()));
        verify(mUtilsJni)
                .toggleAskGoogleSidePanel(
                        mWebContents, LensOverlayInvocationSource.COBROWSE_TOOLBAR_BUTTON);
    }

    @Test
    public void testOnClick_NullTabIsNoOp() {
        mActiveTab = null;
        mController.onClick(null);
        verifyNoInteractions(mUtilsJni);
    }
}
