// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.graphics.RectF;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimator;
import org.chromium.chrome.browser.compositor.animation.FloatProperty;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton.CompositorOnClickHandler;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.VirtualView;
import org.chromium.chrome.browser.compositor.overlays.strip.TabLoadTracker.TabLoadTrackerCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.resources.AndroidResourceType;
import org.chromium.ui.resources.LayoutResource;
import org.chromium.ui.resources.ResourceManager;

import java.util.List;

/**
 * {@link StripLayoutTab} is used to keep track of the strip position and rendering information for
 * a particular tab so it can draw itself onto the GL canvas.
 */
public class StripLayoutTab implements VirtualView {
    /** An observer interface for StripLayoutTab. */
    public interface Observer {
        /** @param visible Whether the StripLayoutTab is visible. */
        void onVisibilityChanged(boolean visible);
    }

    /**
     * Delegate for additional tab functionality.
     */
    public interface StripLayoutTabDelegate {
        /**
         * Handles tab click actions.
         * @param tab The tab clicked.
         */
        void handleTabClick(StripLayoutTab tab);

        /**
         * Handles close button click actions.
         * @param tab  The tab whose close button was clicked.
         * @param time The time the close button was clicked.
         */
        void handleCloseButtonClick(StripLayoutTab tab, long time);
    }

    /** A property for animations to use for changing the X offset of the tab. */
    public static final FloatProperty<StripLayoutTab> X_OFFSET =
            new FloatProperty<StripLayoutTab>("offsetX") {
                @Override
                public void setValue(StripLayoutTab object, float value) {
                    object.setOffsetX(value);
                }

                @Override
                public Float get(StripLayoutTab object) {
                    return object.getOffsetX();
                }
            };

    /** A property for animations to use for changing the Y offset of the tab. */
    public static final FloatProperty<StripLayoutTab> Y_OFFSET =
            new FloatProperty<StripLayoutTab>("offsetY") {
                @Override
                public void setValue(StripLayoutTab object, float value) {
                    object.setOffsetY(value);
                }

                @Override
                public Float get(StripLayoutTab object) {
                    return object.getOffsetY();
                }
            };

    /** A property for animations to use for changing the width of the tab. */
    public static final FloatProperty<StripLayoutTab> WIDTH =
            new FloatProperty<StripLayoutTab>("width") {
                @Override
                public void setValue(StripLayoutTab object, float value) {
                    object.setWidth(value);
                }

                @Override
                public Float get(StripLayoutTab object) {
                    return object.getHeight();
                }
            };

    // Behavior Constants
    private static final float VISIBILITY_FADE_CLOSE_BUTTON_PERCENTAGE = 0.99f;

    // Animation/Timer Constants
    private static final int ANIM_TAB_CLOSE_BUTTON_FADE_MS = 150;

    // Close button width
    private static final int CLOSE_BUTTON_WIDTH_DP = 36;

    private int mId = Tab.INVALID_TAB_ID;

    private final Context mContext;
    private final StripLayoutTabDelegate mDelegate;
    private final TabLoadTracker mLoadTracker;
    private final LayoutRenderHost mRenderHost;
    private final LayoutUpdateHost mUpdateHost;
    private final TintedCompositorButton mCloseButton;

    private boolean mVisible = true;
    private boolean mIsDying;
    private boolean mCanShowCloseButton = true;
    private final boolean mIncognito;
    private float mContentOffsetX;
    private float mVisiblePercentage = 1.f;
    private String mAccessibilityDescription;

    // Ideal intermediate parameters
    private float mIdealX;
    private float mTabOffsetX;
    private float mTabOffsetY;

    // Actual draw parameters
    private float mDrawX;
    private float mDrawY;
    private float mWidth;
    private float mHeight;
    private final RectF mTouchTarget = new RectF();

    private boolean mShowingCloseButton = true;

    // Content Animations
    private CompositorAnimator mButtonOpacityAnimation;

    private float mLoadingSpinnerRotationDegrees;

    // Preallocated
    private final RectF mClosePlacement = new RectF();

    private ObserverList<Observer> mObservers = new ObserverList<>();

