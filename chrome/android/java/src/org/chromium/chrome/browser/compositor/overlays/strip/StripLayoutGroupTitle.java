// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Rect;
import android.util.FloatProperty;
import android.view.View;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesConfig;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabBubbler;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/**
 * {@link StripLayoutGroupTitle} is used to keep track of the strip position and rendering
 * information for a particular tab group title indicator on the tab strip so it can draw itself
 * onto the GL canvas.
 */
@NullMarked
public class StripLayoutGroupTitle extends StripLayoutView {

    /** Delegate for additional group title functionality. */
    public interface StripLayoutGroupTitleDelegate extends StripLayoutViewOnClickHandler {
        /**
         * Releases the resources associated with this group indicator.
         *
         * @param groupId The ID of this group indicator.
         */
        void releaseResourcesForGroupTitle(Token groupId);

        /**
         * Rebuilds the resources associated with this group indicator.
         *
         * @param groupTitle This group indicator.
         */
        void rebuildResourcesForGroupTitle(StripLayoutGroupTitle groupTitle);
    }

    /** A property for animations to use for changing the width of the bottom indicator. */
    public static final FloatProperty<StripLayoutGroupTitle> BOTTOM_INDICATOR_WIDTH =
            new FloatProperty<>("bottomIndicatorWidth") {
                @Override
                public void setValue(StripLayoutGroupTitle object, float value) {
                    object.setBottomIndicatorWidth(value);
                }

                @Override
                public Float get(StripLayoutGroupTitle object) {
                    return object.getBottomIndicatorWidth();
                }
            };

    // Position constants.
    private static final int MIN_VISUAL_WIDTH_DP = 24;
    private static final int MAX_VISUAL_WIDTH_DP = 156;

    private static final int MARGIN_TOP_DP = 7;
    private static final int MARGIN_BOTTOM_DP = 9;
    private static final int MARGIN_START_DP = 13;
    private static final int MARGIN_END_DP = 9;
    private static final int TEXT_PADDING_DP = 8;

    // The padding between the start of the indicator and the avatar when the group is shared. If no
    // avatar is present, the start padding should match the end padding, using `TEXT_PADDING_DP`.
    private static final int AVATAR_START_PADDING_DP = 4;
    private static final int CORNER_RADIUS_DP = 9;
    private static final float BOTTOM_INDICATOR_HEIGHT_DP = 2.f;
    private static final float NOTIFICATION_BUBBLE_SIZE_DP = 6.f;
    private static final float NOTIFICATION_BUBBLE_PADDING_DP = 4.f;

    private static final int WIDTH_MARGINS_DP = MARGIN_START_DP + MARGIN_END_DP;
    private static final int EFFECTIVE_MIN_WIDTH = MIN_VISUAL_WIDTH_DP + WIDTH_MARGINS_DP;
    private static final int EFFECTIVE_MAX_WIDTH = MAX_VISUAL_WIDTH_DP + WIDTH_MARGINS_DP;

    // Reorder background constants.
    public static final float REORDER_BACKGROUND_TOP_MARGIN = StripLayoutTab.TOP_MARGIN_DP;
    public static final float REORDER_BACKGROUND_BOTTOM_MARGIN =
            StripLayoutTabDelegate.FOLIO_DETACHED_BOTTOM_MARGIN_DP;
    public static final float REORDER_BACKGROUND_PADDING_START = 5.f;
    public static final float REORDER_BACKGROUND_PADDING_END = 10.f;
    public static final float REORDER_BACKGROUND_CORNER_RADIUS = 12.f;

    public static final int TOTAL_MARGIN_HEIGHT = MARGIN_TOP_DP + MARGIN_BOTTOM_DP;

    // External influences.
    private final StripLayoutGroupTitleDelegate mDelegate;

    // Tab group variables.
    // Tab group's Id this view refers to.
    private final Token mTabGroupId; // Non-null because we assert in the constructor
    private @Nullable String mTitle;
    @TabGroupColorId private int mColorId;

    // Bottom indicator variables
    private float mBottomIndicatorWidth;

    // Shared state
    private boolean mIsShared;
    private @Nullable SharedImageTilesCoordinator mSharedImageTilesCoordinator;
    private SharedImageTilesConfig.@Nullable Builder mSharedImageTilesConfigBuilder;
    private @Nullable ViewResourceAdapter mAvatarResource;
    private float mAvatarWidthWithPadding;
    @ColorInt private final int mBubbleTint;
    private @Nullable TabBubbler mTabBubbler;

