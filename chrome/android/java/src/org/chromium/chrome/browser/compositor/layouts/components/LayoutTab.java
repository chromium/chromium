// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.components;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.RectF;

import org.chromium.base.MathUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * {@link LayoutTab} is used to keep track of a thumbnail's bitmap and position and to
 * draw itself onto the GL canvas at the desired Y Offset.
 */
public class LayoutTab extends PropertyModel {
    public static final float ALPHA_THRESHOLD = 1.0f / 255.0f;

    // TODO(crbug.com/1070284): Make the following properties be part of the PropertyModel.
    // Begin section --------------
    // Public Layout constants.
    public static final float CLOSE_BUTTON_WIDTH_DP = 36.f;
    public static final float SHADOW_ALPHA_ON_LIGHT_BG = 0.8f;
    public static final float SHADOW_ALPHA_ON_DARK_BG = 1.0f;

    public static float sDpToPx;
    private static float sPxToDp;
    // End section --------------

    // TODO(crbug.com/1070284): Maybe make this a ReadableIntPropertyKey
    public static final WritableIntPropertyKey TAB_ID = new WritableIntPropertyKey();

    // TODO(crbug.com/1070284): Maybe make this a ReadableIntPropertyKey
    public static final WritableBooleanPropertyKey IS_INCOGNITO = new WritableBooleanPropertyKey();

    // Fields initialized in init()
    public static final WritableFloatPropertyKey SCALE = new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey X = new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey Y = new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey RENDER_X = new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey RENDER_Y = new WritableFloatPropertyKey();

    // The top left X offset of the clipped rectangle.
    public static final WritableFloatPropertyKey CLIPPED_X = new WritableFloatPropertyKey();

    // The top left Y offset of the clipped rectangle.
    public static final WritableFloatPropertyKey CLIPPED_Y = new WritableFloatPropertyKey();

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

    public static final WritableFloatPropertyKey BRIGHTNESS = new WritableFloatPropertyKey();

    public static final WritableBooleanPropertyKey IS_VISIBLE = new WritableBooleanPropertyKey();

    public static final WritableBooleanPropertyKey SHOULD_STALL = new WritableBooleanPropertyKey();

    public static final WritableBooleanPropertyKey CAN_USE_LIVE_TEXTURE =
            new WritableBooleanPropertyKey();

    public static final WritableBooleanPropertyKey SHOW_TOOLBAR = new WritableBooleanPropertyKey();

    public static final WritableBooleanPropertyKey ANONYMIZE_TOOLBAR =
            new WritableBooleanPropertyKey();

    public static final WritableFloatPropertyKey TOOLBAR_ALPHA = new WritableFloatPropertyKey();

    public static final WritableBooleanPropertyKey INSET_BORDER_VERTICAL =
            new WritableBooleanPropertyKey();

    public static final WritableFloatPropertyKey TOOLBAR_Y_OFFSET = new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey SIDE_BORDER_SCALE = new WritableFloatPropertyKey();

    public static final WritableBooleanPropertyKey CLOSE_BUTTON_IS_ON_RIGHT =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<RectF> BOUNDS = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<RectF> CLOSE_PLACEMENT =
            new WritableObjectPropertyKey<>();

    /** Whether we need to draw the decoration (border, shadow, ..) at all. */
    public static final WritableFloatPropertyKey DECORATION_ALPHA = new WritableFloatPropertyKey();

    /**
     * Whether this tab need to have its title texture generated. As this is not a free operation
     * knowing that we won't show it might save a few cycles and memory.
     */
    public static final WritableBooleanPropertyKey IS_TITLE_NEEDED =
            new WritableBooleanPropertyKey();

    /**
     * Whether initFromHost() has been called since the last call to init().
     */
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

    public static final WritableFloatPropertyKey TEXT_BOX_ALPHA = new WritableFloatPropertyKey();

    // End section --------------

