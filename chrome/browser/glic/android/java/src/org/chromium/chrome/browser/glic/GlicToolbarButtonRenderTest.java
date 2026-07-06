// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.mockito.Mockito.verify;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.RenderTestRule;

/** Render tests for Glic entrypoint button on toolbar. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures({
    ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL,
    ChromeFeatureList.ANDROID_BOTTOM_BAR
})
public class GlicToolbarButtonRenderTest {
    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_GLIC)
                    .setRevision(0)
                    .build();

    @Rule
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public final MockitoRule mMocks = MockitoJUnit.rule();

    @Mock private GlicKeyedService mGlicKeyedService;

    private WebPageStation mPage;

    @Before
    public void setUp() {
        GlicEnabling.setEnabledForTesting(true, /* forwardToNative= */ true);
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);
        AdaptiveToolbarStatePredictor.setToolbarStateForTesting(AdaptiveToolbarButtonVariant.GLIC);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeSharedPreferences.getInstance()
                            .writeBoolean(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED, true);
                });
        mPage = mActivityTestRule.startOnBlankPage();
    }

    @After
    public void tearDown() {
        GlicEnabling.setEnabledForTesting(false);
        GlicKeyedServiceFactory.setForTesting(null);
        GlicButtonStateController.setPanelOpenForTesting(null);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @DisableIf.Device(DeviceFormFactor.TABLET_OR_DESKTOP) // crbug.com/530605872
    public void testGlicButton_PanelOpen() throws Exception {
        // Capture observer
        ArgumentCaptor<GlicKeyedService.GlobalShowHideObserver> observerCaptor =
                ArgumentCaptor.forClass(GlicKeyedService.GlobalShowHideObserver.class);
        verify(mGlicKeyedService).addGlobalShowHideObserver(observerCaptor.capture());
        GlicKeyedService.GlobalShowHideObserver observer = observerCaptor.getValue();

        // Mock panel is open
        GlicButtonStateController.setPanelOpenForTesting(true);

        ThreadUtils.runOnUiThreadBlocking(() -> observer.onGlobalShowHide());

        ChromeTabbedActivity activity = mActivityTestRule.getActivity();

        // Force recompute the adaptive toolbar state so it reads our mocked state
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    activity.getRootUiCoordinatorForTesting()
                            .getAdaptiveToolbarUiCoordinatorForTesting()
                            .getAdaptiveToolbarButtonControllerForTesting()
                            .recomputeUiState();
                });

        View toolbarView = activity.findViewById(R.id.toolbar);
        mRenderTestRule.render(toolbarView, "glic_toolbar_button_panel_open");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testGlicButton_Default() throws Exception {
        // Mock panel is closed
        GlicButtonStateController.setPanelOpenForTesting(false);

        ChromeTabbedActivity activity = mActivityTestRule.getActivity();

        // Force recompute the adaptive toolbar state
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    activity.getRootUiCoordinatorForTesting()
                            .getAdaptiveToolbarUiCoordinatorForTesting()
                            .getAdaptiveToolbarButtonControllerForTesting()
                            .recomputeUiState();
                });

        View toolbarView = activity.findViewById(R.id.toolbar);
        mRenderTestRule.render(toolbarView, "glic_toolbar_button_default");
    }
}
