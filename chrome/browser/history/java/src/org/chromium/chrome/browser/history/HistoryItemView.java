// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.Callback;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.history.FilterSheetCoordinator.FilterItem;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.DefaultFaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.browser_ui.widget.selectable_list.CheckableSelectableItemView;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListUtils;
import org.chromium.components.favicon.LargeIconBridge;

import java.util.List;
import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/** The CheckableSelectableItemView for items displayed in the browsing history UI. */
@NullMarked
public class HistoryItemView extends CheckableSelectableItemView<HistoryItem> {
    private LinearLayout mContentView;
    private ImageView mStartIconView;
    private TextView mTitleView;
    private TextView mDescriptionView;
    private ImageButton mRemoveButton;
    private @Nullable Drawable mBlockedVisitDrawable;
    private AppInfoCache mAppInfoCache;
    private @Nullable Runnable mClusterToggleHandler;

    private final RoundedIconGenerator mIconGenerator;
    private DefaultFaviconHelper mFaviconHelper;

    private final int mDefaultLevel;
    private final int mSelectedLevel;
    private final int mMinIconSize;
    private final int mDisplayedIconSize;
    private final int mEndPadding;
    private final int mChipLeadingPadding;

    private boolean mIsItemRemoved;
    private BooleanSupplier mShowSourceApp;
    private Callback<HistoryItem> mOnItemClicked;
    private Callback<HistoryItem> mOnItemRemoved;
    private Supplier<@Nullable LargeIconBridge> mLargeIconBridgeSupplier;
    private ChipView mChipView;
    private View mSparkContainer;

    private ImageView mCriticalActionsExpandIcon;
    private ViewGroup mCriticalActionsContainer;

    public HistoryItemView(Context context, AttributeSet attrs) {
        super(context, attrs);

        mDefaultLevel = getResources().getInteger(R.integer.list_item_level_default);
        mSelectedLevel = getResources().getInteger(R.integer.list_item_level_selected);
        mMinIconSize = FaviconUtils.getDefaultFaviconSize(context);
        mDisplayedIconSize = getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
        mIconGenerator = FaviconUtils.createCircularIconGenerator(context);
        mEndPadding = getResources().getDimensionPixelSize(R.dimen.default_list_row_padding);
        mChipLeadingPadding =
                getResources().getDimensionPixelSize(R.dimen.history_item_leading_padding);
    }

    @Override
    protected ImageView getIconView() {
        return mStartIconView;
    }

    @Override
    protected @Nullable ColorStateList getDefaultIconTint() {
        return null;
    }

    @Override
    protected int getDefaultLevel() {
        return mDefaultLevel;
    }

    @Override
    protected int getSelectedLevel() {
        return mSelectedLevel;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mContentView = findViewById(R.id.content);
        mStartIconView = findViewById(R.id.start_icon);
        mTitleView = findViewById(R.id.title);
        mDescriptionView = findViewById(R.id.description);
        ImageViewCompat.setImageTintList(mStartIconView, getDefaultIconTint());
        mStartIconView.setImageResource(R.drawable.default_favicon);

        mRemoveButton = findViewById(R.id.end_button);
        mRemoveButton.setImageResource(R.drawable.btn_delete_24dp);
        mRemoveButton.setContentDescription(getContext().getString(R.string.remove));
        ImageViewCompat.setImageTintList(
                mRemoveButton,
                getContext().getColorStateList(R.color.default_icon_color_secondary_tint_list));
        mRemoveButton.setOnClickListener(_ -> remove());
        mRemoveButton.setScaleType(ScaleType.CENTER_INSIDE);
        mRemoveButton.setPaddingRelative(
                getResources()
                        .getDimensionPixelSize(R.dimen.history_item_remove_button_lateral_padding),
                getPaddingTop(),
                getResources()
                        .getDimensionPixelSize(R.dimen.history_item_remove_button_lateral_padding),
                getPaddingBottom());

        mCriticalActionsExpandIcon = findViewById(R.id.critical_actions_expand_icon);
        mCriticalActionsContainer = findViewById(R.id.critical_actions_container);
        mChipView = findViewById(R.id.chip);
        mChipView.getPrimaryTextView().setEllipsize(TextUtils.TruncateAt.END);

        mSparkContainer = findViewById(R.id.spark_container);
    }

