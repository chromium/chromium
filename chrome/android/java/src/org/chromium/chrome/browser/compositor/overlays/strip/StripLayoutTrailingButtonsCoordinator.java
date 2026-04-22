// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton.ButtonType;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorTextButton;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView.StripLayoutViewOnClickHandler;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView.StripLayoutViewOnKeyboardFocusHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicKeyedService;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlobalShowHideObserver;
import org.chromium.chrome.browser.glic.GlicPrefNames;
import org.chromium.chrome.browser.glic.GlicUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.side_panel.AndroidSidePanelEnabledFn;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.util.ColorUtils;

/** Coordinator for the trailing buttons on the tab strip. */
@NullMarked
public class StripLayoutTrailingButtonsCoordinator {
    /** Observer for changes to the trailing buttons layout state. */
    public interface StripLayoutTrailingButtonsObserver {
        /** Called when the trailing buttons layout footprint (e.g. width or visibility) changes. */
        void onTrailingButtonsLayoutStateChanged();
    }

    // Glic button constants.
    private static final float GLIC_BUTTON_BACKGROUND_Y_OFFSET_DP = 5.f;
    private static final float GLIC_BUTTON_BACKGROUND_WIDTH_DP = 28.f;
    private static final float GLIC_BUTTON_BACKGROUND_HEIGHT_DP = 28.f;
    private static final float GLIC_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY = 0.30f;
    private static final float GLIC_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY = 0.20f;
    private static final float GLIC_BUTTON_UNFOCUSED_OPACITY = 0.65f;
    private static final float GLIC_BUTTON_CLICK_SLOP_DP =
            (StripLayoutHelperManager.BUTTON_DESIRED_TOUCH_TARGET_SIZE
                            - GLIC_BUTTON_BACKGROUND_WIDTH_DP)
                    / 2;
    // Total vertical margin (Tab Strip Height(40dp) - Glic Background Height(28dp) = 12dp).
    public static final float GLIC_BUTTON_MARGIN_HEIGHT_DP = 12.f;
    public static final float GLIC_BUTTON_START_PADDING_DP = 6.f;
    private static final float GLIC_ICON_WIDTH_DP = 16.f;
    public static final float GLIC_ICON_TEXT_PADDING_DP = 4.f;
    private static final float GLIC_BUTTON_END_PADDING_DP = 10.f;
    public static final float GLIC_BUTTON_CORNER_RADIUS = 12.f;
    // 2dp accounts for the smaller Glic background and aligns it with the toolbar buttons
    private static final float GLIC_ALIGNMENT_OFFSET_DP =
            (StripLayoutHelperManager.MODEL_SELECTOR_BUTTON_BACKGROUND_WIDTH_DP
                            - GLIC_BUTTON_BACKGROUND_WIDTH_DP)
                    / 2;

    // Core Dependencies
    private final Context mContext;
    private final LayoutRenderHost mRenderHost;
    private final StripLayoutTrailingButtonsObserver mObserver;

    // Configuration & Delegates
    private final float mDensity;
    private final float mStripEndPadding;
    private final Runnable mGlicClickHandler;
    private final @Nullable GlicKeyedService mGlicKeyedService;
    private final @Nullable GlobalShowHideObserver mGlicUiObserver;

    // Lifecycle & Caching Objects
    private @Nullable Profile mProfile;
    private @Nullable PrefChangeRegistrar mPrefChangeRegistrar;
    private @Nullable LayerTitleCache mLayerTitleCache;

    // UI Components
    private @Nullable TintedCompositorTextButton mGlicButton;

    // Layout & State Parameters
    private float mWidth;
    private float mRightPadding;
    private float mLeftPadding;
    private float mTopPadding;
    private boolean mIsGlicUiVisible;
    private boolean mIsMsbVisible;