    /**
     * Create a {@link StripLayoutGroupTitle} that represents the TabGroup for the {@code
     * tabGroupId}.
     *
     * @param delegate The delegate for additional strip group title functionality.
     * @param keyboardFocusHandler Handles keyboard focus gain/loss on this view.
     * @param incognito Whether or not this tab group is Incognito.
     * @param tabGroupId The tab group ID for the tab group.
     */
    public StripLayoutGroupTitle(
            Context context,
            StripLayoutGroupTitleDelegate delegate,
            StripLayoutViewOnKeyboardFocusHandler keyboardFocusHandler,
            boolean incognito,
            @Nullable Token tabGroupId) {
        super(incognito, delegate, keyboardFocusHandler, context);
        assert tabGroupId != null : "Tried to create a group title for an invalid group.";
        mDelegate = delegate;
        mTabGroupId = tabGroupId;
        mBubbleTint = TabUiThemeUtil.getGroupTitleBubbleColor(mContext);
    }

    @Override
    void onVisibilityChanged(boolean newVisibility) {
        if (newVisibility) {
            mDelegate.rebuildResourcesForGroupTitle(this);
        } else {
            mDelegate.releaseResourcesForGroupTitle(mTabGroupId);
        }
    }

    @Override
    public void setIncognito(boolean incognito) {
        assert false : "Incognito state of a group title cannot change";
    }

    /**
     * @return DrawX accounting for padding.
     */
    public float getPaddedX() {
        return getDrawX() + (LocalizationUtils.isLayoutRtl() ? MARGIN_END_DP : MARGIN_START_DP);
    }

    /**
     * @return DrawY accounting for padding.
     */
    public float getPaddedY() {
        return getDrawY() + MARGIN_TOP_DP;
    }

    /**
     * @return Width accounting for padding.
     */
    public float getPaddedWidth() {
        return getWidth() - MARGIN_START_DP - MARGIN_END_DP;
    }

    /**
     * @return Height accounting for padding.
     */
    public float getPaddedHeight() {
        return getHeight() - MARGIN_TOP_DP - MARGIN_BOTTOM_DP;
    }

    /**
     * Get padded bounds for this view.
     *
     * @param out Rect to set the bounds.
     */
    public void getPaddedBoundsPx(Rect out) {
        float dpToPx = getDpToPx();
        out.set(
                Math.round(getPaddedX() * dpToPx),
                Math.round(getPaddedY() * dpToPx),
                Math.round((getPaddedX() + getPaddedWidth()) * dpToPx),
                Math.round((getPaddedY() + getPaddedHeight()) * dpToPx));
    }

    @Override
    public void getAnchorRect(Rect out) {
        // For the context menu, we should use the unpadded height (so that the anchor rect will
        // stretch to the bottom of the toolbar), but we should use the padded width (so that the
        // left edge of the context menu will visually align with the tab group indicator oval).
        float dpToPx = getDpToPx();
        out.set(
                Math.round(getPaddedX() * dpToPx),
                Math.round(getDrawY() * dpToPx),
                Math.round((getPaddedX() + getPaddedWidth()) * dpToPx),
                Math.round((getDrawY() + getHeight()) * dpToPx));
    }