    /**
     * Create a {@link StripLayoutTab} that represents the {@link Tab} with an id of
     * {@code id}.
     *
     * @param context An Android context for accessing system resources.
     * @param id The id of the {@link Tab} to visually represent.
     * @param delegate The delegate for additional strip tab functionality.
     * @param loadTrackerCallback The {@link TabLoadTrackerCallback} to be notified of loading state
     *                            changes.
     * @param renderHost The {@link LayoutRenderHost}.
     * @param incognito Whether or not this layout tab is incognito.
     */
    public StripLayoutTab(Context context, int id, StripLayoutTabDelegate delegate,
            TabLoadTrackerCallback loadTrackerCallback, LayoutRenderHost renderHost,
            LayoutUpdateHost updateHost, boolean incognito) {
        mId = id;
        mContext = context;
        mDelegate = delegate;
        mLoadTracker = new TabLoadTracker(id, loadTrackerCallback);
        mRenderHost = renderHost;
        mUpdateHost = updateHost;
        mIncognito = incognito;
        CompositorOnClickHandler closeClickAction = new CompositorOnClickHandler() {
            @Override
            public void onClick(long time) {
                mDelegate.handleCloseButtonClick(StripLayoutTab.this, time);
            }
        };
        mCloseButton = new TintedCompositorButton(
                context, 0, 0, closeClickAction, R.drawable.btn_tab_close_normal);
        mCloseButton.setTintResources(R.color.default_icon_color, R.color.default_icon_color_blue,
                R.color.default_icon_color_white, R.color.modern_blue_300);
        mCloseButton.setIncognito(mIncognito);
        mCloseButton.setBounds(getCloseRect());
        mCloseButton.setClickSlop(0.f);
    }

    /** @param observer The observer to add. */
    @VisibleForTesting
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /** @param observer The observer to remove. */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Get a list of virtual views for accessibility events.
     *
     * @param views     A List to populate with virtual views.
     */
    public void getVirtualViews(List<VirtualView> views) {
        views.add(this);
        if (mShowingCloseButton) views.add(mCloseButton);
    }

    /**
     * Set strip tab and close button accessibility description.
     * @param description   A description for accessibility events.
     * @param title The title of the tab.
     */
    public void setAccessibilityDescription(String description, String title) {
        mAccessibilityDescription = description;
        String closeButtonDescription = ContextUtils.getApplicationContext().getString(
                R.string.accessibility_tabstrip_btn_close_tab, title);
        mCloseButton.setAccessibilityDescription(closeButtonDescription, closeButtonDescription);
    }

    @Override
    public String getAccessibilityDescription() {
        return mAccessibilityDescription;
    }

    @Override
    public void getTouchTarget(RectF target) {
        target.set(mTouchTarget);
    }

    @Override
    public boolean checkClicked(float x, float y) {
        // Since both the close button as well as the tab inhabit the same coordinates, the tab
        // should not consider itself hit if the close button is also hit, since it is on top.
        if (checkCloseHitTest(x, y)) return false;
        return mTouchTarget.contains(x, y);
    }

    @Override
    public void handleClick(long time) {
        mDelegate.handleTabClick(this);
    }

    /**
     * @return The id of the {@link Tab} this {@link StripLayoutTab} represents.
     */
    public int getId() {
        return mId;
    }

    /**
     * @return The Android resource that represents the tab background.
     */
    public int getResourceId() {
        return R.drawable.bg_tabstrip_tab;
    }

    /**
     * @return The Android resource that represents the tab outline.
     */
    public int getOutlineResourceId() {
        return R.drawable.bg_tabstrip_background_tab_outline;
    }

    /**
     * @param foreground Whether or not this tab is a foreground tab.
     * @return The tint color resource that represents the tab background.
     */
    public int getTint(boolean foreground) {
        int tint = mIncognito ? R.color.compositor_background_tab_bg_incognito
                              : R.color.compositor_background_tab_bg;
        if (foreground) {
            tint = mIncognito ? R.color.default_bg_color_dark_elev_3
                              : R.color.default_bg_color_elev_3;
        }

        return mContext.getResources().getColor(tint);
    }

    /**
     * @param foreground Whether or not this tab is a foreground tab.
     * @return The tint color resource that represents the tab outline.
     */
    public int getOutlineTint(boolean foreground) {
        int tint = mIncognito ? R.color.compositor_background_tab_outline_incognito
                              : R.color.compositor_background_tab_outline;
        if (foreground) {
            tint = mIncognito ? R.color.default_bg_color_dark_elev_3
                              : R.color.default_bg_color_elev_3;
        }

        return mContext.getResources().getColor(tint);
    }

    /**
     * @param visible Whether or not this {@link StripLayoutTab} should be drawn.
     */
    public void setVisible(boolean visible) {
        mVisible = visible;
        if (!visible) {
            mUpdateHost.releaseResourcesForTab(mId);
        }
        for (Observer observer : mObservers) {
            observer.onVisibilityChanged(mVisible);
        }
    }

    /**
     * @return Whether or not this {@link StripLayoutTab} should be drawn.
     */
    public boolean isVisible() {
        return mVisible;
    }

