// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_DARK;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT;
import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_INITIAL_ACTIVITY_HEIGHT_PX;

import android.content.Intent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabBaseStrategy;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabDisplayManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/**
 * Render tests for Partial Custom Tab modes. TODO(crbug.com/507474541): Add render tests for
 * sidesheet version.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "This test tests activity start up and resizing behavior.")
public class PartialCustomTabActivityRenderTest {
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParameter =
            Arrays.asList(
                    new ParameterSet().value(true).name("NightModeEnabled"),
                    new ParameterSet().value(false).name("NightModeDisabled"));

    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final int PORT_NO = 31415;

    private final boolean mNightModeEnabled;
    private String mUrl;

    @Rule
    public final CustomTabActivityTestRule mCustomTabActivityTestRule =
            new CustomTabActivityTestRule();

    @Rule
    public final EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE_CUSTOM_TABS)
                    .build();

    public PartialCustomTabActivityRenderTest(boolean nightModeEnabled) {
        mNightModeEnabled = nightModeEnabled;
    }

    @Before
    public void setUp() {
        mEmbeddedTestServerRule.setServerPort(PORT_NO);
        mUrl = mEmbeddedTestServerRule.getServer().getURL(TEST_PAGE);
    }

    private Intent createBottomSheetIntent() {
        Intent intent =
                CustomTabsIntentTestUtils.createCustomTabIntent(
                        ApplicationProvider.getApplicationContext(),
                        mUrl,
                        /* launchAsNewTask= */ true,
                        builder ->
                                builder.setColorScheme(
                                        mNightModeEnabled
                                                ? COLOR_SCHEME_DARK
                                                : COLOR_SCHEME_LIGHT));
        var token = SessionHolder.getSessionHolderFromIntent(intent);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.newSession(token.getSessionAsCustomTab());
        connection.overridePackageNameForSessionForTesting(token, "org.chromium.testapp");
        intent.putExtra(EXTRA_INITIAL_ACTIVITY_HEIGHT_PX, 800);
        return intent;
    }

    private void waitForSpinnerToDismiss() {
        CriteriaHelper.pollUiThread(
                () -> {
                    ViewGroup coordinator =
                            mCustomTabActivityTestRule.getActivity().findViewById(R.id.coordinator);
                    for (int i = 0; i < coordinator.getChildCount(); i++) {
                        View child = coordinator.getChildAt(i);
                        if (child instanceof ImageView && child.getId() == View.NO_ID) {
                            if (child.getVisibility() == View.VISIBLE) {
                                throw new CriteriaNotSatisfiedException("Spinner is still visible");
                            }
                        }
                    }
                });
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.CCT_RESIZABLE_FOR_THIRD_PARTIES})
    @DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE)
    public void testPartialCustomTabBottomSheet() throws IOException {
        Intent intent = createBottomSheetIntent();
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        // Wait until spinner is gone and size strategy is stable.
        waitForSpinnerToDismiss();

        BaseCustomTabRootUiCoordinator coordinator =
                (BaseCustomTabRootUiCoordinator)
                        mCustomTabActivityTestRule.getActivity().getRootUiCoordinatorForTesting();
        PartialCustomTabDisplayManager displayManager =
                (PartialCustomTabDisplayManager) coordinator.getCustomTabSizeStrategyForTesting();

        CriteriaHelper.pollUiThread(
                () -> {
                    if (displayManager.getActiveStrategyType()
                            != PartialCustomTabBaseStrategy.PartialCustomTabType.BOTTOM_SHEET) {
                        throw new CriteriaNotSatisfiedException("Not bottom sheet strategy yet");
                    }
                });

        View decorView = mCustomTabActivityTestRule.getActivity().getWindow().getDecorView();
        String nightStr = mNightModeEnabled ? "night" : "day";
        mRenderTestRule.render(decorView, "partial_cct_bottom_sheet_" + nightStr);
    }
}
