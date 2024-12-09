// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.content.Context;
import android.graphics.Rect;
import android.util.FloatProperty;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.base.Token;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesColor;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesCoordinator;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/**
 * {@link StripLayoutGroupTitle} is used to keep track of the strip position and rendering
 * information for a particular tab group title indicator on the tab strip so it can draw itself
 * onto the GL canvas.
 */
public class StripLayoutGroupTitle extends StripLayoutView {

    private final Context mContext;

    /** Delegate for additional group title functionality. */
    public interface StripLayoutGroupTitleDelegate extends StripLayoutViewOnClickHandler {
        /**
         * Releases the resources associated with this group indicator.
         *
         * @param rootId The root ID of the given group indicator.
         */
        void releaseResourcesForGroupTitle(int rootId);

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
    private static final int CORNER_RADIUS_DP = 6;
    private static final float BOTTOM_INDICATOR_HEIGHT_DP = 2.f;

    private static final int WIDTH_MARGINS_DP = MARGIN_START_DP + MARGIN_END_DP;
    private static final int EFFECTIVE_MIN_WIDTH = MIN_VISUAL_WIDTH_DP + WIDTH_MARGINS_DP;
    private static final int EFFECTIVE_MAX_WIDTH = MAX_VISUAL_WIDTH_DP + WIDTH_MARGINS_DP;

    // External influences.
    private final StripLayoutGroupTitleDelegate mDelegate;

    // Tab group variables.
    // Tab group's root Id this view refers to.
    // @TODO(crbug.com/379941150) Deprecate rootId and transition to using tabGroupId
    private int mRootId;
    private Token mTabGroupId;
    private String mTitle;
    @ColorInt private int mColor;

    // Bottom indicator variables
    private float mBottomIndicatorWidth;

    // Shared state
    private boolean mIsShared;
    @Nullable private SharedImageTilesCoordinator mSharedImageTilesCoordinator;
    @Nullable private ViewResourceAdapter mAvatarResource;
    private float mAvatarWidthWithPadding;

    /**
     * Create a {@link StripLayoutGroupTitle} that represents the TabGroup for the {@code rootId}.
     *
     * @param delegate The delegate for additional strip group title functionality.
     * @param incognito Whether or not this tab group is Incognito.
     * @param rootId The root ID for the tab group.
     * @param tabGroupId The tab group ID for the tab group.
     */
    public StripLayoutGroupTitle(
            Context context,
            StripLayoutGroupTitleDelegate delegate,
            boolean incognito,
            int rootId,
            Token tabGroupId) {
        super(incognito, delegate);
        assert rootId != Tab.INVALID_TAB_ID : "Tried to create a group title for an invalid group.";
        mRootId = rootId;
        mContext = context;
        mDelegate = delegate;
        mTabGroupId = tabGroupId;
    }

    @Override
    void onVisibilityChanged(boolean newVisibility) {
        if (newVisibility) {
            mDelegate.rebuildResourcesForGroupTitle(this);
        } else {
            mDelegate.releaseResourcesForGroupTitle(mRootId);
        }
    }

    @Override
    public void setIncognito(boolean incognito) {
        assert false : "Incognito state of a group title cannot change";
    }

    @Override
    public boolean hasClickAction() {
        return ChromeFeatureList.sTabStripGroupCollapse.isEnabled();
    }