    /**
     * Mark this tab as in the process of dying.  This lets us track which tabs are dead after
     * animations.
     * @param isDying Whether or not the tab is dying.
     */
    public void setIsDying(boolean isDying) {
        mIsDying = isDying;
    }

    /**
     * @return Whether or not the tab is dying.
     */
    public boolean isDying() {
        return mIsDying;
    }

    /**
     * @return Whether or not this tab should be visually represented as loading.
     */
    public boolean isLoading() {
        return mLoadTracker.isLoading();
    }

    /**
     * @return The rotation of the loading spinner in degrees.
     */
    public float getLoadingSpinnerRotation() {
        return mLoadingSpinnerRotationDegrees;
    }

    /**
     * Additive spinner rotation update.
     * @param rotation The amount to rotate the spinner by in degrees.
     */
    public void addLoadingSpinnerRotation(float rotation) {
        mLoadingSpinnerRotationDegrees = (mLoadingSpinnerRotationDegrees + rotation) % 1080;
    }

    /**
     * Called when this tab has started loading.
     */
    public void pageLoadingStarted() {
        mLoadTracker.pageLoadingStarted();
    }

    /**
     * Called when this tab has finished loading.
     */
    public void pageLoadingFinished() {
        mLoadTracker.pageLoadingFinished();
    }

    /**
     * Called when this tab has started loading resources.
     */
    public void loadingStarted() {
        mLoadTracker.loadingStarted();
    }

    /**
     * Called when this tab has finished loading resources.
     */
    public void loadingFinished() {
        mLoadTracker.loadingFinished();
    }

    /**
     * @param offsetX How far to offset the tab content (favicons and title).
     */
    public void setContentOffsetX(float offsetX) {
        mContentOffsetX = MathUtils.clamp(offsetX, 0.f, mWidth);
    }

    /**
     * @return How far to offset the tab content (favicons and title).
     */
    public float getContentOffsetX() {
        return mContentOffsetX;
    }

    /**
     * @param visiblePercentage How much of the tab is visible (not overlapped by other tabs).
     */
    public void setVisiblePercentage(float visiblePercentage) {
        mVisiblePercentage = visiblePercentage;
        checkCloseButtonVisibility(true);
    }

    /**
     * @return How much of the tab is visible (not overlapped by other tabs).
     */
    @VisibleForTesting
    public float getVisiblePercentage() {
        return mVisiblePercentage;
    }

    /**
     * @param show Whether or not the close button is allowed to be shown.
     */
    public void setCanShowCloseButton(boolean show) {
        mCanShowCloseButton = show;
        checkCloseButtonVisibility(true);
    }

    /**
     * @param x The actual position in the strip, taking into account stacking, scrolling, etc.
     */
    public void setDrawX(float x) {
        mCloseButton.setX(mCloseButton.getX() + (x - mDrawX));
        mDrawX = x;
        mTouchTarget.left = mDrawX;
        mTouchTarget.right = mDrawX + mWidth;
    }

    /**
     * @return The actual position in the strip, taking into account stacking, scrolling, etc.
     */
    public float getDrawX() {
        return mDrawX;
    }

    /**
     * @param y The vertical position for the tab.
     */
    public void setDrawY(float y) {
        mCloseButton.setY(mCloseButton.getY() + (y - mDrawY));
        mDrawY = y;
        mTouchTarget.top = mDrawY;
        mTouchTarget.bottom = mDrawY + mHeight;
    }

    /**
     * @return The vertical position for the tab.
     */
    public float getDrawY() {
        return mDrawY;
    }

    /**
     * @param width The width of the tab.
     */
    public void setWidth(float width) {
        mWidth = width;
        resetCloseRect();
        mTouchTarget.right = mDrawX + mWidth;
    }

    /**
     * @return The width of the tab.
     */
    public float getWidth() {
        return mWidth;
    }

    /**
     * @param height The height of the tab.
     */
    public void setHeight(float height) {
        mHeight = height;
        resetCloseRect();
        mTouchTarget.bottom = mDrawY + mHeight;
    }

    /**
     * @return The height of the tab.
     */
    public float getHeight() {
        return mHeight;
    }

    /**
     * @param closePressed The current pressed state of the attached button.
     */
    public void setClosePressed(boolean closePressed) {
        mCloseButton.setPressed(closePressed);
    }

    /**
     * @return The current pressed state of the close button.
     */
    public boolean getClosePressed() {
        return mCloseButton.isPressed();
    }

    /**
     * @return The close button for this tab.
     */
    public TintedCompositorButton getCloseButton() {
        return mCloseButton;
    }

