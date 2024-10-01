// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.graphics.Color;
import android.graphics.RectF;
import android.util.FloatProperty;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.MathUtils;
import org.chromium.base.ObserverList;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton.ButtonType;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.overlays.strip.TabLoadTracker.TabLoadTrackerCallback;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.util.ColorUtils;

import java.util.List;
import java.util.Optional;

/**
 * {@link StripLayoutTab} is used to keep track of the strip position and rendering information for
 * a particular tab so it can draw itself onto the GL canvas.
 */
public class StripLayoutTab extends StripLayoutView {
    /** An observer interface for StripLayoutTab. */
    public interface Observer {
        /**
         * @param newVisibility Whether the StripLayoutTab is visible.
         */
        void onVisibilityChanged(boolean newVisibility);
    }

    /** A property for animations to use for changing the Y offset of the tab. */
    public static final FloatProperty<StripLayoutTab> Y_OFFSET =
            new FloatProperty<>("offsetY") {
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
            new FloatProperty<>("width") {
                @Override
                public void setValue(StripLayoutTab object, float value) {
                    object.setWidth(value);
                }

                @Override
                public Float get(StripLayoutTab object) {
                    return object.getWidth();
                }
            };

    /** A property for animations to use for changing the bottom margin of the tab. */
    public static final FloatProperty<StripLayoutTab> BOTTOM_MARGIN =
            new FloatProperty<>("bottomMargin") {
                @Override
                public void setValue(StripLayoutTab object, float value) {
                    object.setBottomMargin(value);
                }

                @Override
                public Float get(StripLayoutTab object) {
                    return object.getBottomMargin();
                }
            };

    /** A property for animations to use for changing the trailingMargin of the tab. */
    public static final FloatProperty<StripLayoutTab> TRAILING_MARGIN =
            new FloatProperty<>("trailingMargin") {
                @Override
                public void setValue(StripLayoutTab object, float value) {
                    object.setTrailingMargin(value);
                }

                @Override
                public Float get(StripLayoutTab object) {
                    return object.getTrailingMargin();
                }
            };

    /** A property for animations to use for changing the opacity of the tab. */
    public static final FloatProperty<StripLayoutTab> OPACITY =
            new FloatProperty<>("opacity") {
                @Override
                public void setValue(StripLayoutTab object, float value) {
                    object.setContainerOpacity(value);
                }

                @Override
                public Float get(StripLayoutTab object) {
                    return object.getContainerOpacity();
                }
            };

    // Animation/Timer Constants
    private static final int ANIM_TAB_CLOSE_BUTTON_FADE_MS = 150;

    // Close Button Constants
    // Close button padding value comes from the built-in padding in the source png.
    private static final int CLOSE_BUTTON_PADDING_DP = 7;
    private static final int CLOSE_BUTTON_OFFSET_X = 12;
    private static final int CLOSE_BUTTON_WIDTH_DP = 48;

    // Strip Tab Offset Constants
    private static final float TOP_MARGIN_DP = 2.f;
    private static final float FOLIO_CONTENT_OFFSET_Y = 8.f;
    protected static final float FOLIO_FOOT_LENGTH_DP = 16.f;

    // Visibility Constants.
    private static final float FAVICON_WIDTH = 16.f;
    protected static final float MIN_WIDTH = FAVICON_WIDTH + (FOLIO_FOOT_LENGTH_DP * 2);

    // Divider Constants
    private static final int DIVIDER_OFFSET_X = 13;

    // Close button hover highlight alpha
    private static final float CLOSE_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY = 0.12f;
    private static final float CLOSE_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY = 0.08f;
    @VisibleForTesting static final float DIVIDER_FOLIO_LIGHT_OPACITY = 0.2f;

    // Tab's ID this view refers to.
    private int mTabId;

    private final Context mContext;
    private final TabLoadTracker mLoadTracker;
    private final LayoutUpdateHost mUpdateHost;
    private TintedCompositorButton mCloseButton;

    private boolean mIsDying;
    private boolean mIsClosed;
    private boolean mIsReordering;
    private boolean mIsDraggedOffStrip;
    private boolean mCanShowCloseButton = true;
    private boolean mFolioAttached = true;
    private boolean mStartDividerVisible;
    private boolean mEndDividerVisible;
    private float mBottomMargin;
    private float mContainerOpacity;

    // For avoiding unnecessary accessibility description updates.
    private Optional<String> mCachedA11yDescriptionTitle = Optional.empty();
    private @StringRes int mCachedA11yTabstripIdentifierResId;

    // Ideal intermediate parameters
    private float mTabOffsetY;
    private float mTrailingMargin;

    // Startup parameters
    private boolean mIsPlaceholder;

    private boolean mShowingCloseButton = true;

    // Content Animations
    private CompositorAnimator mButtonOpacityAnimation;

    private float mLoadingSpinnerRotationDegrees;

    // Preallocated
    private final RectF mClosePlacement = new RectF();

    private ObserverList<Observer> mObservers = new ObserverList<>();

    /**
     * Create a {@link StripLayoutTab} that represents the {@link Tab} with an id of {@code id}.
     *
     * @param context An Android context for accessing system resources.
     * @param id The id of the {@link Tab} to visually represent.
     * @param loadTrackerCallback The {@link TabLoadTrackerCallback} to be notified of loading state
     *     changes.
     * @param updateHost The {@link LayoutRenderHost}.
     * @param incognito Whether or not this layout tab is incognito.
     */
    public StripLayoutTab(
            Context context,
            int id,
            StripLayoutViewOnClickHandler clickHandler,
            TabLoadTrackerCallback loadTrackerCallback,
            LayoutUpdateHost updateHost,
            boolean incognito) {
        super(incognito, clickHandler);
        mTabId = id;
        mContext = context;
        mLoadTracker = new TabLoadTracker(id, loadTrackerCallback);
        mUpdateHost = updateHost;
        mCloseButton =
                new TintedCompositorButton(
                        context,
                        ButtonType.TAB_CLOSE,
                        this,
                        /* width= */ 0,
                        /* height= */ 0,
                        clickHandler,
                        R.drawable.btn_tab_close_normal);
        mCloseButton.setTintResources(
                R.color.default_icon_color_tint_list,
                R.color.default_icon_color_tint_list,
                R.color.default_icon_color_light,
                R.color.default_icon_color_light);

        mCloseButton.setBackgroundResourceId(R.drawable.tab_close_button_bg);
        @ColorInt
        int apsBackgroundHoveredTint =
                ColorUtils.setAlphaComponentWithFloat(
                        SemanticColorUtils.getDefaultTextColor(context),
                        CLOSE_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY);
        @ColorInt
        int apsBackgroundPressedTint =
                ColorUtils.setAlphaComponentWithFloat(
                        SemanticColorUtils.getDefaultTextColor(context),
                        CLOSE_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY);

        @ColorInt
        int apsBackgroundIncognitoHoveredTint =
                ColorUtils.setAlphaComponentWithFloat(
                        context.getColor(R.color.tab_strip_button_hover_bg_color),
                        CLOSE_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY);
        @ColorInt
        int apsBackgroundIncognitoPressedTint =
                ColorUtils.setAlphaComponentWithFloat(
                        context.getColor(R.color.tab_strip_button_hover_bg_color),
                        CLOSE_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY);

        // Only set color for hover bg.
        mCloseButton.setBackgroundTint(
                Color.TRANSPARENT,
                Color.TRANSPARENT,
                Color.TRANSPARENT,
                Color.TRANSPARENT,
                apsBackgroundHoveredTint,
                apsBackgroundPressedTint,
                apsBackgroundIncognitoHoveredTint,
                apsBackgroundIncognitoPressedTint);

        mCloseButton.setIncognito(incognito);
        mCloseButton.setBounds(getCloseRect());
        mCloseButton.setClickSlop(0.f);
    }

    /**
     * @param observer The observer to add.
     */
    @VisibleForTesting
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /** @param observer The observer to remove. */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void getVirtualViews(List<VirtualView> views) {
        if (isCollapsed() || mIsDying) return;
        super.getVirtualViews(views);
        if (mShowingCloseButton) mCloseButton.getVirtualViews(views);
    }

    /**
     * Set strip tab and close button accessibility description.
     *
     * @param description A description for accessibility events.
     * @param title The title of the tab.
     */
    public void setAccessibilityDescription(
            String description,
            @Nullable String title,
            @StringRes int newA11yTabstripIdentifierResId) {
        super.setAccessibilityDescription(description);
        String closeButtonDescription =
                ContextUtils.getApplicationContext()
                        .getString(R.string.accessibility_tabstrip_btn_close_tab, title);
        mCloseButton.setAccessibilityDescription(closeButtonDescription, closeButtonDescription);

        // Cache the title + resource ID used to create this description so we can avoid unnecessary
        // updates.
        mCachedA11yDescriptionTitle = Optional.ofNullable(title);
        mCachedA11yTabstripIdentifierResId = newA11yTabstripIdentifierResId;
    }

    /**
     * @param newTitle The title that would be used in the new accessibility description.
     * @param newA11yTabstripIdentifierResId The String resource ID the description would use.
     * @return True if the accessibility description should be updated, false if the resulting
     *     description would match the current description.
     */
    public boolean needsAccessibilityDescriptionUpdate(
            @Nullable String newTitle, @StringRes int newA11yTabstripIdentifierResId) {
        if (mCachedA11yTabstripIdentifierResId != newA11yTabstripIdentifierResId) {
            // A different resource ID was used to create the description.
            return true;
        }
        if (mCachedA11yDescriptionTitle.isPresent() && newTitle == null) {
            // Going from non-null title to null title.
            return true;
        }
        if (newTitle != null && !newTitle.equals(mCachedA11yDescriptionTitle.orElse(null))) {
            // Going from non-null title to some other title (may even be null).
            return true;
        }
        // The page title is the same as before and the resource ID we'd use to construct the
        // a11y description is the same, so no need to update it.
        return false;
    }

    @Override
    public boolean checkClickedOrHovered(float x, float y) {
        // Since both the close button as well as the tab inhabit the same coordinates, the tab
        // should not consider itself hit if the close button is also hit, since it is on top.
        if (checkCloseHitTest(x, y)) return false;
        return super.checkClickedOrHovered(x, y);
    }

    /**
     * Marks if we are currently reordering this tab.
     *
     * @param isReordering Whether the tab is reordering.
     */
    public void setIsReordering(boolean isReordering) {
        mIsReordering = isReordering;
    }

    /**
     * Marks if the tab has been dragged off the strip for drag and drop.
     *
     * @param isDraggedOffStrip Whether the tab is dragged off the strip.
     */
    public void setIsDraggedOffStrip(boolean isDraggedOffStrip) {
        mIsDraggedOffStrip = isDraggedOffStrip;
    }

    /**
     * @return Whether the tab is dragged off the strip.
     */
    public boolean isDraggedOffStrip() {
        return mIsDraggedOffStrip;
    }

    /**
     * Marks if tab container is attached to the toolbar for the Tab Strip Redesign folio treatment.
     * @param folioAttached Whether the tab should be attached or not.
     */
    public void setFolioAttached(boolean folioAttached) {
        mFolioAttached = folioAttached;
    }

    boolean getFolioAttached() {
        return mFolioAttached;
    }

    void setCloseButtonForTesting(TintedCompositorButton closeButton) {
        mCloseButton = closeButton;
    }

    void setShowingCloseButtonForTesting(boolean showingCloseButton) {
        mShowingCloseButton = showingCloseButton;
    }

    /** Sets the id of the {@link Tab} this {@link StripLayoutTab} represents. */
    public void setTabId(int id) {
        mTabId = id;
    }

    /**
     * @return The id of the {@link Tab} this {@link StripLayoutTab} represents.
     */
    public int getTabId() {
        return mTabId;
    }

    /**
     * @return The Android resource that represents the tab background.
     */
    public @DrawableRes int getResourceId() {
        if (!mFolioAttached || mIsPlaceholder) {
            return TabUiThemeUtil.getDetachedResource();
        } else {
            return TabUiThemeUtil.getTabResource();
        }
    }

    /**
     * @return The Android resource that represents the tab outline.
     */
    // TODO(crbug.com/329561631) Add tint for selected tab outline.
    public @DrawableRes int getOutlineResourceId() {
        return R.drawable.tab_group_outline;
    }

    /**
     * @return The Android resource that represents the tab divider.
     */
    public @DrawableRes int getDividerResourceId() {
        return R.drawable.bg_tabstrip_tab_divider;
    }

    /**
     * @param foreground Whether or not this tab is a foreground tab.
     * @param hovered Whether or not this tab is hovered on.
     * @return The tint color resource that represents the tab background. A foreground tab will
     *     have the same tint irrespective of its hover state.
     */
    public @ColorInt int getTint(boolean foreground, boolean hovered) {
        // TODO(crbug.com/40888366): Avoid calculating every time. Instead, store the tab's
        //  color and only re-determine when the color could have changed (i.e. on selection).
        return TabUiThemeUtil.getTabStripContainerColor(
                mContext, isIncognito(), foreground, mIsReordering, mIsPlaceholder, hovered);
    }

    /**
     * @return The tint color resource for the tab divider.
     */
    public @ColorInt int getDividerTint() {
        if (isIncognito()) {
            return mContext.getColor(R.color.divider_line_bg_color_light);
        }

        if (!ColorUtils.inNightMode(mContext) && !isIncognito()) {
            // This color will not be used at full opacity. We can't set this using the alpha
            // component of the {@code @ColorInt}, since it is ignored when loading resources
            // with a specified tint in the CC layer (instead retaining the alpha of the original
            // image). Instead, this is reflected by setting the opacity of the divider itself.
            // See https://crbug.com/1373634.
            return ColorUtils.setAlphaComponentWithFloat(
                    SemanticColorUtils.getDefaultIconColorAccent1(mContext),
                    DIVIDER_FOLIO_LIGHT_OPACITY);
        }

        return SemanticColorUtils.getDividerLineBgColor(mContext);
    }

    /**
     * @param visible Visibility of tab's start divider.
     */
    public void setStartDividerVisible(boolean visible) {
        mStartDividerVisible = visible;
    }

    /**
     * @return Visibility of tab's start divider.
     */
    public boolean isStartDividerVisible() {
        return mStartDividerVisible;
    }

    /**
     * @param visible Visibility of end divider.
     */
    public void setEndDividerVisible(boolean visible) {
        mEndDividerVisible = visible;
    }

    /**
     * @return Visibility of tab's end divider.
     */
    public boolean isEndDividerVisible() {
        return mEndDividerVisible;
    }

    @Override
    public void onVisibilityChanged(boolean newVisibility) {
        if (!newVisibility) {
            // TODO(crbug.com/358205243): Re-build the bitmaps if the tab becomes visible here.
            mUpdateHost.releaseResourcesForTab(mTabId);
        }

        for (Observer observer : mObservers) {
            observer.onVisibilityChanged(isVisible());
        }
    }

    @Override
    public void setIncognito(boolean incognito) {
        assert false : "Incognito state of a tab cannot change";
    }

    /**
     * Mark this tab as in the process of dying. This lets us track which tabs are closed after
     * animations.
     *
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
     * Mark this tab as closed. We can't immediately remove the tab from the TabModel, since doing
     * so may result in a concurrent modification exception. Track here to treat as removed.
     *
     * @param isClosed Whether or not the tab should be treated as closed.
     */
    public void setIsClosed(boolean isClosed) {
        mIsClosed = isClosed;
    }

    /**
     * Closed tabs should have been removed from the TabModel and mStripTabs. We can't do so
     * immediately, however, since we may try to do so when we are committing all tab closures,
     * resulting in a concurrent modification exception. We instead post the removal and mark such
     * tabs as closed.
     *
     * @return Whether or not the tab should be treated as closed.
     */
    public boolean isClosed() {
        return mIsClosed;
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

    /** Called when this tab has started loading resources. */
    public void loadingStarted() {
        mLoadTracker.loadingStarted();
    }

    /** Called when this tab has finished loading resources. */
    public void loadingFinished() {
        mLoadTracker.loadingFinished();
    }

    /**
     * @param opacity The fraction (from 0.f to 1.f) of how opaque the tab container should be.
     */
    public void setContainerOpacity(float opacity) {
        mContainerOpacity = opacity;
    }

    /**
     * @return The fraction (from 0.f to 1.f) of how opaque the tab container should be.
     */
    public float getContainerOpacity() {
        return mContainerOpacity;
    }

    /**
     * @return How far to vertically offset the tab content.
     */
    public float getContentOffsetY() {
        return FOLIO_CONTENT_OFFSET_Y - (TOP_MARGIN_DP / 2);
    }

    /**
     * @return The trailing offset for the tab divider.
     */
    public float getDividerOffsetX() {
        return DIVIDER_OFFSET_X;
    }

    /**
     * @param bottomMargin How far to offset the bottom of the tab container from the toolbar.
     */
    public void setBottomMargin(float bottomMargin) {
        mBottomMargin = bottomMargin;
    }

    /**
     * @return How far to offset the bottom of the tab container from the toolbar.
     */
    public float getBottomMargin() {
        return mBottomMargin;
    }

    /**
     * @return How far to offset the top of the tab container from the top of the tab strip.
     */
    public float getTopMargin() {
        return TOP_MARGIN_DP;
    }

    /**
     * @param show Whether or not the close button is allowed to be shown.
     * @param animate Whether or not to animate the close button showing/hiding.
     */
    public void setCanShowCloseButton(boolean show, boolean animate) {
        mCanShowCloseButton = show;
        checkCloseButtonVisibility(animate);
    }

    /** {@link StripLayoutView} Implementation */
    @Override
    public void setDrawX(float x) {
        mCloseButton.setDrawX(mCloseButton.getDrawX() + (x - getDrawX()));
        super.setDrawX(x);
        float[] insetsX = getTouchTargetInsetsX();
        super.setTouchTargetInsets(insetsX[0], null, insetsX[1], null);
    }

    @Override
    public void setDrawY(float y) {
        mCloseButton.setDrawY(mCloseButton.getDrawY() + (y - getDrawY()));
        super.setDrawY(y);
    }

    @Override
    public void setWidth(float width) {
        super.setWidth(width);
        resetCloseRect();
        float[] insetsX = getTouchTargetInsetsX();
        super.setTouchTargetInsets(null, null, insetsX[1], null);
    }

    @Override
    public void setHeight(float height) {
        super.setHeight(height);
        resetCloseRect();
    }

    /**
     * @param closePressed The current pressed state of the attached button.
     */
    public void setClosePressed(boolean closePressed, boolean isPressedFromMouse) {
        mCloseButton.setPressed(closePressed, isPressedFromMouse);
    }

    /**
     * @param closeHovered The current hovered state of the attached button.
     */
    public void setCloseHovered(boolean closeHovered) {
        mCloseButton.setHovered(closeHovered);
    }

    /**
     * @return The current hovered state of the close button.
     */
    public boolean isCloseHovered() {
        return mCloseButton.isHovered();
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
     * stack. As tabs close or open, their width weight is increased. They visually take up the same
     * amount of space but the other tabs will smoothly move out of the way to make room.
     *
     * @return The weight from 0 to 1 that the width of this tab should have on the stack.
     */
    public float getWidthWeight() {
        return MathUtils.clamp(1.f - getDrawY() / getHeight(), 0.f, 1.f);
    }

    /**
     * @param x The x position of the position to test.
     * @param y The y position of the position to test.
     * @return Whether or not {@code x} and {@code y} is over the close button for this tab and if
     *     the button can be clicked.
     */
    public boolean checkCloseHitTest(float x, float y) {
        return mShowingCloseButton ? mCloseButton.checkClickedOrHovered(x, y) : false;
    }

    /**
     * This is used to help calculate the tab's position and is not used for rendering.
     *
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
     * This is used to help calculate the tab's position and is not used for rendering.
     * @param trailingMargin The trailing margin of the tab (used for margins around tab groups
     *                       when reordering, etc.).
     */
    public void setTrailingMargin(float trailingMargin) {
        mTrailingMargin = trailingMargin;
    }

    /**
     * This is used to help calculate the tab's position and is not used for rendering.
     * @return The trailing margin of the tab.
     */
    public float getTrailingMargin() {
        return mTrailingMargin;
    }

    /**
     * This is used to determine if the tab is a placeholder or not. If it is a placeholder, it will
     * show as an empty tab on the tab strip (without tab contents, such as title & favicon,
     * generated).
     * @param isPlaceholder Whether or not the tab is a placeholder used on startup.
     */
    public void setIsPlaceholder(boolean isPlaceholder) {
        mIsPlaceholder = isPlaceholder;
        checkCloseButtonVisibility(false);
    }

    /**
     * This is used to determine if the tab is a placeholder or not. If it is a placeholder, it will
     * show as an empty tab on the tab strip (without tab contents, such as title & favicon,
     * generated).
     * @return Whether or not the tab is a placeholder used on startup.
     */
    public boolean getIsPlaceholder() {
        return mIsPlaceholder;
    }

    /**
     * @return The left-side of the tab's touch target.
     */
    public float getTouchTargetLeft() {
        return getTouchTargetBounds().left;
    }

    /**
     * @return The right-side of the tab's touch target.
     */
    public float getTouchTargetRight() {
        return getTouchTargetBounds().right;
    }

    private void resetCloseRect() {
        RectF closeRect = getCloseRect();
        mCloseButton.setBounds(closeRect);
    }

    private RectF getCloseRect() {
        int closeButtonWidth = CLOSE_BUTTON_WIDTH_DP;
        int closeButtonOffsetX = getCloseButtonOffsetX();
        if (!LocalizationUtils.isLayoutRtl()) {
            mClosePlacement.left = getWidth() - closeButtonWidth - closeButtonOffsetX;
            mClosePlacement.right = mClosePlacement.left + closeButtonWidth;
        } else {
            mClosePlacement.left = closeButtonOffsetX;
            mClosePlacement.right = closeButtonWidth + closeButtonOffsetX;
        }

        mClosePlacement.top = 0;
        mClosePlacement.bottom = getHeight();

        mClosePlacement.offset(getDrawX(), getDrawY());
        return mClosePlacement;
    }

    public int getCloseButtonPadding() {
        return CLOSE_BUTTON_PADDING_DP;
    }

    public int getCloseButtonOffsetX() {
        return CLOSE_BUTTON_OFFSET_X;
    }

    // TODO(dtrainor): Don't animate this if we're selecting or deselecting this tab.
    private void checkCloseButtonVisibility(boolean animate) {
        boolean shouldShow = mCanShowCloseButton && !mIsPlaceholder;

        if (shouldShow != mShowingCloseButton) {
            float opacity = shouldShow ? 1.f : 0.f;
            if (animate) {
                if (mButtonOpacityAnimation != null) mButtonOpacityAnimation.end();
                mButtonOpacityAnimation =
                        CompositorAnimator.ofFloatProperty(
                                mUpdateHost.getAnimationHandler(),
                                mCloseButton,
                                CompositorButton.OPACITY,
                                mCloseButton.getOpacity(),
                                opacity,
                                ANIM_TAB_CLOSE_BUTTON_FADE_MS);
                mButtonOpacityAnimation.addListener(
                        new AnimatorListenerAdapter() {
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

    private float[] getTouchTargetInsetsX() {
        float leftInset;
        float rightInset;
        if (LocalizationUtils.isLayoutRtl()) {
            leftInset = getCloseButtonOffsetX();
            rightInset = FOLIO_FOOT_LENGTH_DP;
        } else {
            leftInset = FOLIO_FOOT_LENGTH_DP;
            rightInset = getCloseButtonOffsetX();
        }
        return new float[] {leftInset, rightInset};
    }
}
