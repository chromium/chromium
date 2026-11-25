// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.components;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;

import org.chromium.build.annotations.NullMarked;
import org.chromium.cc.input.OffsetTag;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * {@link LayoutTab} is used to keep track of a thumbnail's bitmap and position and to draw itself
 * onto the GL canvas at the desired Y Offset.
 */
@NullMarked
public class LayoutTab extends PropertyModel {
    public static float sDpToPx;
    private static float sPxToDp;

    // TODO(crbug.com/40126260): Maybe make this a ReadableIntPropertyKey
    public static final WritableIntPropertyKey TAB_ID = new WritableIntPropertyKey();

    // Fields initialized in init()
    public static final WritableFloatPropertyKey X = new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey Y = new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey RENDER_X = new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey RENDER_Y = new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey ORIGINAL_CONTENT_WIDTH_IN_DP =
            new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey ORIGINAL_CONTENT_HEIGHT_IN_DP =
            new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey MAX_CONTENT_WIDTH = new WritableFloatPropertyKey();

    public static final WritableFloatPropertyKey MAX_CONTENT_HEIGHT =
            new WritableFloatPropertyKey();

    public static final WritableBooleanPropertyKey CAN_USE_LIVE_TEXTURE =
            new WritableBooleanPropertyKey();

    /** Whether initFromHost() has been called since the last call to init(). */
    public static final WritableBooleanPropertyKey INIT_FROM_HOST_CALLED =
            new WritableBooleanPropertyKey();

    // All the members bellow are initialized from the delayed initialization.
    //
    // Begin section --------------

    /** The color of the background of the tab. Used as the best approximation to fill in. */
    public static final WritableIntPropertyKey BACKGROUND_COLOR = new WritableIntPropertyKey();

    // End section --------------

    /**
     * The x-offset for the content layer. Primarily used to offset in response to side-anchored UI.
     */
    public static final PropertyModel.WritableFloatPropertyKey CONTENT_OFFSET_X =
            new PropertyModel.WritableFloatPropertyKey();

    /**
     * The y-offset for the content layer. Primarily used to offset in response to the top controls.
     */
    public static final PropertyModel.WritableFloatPropertyKey CONTENT_OFFSET_Y =
            new PropertyModel.WritableFloatPropertyKey();

    public static final PropertyModel.WritableBooleanPropertyKey IS_ACTIVE_LAYOUT =
            new WritableBooleanPropertyKey();

    /** The tag indicating that this layer should be moved by viz. */
    public static final PropertyModel.WritableObjectPropertyKey<OffsetTag> CONTENT_OFFSET_TAG =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                TAB_ID,
                X,
                Y,
                RENDER_X,
                RENDER_Y,
                ORIGINAL_CONTENT_WIDTH_IN_DP,
                ORIGINAL_CONTENT_HEIGHT_IN_DP,
                MAX_CONTENT_WIDTH,
                MAX_CONTENT_HEIGHT,
                CAN_USE_LIVE_TEXTURE,
                INIT_FROM_HOST_CALLED,
                BACKGROUND_COLOR,
                CONTENT_OFFSET_X,
                CONTENT_OFFSET_Y,
                IS_ACTIVE_LAYOUT,
                CONTENT_OFFSET_TAG
            };

    /**
     * Default constructor for a {@link LayoutTab}.
     *
     * @param tabId The id of the source {@link Tab}.
     * @param maxContentTextureWidth The maximum width for drawing the content in px.
     * @param maxContentTextureHeight The maximum height for drawing the content in px.
     */
    public LayoutTab(int tabId, int maxContentTextureWidth, int maxContentTextureHeight) {
        super(ALL_KEYS);

        set(TAB_ID, tabId);
        set(BACKGROUND_COLOR, Color.WHITE);

        init(maxContentTextureWidth, maxContentTextureHeight);
    }

    /**
     * Initializes a {@link LayoutTab} to its default value so it can be reused.
     *
     * @param maxContentTextureWidth The maximum width of the page content in px.
     * @param maxContentTextureHeight The maximum height of the page content in px.
     */
    public void init(int maxContentTextureWidth, int maxContentTextureHeight) {
        set(X, 0.0f);
        set(Y, 0.0f);
        set(RENDER_X, 0.0f);
        set(RENDER_Y, 0.0f);
        set(CAN_USE_LIVE_TEXTURE, true);
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
     * @param canUseLiveTexture Whether the tab can use a live texture when being displayed.
     */
    public void initFromHost(int backgroundColor, boolean canUseLiveTexture) {
        set(BACKGROUND_COLOR, backgroundColor);
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
     * @return The id of the tab, same as the id from the Tab in TabModel.
     */
    public int getId() {
        return get(TAB_ID);
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
     * @param originalContentWidth The maximum content width for the given orientation in px.
     * @param originalContentHeight The maximum content height for the given orientation in px.
     */
    public void setContentSize(int originalContentWidth, int originalContentHeight) {
        set(ORIGINAL_CONTENT_WIDTH_IN_DP, originalContentWidth * sPxToDp);
        set(ORIGINAL_CONTENT_HEIGHT_IN_DP, originalContentHeight * sPxToDp);
    }

    /**
     * @return Whether the tab can use a live texture to render.
     */
    public boolean canUseLiveTexture() {
        return get(CAN_USE_LIVE_TEXTURE);
    }

    /**
     * @return The color of the background of the tab. Used as the best approximation to fill in.
     */
    public int getBackgroundColor() {
        return get(BACKGROUND_COLOR);
    }
}