    public static final PropertyModel.WritableFloatPropertyKey CONTENT_OFFSET =
            new PropertyModel.WritableFloatPropertyKey();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {TAB_ID, IS_INCOGNITO, SCALE, X,
            Y, RENDER_X, RENDER_Y, CLIPPED_X, CLIPPED_Y, CLIPPED_WIDTH, CLIPPED_HEIGHT, ALPHA,
            SATURATION, BORDER_ALPHA, BORDER_SCALE, ORIGINAL_CONTENT_WIDTH_IN_DP,
            ORIGINAL_CONTENT_HEIGHT_IN_DP, MAX_CONTENT_WIDTH, MAX_CONTENT_HEIGHT,
            STATIC_TO_VIEW_BLEND, BRIGHTNESS, IS_VISIBLE, SHOULD_STALL, CAN_USE_LIVE_TEXTURE,
            SHOW_TOOLBAR, ANONYMIZE_TOOLBAR, TOOLBAR_ALPHA, INSET_BORDER_VERTICAL, TOOLBAR_Y_OFFSET,
            SIDE_BORDER_SCALE, CLOSE_BUTTON_IS_ON_RIGHT, BOUNDS, CLOSE_PLACEMENT, DECORATION_ALPHA,
            IS_TITLE_NEEDED, INIT_FROM_HOST_CALLED, BACKGROUND_COLOR, TOOLBAR_BACKGROUND_COLOR,
            TEXT_BOX_BACKGROUND_COLOR, TEXT_BOX_ALPHA, CONTENT_OFFSET};

    /**
     * Default constructor for a {@link LayoutTab}.
     *
     * @param tabId                   The id of the source {@link Tab}.
     * @param isIncognito             Whether the tab in the in the incognito stack.
     * @param maxContentTextureWidth  The maximum width for drawing the content in px.
     * @param maxContentTextureHeight The maximum height for drawing the content in px.
     */
    public LayoutTab(int tabId, boolean isIncognito, int maxContentTextureWidth,
            int maxContentTextureHeight) {
        super(ALL_KEYS);

        set(TAB_ID, tabId);
        set(IS_INCOGNITO, isIncognito);
        set(BOUNDS, new RectF());
        set(CLOSE_PLACEMENT, new RectF());
        set(BACKGROUND_COLOR, Color.WHITE);
        set(TOOLBAR_BACKGROUND_COLOR, 0xfff2f2f2);
        set(TEXT_BOX_BACKGROUND_COLOR, Color.WHITE);
        set(TEXT_BOX_ALPHA, 1.0f);

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
        set(BRIGHTNESS, 1.0f);
        set(BORDER_ALPHA, 1.0f);
        set(BORDER_SCALE, 1.0f);
        set(CLIPPED_X, 0.0f);
        set(CLIPPED_Y, 0.0f);
        set(CLIPPED_WIDTH, Float.MAX_VALUE);
        set(CLIPPED_HEIGHT, Float.MAX_VALUE);
        set(SCALE, 1.0f);
        set(IS_VISIBLE, true);
        set(X, 0.0f);
        set(Y, 0.0f);
        set(RENDER_X, 0.0f);
        set(RENDER_Y, 0.0f);
        set(STATIC_TO_VIEW_BLEND, 0.0f);
        set(DECORATION_ALPHA, 1.0f);
        set(CAN_USE_LIVE_TEXTURE, true);
        set(SHOW_TOOLBAR, false);
        set(ANONYMIZE_TOOLBAR, false);
        set(TOOLBAR_ALPHA, 1.0f);
        set(INSET_BORDER_VERTICAL, false);
        set(TOOLBAR_Y_OFFSET, 0.f);
        set(SIDE_BORDER_SCALE, 1.f);
        set(ORIGINAL_CONTENT_WIDTH_IN_DP, maxContentTextureWidth * sPxToDp);
        set(ORIGINAL_CONTENT_HEIGHT_IN_DP, maxContentTextureHeight * sPxToDp);
        set(MAX_CONTENT_WIDTH, maxContentTextureWidth * sPxToDp);
        set(MAX_CONTENT_HEIGHT, maxContentTextureHeight * sPxToDp);
        set(INIT_FROM_HOST_CALLED, false);
    }