    /**
     * @return The tab group color id that represents the tab group title indicator background.
     */
    public @TabGroupColorId int getTint() {
        return TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                mContext, mColorId, isIncognito());
    }

    /**
     * @param colorId The colorId used when displaying this group.
     */
    public void updateTint(@TabGroupColorId int colorId) {
        mColorId = colorId;

        // Update the shared group avatar border color if a shared image tiles coordinator exists.
        if (mSharedImageTilesCoordinator != null && mSharedImageTilesConfigBuilder != null) {
            mSharedImageTilesCoordinator.updateConfig(
                    mSharedImageTilesConfigBuilder.setTabGroupColor(mContext, colorId).build());
        }
    }

    /**
     * @return The group's title.
     */
    protected @Nullable String getTitle() {
        return mTitle;
    }

    protected void updateTitle(String title, float textWidth) {
        mTitle = title;

        // Account for view padding, margins and width of the avatar and its padding, if applicable.
        // Increment to prevent off-by-one rounding errors
        // adding a title fade when unnecessary.
        float viewWidth =
                getAvatarWidthWithPadding()
                        + getBubbleWidthWithPadding()
                        + textWidth
                        + getTitleStartPadding()
                        + getTitleEndPadding()
                        + WIDTH_MARGINS_DP
                        + 1;
        setWidth(MathUtils.clamp(viewWidth, EFFECTIVE_MIN_WIDTH, EFFECTIVE_MAX_WIDTH));
    }

    /**
     * @return The group's tab group ID.
     */
    public Token getTabGroupId() {
        return mTabGroupId;
    }

    /**
     * @return The start padding for the title.
     */
    public int getTitleStartPadding() {
        return mAvatarWidthWithPadding > 0 ? AVATAR_START_PADDING_DP : TEXT_PADDING_DP;
    }

    /**
     * @return The end padding for the title.
     */
    public int getTitleEndPadding() {
        return TEXT_PADDING_DP;
    }

    /**
     * @return The corner radius for the title container.
     */
    public int getCornerRadius() {
        return CORNER_RADIUS_DP;
    }

    /**
     * @return The width of the bottom indicator should be applied to this tab group.
     */
    public float getBottomIndicatorWidth() {
        return mBottomIndicatorWidth;
    }

    /**
     * @param bottomIndicatorWidth The width of the bottom indicator should be applied to this tab
     *     group.
     */
    public void setBottomIndicatorWidth(float bottomIndicatorWidth) {
        mBottomIndicatorWidth = bottomIndicatorWidth;
    }

    /**
     * @return The height of the bottom indicator should be applied to this tab group.
     */
    public float getBottomIndicatorHeight() {
        return BOTTOM_INDICATOR_HEIGHT_DP;
    }

    /** Returns the {@link ColorInt} for the reorder background. */
    public @ColorInt int getReorderBackgroundTint() {
        return getIsNonDragReordering()
                ? TabUiThemeUtil.getTabStripBackgroundColor(mContext, isIncognito())
                : TabUiThemeUtil.getReorderBackgroundColor(mContext, isIncognito());
    }

    /**
     * Updates the shared tab group state by fetching the group avatar from the PeopleKit service,
     * capturing the avatar view as a bitmap, and updating the group title with the captured avatar.
     * *
     *
     * @param collaborationId The id to identify a shared tab group.
     * @param dataSharingService Used to fetch and observe current share data.
     * @param collaborationService Used to fetch collaboration group data.
     * @param registerAvatarResource A callback to register the avatar resource once it is captured.
     * @param updateGroupTitleBitmap A {@link Runnable} to update the group title bitmap after the
     *     avatar is captured.
     */
    public void updateSharedTabGroup(
            String collaborationId,
            DataSharingService dataSharingService,
            CollaborationService collaborationService,
            Callback<ViewResourceAdapter> registerAvatarResource,
            Runnable updateGroupTitleBitmap) {
        // Mark the group as shared.
        mIsShared = true;

        // Initialize the shared image tiles coordinator if it doesn't exist.
        if (mSharedImageTilesCoordinator == null) {
            mSharedImageTilesConfigBuilder =
                    SharedImageTilesConfig.Builder.createForTabGroupColorContext(
                            mContext, mColorId);
            mSharedImageTilesCoordinator =
                    new SharedImageTilesCoordinator(
                            mContext,
                            mSharedImageTilesConfigBuilder.build(),
                            dataSharingService,
                            collaborationService);
        }

        // Update the collaboration ID and fetch group data from the data sharing service.
        mSharedImageTilesCoordinator.fetchImagesForCollaborationId(
                collaborationId,
                (result) -> {
                    if (result) {
                        // Capture and register the avatar bitmap if the group data is successfully
                        // fetched.
                        View avatarView = assumeNonNull(mSharedImageTilesCoordinator).getView();
                        if (LocalizationUtils.isLayoutRtl()) {
                            avatarView.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
                        }
                        captureSharedAvatarBitmap(
                                avatarView, registerAvatarResource, updateGroupTitleBitmap);
                    }
                });
    }

    /**
     * This method measures and lays out the avatar view, registers the avatar resource and triggers
     * an update to the group title bitmap
     *
     * @params avatarView The Android view of the avatar.
     * @param registerAvatarResource A callback to register the avatar resource once it is captured.
     * @param updateGroupTitleBitmap A {@link Runnable} to update the group title bitmap after the
     *     avatar is captured.
     */
    private void captureSharedAvatarBitmap(
            View avatarView,
            Callback<ViewResourceAdapter> registerAvatarResource,
            Runnable updateGroupTitleBitmap) {
        avatarView.measure(
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
        avatarView.layout(0, 0, avatarView.getMeasuredWidth(), avatarView.getMeasuredHeight());

        // Register the avatar resource if it does not already exist.
        if (mAvatarResource == null) {
            mAvatarResource = new ViewResourceAdapter(avatarView);
            registerAvatarResource.onResult(mAvatarResource);
        }

        // Calculate the avatar width including padding.
        int avatarWidthPx =
                avatarView.getWidth()
                        + mContext.getResources()
                                .getDimensionPixelSize(R.dimen.tablet_shared_group_avatar_padding);
        mAvatarWidthWithPadding =
                avatarWidthPx / mContext.getResources().getDisplayMetrics().density;

        // Trigger an update to the group title bitmap.
        updateGroupTitleBitmap.run();
    }

    public void clearSharedTabGroup() {
        mIsShared = false;
        mAvatarResource = null;
        mAvatarWidthWithPadding = 0;
        if (mSharedImageTilesCoordinator != null) {
            mSharedImageTilesCoordinator.destroy();
            mSharedImageTilesCoordinator = null;
        }
    }

    /**
     * @return The width of the shared group avatar and padding.
     */
    public float getAvatarWidthWithPadding() {
        return mAvatarWidthWithPadding;
    }

    /**
     * @param tabBubbler The {@link TabBubbler} that responsible for managing shared group
     *     notification bubbles. The current {@link TabBubbler} is destroyed if set null.
     */
    public void setTabBubbler(@Nullable TabBubbler tabBubbler) {
        if (mTabBubbler != null && tabBubbler == null) {
            mTabBubbler.destroy();
        }
        mTabBubbler = tabBubbler;
    }

    /**
     * @return The {@link TabBubbler} that responsible for managing shared group notification
     *     bubbles.
     */
    public @Nullable TabBubbler getTabBubbler() {
        return mTabBubbler;
    }

    /**
     * @return The total horizontal space needed for the notification bubble and its padding, or 0
     *     if the bubble is not shown.
     */
    public float getBubbleWidthWithPadding() {
        return getNotificationBubbleShown()
                ? NOTIFICATION_BUBBLE_PADDING_DP + NOTIFICATION_BUBBLE_SIZE_DP
                : 0;
    }

    /**
     * @return Notification bubble drawX accounting for padding.
     */
    public float getBubbleDrawX() {
        assert getNotificationBubbleShown();
        return LocalizationUtils.isLayoutRtl()
                ? getPaddedX() + getTitleEndPadding()
                : getPaddedX() + getPaddedWidth() - getTitleEndPadding() - getBubbleSize();
    }

    /**
     * @return The tint of the notification bubble.
     */
    public @ColorInt int getBubbleTint() {
        return mBubbleTint;
    }

    /**
     * @return The size of the notification bubble circle.
     */
    public float getBubbleSize() {
        return NOTIFICATION_BUBBLE_SIZE_DP;
    }

    /**
     * @return The padding between title text end and bubble.
     */
    public float getBubblePadding() {
        return NOTIFICATION_BUBBLE_PADDING_DP;
    }

    /**
     * @return Whether the group is shared.
     */
    public boolean isGroupShared() {
        return mIsShared;
    }

    /**
     * @return The coordinator to retrieve the avatar face pile for shared group.
     */
    public @Nullable SharedImageTilesCoordinator getSharedImageTilesCoordinatorForTesting() {
        return mSharedImageTilesCoordinator;
    }

    /**
     * @return The avatar face pile resource displayed on the tab group title for shared group.
     */
    public @Nullable ViewResourceAdapter getAvatarResourceForTesting() {
        return mAvatarResource;
    }

    /**
     * {@return The keyboard focus ring's offset (how far it is outside the group indicator) in px}
     */
    public int getKeyboardFocusRingOffset() {
        return TabUiThemeUtil.getFocusRingOffset(mContext);
    }

    /** {@return The width of the keyboard focus ring stroke in px} */
    public int getKeyboardFocusRingWidth() {
        return TabUiThemeUtil.getLineWidth(mContext);
    }
}
