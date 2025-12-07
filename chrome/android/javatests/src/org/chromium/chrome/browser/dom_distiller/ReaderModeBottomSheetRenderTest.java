// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.dom_distiller.mojom.Theme;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;
import java.util.List;

/** Render tests for the Reader Mode bottom sheet. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ReaderModeBottomSheetRenderTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_READER_MODE)
                    .setRevision(6)
                    .setDescription("Updated capitalization of bottomsheet title")
                    .build();

    private @Captor ArgumentCaptor<ThemeColorProvider.ThemeColorObserver> mThemeColorObserverCaptor;
    private @Captor ArgumentCaptor<ThemeColorProvider.TintObserver> mTintObserverCaptor;
    private @Mock ThemeColorProvider mThemeColorProvider;

    private final boolean mNightModeEnabled;
    private ReaderModeBottomSheetCoordinator mCoordinator;
    private FrameLayout mContentView;
    private View mView;

    public ReaderModeBottomSheetRenderTest(boolean nightModeEnabled) {
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mNightModeEnabled = nightModeEnabled;
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startOnBlankPage();
        mActivityTestRule.waitForActivityCompletelyLoaded();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContentView = new FrameLayout(mActivityTestRule.getActivity());
                    mContentView.setBackgroundColor(Color.WHITE);

                    FrameLayout.LayoutParams params =
                            new FrameLayout.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.WRAP_CONTENT);
                    mActivityTestRule.getActivity().setContentView(mContentView, params);
                    mCoordinator =
                            new ReaderModeBottomSheetCoordinator(
                                    mActivityTestRule.getActivity(),
                                    mActivityTestRule.getProfile(/* incognito= */ false),
                                    mActivityTestRule
                                            .getActivity()
                                            .getRootUiCoordinatorForTesting()
                                            .getBottomSheetController(),
                                    mThemeColorProvider);
                    mCoordinator.setTab(mActivityTestRule.getActivity().getActivityTabProvider().get());
                    mView = mCoordinator.getViewForTesting();
                    mContentView.addView(mView);

                    verify(mThemeColorProvider)
                            .addThemeColorObserver(mThemeColorObserverCaptor.capture());
                    verify(mThemeColorProvider).addTintObserver(mTintObserverCaptor.capture());
                });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testLightTheme() throws IOException {
        updateThemeColor(Theme.LIGHT);
        mRenderTestRule.render(mContentView, "light_theme");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testSepiaTheme() throws IOException {
        updateThemeColor(Theme.SEPIA);
        mRenderTestRule.render(mContentView, "sepia_theme");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testDarkTheme() throws IOException {
        updateThemeColor(Theme.DARK);
        mRenderTestRule.render(mContentView, "dark_theme");
    }

    public void updateThemeColor(@Theme.EnumType int theme) {
        when(mThemeColorProvider.getThemeColor()).thenReturn(getThemeColor(theme));
        when(mThemeColorProvider.getBrandedColorScheme()).thenReturn(getBrandedColorScheme(theme));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mThemeColorObserverCaptor
                            .getValue()
                            .onThemeColorChanged(getThemeColor(theme), false);
                    mTintObserverCaptor
                            .getValue()
                            .onTintChanged(null, null, getBrandedColorScheme(theme));
                });
    }

    public @ColorInt int getThemeColor(@Theme.EnumType int theme) {
        if (mNightModeEnabled) {
            return Color.parseColor("#1A1A1A");
        }
        switch (theme) {
            case Theme.LIGHT:
                return Color.parseColor("#F5F5F5");
            case Theme.SEPIA:
                return Color.parseColor("#BF9A73");
            case Theme.DARK:
                return Color.parseColor("#1A1A1A");
            default:
                throw new AssertionError();
        }
    }

    public @BrandedColorScheme int getBrandedColorScheme(@Theme.EnumType int theme) {
        if (mNightModeEnabled) {
            return BrandedColorScheme.DARK_BRANDED_THEME;
        }
        switch (theme) {
            case Theme.LIGHT:
                return BrandedColorScheme.LIGHT_BRANDED_THEME;
            case Theme.SEPIA:
                return BrandedColorScheme.LIGHT_BRANDED_THEME;
            case Theme.DARK:
                return BrandedColorScheme.DARK_BRANDED_THEME;
            default:
                throw new AssertionError();
        }
    }
}
