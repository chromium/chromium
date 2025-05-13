// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.OPEN_IN_BROWSER_STATE_DEFAULT;
import static androidx.browser.customtabs.CustomTabsIntent.OPEN_IN_BROWSER_STATE_OFF;
import static androidx.browser.customtabs.CustomTabsIntent.OPEN_IN_BROWSER_STATE_ON;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.OPEN_IN_BROWSER;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.SHARE;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.TRANSLATE;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.UNKNOWN;

import android.content.Context;
import android.graphics.drawable.Drawable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams.ButtonType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.components.feature_engagement.Tracker;

import java.util.List;

/** Tests for {@link CustomTabAdaptiveToolbarBehavior}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.CCT_ADAPTIVE_BUTTON)
public class CustomTabAdaptiveToolbarBehaviorUnitTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Context mContext;
    @Mock private ActivityTabProvider mActivityTabProvider;
    @Mock private Runnable mOpenInBrowserRunnable;
    @Mock private Runnable mRegisterVoiceSearchRunnable;
    @Mock private Drawable mOpenInBrowserButton;
    @Mock private BrowserServicesIntentDataProvider mIntentDataProvider;

    private CustomTabAdaptiveToolbarBehavior mBehavior;
    private List<CustomButtonParams> mToolbarCustomButtons;

    @Before
    public void setUp() {
        initBehavior(List.of());
    }

    private void initBehavior(List<CustomButtonParams> customButtons) {
        when(mIntentDataProvider.getCustomButtonsOnToolbar()).thenReturn(customButtons);
        mBehavior =
                new CustomTabAdaptiveToolbarBehavior(
                        mContext,
                        mActivityTabProvider,
                        mIntentDataProvider,
                        mOpenInBrowserButton,
                        mOpenInBrowserRunnable,
                        mRegisterVoiceSearchRunnable);
    }

    @Test
    public void registerPerSurfaceButtons_voiceSearch() {
        AdaptiveToolbarButtonController controller =
                Mockito.mock(AdaptiveToolbarButtonController.class);
        Supplier<Tracker> trackerSupplier = Mockito.mock(Supplier.class);

        mBehavior.registerPerSurfaceButtons(controller, trackerSupplier);
        verify(mRegisterVoiceSearchRunnable, never()).run();

        ChromeFeatureList.sCctAdaptiveButtonEnableVoice.setForTesting(true);
        mBehavior.registerPerSurfaceButtons(controller, trackerSupplier);
        verify(mRegisterVoiceSearchRunnable).run();
    }

    @Test
    public void registerPerSurfaceButtons_openInBrowser_WhenOpenInBrowserButtonSetToDefault() {
        AdaptiveToolbarButtonController controller =
                Mockito.mock(AdaptiveToolbarButtonController.class);
        Supplier<Tracker> trackerSupplier = Mockito.mock(Supplier.class);

        mBehavior.registerPerSurfaceButtons(controller, trackerSupplier);

        ChromeFeatureList.sCctAdaptiveButtonEnableOpenInBrowser.setForTesting(true);
        when(mIntentDataProvider.getOpenInBrowserButtonState())
                .thenReturn(OPEN_IN_BROWSER_STATE_DEFAULT);
        mBehavior.registerPerSurfaceButtons(controller, trackerSupplier);
        verify(controller)
                .addButtonVariant(eq(AdaptiveToolbarButtonVariant.OPEN_IN_BROWSER), any());
    }

    @Test
    public void registerPerSurfaceButtons_DoesNotAddOpenInBrowser_WhenOpenInBrowserButtonEnabled() {
        AdaptiveToolbarButtonController controller =
                Mockito.mock(AdaptiveToolbarButtonController.class);
        Supplier<Tracker> trackerSupplier = Mockito.mock(Supplier.class);

        mBehavior.registerPerSurfaceButtons(controller, trackerSupplier);

        ChromeFeatureList.sCctAdaptiveButtonEnableOpenInBrowser.setForTesting(true);
        when(mIntentDataProvider.getOpenInBrowserButtonState())
                .thenReturn(OPEN_IN_BROWSER_STATE_ON);
        mBehavior.registerPerSurfaceButtons(controller, trackerSupplier);
        verify(controller, never())
                .addButtonVariant(eq(AdaptiveToolbarButtonVariant.OPEN_IN_BROWSER), any());
    }

    @Test
    public void
            registerPerSurfaceButtons_DoesNotAddOpenInBrowser_WhenOpenInBrowserButtonDisabled() {
        AdaptiveToolbarButtonController controller =
                Mockito.mock(AdaptiveToolbarButtonController.class);
        Supplier<Tracker> trackerSupplier = Mockito.mock(Supplier.class);

        ChromeFeatureList.sCctAdaptiveButtonEnableOpenInBrowser.setForTesting(true);
        when(mIntentDataProvider.getOpenInBrowserButtonState())
                .thenReturn(OPEN_IN_BROWSER_STATE_OFF);
        mBehavior.registerPerSurfaceButtons(controller, trackerSupplier);
        verify(controller, never())
                .addButtonVariant(eq(AdaptiveToolbarButtonVariant.OPEN_IN_BROWSER), any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ADAPTIVE_BUTTON + ":open_in_browser/true")
    public void resultFilter_avoidDuplicationWithDeveloperCustomButtons() {
        CustomButtonParams openInBrowser = Mockito.mock(CustomButtonParams.class);
        when(openInBrowser.getType()).thenReturn(ButtonType.CCT_OPEN_IN_BROWSER_BUTTON);
        CustomButtonParams share = Mockito.mock(CustomButtonParams.class);
        when(share.getType()).thenReturn(ButtonType.CCT_SHARE_BUTTON);
        List<Integer> segmentationResults = List.of(OPEN_IN_BROWSER, SHARE, TRANSLATE);

        assertEquals(OPEN_IN_BROWSER, mBehavior.resultFilter(segmentationResults));

        initBehavior(List.of(openInBrowser));
        assertEquals(SHARE, mBehavior.resultFilter(segmentationResults));

        // Verify that the segmentation results down to the 2nd one can be picked up,
        // and the 3rd one (translate) is ignored.
        initBehavior(List.of(openInBrowser, share));
        assertEquals(UNKNOWN, mBehavior.resultFilter(segmentationResults));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ADAPTIVE_BUTTON + ":open_in_browser/true")
    public void hideManuallySetButton() {
        // Initialize custom action button types.
        CustomButtonParams openInBrowser = Mockito.mock(CustomButtonParams.class);
        when(openInBrowser.getType()).thenReturn(ButtonType.CCT_OPEN_IN_BROWSER_BUTTON);
        CustomButtonParams share = Mockito.mock(CustomButtonParams.class);
        when(share.getType()).thenReturn(ButtonType.CCT_SHARE_BUTTON);

        assertTrue(mBehavior.canShowManualOverride(OPEN_IN_BROWSER));

        initBehavior(List.of(openInBrowser));
        assertTrue(mBehavior.canShowManualOverride(SHARE));
        assertFalse(mBehavior.canShowManualOverride(OPEN_IN_BROWSER));

        initBehavior(List.of(share));
        assertFalse(mBehavior.canShowManualOverride(SHARE));
        assertTrue(mBehavior.canShowManualOverride(OPEN_IN_BROWSER));
    }
}