    /**
     * Initializes the {@link LayoutTab} from data extracted from a {@link Tab}.
     * As this function may be expensive and can be delayed we initialize it as a separately.
     *
     * @param backgroundColor       The color of the page background.
     * @param fallbackThumbnailId   The id of a cached thumbnail to show if the current
     *                              thumbnail is unavailable, or {@link Tab.INVALID_TAB_ID}
     *                              if none exists.
     * @param shouldStall           Whether the tab should display a desaturated thumbnail and
     *                              wait for the content layer to load.
     * @param canUseLiveTexture     Whether the tab can use a live texture when being displayed.
     */
    public void initFromHost(int backgroundColor, boolean shouldStall, boolean canUseLiveTexture,
            int toolbarBackgroundColor, int textBoxBackgroundColor, float textBoxAlpha) {
        set(BACKGROUND_COLOR, backgroundColor);
        set(TOOLBAR_BACKGROUND_COLOR, toolbarBackgroundColor);
        set(TEXT_BOX_BACKGROUND_COLOR, textBoxBackgroundColor);
        set(TEXT_BOX_ALPHA, textBoxAlpha);
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
     * Set the clipping offset. This apply on the scaled content.
     *
     * @param clippedX The top left X offset of the clipped rectangle.
     * @param clippedY The top left Y offset of the clipped rectangle.
     */
    public void setClipOffset(float clippedX, float clippedY) {
        set(CLIPPED_X, clippedX);
        set(CLIPPED_Y, clippedY);
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
     * @return The top left X offset of the clipped rectangle.
     */
    public float getClippedX() {
        return get(CLIPPED_X);
    }

    /**
     * @return The top left Y offset of the clipped rectangle.
     */
    public float getClippedY() {
        return get(CLIPPED_Y);
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
     * @return The height of the drawn content (clipped and scaled).
     */
    public float getFinalContentHeight() {
        return Math.min(get(CLIPPED_HEIGHT), getScaledContentHeight());
    }

    /**
     * @return The maximum width the content can be.
     */
    public float getMaxContentWidth() {
        return get(MAX_CONTENT_WIDTH);
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
        return Math.min(get(BORDER_ALPHA) * (1.0f - get(TOOLBAR_ALPHA)), get(ALPHA));
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
     * @param toolbarYOffset The y offset of the toolbar.
     */
    public void setToolbarYOffset(float toolbarYOffset) {
        set(TOOLBAR_Y_OFFSET, toolbarYOffset);
    }

    /**
     * @return The y offset of the toolbar.
     */
    public float getToolbarYOffset() {
        return get(TOOLBAR_Y_OFFSET);
    }

    /**
     * @param scale The scale of the side border (from 0 to 1).
     */
    public void setSideBorderScale(float scale) {
        set(SIDE_BORDER_SCALE, MathUtils.clamp(scale, 0.f, 1.f));
    }

    /**
     * @return The scale of the side border (from 0 to 1).
     */
    public float getSideBorderScale() {
        return get(SIDE_BORDER_SCALE);
    }

    /**
     * @param brightness The brightness value to apply to the tab.
     */
    public void setBrightness(float brightness) {
        set(BRIGHTNESS, brightness);
    }

    /**
     * @return The brightness of the tab.
     */
    public float getBrightness() {
        return get(BRIGHTNESS);
    }

    /**
     * @param drawDecoration Whether or not to draw decoration.
     */
    public void setDrawDecoration(boolean drawDecoration) {
        set(DECORATION_ALPHA, drawDecoration ? 1.0f : 0.0f);
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
     * @param visible True if the {@link LayoutTab} is visible and need to be drawn.
     */
    public void setVisible(boolean visible) {
        set(IS_VISIBLE, visible);
    }

    /**
     * @return True if the {@link LayoutTab} is visible and will be drawn.
     */
    public boolean isVisible() {
        return get(IS_VISIBLE);
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
     * @param alpha The alpha of the toolbar.
     */
    public void setToolbarAlpha(float alpha) {
        set(TOOLBAR_ALPHA, alpha);
    }

    /**
     * @return The alpha of the toolbar.
     */
    public float getToolbarAlpha() {
        return get(TOOLBAR_ALPHA);
    }

    /**
     * @param inset Whether or not to inset the top vertical component of the tab border or not.
     */
    public void setInsetBorderVertical(boolean inset) {
        set(INSET_BORDER_VERTICAL, inset);
    }

    /**
     * @return Whether or not to inset the top vertical component of the tab border or not.
     */
    public boolean insetBorderVertical() {
        return get(INSET_BORDER_VERTICAL);
    }

    public void setCloseButtonIsOnRight(boolean closeButtonIsOnRight) {
        set(CLOSE_BUTTON_IS_ON_RIGHT, closeButtonIsOnRight);
    }

    public boolean isCloseButtonOnRight() {
        return get(CLOSE_BUTTON_IS_ON_RIGHT);
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

    /**
     * @return The alpha value of the textbox in the toolbar.
     */
    public float getTextBoxAlpha() {
        return get(TEXT_BOX_ALPHA);
    }
}
