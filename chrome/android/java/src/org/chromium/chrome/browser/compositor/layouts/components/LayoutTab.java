// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.components;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;

import org.chromium.cc.input.OffsetTag;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * {@link LayoutTab} is used to keep track of a thumbnail's bitmap and position and to draw itself
 * onto the GL canvas at the desired Y Offset.
 */
public class LayoutTab extends PropertyModel {
    // TODO(crbug.com/40126260): Make the following properties be part of the PropertyModel.
    // Begin section --------------
    // Public Layout constants.
    public static final float SHADOW_ALPHA_ON_LIGHT_BG = 0.8f;
    public static final float SHADOW_ALPHA_ON_DARK_BG = 1.0f;

    public static float sDpToPx;
    private static float sPxToDp;

    // End section --------------

    /** Supplier for determining if the current layout is active. */
    public interface IsActiveLayoutSupplier {
        /**
         * @return whether the current layout is active.
         */
        public boolean isActiveLayout();
    }

    // TODO(crbug.com/40126260): Maybe make this a ReadableIntPropertyKey
    public static final WritableIntPropertyKey TAB_ID = new WritableIntPropertyKey();

    // TODO(crbug.com/40126260): Maybe make this a ReadableIntPropertyKey
    public static final WritableBooleanPropertyKey IS_INCOGNITO = new WritableBooleanPropertyKey();

    // Fields initialized in init()
    public static final WritableFloatPropertyKey SCALE = new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey X = new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey Y = new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey RENDER_X = new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey RENDER_Y = new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey CLIPPED_WIDTH = new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey CLIPPED_HEIGHT = new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey ALPHA = new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey SATURATION = new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey BORDER_ALPHA = new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey BORDER_SCALE = new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey ORIGINAL_CONTENT_WIDTH_IN_DP =
            new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey ORIGINAL_CONTENT_HEIGHT_IN_DP =
            new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey MAX_CONTENT_WIDTH = new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey MAX_CONTENT_HEIGHT =
            new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey STATIC_TO_VIEW_BLEND =
            new WritableFloatPropertyKey();

    public static final WritableBooleanPropertyKey SHOULD_STALL = new WritableBooleanPropertyKey();

    public static final WritableBooleanPropertyKey CAN_USE_LIVE_TEXTURE =
            new WritableBooleanPropertyKey();

    public static final WritableBooleanPropertyKey SHOW_TOOLBAR = new WritableBooleanPropertyKey();

    public static final WritableBooleanPropertyKey ANONYMIZE_TOOLBAR =
            new WritableBooleanPropertyKey();

    /** Whether we need to draw the decoration (border, shadow, ..) at all. */
    public static final WritableFloatPropertyKey DECORATION_ALPHA = new WritableFloatPropertyKey();

    /** Whether initFromHost() has been called since the last call to init(). */
    public static final WritableBooleanPropertyKey INIT_FROM_HOST_CALLED =
            new WritableBooleanPropertyKey();

    // All the members bellow are initialized from the delayed initialization.
    //
    // Begin section --------------

    /** The color of the background of the tab. Used as the best approximation to fill in. */
    public static final WritableIntPropertyKey BACKGROUND_COLOR = new WritableIntPropertyKey();

    public static final WritableIntPropertyKey TOOLBAR_BACKGROUND_COLOR =
            new WritableIntPropertyKey();

    public static final WritableIntPropertyKey TEXT_BOX_BACKGROUND_COLOR =
            new WritableIntPropertyKey();
    // End section --------------

    public static final PropertyModel.WritableFloatPropertyKey CONTENT_OFFSET =
            new PropertyModel.WritableFloatPropertyKey();

    public static final PropertyModel.WritableObjectPropertyKey<IsActiveLayoutSupplier>
            IS_ACTIVE_LAYOUT_SUPPLIER = new WritableObjectPropertyKey<>();

