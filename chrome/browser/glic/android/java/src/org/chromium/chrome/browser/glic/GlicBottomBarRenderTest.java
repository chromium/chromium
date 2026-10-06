// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
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
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlobalShowHideObserver;
import org.chromium.chrome.browser.ui.bottombar.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.RenderTestRule;

/** Render tests for the Glic entrypoint button in the Android Bottom Bar. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR + ":bypass_glic_geofencing/true"})
@DisableFeatures({ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL})
@DisableIf.Device(DeviceFormFactor.TABLET_OR_DESKTOP)
public class GlicBottomBarRenderTest {
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

    @Before
    public void setUp() {
        GlicEnabling.setEnabledForTesting(true, /* forwardToNative= */ true);
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);
        mActivityTestRule.startOnBlankPage();

        CriteriaHelper.pollUiThread(
                () -> {
                    ChromeTabbedActivity activity = mActivityTestRule.getActivity();
                    View bottomBarView = activity.findViewById(R.id.bottom_bar_container);
                    Criteria.checkThat(bottomBarView, Matchers.notNullValue());
                    Criteria.checkThat(bottomBarView.isShown(), Matchers.is(true));
                    View glicButton = bottomBarView.findViewById(R.id.extra_button);
                    Criteria.checkThat(glicButton, Matchers.notNullValue());
                    Criteria.checkThat(glicButton.isShown(), Matchers.is(true));
                });
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
    public void testGlicBottomBarButton_Default() throws Exception {
        renderWithPanelOpen(/* panelOpen= */ false, "glic_bottom_bar_button_default");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testGlicBottomBarButton_PanelOpen() throws Exception {
        renderWithPanelOpen(/* panelOpen= */ true, "glic_bottom_bar_button_panel_open");
    }

    /** Updates the Glic panel state, notifies observers, and renders the bottom bar. */
    private void renderWithPanelOpen(boolean panelOpen, String goldenId) throws Exception {
        ArgumentCaptor<GlobalShowHideObserver> observerCaptor =
                ArgumentCaptor.forClass(GlobalShowHideObserver.class);
        verify(mGlicKeyedService, atLeastOnce())
                .addGlobalShowHideObserver(observerCaptor.capture());

        GlicButtonStateController.setPanelOpenForTesting(panelOpen);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (GlobalShowHideObserver observer : observerCaptor.getAllValues()) {
                        observer.onGlobalShowHide();
                    }
                });

        View bottomBarView =
                mActivityTestRule.getActivity().findViewById(R.id.bottom_bar_container);
        mRenderTestRule.render(bottomBarView, goldenId);
    }
}
