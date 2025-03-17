// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.OPEN_IN_BROWSER;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.SHARE;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.UNKNOWN;

import android.content.Context;
import android.graphics.drawable.Drawable;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ActivityTabProvider;
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

    @Mock private Context mContext;
    @Mock private ActivityTabProvider mActivityTabProvider;
    @Mock private Runnable mOpenInBrowserRunnable;
    @Mock private Runnable mRegisterVoiceSearchRunnable;
    @Mock private Drawable mOpenInBrowserButton;

    private CustomTabAdaptiveToolbarBehavior mBehavior;
    private List<CustomButtonParams> mToolbarCustomButtons;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        initBehavior(List.of());
    }

    private void initBehavior(List<CustomButtonParams> customButtons) {
        mBehavior =
                new CustomTabAdaptiveToolbarBehavior(
                        mContext,
                        mActivityTabProvider,
                        customButtons,
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
    public void registerPerSurfaceButtons_openInBrowser() {
        AdaptiveToolbarButtonController controller =
                Mockito.mock(AdaptiveToolbarButtonController.class);
        Supplier<Tracker> trackerSupplier = Mockito.mock(Supplier.class);

        mBehavior.registerPerSurfaceButtons(controller, trackerSupplier);
        verify(controller, never())
                .addButtonVariant(eq(AdaptiveToolbarButtonVariant.OPEN_IN_BROWSER), any());

        ChromeFeatureList.sCctAdaptiveButtonEnableOpenInBrowser.setForTesting(true);
        mBehavior.registerPerSurfaceButtons(controller, trackerSupplier);
        verify(controller)
                .addButtonVariant(eq(AdaptiveToolbarButtonVariant.OPEN_IN_BROWSER), any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ADAPTIVE_BUTTON + ":open_in_browser/true")
    public void resultFilter_avoidDuplicationWithDeveloperCustomButtons() {
        CustomButtonParams openInBrowser = Mockito.mock(CustomButtonParams.class);
        when(openInBrowser.getType()).thenReturn(ButtonType.CCT_OPEN_IN_BROWSER_BUTTON);
        CustomButtonParams share = Mockito.mock(CustomButtonParams.class);
        when(share.getType()).thenReturn(ButtonType.CCT_SHARE_BUTTON);
        List<Integer> segmentationResults = List.of(OPEN_IN_BROWSER, SHARE);

        assertEquals(OPEN_IN_BROWSER, mBehavior.resultFilter(segmentationResults));

        initBehavior(List.of(openInBrowser));
        assertEquals(SHARE, mBehavior.resultFilter(segmentationResults));

        initBehavior(List.of(openInBrowser, share));
        assertEquals(UNKNOWN, mBehavior.resultFilter(segmentationResults));
    }
}
