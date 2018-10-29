// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.languages;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.support.annotation.DrawableRes;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.view.ViewCompat;
import android.support.v7.widget.AppCompatImageView;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.support.v7.widget.helper.ItemTouchHelper;
import android.text.TextUtils;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.ListMenuButton;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * BaseAdapter for {@link RecyclerView}. It manages languages to list there.
 */
public class LanguageListBaseAdapter
        extends RecyclerView.Adapter<LanguageListBaseAdapter.LanguageRowViewHolder> {
    private static final int ANIMATION_DELAY_MS = 100;

    /**
     * Listener used to respond to click event on a language item.
     */
    interface ItemClickListener {
        /**
         * @param item The clicked LanguageItem.
         */
        void onLanguageClicked(LanguageItem item);
    }

    static class LanguageRowViewHolder extends ViewHolder {
        private TextView mTitle;
        private TextView mDescription;

        private AppCompatImageView mStartIcon;
        private ListMenuButton mMoreButton;

        LanguageRowViewHolder(View view) {
            super(view);

            mTitle = (TextView) view.findViewById(R.id.title);
            mDescription = (TextView) view.findViewById(R.id.description);

            mStartIcon = (AppCompatImageView) view.findViewById(R.id.icon_view);
            mMoreButton = (ListMenuButton) view.findViewById(R.id.more);
        }

        /**
         * Update the current {@link LanguageRowViewHolder} with basic language info.
         * @param item A {@link LanguageItem} with the language details.
         */
        private void updateLanguageInfo(LanguageItem item) {
            mTitle.setText(item.getDisplayName());

            // Avoid duplicate display names.
            if (TextUtils.equals(item.getDisplayName(), item.getNativeDisplayName())) {
                mDescription.setVisibility(View.GONE);
            } else {
                mDescription.setVisibility(View.VISIBLE);
                mDescription.setText(item.getNativeDisplayName());
            }

            mMoreButton.setContentDescriptionContext(item.getDisplayName());

            // The default visibility for the views below is GONE.
            mStartIcon.setVisibility(View.GONE);
            mMoreButton.setVisibility(View.GONE);
        }

        /**
         * Sets drawable for the icon at the beginning of this row with the given resId.
         * @param iconResId The identifier of the drawable resource for the icon.
         */
        void setStartIcon(@DrawableRes int iconResId) {
            mStartIcon.setVisibility(View.VISIBLE);
            mStartIcon.setImageResource(iconResId);
        }

        /**
         * Sets up the menu button at the end of this row with a given delegate.
         * @param delegate A {@link ListMenuButton.Delegate}.
         */
        void setMenuButtonDelegate(@NonNull ListMenuButton.Delegate delegate) {
            mMoreButton.setVisibility(View.VISIBLE);
            mMoreButton.setDelegate(delegate);
            // Set item row end padding 0 when MenuButton is visible.
            ViewCompat.setPaddingRelative(itemView, ViewCompat.getPaddingStart(itemView),
                    itemView.getPaddingTop(), 0, itemView.getPaddingBottom());
        }

        /**
         * Set the OnClickListener for this row with a given callback.
         * @param item The {@link LanguageItem} with language details.
         * @param listener A {@link ItemClickListener} to respond to click event.
         */
        void setItemClickListener(LanguageItem item, @NonNull ItemClickListener listener) {
            itemView.setOnClickListener(view -> listener.onLanguageClicked(item));
        }
    }

    private final int mDraggedBackgroundColor;
    private final float mDraggedElevation;

    private boolean mDragEnabled;
    private ItemTouchHelper mItemTouchHelper;
    private List<LanguageItem> mLanguageList;

    protected Context mContext;

    LanguageListBaseAdapter(Context context) {
        mContext = context;
        mLanguageList = new ArrayList<>();

        Resources resource = context.getResources();
        mDraggedBackgroundColor =
                ApiCompatibilityUtils.getColor(resource, R.color.pref_dragged_row_background);
        mDraggedElevation = resource.getDimension(R.dimen.pref_languages_item_dragged_elevation);
    }

    @Override
    public LanguageRowViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        View row = LayoutInflater.from(parent.getContext())
                           .inflate(R.layout.accept_languages_item, parent, false);
        return new LanguageRowViewHolder(row);
    }

    @Override
    public void onBindViewHolder(LanguageRowViewHolder holder, int position) {
        holder.updateLanguageInfo(mLanguageList.get(position));
    }

    @Override
    public int getItemCount() {
        return mLanguageList.size();
    }

    LanguageItem getItemByPosition(int position) {
        return mLanguageList.get(position);
    }

    void reload(List<LanguageItem> languageList) {
        mLanguageList.clear();
        mLanguageList.addAll(languageList);
        notifyDataSetChanged();
    }

    /**
     * Show a drag indicator at the start of the row if applicable.
     *
     * @param holder The LanguageRowViewHolder of the row.
     * @param indicatorResId The identifier of the drawable resource for the indicator.
     */
    void showDragIndicatorInRow(LanguageRowViewHolder holder, @DrawableRes int indicatorResId) {
        // Quit if it's not applicable.
        if (getItemCount() <= 1 || !mDragEnabled) return;

        assert mItemTouchHelper != null;
        holder.setStartIcon(indicatorResId);
        holder.mStartIcon.setOnTouchListener((v, event) -> {
            if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                mItemTouchHelper.startDrag(holder);
            }
            return false;
        });
    }

    /**
     * Enables drag & drop interaction on the given RecyclerView.
     * @param recyclerView The RecyclerView you want to drag from.
     */
    void enableDrag(RecyclerView recyclerView) {
        mDragEnabled = true;

        if (mItemTouchHelper == null) {
            ItemTouchHelper.Callback touchHelperCallBack = new ItemTouchHelper.Callback() {

                // The dragged language info during a single drag operation.
                // The first is its start postion when it's dragged, the second is its language
                // code.
                @Nullable
                private Pair<Integer, String> mDraggedLanguage;

                @Override
                public int getMovementFlags(RecyclerView recyclerView, ViewHolder viewHolder) {
                    return makeMovementFlags(
                            ItemTouchHelper.UP | ItemTouchHelper.DOWN, 0 /* swipe flags */);
                }

                @Override
                public boolean onMove(
                        RecyclerView recyclerView, ViewHolder current, ViewHolder target) {
                    int from = current.getAdapterPosition();
                    int to = target.getAdapterPosition();
                    if (from == to) return false;

                    Collections.swap(mLanguageList, from, to);
                    notifyItemMoved(from, to);
                    return true;
                }

                @Override
                public void onSelectedChanged(ViewHolder viewHolder, int actionState) {
                    super.onSelectedChanged(viewHolder, actionState);
                    if (actionState == ItemTouchHelper.ACTION_STATE_DRAG) {
                        // mDraggedLanguage should be cleaned up before.
                        assert mDraggedLanguage == null;
                        int start = viewHolder.getAdapterPosition();
                        mDraggedLanguage = Pair.create(start, getItemByPosition(start).getCode());

                        updateVisualState(true, viewHolder.itemView);
                    }
                }

                @Override
                public void clearView(RecyclerView recyclerView, ViewHolder viewHolder) {
                    super.clearView(recyclerView, viewHolder);

                    // Commit the postion change for the dragged language when it's dropped.
                    if (mDraggedLanguage != null) {
                        LanguagesManager.getInstance().moveLanguagePosition(mDraggedLanguage.second,
                                viewHolder.getAdapterPosition() - mDraggedLanguage.first, false);
                        mDraggedLanguage = null;
                    }

                    updateVisualState(false, viewHolder.itemView);
                }

                @Override
                public boolean isLongPressDragEnabled() {
                    return true;
                }

                @Override
                public boolean isItemViewSwipeEnabled() {
                    return false;
                }

                @Override
                public void onSwiped(ViewHolder viewHolder, int direction) {
                    // no-op
                }

                private void updateVisualState(boolean dragged, View view) {
                    ViewCompat.animate(view)
                            .translationZ(dragged ? mDraggedElevation : 0)
                            .withEndAction(()
                                                   -> view.setBackgroundColor(dragged
                                                                   ? mDraggedBackgroundColor
                                                                   : Color.TRANSPARENT))
                            .setDuration(ANIMATION_DELAY_MS)
                            .start();
                }
            };

            mItemTouchHelper = new ItemTouchHelper(touchHelperCallBack);
        }
        mItemTouchHelper.attachToRecyclerView(recyclerView);
    }

    /**
     * Disables drag & drop interaction.
     * @param recyclerView The RecyclerView you want to drag from.
     */
    void disableDrag() {
        mDragEnabled = false;
        if (mItemTouchHelper != null) mItemTouchHelper.attachToRecyclerView(null);
    }

    /**
     * Returns whether the drag & drop is enabled.
     */
    boolean isDragEnabled() {
        return mDragEnabled;
    }
}