    @Override
    public void setItem(HistoryItem item) {
        if (getItem() == item) return;

        super.setItem(item);

        mTitleView.setText(item.isClusterHead() ? item.getDomain() : item.getTitle());
        mDescriptionView.setText(item.getDomain());
        // Try to make the TLD part of the URL string visible.
        mDescriptionView.setEllipsize(TextUtils.TruncateAt.START);
        updateChipView(item);
        SelectableListUtils.setContentDescriptionContext(
                getContext(),
                mRemoveButton,
                item.getTitle(),
                SelectableListUtils.ContentDescriptionSource.REMOVE_BUTTON);
        mIsItemRemoved = false;

        if (item.wasBlockedVisit()) {
            if (mBlockedVisitDrawable == null) {
                mBlockedVisitDrawable =
                        TraceEventVectorDrawableCompat.create(
                                getContext().getResources(),
                                R.drawable.ic_block_red,
                                getContext().getTheme());
            }
            setIconDrawable(mBlockedVisitDrawable);
            mTitleView.setTextColor(getContext().getColor(R.color.default_red));
        } else {
            setIconDrawable(
                    mFaviconHelper.getDefaultFaviconDrawable(getContext(), item.getUrl(), true));
            requestIcon();

            mTitleView.setTextColor(
                    getContext().getColorStateList(R.color.default_text_color_list));
        }

        if (item.isClusterHead()) {
            List<HistoryItem> subItems = item.getSubItems();
            assumeNonNull(subItems);

            int count = subItems.size();
            mDescriptionView.setText(
                    getContext()
                            .getResources()
                            .getQuantityString(
                                    R.plurals.history_clusters_item_count, count, count));

            mRemoveButton.setImageResource(
                    item.isExpanded()
                            ? R.drawable.ic_expand_less_black_24dp
                            : R.drawable.ic_expand_more_black_24dp);
            mRemoveButton.setOnClickListener(
                    _ -> {
                        if (mClusterToggleHandler != null) mClusterToggleHandler.run();
                    });
            mRemoveButton.setContentDescription(
                    getContext()
                            .getString(
                                    item.isExpanded()
                                            ? R.string.accessibility_collapse_section
                                            : R.string.accessibility_expand_section));
        } else {
            mRemoveButton.setImageResource(R.drawable.btn_delete_24dp);
            mRemoveButton.setOnClickListener(_ -> remove());
            mRemoveButton.setContentDescription(getContext().getString(R.string.remove));
        }
        updateSparkVisibility();
    }

    @Override
    protected void updateView(boolean animate) {
        super.updateView(animate);
        updateSparkVisibility();
    }

    private void updateSparkVisibility() {
        HistoryItem item = getItem();
        // The spark should be shown only if the item is not selected (checked), blocked, and is an
        // actor visit.
        boolean showSpark =
                item != null && item.isActorVisit() && !item.wasBlockedVisit() && !isChecked();
        mSparkContainer.setVisibility(showSpark ? View.VISIBLE : View.GONE);
    }

    /** Returns the chevron that CriticalActionsCoordinator binds. */
    ImageView getCriticalActionsExpandIcon() {
        return mCriticalActionsExpandIcon;
    }

    /** Returns the container below the row that hosts the critical actions panel. */
    ViewGroup getCriticalActionsContainer() {
        return mCriticalActionsContainer;
    }

