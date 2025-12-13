// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil.FOLIO_FOOT_LENGTH_DP;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.RectF;
import android.util.FloatProperty;
import android.util.Size;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton.ButtonType;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTabDelegate.VisualState;
import org.chromium.chrome.browser.compositor.overlays.strip.TabLoadTracker.TabLoadTrackerCallback;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.MediaState;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.util.MotionEventUtils;

import java.util.List;

/**
 * {@link StripLayoutTab} is used to keep track of the strip position and rendering information for
 * a particular tab so it can draw itself onto the GL canvas.
 */
@NullMarked
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
    // 16dp(Folio foot) + 10dp(Close end offset) - 7dp(Close icon padding) = 19dp.
    private static final int DESKTOP_CLOSE_BUTTON_OFFSET_X_DP = 19;
    // 7dp(ContentOffsetY) - (24dp(Close button height) - 20dp(Divider height)) / 2 + 2dp(TabDrawY)
    // = 7dp.
    private static final int DESKTOP_CLOSE_BUTTON_OFFSET_Y_DP = 7;

    // Strip Tab Offset Constants
    protected static final float TOP_MARGIN_DP = 2.f;
    private static final float FOLIO_CONTENT_OFFSET_Y = 8.f;
    private static final int TAB_TOUCH_TARGET_END_OFFSET_X_DP = 12;

    // Visibility Constants.
    private static final float FAVICON_WIDTH = 16.f;
    private static final float FAVICON_PADDING = 26.f;
    protected static final float MIN_WIDTH = FAVICON_WIDTH + (FOLIO_FOOT_LENGTH_DP * 2);
    // TODO(crbug.com/454048975): Check media indicator constants with UX.
    private static final float MEDIA_INDICATOR_WIDTH = 16.f;
    private static final float WIDTH_TO_HIDE_ICON = 86.f;
    private static final float WIDTH_TO_HIDE_FAVICON_FOR_MEDIA_INDICATOR =
            WIDTH_TO_HIDE_ICON + MEDIA_INDICATOR_WIDTH;

    // Divider Constants
    private static final int DIVIDER_OFFSET_X = 13;

    // Close button hover highlight alpha
    private static final float CLOSE_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY = 0.12f;
    private static final float CLOSE_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY = 0.08f;

    // Tab's ID this view refers to.
    private int mTabId;

    private final TabLoadTracker mLoadTracker;
    private final LayoutUpdateHost mUpdateHost;
    private final Size mCloseButtonSize;
    private TintedCompositorButton mCloseButton;

    private boolean mIsClosed;
    private boolean mIsSelected;
    private boolean mIsPinned;
    private boolean mIsHovered;
    private boolean mIsMultiSelected;
    private boolean mCanShowCloseButton = true;
    private boolean mFolioAttached = true;
    private boolean mStartDividerVisible;
    private boolean mEndDividerVisible;
    private boolean mForceHideEndDivider;
    private boolean mSkipAsyncClosure;
    private float mBottomMargin;
    private float mContainerOpacity;
    private @MediaState int mMediaState;

    // For avoiding unnecessary accessibility description updates.
    private @Nullable String mCachedA11yDescriptionTitle;
    private @StringRes int mCachedA11yTabstripIdentifierResId;

    // Startup parameters
    private boolean mIsPlaceholder;

    private boolean mShowingCloseButton = true;

    // Content Animations
    private @Nullable CompositorAnimator mButtonOpacityAnimation;

    private float mLoadingSpinnerRotationDegrees;

    // Preallocated
    private final RectF mClosePlacement = new RectF();

    private final ObserverList<Observer> mObservers = new ObserverList<>();

    private @VisualState int mVisualState = VisualState.NORMAL;

    /**
     * Create a {@link StripLayoutTab} that represents the {@link Tab} with an id of {@code id}.
     *
     * @param context An Android context for accessing system resources.
     * @param id The id of the {@link Tab} to visually represent.
     * @param clickHandler Handles clicks on this {@link StripLayoutTab}.
     * @param keyboardFocusHandler Handles keyboard focus gain/loss on this {@link StripLayoutTab}.
     * @param loadTrackerCallback The {@link TabLoadTrackerCallback} to be notified of loading state
     *     changes.
     * @param updateHost The {@link LayoutRenderHost}.
     * @param incognito Whether or not this layout tab is incognito.
     */
    public StripLayoutTab(
            Context context,
            int id,
            StripLayoutViewOnClickHandler clickHandler,
            StripLayoutViewOnKeyboardFocusHandler keyboardFocusHandler,
            TabLoadTrackerCallback loadTrackerCallback,
            LayoutUpdateHost updateHost,
            boolean incognito,
            boolean isPinned) {
        super(incognito, clickHandler, keyboardFocusHandler, context);
        mTabId = id;
        mIsPinned = isPinned;
        mLoadTracker = new TabLoadTracker(id, loadTrackerCallback);
        mUpdateHost = updateHost;
        mCloseButton =
                new TintedCompositorButton(
                        context,
                        ButtonType.TAB_CLOSE,
                        this,
                        /* width= */ 0,
                        /* height= */ 0,
                        /* tooltipHandler= */ null,
                        clickHandler,
                        keyboardFocusHandler,
                        R.drawable.btn_tab_close_normal,
                        0f);
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
        mCloseButtonSize = getCloseButtonSize();
        resetCloseRect();
    }

    /**
     * Sets the selected state for this tab.
     *
     * @param isSelected Whether the tab is selected.
     */
    public void setIsSelected(boolean isSelected) {
        mIsSelected = isSelected;
    }

    /** Gets the selected state for this tab. */
    public boolean getIsSelected() {
        return mIsSelected;
    }

    /**
     * Sets the hovered state for this tab.
     *
     * @param isHovered Whether the tab is hovered.
     */
    public void setIsHovered(boolean isHovered) {
        mIsHovered = isHovered;
    }

    /** Gets the hovered state for this tab. */
    public boolean getIsHovered() {
        return mIsHovered;
    }

    /** Sets the {@link VisualState} for this tab. */
    public void setVisualState(@VisualState int visualState) {
        mVisualState = visualState;
    }

    /** Gets the {@link VisualState} for this tab. */
    public int getVisualState() {
        return mVisualState;
    }

    /**
     * Sets the multi-selected state for this tab.
     *
     * @param isMultiSelected whether this tab is multi-selected. ie, Ctrl Clicked or Shift Clicked.
     */
    public void setIsMultiSelected(boolean isMultiSelected) {
        mIsMultiSelected = isMultiSelected;
    }

    /** gets the multi-selected state of this tab */
    public boolean getIsMultiSelected() {
        return mIsMultiSelected;
    }

    /**
     * Sets whether this tab has been pinned.
     *
     * @param isPinned whether this tab has been pinned.
     */
    public void setIsPinned(boolean isPinned) {
        mIsPinned = StripLayoutUtils.isTabPinningFromStripEnabled() ? isPinned : false;
    }

    /** Gets whether this tab has been pinned */
    public boolean getIsPinned() {
        return mIsPinned;
    }

    /* package */ void setMediaState(@MediaState int mediaState) {
        mMediaState = mediaState;
    }

    public @MediaState int getMediaState() {
        return mMediaState;
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
        if (isCollapsed() || isDying()) return;
        super.getVirtualViews(views);
        if (mShowingCloseButton || mIsSelected) mCloseButton.getVirtualViews(views);
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
        mCachedA11yDescriptionTitle = title;
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
        if (mCachedA11yDescriptionTitle != null && newTitle == null) {
            // Going from non-null title to null title.
            return true;
        }
        if (newTitle != null && !newTitle.equals(mCachedA11yDescriptionTitle)) {
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
     * Marks if tab container is attached to the toolbar for the Tab Strip Redesign folio treatment.
     *
     * @param folioAttached Whether the tab should be attached or not.
     */
    public void setFolioAttached(boolean folioAttached) {
        mFolioAttached = folioAttached;
    }

    public boolean getFolioAttached() {
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
     * Gets the tint color for the tab background based on its current VisualState.
     *
     * @return The tint color resource that represents the tab background.
     */
    public @ColorInt int getTint() {
        switch (mVisualState) {
            case VisualState.SELECTED_HOVERED:
                return TabUiThemeUtil.getTabStripSelectedTabColor(mContext, isIncognito());
            case VisualState.SELECTED:
                return TabUiThemeUtil.getTabStripSelectedTabColor(mContext, isIncognito());
            case VisualState.NON_DRAG_REORDERING:
                return TabUiThemeUtil.getTabStripBackgroundColor(mContext, isIncognito());
            case VisualState.MULTISELECT_HOVERED:
                return TabUiThemeUtil.getTabStripMultiSelectedHoveredTabColor(
                        mContext, isIncognito());
            case VisualState.MULTISELECT:
                return TabUiThemeUtil.getTabStripMultiSelectedTabColor(mContext, isIncognito());
            case VisualState.HOVERED:
                return TabUiThemeUtil.getHoveredTabContainerColor(mContext, isIncognito());
            case VisualState.PLACEHOLDER:
                return TabUiThemeUtil.getTabStripStartupContainerColor(mContext);
            case VisualState.NORMAL:
                return ChromeColors.getDefaultBgColor(mContext, isIncognito());
            default:
                assert false : "Invalid Visual State";
                return -1;
        }
    }

    /**
     * @return The tint color resource for the tab divider.
     */
    public @ColorInt int getDividerTint() {
        return TabUiThemeUtil.getDividerTint(mContext, isIncognito());
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

    /** Sets if the end divider will be forced hidden for group reorder. */
    public void setForceHideEndDivider(boolean forceHide) {
        mForceHideEndDivider = forceHide;
    }

    /** Returns {@code true} if the end divider will be forced hidden for group reorder. */
    boolean shouldForceHideEndDivider() {
        return mForceHideEndDivider;
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
     * Mark that this tab is closing through the new tab closure flow, and needs to skip the async
     * closure from the old tab closure flow. Can be removed once the migration is complete. See
     * crbug.com/443337907.
     */
    public void setSkipAsyncClosure(boolean skipAsyncClosure) {
        mSkipAsyncClosure = skipAsyncClosure;
    }

    /**
     * Returns true if the tab should skip the async closure from the old tab closure flow. Can be
     * removed once the migration is complete. See crbug.com/443337907.
     */
    public boolean shouldSkipAsyncClosure() {
        return mSkipAsyncClosure;
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

    /** Returns {@code true} if the tab should be visible. */
    public boolean shouldBeVisible() {
        return mIsSelected || mIsPlaceholder || mIsMultiSelected || getIsNonDragReordering();
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
     * @return The padding between the start of a tab and its favicon.
     */
    public float getFaviconPadding() {
        return FAVICON_PADDING;
    }

    /**
     * @return The size of the tab favicon.
     */
    public float getFaviconSize() {
        return FAVICON_WIDTH;
    }

    /**
     * @param show Whether or not the close button is allowed to be shown.
     * @param animate Whether or not to animate the close button showing/hiding.
     */
    public void setCanShowCloseButton(boolean show, boolean animate) {
        mCanShowCloseButton = show;
        checkCloseButtonVisibility(animate);
    }

    /** Returns whether the close button is allowed to be shown. */
    public boolean canShowCloseButton() {
        return mCanShowCloseButton;
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

    @Override
    public void setTouchTargetInsets(
            @Nullable Float left,
            @Nullable Float top,
            @Nullable Float right,
            @Nullable Float bottom) {
        super.setTouchTargetInsets(left, top, right, bottom);

        mCloseButton.setTouchTargetInsets(null, top, null, bottom);
    }

    /**
     * @param closePressed The current pressed state of the attached button.
     * @param buttons Buttons in the motion event.
     */
    public void setClosePressed(boolean closePressed, int buttons) {
        mCloseButton.setPressed(closePressed, MotionEventUtils.isPrimaryButton(buttons));
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
     * @param x The x position of the position to test.
     * @param y The y position of the position to test.
     * @return Whether or not {@code x} and {@code y} is over the close button for this tab and if
     *     the button can be clicked.
     */
    public boolean checkCloseHitTest(float x, float y) {
        return mShowingCloseButton ? mCloseButton.checkClickedOrHovered(x, y) : false;
    }

    /**
     * This is used to determine if the tab is a placeholder or not. If it is a placeholder, it will
     * show as an empty tab on the tab strip (without tab contents, such as title & favicon,
     * generated).
     *
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

    @Override
    public void setIsDraggedOffStrip(boolean isDraggedOffStrip) {
        super.setIsDraggedOffStrip(isDraggedOffStrip);

        // Views that are dragged off strip need to hide their dividers.
        setStartDividerVisible(/* visible= */ false);
        setEndDividerVisible(/* visible= */ false);
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
        int closeButtonWidth = mCloseButtonSize.getWidth();
        int closeButtonHeight = mCloseButtonSize.getHeight();
        int closeButtonOffsetX = getCloseButtonOffsetX();
        if (!LocalizationUtils.isLayoutRtl()) {
            mClosePlacement.left = getWidth() - closeButtonWidth - closeButtonOffsetX;
            mClosePlacement.right = mClosePlacement.left + closeButtonWidth;
        } else {
            mClosePlacement.left = closeButtonOffsetX;
            mClosePlacement.right = closeButtonWidth + closeButtonOffsetX;
        }

        mClosePlacement.top =
                StripLayoutUtils.shouldApplyMoreDensity() ? DESKTOP_CLOSE_BUTTON_OFFSET_Y_DP : 0;
        mClosePlacement.bottom =
                StripLayoutUtils.shouldApplyMoreDensity()
                        ? mClosePlacement.top + closeButtonHeight
                        : getHeight();

        mClosePlacement.offset(getDrawX(), getDrawY());
        return mClosePlacement;
    }

    private Size getCloseButtonSize() {
        float dpToPx = getDpToPx();
        TypedArray closeAttributes =
                mContext.obtainStyledAttributes(
                        new int[] {R.attr.closeButtonWidth, R.attr.closeButtonHeight});
        int widthPx = closeAttributes.getDimensionPixelSize(0, 0);
        int heightPx = closeAttributes.getDimensionPixelSize(1, 0);
        closeAttributes.recycle();
        return new Size(Math.round(widthPx / dpToPx), Math.round(heightPx / dpToPx));
    }

    public int getCloseButtonPadding() {
        return CLOSE_BUTTON_PADDING_DP;
    }

    public int getTabTouchTargetEndOffsetX() {
        return TAB_TOUCH_TARGET_END_OFFSET_X_DP;
    }

    public int getCloseButtonOffsetX() {
        return StripLayoutUtils.shouldApplyMoreDensity()
                ? DESKTOP_CLOSE_BUTTON_OFFSET_X_DP
                : getTabTouchTargetEndOffsetX();
    }

    public boolean shouldHideFavicon(boolean mediaIndicatorIsPresent) {
        if (mIsPinned) return mediaIndicatorIsPresent;

        final float width = getWidth();
        final boolean closeButtonVisible = mCloseButton.getOpacity() > 0.f;

        if (mediaIndicatorIsPresent) {
            float widthThreshold =
                    closeButtonVisible
                            ? WIDTH_TO_HIDE_FAVICON_FOR_MEDIA_INDICATOR
                            : WIDTH_TO_HIDE_ICON;
            return width <= widthThreshold;
        }

        return closeButtonVisible && width <= WIDTH_TO_HIDE_ICON;
    }

    public boolean shouldHideMediaIndicator() {
        if (!ChromeFeatureList.sMediaIndicatorsAndroid.isEnabled()) return true;

        final boolean closeButtonVisible = mCloseButton.getOpacity() > 0.f;
        return closeButtonVisible && getWidth() <= WIDTH_TO_HIDE_ICON;
    }

    public float getMediaIndicatorWidth() {
        return MEDIA_INDICATOR_WIDTH;
    }

    @Override
    public void getAnchorRect(Rect out) {
        float dpToPx = getDpToPx();
        out.set(
                Math.round((getDrawX() + FOLIO_FOOT_LENGTH_DP) * dpToPx),
                Math.round(getDrawY() * dpToPx),
                Math.round((getDrawX() + getWidth() - FOLIO_FOOT_LENGTH_DP) * dpToPx),
                Math.round((getDrawY() + getHeight()) * dpToPx));
    }

    /** {@return The keyboard focus ring's offset (how far it is inside the tab outline) in DP} */
    public int getKeyboardFocusRingOffset() {
        return TabUiThemeUtil.getFocusRingOffset(mContext);
    }

    /** {@return The width of the keyboard focus ring stroke and tab group color line in px} */
    public int getLineWidth() {
        return TabUiThemeUtil.getLineWidth(mContext);
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
            leftInset = getTabTouchTargetEndOffsetX();
            rightInset = FOLIO_FOOT_LENGTH_DP;
        } else {
            leftInset = FOLIO_FOOT_LENGTH_DP;
            rightInset = getTabTouchTargetEndOffsetX();
        }
        return new float[] {leftInset, rightInset};
    }
}
