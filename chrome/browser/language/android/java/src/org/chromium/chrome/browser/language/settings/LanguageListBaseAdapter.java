// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

import android.content.Context;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.chrome.browser.language.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableListAdapter;
import org.chromium.components.browser_ui.widget.dragreorder.DragStateDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListUtils;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuButtonDelegate;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/** BaseAdapter for {@link RecyclerView}. It manages languages to list there. */
public class LanguageListBaseAdapter extends DragReorderableListAdapter<LanguageItem> {
    /** Listener used to respond to click event on a language item. */
    interface ItemClickListener {
        /**
         * @param item The clicked LanguageItem.
         */
        void onLanguageClicked(LanguageItem item);
    }

    static class LanguageRowViewHolder extends ViewHolder {
        private TextView mTitle;
        private TextView mDescription;

        private ImageView mStartIcon;
        private ListMenuButton mMoreButton;

        LanguageRowViewHolder(View view) {
            super(view);

            mTitle = view.findViewById(R.id.title);
            mDescription = view.findViewById(R.id.description);

            mStartIcon = view.findViewById(R.id.icon_view);
            mMoreButton = view.findViewById(R.id.more);
        }

        /**
         * Update the current {@link LanguageRowViewHolder} with basic language info.
         * @param item A {@link LanguageItem} with the language details.
         */
        protected void updateLanguageInfo(LanguageItem item) {
            mTitle.setText(item.getDisplayName());

            // Avoid duplicate display names.
            if (TextUtils.equals(item.getDisplayName(), item.getNativeDisplayName())) {
                mDescription.setVisibility(View.GONE);
            } else {
                mDescription.setVisibility(View.VISIBLE);
                mDescription.setText(item.getNativeDisplayName());
            }

            SelectableListUtils.setContentDescriptionContext(
                    mMoreButton.getContext(),
                    mMoreButton,
                    item.getDisplayName(),
                    SelectableListUtils.ContentDescriptionSource.MENU_BUTTON);

            // The more button will become visible if setMenuButtonDelegate is called.
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
         * @param delegate A {@link ListMenuButtonDelegate}.
         */
        void setMenuButtonDelegate(@NonNull ListMenuButtonDelegate delegate) {
            mMoreButton.setVisibility(View.VISIBLE);
            mMoreButton.setDelegate(delegate);
            // Set item row end padding 0 when MenuButton is visible.
            itemView.setPaddingRelative(
                    ViewCompat.getPaddingStart(itemView),
                    itemView.getPaddingTop(),
                    0,
                    itemView.getPaddingBottom());
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

    /** Keeps track of whether drag is enabled / active for language preference lists. */
    private class LanguageDragStateDelegate
            implements DragStateDelegate, AccessibilityState.Listener {
        public LanguageDragStateDelegate() {
            AccessibilityState.addListener(this);
        }

        // DragStateDelegate implementation
        @Override
        public boolean getDragEnabled() {
            return !AccessibilityState.isPerformGesturesEnabled();
        }

        @Override
        public boolean getDragActive() {
            return getDragEnabled();
        }

        @Override
        public void onAccessibilityStateChanged(
                AccessibilityState.State oldAccessibilityState,
                AccessibilityState.State newAccessibilityState) {
            notifyDataSetChanged();
        }
    }

    private final Profile mProfile;

    LanguageListBaseAdapter(Context context, Profile profile) {
        super(context);
        mProfile = profile;
        setDragStateDelegate(new LanguageDragStateDelegate());
    }

    /** Return the Profile associated with the displayed data. */
    Profile getProfile() {
        return mProfile;
    }

    /**
     * Show a drag indicator at the start of the row if applicable.
     *
     * @param holder The LanguageRowViewHolder of the row.
     */
    void showDragIndicatorInRow(LanguageRowViewHolder holder) {
        // Quit if it's not applicable.
        if (getItemCount() <= 1 || !mDragStateDelegate.getDragEnabled()) return;

        assert mItemTouchHelper != null;
        holder.setStartIcon(R.drawable.ic_drag_handle_24dp);
        holder.mStartIcon.setOnTouchListener(
                (v, event) -> {
                    if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                        mItemTouchHelper.startDrag(holder);
                    }
                    return false;
                });
    }

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup viewGroup, int i) {
        View row =
                LayoutInflater.from(viewGroup.getContext())
                        .inflate(R.layout.accept_languages_item, viewGroup, false);
        return new LanguageRowViewHolder(row);
    }

    @Override
    public void onBindViewHolder(ViewHolder viewHolder, int i) {
        ((LanguageRowViewHolder) viewHolder).updateLanguageInfo(mElements.get(i));
    }

    @Override
    protected void setOrder(List<LanguageItem> order) {
        String[] codes = new String[order.size()];
        for (int i = 0; i < order.size(); i++) {
            codes[i] = order.get(i).getCode();
        }
        LanguagesManager.getForProfile(mProfile).setOrder(codes, false);
        notifyDataSetChanged();
    }

    /**
     * Sets the displayed languages (not the order of the user's preferred languages;
     * see setOrder above).
     *
     * @param languages The language items to show.
     */
    void setDisplayedLanguages(Collection<LanguageItem> languages) {
        mElements = new ArrayList<>(languages);
        notifyDataSetChanged();
    }

    @Override
    protected boolean isActivelyDraggable(ViewHolder viewHolder) {
        return isPassivelyDraggable(viewHolder);
    }

    @Override
    protected boolean isPassivelyDraggable(ViewHolder viewHolder) {
        return viewHolder instanceof LanguageRowViewHolder;
    }

    @VisibleForTesting
    public List<LanguageItem> getLanguageItemList() {
        return mElements;
    }
}