    @Override
    public boolean hasLongClickAction() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_STRIP_GROUP_CONTEXT_MENU);
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
        float dpToPx = mContext.getResources().getDisplayMetrics().density;
        out.set(
                (int) (getPaddedX() * dpToPx),
                (int) (getPaddedY() * dpToPx),
                (int) ((getPaddedX() + getPaddedWidth()) * dpToPx),
                (int) ((getPaddedY() + getPaddedHeight()) * dpToPx));
    }

    /**
     * @return The tint color resource that represents the tab group title indicator background.
     */
    public @ColorInt int getTint() {
        return mColor;
    }

    /**
     * @param color The color used when displaying this group.
     */
    public void updateTint(@ColorInt int color) {
        mColor = color;

        // Update the shared group avatar border color if a shared image tiles coordinator exists.
        if (mSharedImageTilesCoordinator != null) {
            mSharedImageTilesCoordinator.updateColorStyle(
                    new SharedImageTilesColor(SharedImageTilesColor.Style.TAB_GROUP, mColor));
        }
    }

    /**
     * @return The group's title.
     */
    protected String getTitle() {
        return mTitle;
    }

    protected void updateTitle(String title, float textWidth) {
        mTitle = title;

        // Account for view padding, margins and width of the avatar and its padding, if applicable.
        // Increment to prevent off-by-one rounding errors
        // adding a title fade when unnecessary.
        float viewWidth =
                getAvatarWidthWithPadding()
                        + textWidth
                        + getTitleStartPadding()
                        + getTitleEndPadding()
                        + WIDTH_MARGINS_DP
                        + 1;
        setWidth(MathUtils.clamp(viewWidth, EFFECTIVE_MIN_WIDTH, EFFECTIVE_MAX_WIDTH));
    }

    /**
     * @return The group's root ID.
     */
    public int getRootId() {
        return mRootId;
    }

    /**
     * @return The group's tab group ID.
     */
    public Token getTabGroupId() {
        return mTabGroupId;
    }

    /**
     * @param rootId The tab group's new rootId. Should be synced with the {@link
     *     org.chromium.chrome.browser.tabmodel.TabGroupModelFilter}.
     */
    protected void updateRootId(int rootId) {
        mRootId = rootId;
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

    /**
     * Updates the shared tab group state by fetching the group avatar from the PeopleKit service,
     * capturing the avatar view as a bitmap, and updating the group title with the captured avatar.
     * *
     *
     * @param collaborationId The id to identify a shared tab group.
     * @param dataSharingService Used to fetch and observe current share data.
     * @param registerAvatarResource A callback to register the avatar resource once it is captured.
     * @param updateGroupTitleBitmap A {@link Runnable} to update the group title bitmap after the
     *     avatar is captured.
     */
    public void updateSharedTabGroup(
            String collaborationId,
            DataSharingService dataSharingService,
            Callback<ViewResourceAdapter> registerAvatarResource,
            Runnable updateGroupTitleBitmap) {
        // Mark the group as shared.
        mIsShared = true;

        // Initialize the shared image tiles coordinator if it doesn't exist.
        if (mSharedImageTilesCoordinator == null) {
            mSharedImageTilesCoordinator =
                    new SharedImageTilesCoordinator(
                            mContext,
                            SharedImageTilesType.SMALL,
                            new SharedImageTilesColor(
                                    SharedImageTilesColor.Style.TAB_GROUP, mColor),
                            dataSharingService);
        }

        // Update the collaboration ID and fetch group data from the data sharing service.
        mSharedImageTilesCoordinator.updateCollaborationId(
                collaborationId,
                (result) -> {
                    if (result) {
                        // Capture and register the avatar bitmap if the group data is successfully
                        // fetched.
                        captureSharedAvatarBitmap(
                                mSharedImageTilesCoordinator.getView(),
                                registerAvatarResource,
                                updateGroupTitleBitmap);
                    }
                });
    }

    /**
     * This method measures and lays out the avatar view, registers the avatar resource and triggers
     * an update to the group title bitmap
     *
     * @params view The Android view of the avatar.
     * @param registerAvatarResource A callback to register the avatar resource once it is captured.
     * @param updateGroupTitleBitmap A {@link Runnable} to update the group title bitmap after the
     *     avatar is captured.
     */
    private void captureSharedAvatarBitmap(
            View view,
            Callback<ViewResourceAdapter> registerAvatarResource,
            Runnable updateGroupTitleBitmap) {
        view.measure(
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
        view.layout(0, 0, view.getMeasuredWidth(), view.getMeasuredHeight());

        // Register the avatar resource if it does not already exist.
        if (mAvatarResource == null) {
            mAvatarResource = new ViewResourceAdapter(view);
            registerAvatarResource.onResult(mAvatarResource);
        }

        // Calculate the avatar width including padding.
        int avatarWidthPx =
                view.getWidth()
                        + mContext.getResources()
                                .getDimensionPixelSize(R.dimen.tablet_shared_group_avatar_padding);
        mAvatarWidthWithPadding =
                avatarWidthPx / mContext.getResources().getDisplayMetrics().density;

        // Trigger an update to the group title bitmap.
        updateGroupTitleBitmap.run();
    }

    public void clearSharedTabGroup() {
        mIsShared = false;
        mSharedImageTilesCoordinator = null;
        mAvatarResource = null;
        mAvatarWidthWithPadding = 0;
    }

    /**
     * @return The width of the shared group avatar and padding.
     */
    public float getAvatarWidthWithPadding() {
        return mAvatarWidthWithPadding;
    }

    /**
     * @return Whether the group is shared.
     */
    public boolean isGroupSharedForTesting() {
        return mIsShared;
    }

    /**
     * @return The coordinator to retrieve the avatar face pile for shared group.
     */
    public SharedImageTilesCoordinator getSharedImageTilesCoordinatorForTesting() {
        return mSharedImageTilesCoordinator;
    }

    /**
     * @return The avatar face pile resource displayed on the tab group title for shared group.
     */
    public ViewResourceAdapter getAvatarResourceForTesting() {
        return mAvatarResource;
    }
}
