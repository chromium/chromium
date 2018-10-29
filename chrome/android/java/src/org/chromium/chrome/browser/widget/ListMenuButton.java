// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Rect;
import android.support.annotation.DrawableRes;
import android.support.annotation.StringRes;
import android.support.v7.widget.AppCompatImageButton;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * A menu button meant to be used with modern lists throughout Chrome.  Will automatically show and
 * anchor a popup on press and will rely on a delegate for both querying the items and returning the
 * results.
 */
public class ListMenuButton
        extends AppCompatImageButton implements AnchoredPopupWindow.LayoutObserver {
    private final static int INVALID_RES_ID = 0;

    /** A class that represents a single item in the popup menu. */
    public static class Item {
        private final String mTextString;
        private final @StringRes int mTextId;
        private final @DrawableRes int mEndIconId;
        private final boolean mEnabled;

        /**
         * Creates a new {@link Item} without an end icon.
         */
        public Item(Context context, @StringRes int textId, boolean enabled) {
            this(context, textId, INVALID_RES_ID, enabled);
        }

        /**
         * Creates a new {@link Item}.
         * @param textId The string resource id for the text to show for this item.
         * @param endIconId The optional drawable resource id to display at the end of the item.
         * @param enabled Whether or not this item should be enabled.
         */
        public Item(Context context, @StringRes int textId, @DrawableRes int endIconId,
                boolean enabled) {
            mTextString = context.getString(textId);
            mTextId = textId;
            mEnabled = enabled;
            mEndIconId = endIconId;
        }

        @Override
        public String toString() {
            return mTextString;
        }

        /** @return Whether or not this {@link Item} should be enabled. */
        public boolean getIsEnabled() {
            return mEnabled;
        }

        /** @return The string resource id this {@link Item} will show. */
        public @StringRes int getTextId() {
            return mTextId;
        }

        public @DrawableRes int getEndIconId() {
            return mEndIconId;
        }
    }

    /** A delegate used to populate the menu and to be notified of menu selection events. */
    public static interface Delegate {
        /**
         * Will be called every time the menu is about to be created to determine what content
         * should live in the menu.
         * @return A list of {@link Item}s to show in the menu.
         */
        Item[] getItems();

        /**
         * Will be called when an item was selected from the menu.
         * @param item The {@link Item} that was selected.
         */
        void onItemSelected(Item item);
    }

    private final int mMenuWidth;

    private AnchoredPopupWindow mPopupMenu;
    private Delegate mDelegate;

    /**
     * Creates a new {@link ListMenuButton}.
     * @param context The {@link Context} used to build the visuals from.
     * @param attrs   The specific {@link AttributeSet} used to build the button.
     */
    public ListMenuButton(Context context, AttributeSet attrs) {
        super(context, attrs);
        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.ListMenuButton);
        mMenuWidth = a.getDimensionPixelSize(R.styleable.ListMenuButton_menuWidth,
                getResources().getDimensionPixelSize(R.dimen.list_menu_width));
        a.recycle();
    }

    /**
     * Text that represents the item this menu button is related to.  This will affect the content
     * description of the view {@see #setContentDescription(CharSequence)}.
     * @param context The string representation of the list item this button represents.
     */
    public void setContentDescriptionContext(String context) {
        if (context == null) context = "";
        setContentDescription(getContext().getResources().getString(
                R.string.accessibility_list_menu_button, context));
    }

    /**
     * Sets the delegate this menu will rely on for populating the popup menu and handling selection
     * responses.  The menu will not show or work without it.
     *
     * @param delegate The {@link Delegate} to use for menu creation and selection handling.
     */
    public void setDelegate(Delegate delegate) {
        dismiss();
        mDelegate = delegate;
    }

    /** Called to dismiss any popup menu that might be showing for this button. */
    public void dismiss() {
        if (mPopupMenu == null) return;
        mPopupMenu.dismiss();
    }

    // View implementation.
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        setContentDescriptionContext("");
        setOnClickListener((view) -> showMenu());
    }

    @Override
    protected void onDetachedFromWindow() {
        dismiss();
        super.onDetachedFromWindow();
    }

    @Override
    public void onPreLayoutChange(
            boolean positionBelow, int x, int y, int width, int height, Rect anchorRect) {
        mPopupMenu.setAnimationStyle(
                positionBelow ? R.style.OverflowMenuAnim : R.style.OverflowMenuAnimBottom);
    }

    private void showMenu() {
        if (mDelegate == null) throw new IllegalStateException("Delegate was not set.");

        // Referenced by PopupWindow helper classes (ArrayAdapter and OnItemClickListener).
        final Item[] items = mDelegate.getItems();

        if (items == null || items.length == 0) {
            throw new IllegalStateException("Delegate provided no items.");
        }

        // If we are about to show the menu, dismiss any previous incarnation in case the items have
        // changed.
        dismiss();

        // Create the adapter for the ListView.
        ArrayAdapter<Item> adapter = new ArrayAdapter<Item>(
                getContext(), R.layout.list_menu_item, items) {
            @Override
            public boolean areAllItemsEnabled() {
                return false;
            }

            @Override
            public boolean isEnabled(int position) {
                return items[position].getIsEnabled();
            }

            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                View view = super.getView(position, convertView, parent);
                view.setEnabled(isEnabled(position));

                // Set the compound drawable at the end for items with a valid endIconId,
                // otherwise clear the compound drawable if the endIconId is 0.
                ApiCompatibilityUtils.setCompoundDrawablesRelativeWithIntrinsicBounds(
                        (TextView) view, 0, 0, items[position].getEndIconId(), 0);

                return view;
            }
        };

        // Create the content view and set up its ListView.
        ViewGroup contentView = (ViewGroup) LayoutInflater.from(getContext())
                                        .inflate(R.layout.app_menu_layout, null);
        ListView list = (ListView) contentView.findViewById(R.id.app_menu_list);
        list.setAdapter(adapter);
        list.setOnItemClickListener((parent, view, position, id) -> {
            if (mDelegate != null) mDelegate.onItemSelected(items[position]);

            // TODO(crbug.com/600642): Somehow the on click event can be triggered way after we
            // dismiss the popup.
            if (mPopupMenu != null) mPopupMenu.dismiss();
        });
        list.setDivider(null);

        // Create the popup window and set properties.
        ViewRectProvider rectProvider = new ViewRectProvider(this);
        rectProvider.setIncludePadding(true);
        mPopupMenu = new AnchoredPopupWindow(getContext(), this,
                ApiCompatibilityUtils.getDrawable(getResources(), R.drawable.popup_bg), contentView,
                rectProvider);
        mPopupMenu.setVerticalOverlapAnchor(true);
        mPopupMenu.setHorizontalOverlapAnchor(true);
        mPopupMenu.setMaxWidth(mMenuWidth);
        mPopupMenu.setFocusable(true);
        mPopupMenu.setLayoutObserver(this);
        mPopupMenu.addOnDismissListener(() -> { mPopupMenu = null; });

        mPopupMenu.show();
    }
}