    /**
     * Creates the trailing buttons coordinator.
     *
     * @param context The {@link Context} for constructing the button.
     * @param renderHost The {@link LayoutRenderHost} for requesting renders.
     * @param glicClickHandler The {@link Runnable} to execute on Glic button click.
     * @param density The display density.
     * @param stripEndPadding The end padding of the tab strip.
     * @param toolbarControlContainer The view containing toolbar controls.
     * @param keyboardFocusHandler The {@link StripLayoutViewOnKeyboardFocusHandler} for the button.
     * @param isAppInDesktopWindow Whether the app is in a desktop window.
     * @param isTopResumedActivity Whether the app is the top resumed activity.
     * @param glicKeyedService The {@link GlicKeyedService} for observing Glic UI state.
     */
    public StripLayoutTrailingButtonsCoordinator(
            Context context,
            LayoutRenderHost renderHost,
            Runnable glicClickHandler,
            float density,
            float stripEndPadding,
            View toolbarControlContainer,
            StripLayoutViewOnKeyboardFocusHandler keyboardFocusHandler,
            boolean isAppInDesktopWindow,
            boolean isTopResumedActivity,
            @Nullable GlicKeyedService glicKeyedService,
            StripLayoutTrailingButtonsObserver observer) {
        mContext = context;
        mRenderHost = renderHost;
        mGlicClickHandler = glicClickHandler;
        mDensity = density;
        mStripEndPadding = stripEndPadding;
        mGlicKeyedService = glicKeyedService;
        mObserver = observer;

        if (mGlicKeyedService != null) {
            mGlicUiObserver =
                    isOpened -> {
                        mIsGlicUiVisible = isOpened;
                        if (mGlicButton != null) {
                            mGlicButton.setPressed(isOpened);
                            mRenderHost.requestRender();
                        }
                    };
            mGlicKeyedService.addGlobalShowHideObserver(mGlicUiObserver);
        } else {
            mGlicUiObserver = null;
        }

        StripLayoutViewOnClickHandler glicClickHandlerOnButton =
                (time, view, motionEventButtonState, modifiers) -> mGlicClickHandler.run();

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.GLIC)
                && AndroidSidePanelEnabledFn.isEnabled()) {
            mGlicButton =
                    new TintedCompositorTextButton(
                            mContext,
                            /* incognito= */ false,
                            ButtonType.GLIC,
                            /* parentView= */ null,
                            GLIC_BUTTON_BACKGROUND_WIDTH_DP,
                            GLIC_BUTTON_BACKGROUND_HEIGHT_DP,
                            (tooltipText) -> {
                                toolbarControlContainer.setTooltipText(tooltipText);
                            },
                            glicClickHandlerOnButton,
                            keyboardFocusHandler,
                            R.drawable.ic_spark_4c_16dp,
                            GLIC_BUTTON_CLICK_SLOP_DP,
                            /* hasLongClickAction= */ false,
                            /* dismissButton= */ null);

            mGlicButton.setDrawY(GLIC_BUTTON_BACKGROUND_Y_OFFSET_DP);
            mGlicButton.setVisible(false);

            @ColorInt
            int backgroundDefaultColor = SemanticColorUtils.getColorSurfaceContainerLow(mContext);

            @ColorInt
            int backgroundHoverColor =
                    ColorUtils.setAlphaComponentWithFloat(
                            SemanticColorUtils.getColorPrimary(mContext),
                            GLIC_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY);

            @ColorInt
            int backgroundPressedColor =
                    ColorUtils.setAlphaComponentWithFloat(
                            SemanticColorUtils.getColorPrimary(mContext),
                            GLIC_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY);

            mGlicButton.setBackgroundTint(
                    backgroundDefaultColor,
                    backgroundHoverColor,
                    backgroundPressedColor,
                    backgroundPressedColor);

            mGlicButton.setTint(SemanticColorUtils.getDefaultIconColor(mContext));

            mGlicButton.setAccessibilityDescription(
                    mContext.getString(R.string.glic_tab_strip_button_tooltip));

            mGlicButton.setText(
                    mContext.getString(R.string.glic_button_entrypoint_ask_gemini_label));
        }

        updateGlicButtonOpacity(isAppInDesktopWindow, isTopResumedActivity);
    }

    /** Destroys the coordinator and unregisters observers. */
    public void destroy() {
        if (mGlicKeyedService != null && mGlicUiObserver != null) {
            mGlicKeyedService.removeGlobalShowHideObserver(mGlicUiObserver);
        }
        if (mPrefChangeRegistrar != null) {
            mPrefChangeRegistrar.destroy();
            mPrefChangeRegistrar = null;
        }
    }

    /**
     * Registers a pref observer for Glic button changes when the profile is available.
     *
     * @param profile The {@link Profile} to observe.
     */
    public void onProfileAvailable(Profile profile) {
        if (mProfile == profile) return;

        mProfile = profile;
        if (mPrefChangeRegistrar != null) {
            mPrefChangeRegistrar.destroy();
            mPrefChangeRegistrar = null;
        }

        mPrefChangeRegistrar = new PrefChangeRegistrar(UserPrefs.get(profile));
        mPrefChangeRegistrar.addObserver(
                GlicPrefNames.GLIC_PINNED_TO_TABSTRIP, this::onGlicPrefChanged);

        onGlicPrefChanged();
    }

    private void onGlicPrefChanged() {
        if (mGlicButton == null || mProfile == null) return;
        boolean isPinned =
                UserPrefs.get(mProfile).getBoolean(GlicPrefNames.GLIC_PINNED_TO_TABSTRIP);

        if (mGlicButton.isVisible() != isPinned) {
            mGlicButton.setVisible(isPinned);
            updateGlicButtonPosition();
            mObserver.onTrailingButtonsLayoutStateChanged();
        }
    }

    /** Returns the Glic button instance. */
    public @Nullable TintedCompositorTextButton getGlicButton() {
        return mGlicButton;
    }

    /**
     * Handles strip size changes.
     *
     * @param width The full width of the strip layout.
     * @param rightPadding The padding on the right of the strip layout.
     * @param leftPadding The padding on the left of the strip layout.
     * @param topPadding The padding on the top of the strip layout.
     */
    public void onSizeChanged(
            float width, float rightPadding, float leftPadding, float topPadding) {
        mWidth = width;
        mRightPadding = rightPadding;
        mLeftPadding = leftPadding;
        mTopPadding = topPadding;
        updateGlicButtonPosition();
    }

    /** Sets the cache used for generating textures for the trailing buttons. */
    public void setLayerTitleCache(@Nullable LayerTitleCache titleCache) {
        mLayerTitleCache = titleCache;
        if (mGlicButton != null) {
            updateGlicButtonTextProperties();
        }
    }

    /**
     * Updates the text displayed on the Glic button.
     *
     * @param text The new text.
     */
    public void setGlicButtonText(@Nullable String text) {
        if (mGlicButton == null || TextUtils.equals(mGlicButton.getText(), text)) return;
        mGlicButton.setText(text);
        updateGlicButtonTextProperties();
    }

    private void updateGlicButtonTextProperties() {
        if (mGlicButton == null) return;
        String text = mGlicButton.getText();

        if (mLayerTitleCache != null && !TextUtils.isEmpty(text)) {
            mGlicButton.setTextResourceId(mLayerTitleCache.getUpdatedGlicButtonText(text));
        } else {
            mGlicButton.setTextResourceId(Resources.ID_NULL);
        }

        updateGlicButtonWidth(mLayerTitleCache);
        updateGlicButtonPosition();
        mObserver.onTrailingButtonsLayoutStateChanged();
    }

    private void updateGlicButtonWidth(@Nullable LayerTitleCache titleCache) {
        if (mGlicButton == null) return;
        String glicButtonText = mGlicButton.getText();

        float width = GLIC_BUTTON_BACKGROUND_WIDTH_DP;
        if (!TextUtils.isEmpty(glicButtonText) && titleCache != null) {
            float textWidthDp = titleCache.getButtonTextWidth(glicButtonText) / mDensity;
            width =
                    GLIC_BUTTON_START_PADDING_DP
                            + GLIC_ICON_WIDTH_DP
                            + GLIC_ICON_TEXT_PADDING_DP
                            + textWidthDp
                            + GLIC_BUTTON_END_PADDING_DP;
        }
        mGlicButton.setWidth(width);
    }

    /**
     * Updates the visibility of the model selector button.
     *
     * @param visible Whether the model selector button should be visible.
     */
    public void setModelSelectorButtonVisible(boolean visible) {
        mIsMsbVisible = visible;
        updateGlicButtonPosition();
    }

    /** Updates the position of the Glic button based on layout parameters. */
    public void updateGlicButtonPosition() {
        if (mGlicButton == null) return;

        mGlicButton.setDrawY(GLIC_BUTTON_BACKGROUND_Y_OFFSET_DP);
        mGlicButton.setTouchTargetInsets(null, mTopPadding, null, -mTopPadding);

        if (!LocalizationUtils.isLayoutRtl()) {
            float rightSideAnchor =
                    mWidth - mRightPadding - mStripEndPadding - GLIC_ALIGNMENT_OFFSET_DP;
            if (mIsMsbVisible) {
                rightSideAnchor -= StripLayoutHelperManager.BUTTON_DESIRED_TOUCH_TARGET_SIZE;
            }
            mGlicButton.setDrawX(rightSideAnchor - mGlicButton.getWidth());
        } else {
            float leftSideAnchor = mLeftPadding + mStripEndPadding + GLIC_ALIGNMENT_OFFSET_DP;
            if (mIsMsbVisible) {
                leftSideAnchor += StripLayoutHelperManager.BUTTON_DESIRED_TOUCH_TARGET_SIZE;
            }
            mGlicButton.setDrawX(leftSideAnchor);
        }
    }

    /**
     * Updates the opacity of the Glic button based on app focus state.
     *
     * @param isAppInDesktopWindow Whether the app is in a desktop window.
     * @param isTopResumedActivity Whether the app is the top resumed activity.
     */
    public void updateGlicButtonOpacity(
            boolean isAppInDesktopWindow, boolean isTopResumedActivity) {
        if (mGlicButton == null) return;
        boolean isUnfocusedInDw = isAppInDesktopWindow && !isTopResumedActivity;
        mGlicButton.setOpacity(isUnfocusedInDw ? GLIC_BUTTON_UNFOCUSED_OPACITY : 1.0f);
    }

    /** Returns the total width used by the Glic button including end padding. */
    public float getGlicButtonWidthWithEndPadding() {
        if (mGlicButton == null) return 0.0f;
        return mGlicButton.getWidth() + mStripEndPadding + GLIC_ALIGNMENT_OFFSET_DP;
    }

    /** Returns the start padding required for the Glic button's touch target. */
    public float getGlicButtonStartPaddingForTouchTarget() {
        if (mGlicButton != null && mGlicButton.isVisible()) {
            return mStripEndPadding + GLIC_ALIGNMENT_OFFSET_DP;
        } else {
            return 0.0f;
        }
    }

    /**
     * Sets the visibility of the Glic button.
     *
     * @param visible Whether the button should be visible.
     */
    public void setGlicButtonVisible(boolean visible) {
        if (mGlicButton != null) {
            mGlicButton.setVisible(visible);
        }
    }

    /** Returns whether the Glic button is currently visible. */
    public boolean isGlicButtonVisible() {
        return mGlicButton != null && mGlicButton.isVisible();
    }

    /** Returns whether the Glic UI is currently visible (e.g. panel is open). */
    public boolean isGlicUiVisible() {
        return mIsGlicUiVisible;
    }

    /**
     * Determines if the Glic button should be visible in the tab strip.
     *
     * @param isIncognito Whether the current tab model is incognito.
     * @param tabModelSelector The TabModelSelector to retrieve the current Profile.
     */
    public boolean shouldGlicBeVisible(
            boolean isIncognito, @Nullable TabModelSelector tabModelSelector) {
        if (mGlicButton == null
                || isIncognito
                || tabModelSelector == null
                || tabModelSelector.getCurrentModel() == null) {
            return false;
        }
        Profile profile = tabModelSelector.getCurrentModel().getProfile();
        return profile != null && GlicUtils.isButtonPinnedToTabStrip(profile);
    }

    /**
     * Handles down touch events.
     *
     * @param x The x coordinate of the event.
     * @param y The y coordinate of the event.
     * @param buttons The buttons pressed.
     * @return true if the event was handled.
     */
    public boolean onDown(float x, float y, int buttons) {
        return mGlicButton != null && mGlicButton.onDown(x, y, buttons);
    }

    /**
     * Handles up or cancel touch events.
     *
     * @return true if the event was handled.
     */
    public boolean onUpOrCancel() {
        boolean handled =
                mGlicButton != null && mGlicButton.isVisible() && mGlicButton.onUpOrCancel();
        if (handled) {
            mGlicClickHandler.run();
        }
        return handled;
    }

    /**
     * Handles drag events.
     *
     * @param x The x coordinate of the event.
     * @param y The y coordinate of the event.
     */
    public void drag(float x, float y) {
        if (mGlicButton != null) {
            mGlicButton.drag(x, y);
        }
    }

    /**
     * Handles click events.
     *
     * @param time The timestamp of the event.
     * @param x The x coordinate of the event.
     * @param y The y coordinate of the event.
     * @param buttons The buttons pressed.
     * @param modifiers Any key modifiers.
     * @return true if the event was handled.
     */
    public boolean click(long time, float x, float y, int buttons, int modifiers) {
        if (mGlicButton != null && mGlicButton.click(x, y, buttons)) {
            mGlicButton.handleClick(time, buttons, modifiers);
            return true;
        }
        return false;
    }
}