    @Initializer
    void initialize(
            AppInfoCache appInfoCache,
            BooleanSupplier showSourceApp,
            Callback<HistoryItem> onItemClicked,
            Callback<HistoryItem> onItemRemoved,
            Supplier<@Nullable LargeIconBridge> largeIconBridgeSupplier) {
        mAppInfoCache = appInfoCache;
        // ItemView can be reused every time a new query is made. Use a supplier to
        // check the condition that changes dynamically.
        mShowSourceApp = showSourceApp;
        mOnItemClicked = onItemClicked;
        mOnItemRemoved = onItemRemoved;
        mLargeIconBridgeSupplier = largeIconBridgeSupplier;
    }

    private void updateChipView(HistoryItem item) {
        boolean showChipView = false;
        if (mShowSourceApp.getAsBoolean()) {
            String appId = item.getAppId();
            if (appId != null) {
                FilterItem<String> appInfo = mAppInfoCache.get(appId);
                if (appInfo != null) {
                    var sourceApp =
                            getResources()
                                    .getString(R.string.history_app_attribution, appInfo.label);
                    mChipView.setPaddingRelative(
                            mChipLeadingPadding,
                            mChipView.getPaddingTop(),
                            mChipView.getPaddingEnd(),
                            mChipView.getPaddingBottom());
                    mChipView.getPrimaryTextView().setText(sourceApp);
                    mChipView.setIcon(appInfo.icon, false);
                    showChipView = true;
                }
            }
        }
        mChipView.setVisibility(showChipView ? View.VISIBLE : View.GONE);
    }

    /**
     * @param helper The helper for fetching default favicons.
     */
    @Initializer
    public void setFaviconHelper(DefaultFaviconHelper helper) {
        mFaviconHelper = helper;
    }

    /**
     * @param handler The handler for toggling cluster expansion.
     */
    public void setClusterToggleHandler(@Nullable Runnable handler) {
        mClusterToggleHandler = handler;
    }

    /** Removes the item associated with this view. */
    public void remove() {
        // If the remove button is double tapped, this method may be called twice.
        HistoryItem item = getItem();
        if (item == null || mIsItemRemoved) return;

        mIsItemRemoved = true;
        mOnItemRemoved.onResult(item);
    }

    /**
     * @param visibility The visibility (VISIBLE, INVISIBLE, GONE) for the remove button.
     */
    public void setRemoveButtonVisiblity(int visibility) {
        mRemoveButton.setVisibility(visibility);
        updateContentEndPadding();
    }

    /**
     * Adds end padding to the row only when it has no end button, so that its text doesn't touch
     * the edge.
     */
    void updateContentEndPadding() {
        boolean hasEndButton =
                mRemoveButton.getVisibility() != View.GONE
                        || mCriticalActionsExpandIcon.getVisibility() != View.GONE;
        mContentView.setPaddingRelative(
                ViewCompat.getPaddingStart(mContentView),
                mContentView.getPaddingTop(),
                hasEndButton ? 0 : mEndPadding,
                mContentView.getPaddingBottom());
    }

    @Override
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public void handleNonSelectionClick() {
        HistoryItem item = getItem();
        if (item != null) mOnItemClicked.onResult(item);
    }

    private void requestIcon() {
        HistoryItem item = getItem();
        if (item == null || item.wasBlockedVisit()) return;
        LargeIconBridge largeIconBridge = mLargeIconBridgeSupplier.get();
        if (largeIconBridge == null) return;
        largeIconBridge.getLargeIconForUrl(
                item.getUrl(),
                mMinIconSize,
                (@Nullable Bitmap icon, int fallbackColor, boolean _, int _) -> {
                    // Prevent stale icons from making it through to the UI.
                    if (item != getItem()) return;

                    Drawable drawable =
                            FaviconUtils.getIconDrawableWithoutFilter(
                                    icon,
                                    item.getUrl(),
                                    fallbackColor,
                                    mIconGenerator,
                                    getResources(),
                                    mDisplayedIconSize);
                    setIconDrawable(drawable);
                });
    }

    @VisibleForTesting
    View getRemoveButtonForTests() {
        return mRemoveButton;
    }

    @VisibleForTesting
    View getSparkContainerForTests() {
        return mSparkContainer;
    }
}
