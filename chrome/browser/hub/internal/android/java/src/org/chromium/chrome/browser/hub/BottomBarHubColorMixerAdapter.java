// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_COLOR_BLEND_ANIMATION_DURATION_MS;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.bottombar.BottomBarUtils;
import org.chromium.chrome.browser.ui.bottombar.BottomBarView;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.IncognitoColors;

/**
 * Adapter that registers color blend animations on {@link HubColorMixer} for a {@link
 * BottomBarView} when hosted inside the Hub.
 */
@NullMarked
public class BottomBarHubColorMixerAdapter {
    private final HubColorMixerRegistrationHelper mColorMixerHelper;
    private final BottomBarView mBottomBarView;
    private final NullableObservableSupplier<Tab> mCurrentTabSupplier;
    private final NonNullObservableSupplier<Boolean> mIsHidingSupplier;
    private final Callback<Boolean> mOnHidingChanged = this::onHidingChanged;
    private final Callback<@Nullable Tab> mOnCurrentTabChanged = this::onCurrentTabChanged;

    /**
     * Creates an adapter and registers color blends for the given {@link BottomBarView},
     * currentTabSupplier, and isHidingSupplier.
     *
     * @param bottomBarView The {@link BottomBarView} to animate.
     * @param hubColorMixer The {@link HubColorMixer} to register the blends on.
     * @param currentTabSupplier The supplier of the current tab.
     * @param isHidingSupplier Supplies whether the Hub is currently hiding / exiting.
     */
    public BottomBarHubColorMixerAdapter(
            BottomBarView bottomBarView,
            @Nullable HubColorMixer hubColorMixer,
            NullableObservableSupplier<Tab> currentTabSupplier,
            NonNullObservableSupplier<Boolean> isHidingSupplier) {
        this(
                bottomBarView,
                hubColorMixer,
                new HubColorMixerRegistrationHelper(),
                currentTabSupplier,
                isHidingSupplier);
    }

    /**
     * Creates an adapter with an injected {@link HubColorMixerRegistrationHelper},
     * currentTabSupplier, and isHidingSupplier.
     *
     * @param bottomBarView The {@link BottomBarView} to animate.
     * @param hubColorMixer The {@link HubColorMixer} to register the blends on.
     * @param colorMixerHelper The helper to manage registration of blends.
     * @param currentTabSupplier The supplier of the current tab.
     * @param isHidingSupplier Supplies whether the Hub is currently hiding / exiting.
     */
    @VisibleForTesting
    BottomBarHubColorMixerAdapter(
            BottomBarView bottomBarView,
            @Nullable HubColorMixer hubColorMixer,
            HubColorMixerRegistrationHelper colorMixerHelper,
            NullableObservableSupplier<Tab> currentTabSupplier,
            NonNullObservableSupplier<Boolean> isHidingSupplier) {
        mBottomBarView = bottomBarView;
        mColorMixerHelper = colorMixerHelper;
        mCurrentTabSupplier = currentTabSupplier;
        mIsHidingSupplier = isHidingSupplier;
        Context context = bottomBarView.getContext();

        HubViewColorBlend bgBlend =
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme ->
                                BottomBarUtils.getBottomBarBackgroundColor(
                                        context, colorScheme == HubColorScheme.INCOGNITO),
                        bottomBarView::setBackgroundColor);
        mColorMixerHelper.registerBlend(bgBlend);

        HubViewColorBlend newTabBgBlend =
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme ->
                                BottomBarUtils.getColorSurfaceBright(
                                        context, colorScheme == HubColorScheme.INCOGNITO),
                        bottomBarView::setNewTabBackgroundTint);
        mColorMixerHelper.registerBlend(newTabBgBlend);

        HubViewColorBlend newTabRippleBlend =
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme ->
                                BottomBarUtils.getRippleColorBackground(
                                        context, colorScheme == HubColorScheme.INCOGNITO),
                        bottomBarView::setNewTabRippleBackgroundColor);
        mColorMixerHelper.registerBlend(newTabRippleBlend);

        HubViewColorBlend otherRipplesBlend =
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme ->
                                BottomBarUtils.getRippleColorNoBackground(
                                        context, colorScheme == HubColorScheme.INCOGNITO),
                        bottomBarView::setOtherRipplesColor);
        mColorMixerHelper.registerBlend(otherRipplesBlend);

        HubViewColorBlend iconOnSurfaceBlend =
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme ->
                                IncognitoColors.getColorOnSurface(
                                        context, colorScheme == HubColorScheme.INCOGNITO),
                        bottomBarView::setIconOnSurfaceColor);
        mColorMixerHelper.registerBlend(iconOnSurfaceBlend);

        mColorMixerHelper.setColorMixer(hubColorMixer);

        mCurrentTabSupplier.addSyncObserverAndCallIfNonNull(mOnCurrentTabChanged);
        mIsHidingSupplier.addSyncObserverAndCallIfNonNull(mOnHidingChanged);
    }

    private void onHidingChanged(Boolean isHiding) {
        if (Boolean.TRUE.equals(isHiding)) {
            updateColorSchemeForTab(mCurrentTabSupplier.get());
        }
    }

    private void onCurrentTabChanged(@Nullable Tab tab) {
        if (Boolean.TRUE.equals(mIsHidingSupplier.get())) {
            updateColorSchemeForTab(tab);
        }
    }

    private void updateColorSchemeForTab(@Nullable Tab tab) {
        @BrandedColorScheme int colorScheme = HubColors.getBrandedColorSchemeForTab(tab);
        mBottomBarView.setColorScheme(colorScheme);
    }

    /**
     * Unregisters all blends, detaches observers, and restores the view colors to its configured
     * baseline color scheme.
     *
     * <p><b>Note on Incognito & Pane Schemes:</b> {@link BottomBarView} is an embedded view rather
     * than an independent Hub pane. While inside the Hub, per-pane color schemes and interactive
     * drag sweeps are dynamically blended by {@link HubColorMixer} (via registered {@link
     * HubViewColorBlend}s). Upon adapter teardown, calling {@link BottomBarView#resetColors()}
     * clears any transient blended colors left over from {@link HubColorMixer} transitions and
     * restores the configured {@link BrandedColorScheme} so the browser's {@link BottomBarMediator}
     * can resume deterministic control.
     */
    public void destroy() {
        mIsHidingSupplier.removeObserver(mOnHidingChanged);
        mCurrentTabSupplier.removeObserver(mOnCurrentTabChanged);
        mColorMixerHelper.destroy();
        mBottomBarView.resetColors();
    }
}
