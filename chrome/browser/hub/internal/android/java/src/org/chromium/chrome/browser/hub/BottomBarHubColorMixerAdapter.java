// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_COLOR_BLEND_ANIMATION_DURATION_MS;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.bottombar.BottomBarUtils;
import org.chromium.chrome.browser.ui.bottombar.BottomBarView;
import org.chromium.components.browser_ui.styles.IncognitoColors;

/**
 * Adapter that registers color blend animations on {@link HubColorMixer} for a {@link
 * BottomBarView} when hosted inside the Hub.
 */
@NullMarked
public class BottomBarHubColorMixerAdapter {
    private final HubColorMixerRegistrationHelper mColorMixerHelper;
    private final BottomBarView mBottomBarView;

    /**
     * Creates an adapter and registers the color blends for the given {@link BottomBarView}.
     *
     * @param bottomBarView The {@link BottomBarView} to animate.
     * @param hubColorMixer The {@link HubColorMixer} to register the blends on.
     */
    public BottomBarHubColorMixerAdapter(
            BottomBarView bottomBarView, @Nullable HubColorMixer hubColorMixer) {
        this(bottomBarView, hubColorMixer, new HubColorMixerRegistrationHelper());
    }

    /**
     * Creates an adapter with an injected {@link HubColorMixerRegistrationHelper}.
     *
     * @param bottomBarView The {@link BottomBarView} to animate.
     * @param hubColorMixer The {@link HubColorMixer} to register the blends on.
     * @param colorMixerHelper The helper to manage registration of blends.
     */
    public BottomBarHubColorMixerAdapter(
            BottomBarView bottomBarView,
            @Nullable HubColorMixer hubColorMixer,
            HubColorMixerRegistrationHelper colorMixerHelper) {
        mBottomBarView = bottomBarView;
        mColorMixerHelper = colorMixerHelper;
        Context context = bottomBarView.getContext();

        mColorMixerHelper.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme ->
                                BottomBarUtils.getBottomBarBackgroundColor(
                                        context, colorScheme == HubColorScheme.INCOGNITO),
                        bottomBarView::setBackgroundColor));

        mColorMixerHelper.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme ->
                                BottomBarUtils.getColorSurfaceBright(
                                        context, colorScheme == HubColorScheme.INCOGNITO),
                        bottomBarView::setNewTabBackgroundTint));

        mColorMixerHelper.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme ->
                                BottomBarUtils.getRippleColorBackground(
                                        context, colorScheme == HubColorScheme.INCOGNITO),
                        bottomBarView::setNewTabRippleBackgroundColor));

        mColorMixerHelper.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme ->
                                BottomBarUtils.getRippleColorNoBackground(
                                        context, colorScheme == HubColorScheme.INCOGNITO),
                        bottomBarView::setOtherRipplesColor));

        mColorMixerHelper.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme ->
                                IncognitoColors.getColorOnSurface(
                                        context, colorScheme == HubColorScheme.INCOGNITO),
                        bottomBarView::setIconOnSurfaceColor));

        mColorMixerHelper.setColorMixer(hubColorMixer);
    }

    /** Unregisters all blends and resets the view colors to its baseline color scheme. */
    public void destroy() {
        mColorMixerHelper.destroy();
        mBottomBarView.resetColors();
    }
}