    /**
     * This represents how much this tab's width should be counted when positioning tabs in the
     * stack.  As tabs close or open, their width weight is increased.  They visually take up
     * the same amount of space but the other tabs will smoothly move out of the way to make room.
     * @return The weight from 0 to 1 that the width of this tab should have on the stack.
     */
    public float getWidthWeight() {
        return MathUtils.clamp(1.f - mDrawY / mHeight, 0.f, 1.f);
    }

    /**
     * @param x The x position of the position to test.
     * @param y The y position of the position to test.
     * @return Whether or not {@code x} and {@code y} is over the close button for this tab and
     *         if the button can be clicked.
     */
    public boolean checkCloseHitTest(float x, float y) {
        return mShowingCloseButton ? mCloseButton.checkClicked(x, y) : false;
    }

    /**
     * This is used to help calculate the tab's position and is not used for rendering.
     * @param offsetX The offset of the tab (used for drag and drop, slide animating, etc).
     */
    public void setOffsetX(float offsetX) {
        mTabOffsetX = offsetX;
    }

    /**
     * This is used to help calculate the tab's position and is not used for rendering.
     * @return The offset of the tab (used for drag and drop, slide animating, etc).
     */
    public float getOffsetX() {
        return mTabOffsetX;
    }

    /**
     * This is used to help calculate the tab's position and is not used for rendering.
     * @param x The ideal position, in an infinitely long strip, of this tab.
     */
    public void setIdealX(float x) {
        mIdealX = x;
    }

    /**
     * This is used to help calculate the tab's position and is not used for rendering.
     * @return The ideal position, in an infinitely long strip, of this tab.
     */
    public float getIdealX() {
        return mIdealX;
    }

    /**
     * This is used to help calculate the tab's position and is not used for rendering.
     * @param offsetY The vertical offset of the tab.
     */
    public void setOffsetY(float offsetY) {
        mTabOffsetY = offsetY;
    }

    /**
     * This is used to help calculate the tab's position and is not used for rendering.
     * @return The vertical offset of the tab.
     */
    public float getOffsetY() {
        return mTabOffsetY;
    }

    /**
     * Finishes any content animations currently owned and running on this StripLayoutTab.
     */
    public void finishAnimation() {
        if (mButtonOpacityAnimation != null) mButtonOpacityAnimation.end();
    }

    private void resetCloseRect() {
        RectF closeRect = getCloseRect();
        mCloseButton.setWidth(closeRect.width());
        mCloseButton.setHeight(closeRect.height());
        mCloseButton.setX(closeRect.left);
        mCloseButton.setY(closeRect.top);
    }

    private RectF getCloseRect() {
        if (!LocalizationUtils.isLayoutRtl()) {
            mClosePlacement.left = getWidth() - CLOSE_BUTTON_WIDTH_DP;
            mClosePlacement.right = mClosePlacement.left + CLOSE_BUTTON_WIDTH_DP;
        } else {
            mClosePlacement.left = 0;
            mClosePlacement.right = CLOSE_BUTTON_WIDTH_DP;
        }

        mClosePlacement.top = 0;
        mClosePlacement.bottom = getHeight();

        float xOffset = 0;
        ResourceManager manager = mRenderHost.getResourceManager();
        if (manager != null) {
            LayoutResource resource =
                    manager.getResource(AndroidResourceType.STATIC, getResourceId());
            if (resource != null) {
                xOffset = LocalizationUtils.isLayoutRtl()
                        ? resource.getPadding().left
                        : -(resource.getBitmapSize().width() - resource.getPadding().right);
            }
        }

        mClosePlacement.offset(getDrawX() + xOffset, getDrawY());
        return mClosePlacement;
    }

    // TODO(dtrainor): Don't animate this if we're selecting or deselecting this tab.
    private void checkCloseButtonVisibility(boolean animate) {
        boolean shouldShow =
                mCanShowCloseButton && mVisiblePercentage > VISIBILITY_FADE_CLOSE_BUTTON_PERCENTAGE;

        if (shouldShow != mShowingCloseButton) {
            float opacity = shouldShow ? 1.f : 0.f;
            if (animate) {
                if (mButtonOpacityAnimation != null) mButtonOpacityAnimation.end();
                mButtonOpacityAnimation = CompositorAnimator.ofFloatProperty(
                        mUpdateHost.getAnimationHandler(), mCloseButton, CompositorButton.OPACITY,
                        mCloseButton.getOpacity(), opacity, ANIM_TAB_CLOSE_BUTTON_FADE_MS);
                mButtonOpacityAnimation.addListener(new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mButtonOpacityAnimation = null;
                    }
                });
                mButtonOpacityAnimation.start();
            } else {
                mCloseButton.setOpacity(opacity);
            }
            mShowingCloseButton = shouldShow;
            if (!mShowingCloseButton) mCloseButton.setPressed(false);
        }
    }
}