    /** The tag indicating that this layer should be moved by viz. */
    public static final PropertyModel.WritableObjectPropertyKey<OffsetTag> CONTENT_OFFSET_TAG =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                TAB_ID,
                IS_INCOGNITO,
                SCALE,
                X,
                Y,
                RENDER_X,
                RENDER_Y,
                CLIPPED_WIDTH,
                CLIPPED_HEIGHT,
                ALPHA,
                SATURATION,
                BORDER_ALPHA,
                BORDER_SCALE,
                ORIGINAL_CONTENT_WIDTH_IN_DP,
                ORIGINAL_CONTENT_HEIGHT_IN_DP,
                MAX_CONTENT_WIDTH,
                MAX_CONTENT_HEIGHT,
                STATIC_TO_VIEW_BLEND,
                SHOULD_STALL,
                CAN_USE_LIVE_TEXTURE,
                SHOW_TOOLBAR,
                ANONYMIZE_TOOLBAR,
                DECORATION_ALPHA,
                INIT_FROM_HOST_CALLED,
                BACKGROUND_COLOR,
                TOOLBAR_BACKGROUND_COLOR,
                TEXT_BOX_BACKGROUND_COLOR,
                CONTENT_OFFSET,
                IS_ACTIVE_LAYOUT_SUPPLIER,
                CONTENT_OFFSET_TAG
            };

    /**
     * Default constructor for a {@link LayoutTab}.
     *
     * @param tabId The id of the source {@link Tab}.
     * @param isIncognito Whether the tab in the in the incognito stack.
     * @param maxContentTextureWidth The maximum width for drawing the content in px.
     * @param maxContentTextureHeight The maximum height for drawing the content in px.
     */
    public LayoutTab(
            int tabId,
            boolean isIncognito,
            int maxContentTextureWidth,
            int maxContentTextureHeight) {
        super(ALL_KEYS);

        set(TAB_ID, tabId);
        set(IS_INCOGNITO, isIncognito);
        set(BACKGROUND_COLOR, Color.WHITE);
        set(TOOLBAR_BACKGROUND_COLOR, 0xfff2f2f2);
        set(TEXT_BOX_BACKGROUND_COLOR, Color.WHITE);

        init(maxContentTextureWidth, maxContentTextureHeight);
    }

    /**
     * Initializes a {@link LayoutTab} to its default value so it can be reused.
     *
     * @param maxContentTextureWidth  The maximum width of the page content in px.
     * @param maxContentTextureHeight The maximum height of the page content in px.
     */
    public void init(int maxContentTextureWidth, int maxContentTextureHeight) {
        set(ALPHA, 1.0f);
        set(SATURATION, 1.0f);
        set(BORDER_ALPHA, 1.0f);
        set(BORDER_SCALE, 1.0f);
        set(CLIPPED_WIDTH, Float.MAX_VALUE);
        set(CLIPPED_HEIGHT, Float.MAX_VALUE);
        set(SCALE, 1.0f);
        set(X, 0.0f);
        set(Y, 0.0f);
        set(RENDER_X, 0.0f);
        set(RENDER_Y, 0.0f);
        set(STATIC_TO_VIEW_BLEND, 0.0f);
        set(DECORATION_ALPHA, 1.0f);
        set(CAN_USE_LIVE_TEXTURE, true);
        set(SHOW_TOOLBAR, false);
        set(ANONYMIZE_TOOLBAR, false);
        set(ORIGINAL_CONTENT_WIDTH_IN_DP, maxContentTextureWidth * sPxToDp);
        set(ORIGINAL_CONTENT_HEIGHT_IN_DP, maxContentTextureHeight * sPxToDp);
        set(MAX_CONTENT_WIDTH, maxContentTextureWidth * sPxToDp);
        set(MAX_CONTENT_HEIGHT, maxContentTextureHeight * sPxToDp);
        set(INIT_FROM_HOST_CALLED, false);
    }

    /**
     * Initializes the {@link LayoutTab} from data extracted from a {@link Tab}. As this function
     * may be expensive and can be delayed we initialize it as a separately.
     *
     * @param backgroundColor The color of the page background.
     * @param shouldStall Whether the tab should display a desaturated thumbnail and wait for the
     *     content layer to load.
     * @param canUseLiveTexture Whether the tab can use a live texture when being displayed.
     */
    public void initFromHost(
            int backgroundColor,
            boolean shouldStall,
            boolean canUseLiveTexture,
            int toolbarBackgroundColor,
            int textBoxBackgroundColor) {
        set(BACKGROUND_COLOR, backgroundColor);
        set(TOOLBAR_BACKGROUND_COLOR, toolbarBackgroundColor);
        set(TEXT_BOX_BACKGROUND_COLOR, textBoxBackgroundColor);
        set(SHOULD_STALL, shouldStall);
        set(CAN_USE_LIVE_TEXTURE, canUseLiveTexture);
        set(INIT_FROM_HOST_CALLED, true);
    }

    /**
     * @return Whether {@link #initFromHost} needs to be called on this {@link LayoutTab}.
     */
    public boolean isInitFromHostNeeded() {
        return !get(INIT_FROM_HOST_CALLED);
    }

    /**
     * Helper function that gather the static constants from values/dimens.xml.
     *
     * @param context The Android Context.
     */
    public static void resetDimensionConstants(Context context) {
        Resources res = context.getResources();
        sDpToPx = res.getDisplayMetrics().density;
        sPxToDp = 1.0f / sDpToPx;
    }

    /**
     * @return The scale applied to the the content of the tab. Default is 1.0.
     */
    public float getScale() {
        return get(SCALE);
    }

    /**
     * @param scale The scale to apply on the content of the tab (everything except the borders).
     */
    public void setScale(float scale) {
        set(SCALE, scale);
    }

    /**
     * Set the clipping sizes. This apply on the scaled content.
     *
     * @param clippedWidth  The width of the clipped rectangle. Float.MAX_VALUE for no clipping.
     * @param clippedHeight The height of the clipped rectangle. Float.MAX_VALUE for no clipping.
     */
    public void setClipSize(float clippedWidth, float clippedHeight) {
        set(CLIPPED_WIDTH, clippedWidth);
        set(CLIPPED_HEIGHT, clippedHeight);
    }

    /**
     * @return The width of the clipped rectangle. Float.MAX_VALUE for no clipping.
     */
    public float getClippedWidth() {
        return get(CLIPPED_WIDTH);
    }

    /**
     * @return The height of the clipped rectangle. Float.MAX_VALUE for no clipping.
     */
    public float getClippedHeight() {
        return get(CLIPPED_HEIGHT);
    }

    /**
     * @return The maximum drawable width (scaled) of the tab contents in dp.
     */
    public float getScaledContentWidth() {
        return getOriginalContentWidth() * get(SCALE);
    }

    /**
     * @return The maximum drawable height (scaled) of the tab contents in dp.
     */
    public float getScaledContentHeight() {
        return getOriginalContentHeight() * get(SCALE);
    }

    /**
     * @return The maximum drawable width (not scaled) of the tab contents texture.
     */
    public float getOriginalContentWidth() {
        return Math.min(get(ORIGINAL_CONTENT_WIDTH_IN_DP), get(MAX_CONTENT_WIDTH));
    }

    /**
     * @return The maximum drawable height (not scaled) of the tab contents texture.
     */
    public float getOriginalContentHeight() {
        return Math.min(get(ORIGINAL_CONTENT_HEIGHT_IN_DP), get(MAX_CONTENT_HEIGHT));
    }

    /**
     * @return The original unclamped width (not scaled) of the tab contents texture.
     */
    public float getUnclampedOriginalContentHeight() {
        return get(ORIGINAL_CONTENT_HEIGHT_IN_DP);
    }

    /**
     * @return The width of the drawn content (clipped and scaled).
     */
    public float getFinalContentWidth() {
        return Math.min(get(CLIPPED_WIDTH), getScaledContentWidth());
    }

    /**
     * @return The maximum height the content can be.
     */
    public float getMaxContentHeight() {
        return get(MAX_CONTENT_HEIGHT);
    }

    /**
     * @param width The maximum width the content can be.
     */
    public void setMaxContentWidth(float width) {
        set(MAX_CONTENT_WIDTH, width);
    }

    /**
     * @param height The maximum height the content can be.
     */
    public void setMaxContentHeight(float height) {
        set(MAX_CONTENT_HEIGHT, height);
    }

    /**
     * @return The id of the tab, same as the id from the Tab in TabModel.
     */
    public int getId() {
        return get(TAB_ID);
    }

    /**
     * @return Whether the underlying tab is incognito or not.
     */
    public boolean isIncognito() {
        return get(IS_INCOGNITO);
    }

    /**
     * @param y The vertical draw position.
     */
    public void setY(float y) {
        set(Y, y);
    }

    /**
     * @return The vertical draw position for the update logic.
     */
    public float getY() {
        return get(Y);
    }

    /**
     * @return The vertical draw position for the renderer.
     */
    public float getRenderY() {
        return get(RENDER_Y);
    }

    /**
     * @param x The horizontal draw position.
     */
    public void setX(float x) {
        set(X, x);
    }

    /**
     * @return The horizontal draw position for the update logic.
     */
    public float getX() {
        return get(X);
    }

    /**
     * @return The horizontal draw position for the renderer.
     */
    public float getRenderX() {
        return get(RENDER_X);
    }

    /**
     * Set the transparency value for all of the tab (the contents,
     * border, etc...).  For components that allow specifying
     * their own alpha values, it will use the min of these two fields.
     *
     * @param f The transparency value for the tab.
     */
    public void setAlpha(float f) {
        set(ALPHA, f);
    }

    /**
     * @return The transparency value for all of the tab components.
     */
    public float getAlpha() {
        return get(ALPHA);
    }

    /**
     * Set the saturation value for the tab contents.
     *
     * @param f The saturation value for the contents.
     */
    public void setSaturation(float f) {
        set(SATURATION, f);
    }

    /**
     * @return The saturation value for the tab contents.
     */
    public float getSaturation() {
        return get(SATURATION);
    }

    /**
     * @param alpha The maximum alpha value of the tab border.
     */
    public void setBorderAlpha(float alpha) {
        set(BORDER_ALPHA, alpha);
    }

    /**
     * @return The current alpha value at which the tab border is drawn.
     */
    public float getBorderAlpha() {
        return Math.min(get(BORDER_ALPHA), get(ALPHA));
    }

    /**
     * @return The current alpha value at which the tab border inner shadow is drawn.
     */
    public float getBorderInnerShadowAlpha() {
        return Math.min(0, get(ALPHA));
    }

    /**
     * @param scale The scale factor of the border.
     *              1.0f yields 1:1 pixel with the source image.
     */
    public void setBorderScale(float scale) {
        set(BORDER_SCALE, scale);
    }

    /**
     * @return The current scale applied on the tab border.
     */
    public float getBorderScale() {
        return get(BORDER_SCALE);
    }

    /**
     * @param decorationAlpha Whether or not to draw the decoration for this card.
     */
    public void setDecorationAlpha(float decorationAlpha) {
        set(DECORATION_ALPHA, decorationAlpha);
    }

    /**
     * @return The opacity of the decoration.
     */
    public float getDecorationAlpha() {
        return get(DECORATION_ALPHA);
    }

    /**
     * @param percentageView The blend between the old static tab and the new live one.
     */
    public void setStaticToViewBlend(float percentageView) {
        set(STATIC_TO_VIEW_BLEND, percentageView);
    }

    /**
     * @return The current blend between the old static tab and the new live one.
     */
    public float getStaticToViewBlend() {
        return get(STATIC_TO_VIEW_BLEND);
    }

    @Override
    public String toString() {
        return Integer.toString(getId());
    }

    /**
     * @param originalContentWidth  The maximum content width for the given orientation in px.
     * @param originalContentHeight The maximum content height for the given orientation in px.
     */
    public void setContentSize(int originalContentWidth, int originalContentHeight) {
        set(ORIGINAL_CONTENT_WIDTH_IN_DP, originalContentWidth * sPxToDp);
        set(ORIGINAL_CONTENT_HEIGHT_IN_DP, originalContentHeight * sPxToDp);
    }

    /**
     * @param shouldStall Whether or not the tab should wait for the live layer to load.
     */
    public void setShouldStall(boolean shouldStall) {
        set(SHOULD_STALL, shouldStall);
    }

    /**
     * @return Whether or not the tab should wait for the live layer to load.
     */
    public boolean shouldStall() {
        return get(SHOULD_STALL);
    }

    /**
     * @return Whether the tab can use a live texture to render.
     */
    public boolean canUseLiveTexture() {
        return get(CAN_USE_LIVE_TEXTURE);
    }

    /**
     * @param showToolbar Whether or not to show a toolbar at the top of the content.
     */
    public void setShowToolbar(boolean showToolbar) {
        set(SHOW_TOOLBAR, showToolbar);
    }

    /**
     * @return Whether or not to show a toolbar at the top of the content.
     */
    public boolean showToolbar() {
        return get(SHOW_TOOLBAR);
    }

    /**
     * This value is only used if {@link #showToolbar()} is {@code true}.
     *
     * @param anonymize Whether or not to anonymize the toolbar (hiding URL, etc.).
     */
    public void setAnonymizeToolbar(boolean anonymize) {
        set(ANONYMIZE_TOOLBAR, anonymize);
    }

    /**
     * This value is only used if {@link #showToolbar()} is {@code true}.
     *
     * @return Whether or not to anonymize the toolbar (hiding URL, etc.).
     */
    public boolean anonymizeToolbar() {
        return get(ANONYMIZE_TOOLBAR);
    }

    /**
     * @return The color of the background of the tab. Used as the best approximation to fill in.
     */
    public int getBackgroundColor() {
        return get(BACKGROUND_COLOR);
    }

    /**
     * @return The color of the background of the toolbar.
     */
    public int getToolbarBackgroundColor() {
        return get(TOOLBAR_BACKGROUND_COLOR);
    }

    /**
     * @return The color of the textbox in the toolbar. Used as the color for the anonymize rect.
     */
    public int getTextBoxBackgroundColor() {
        return get(TEXT_BOX_BACKGROUND_COLOR);
    }
}
