// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.graphics.drawable.Drawable;
import android.support.v7.content.res.AppCompatResources;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.ui.appmenu.internal.R;
import org.chromium.chrome.browser.ui.widget.highlight.ViewHighlighter;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.interpolators.BakedBezierInterpolator;
import org.chromium.ui.widget.ChromeImageButton;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * ListAdapter to customize the view of items in the list.
 *
 * The icon row in the menu is a special case of a MenuItem that displays multiple buttons in a row.
 * If, for some unfathomable reason, you need to add yet another icon to the row (the current max
 * is five), then you will need to:
 *
 * 1) Update icon_row_menu_item.xml to have as many buttons as you need.
 * 2) Edit the {@link BUTTON_IDS} to reference your new button.
 * 3) Hope that the icon row still fits on small phones.
 */
class AppMenuAdapter extends BaseAdapter {
    /**
     * Interface to handle clicks and long-clicks on menu items.
     */
    interface OnClickHandler {
        /**
         * Handles clicks on the AppMenu popup.
         * @param menuItem The menu item in that was clicked.
         */
        void onItemClick(MenuItem menuItem);

        /**
         * Handles long clicks on image buttons on the AppMenu popup.
         * @param menuItem The menu item that was long clicked.
         * @param view The anchor view of the menu item.
         */
        boolean onItemLongClick(MenuItem menuItem, View view);
    }

    @IntDef({MenuItemType.STANDARD, MenuItemType.TITLE_BUTTON, MenuItemType.THREE_BUTTON,
            MenuItemType.FOUR_BUTTON, MenuItemType.FIVE_BUTTON})
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    @interface MenuItemType {
        /**
         * Regular Android menu item that contains a title and an icon if icon is specified.
         */
        int STANDARD = 0;
        /**
         * Menu item that has two buttons, the first one is a title and the second one is an icon.
         * It is different from the regular menu item because it contains two separate buttons.
         */
        int TITLE_BUTTON = 1;
        /**
         * Menu item that has three buttons. Every one of these buttons is displayed as an icon.
         */
        int THREE_BUTTON = 2;
        /**
         * Menu item that has four buttons. Every one of these buttons is displayed as an icon.
         */
        int FOUR_BUTTON = 3;
        /**
         * Menu item that has five buttons. Every one of these buttons is displayed as an icon.
         */
        int FIVE_BUTTON = 4;
        /**
         * The number of view types specified above.  If you add a view type you MUST increment
         * this.
         */
        int NUM_ENTRIES = 5;
    }

    /** IDs of all of the buttons in icon_row_menu_item.xml. */
    private static final int[] BUTTON_IDS = {R.id.button_one, R.id.button_two, R.id.button_three,
            R.id.button_four, R.id.button_five};

    /** MenuItem Animation Constants */
    private static final int ENTER_ITEM_DURATION_MS = 350;
    private static final int ENTER_ITEM_BASE_DELAY_MS = 80;
    private static final int ENTER_ITEM_ADDL_DELAY_MS = 30;
    private static final float ENTER_STANDARD_ITEM_OFFSET_Y_DP = -10.f;
    private static final float ENTER_STANDARD_ITEM_OFFSET_X_DP = 10.f;

    private final OnClickHandler mOnClickHandler;
    private final LayoutInflater mInflater;
    private final List<MenuItem> mMenuItems;
    private final int mNumMenuItems;
    private final Integer mHighlightedItemId;
    private final float mDpToPx;
    private final @Nullable List<CustomViewBinder> mCustomViewBinders;
    private final int mCustomViewTypes;
    private final Map<CustomViewBinder, Integer> mViewTypeOffsetMap;

    public AppMenuAdapter(OnClickHandler onClickHandler, List<MenuItem> menuItems,
            LayoutInflater inflater, Integer highlightedItemId,
            @Nullable List<CustomViewBinder> customViewBinders) {
        mOnClickHandler = onClickHandler;
        mMenuItems = menuItems;
        mInflater = inflater;
        mHighlightedItemId = highlightedItemId;
        mCustomViewBinders = customViewBinders;
        mNumMenuItems = menuItems.size();
        mDpToPx = inflater.getContext().getResources().getDisplayMetrics().density;

        mCustomViewTypes = getCustomViewTypeCount(customViewBinders);
        mViewTypeOffsetMap = new HashMap<>();
        populateCustomViewBinderOffsetMap(
                customViewBinders, mViewTypeOffsetMap, MenuItemType.NUM_ENTRIES);
    }

    @Override
    public int getCount() {
        return mNumMenuItems;
    }

    @Override
    public int getViewTypeCount() {
        return MenuItemType.NUM_ENTRIES + mCustomViewTypes;
    }

    @Override
    public @MenuItemType int getItemViewType(int position) {
        MenuItem item = getItem(position);
        int viewCount = item.hasSubMenu() ? item.getSubMenu().size() : 1;
        int customItemViewType = getCustomItemViewType(item);
        if (customItemViewType != CustomViewBinder.NOT_HANDLED) {
            return customItemViewType;
        } else if (viewCount == 2) {
            return MenuItemType.TITLE_BUTTON;
        } else if (viewCount == 3) {
            return MenuItemType.THREE_BUTTON;
        } else if (viewCount == 4) {
            return MenuItemType.FOUR_BUTTON;
        } else if (viewCount == 5) {
            return MenuItemType.FIVE_BUTTON;
        }
        return MenuItemType.STANDARD;
    }

    @Override
    public long getItemId(int position) {
        return getItem(position).getItemId();
    }

    @Override
    public MenuItem getItem(int position) {
        if (position == ListView.INVALID_POSITION) return null;
        assert position >= 0;
        assert position < mMenuItems.size();
        return mMenuItems.get(position);
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        final MenuItem item = getItem(position);
        int itemViewType = getItemViewType(position);
        switch (itemViewType) {
            case MenuItemType.STANDARD: {
                StandardMenuItemViewHolder holder = null;
                if (convertView == null
                        || (int) convertView.getTag(R.id.menu_item_view_type)
                                != MenuItemType.STANDARD) {
                    holder = new StandardMenuItemViewHolder();
                    convertView = mInflater.inflate(R.layout.menu_item, parent, false);
                    holder.text = (TextView) convertView.findViewById(R.id.menu_item_text);
                    holder.image = (AppMenuItemIcon) convertView.findViewById(R.id.menu_item_icon);
                    convertView.setTag(holder);
                    convertView.setTag(R.id.menu_item_enter_anim_id,
                            buildStandardItemEnterAnimator(convertView, position));
                } else {
                    holder = (StandardMenuItemViewHolder) convertView.getTag();
                }
                setupStandardMenuItemViewHolder(holder, convertView, item);
                break;
            }
            case MenuItemType.THREE_BUTTON:
                convertView = createMenuItemRow(convertView, parent, item, 3, itemViewType);
                break;
            case MenuItemType.FOUR_BUTTON:
                convertView = createMenuItemRow(convertView, parent, item, 4, itemViewType);
                break;
            case MenuItemType.FIVE_BUTTON:
                convertView = createMenuItemRow(convertView, parent, item, 5, itemViewType);
                break;
            case MenuItemType.TITLE_BUTTON: {
                assert item.hasSubMenu();
                final MenuItem titleItem = item.getSubMenu().getItem(0);
                final MenuItem subItem = item.getSubMenu().getItem(1);

                TitleButtonMenuItemViewHolder holder = null;
                if (convertView == null
                        || (int) convertView.getTag(R.id.menu_item_view_type)
                                != MenuItemType.TITLE_BUTTON) {
                    convertView = mInflater.inflate(R.layout.title_button_menu_item, parent, false);

                    holder = new TitleButtonMenuItemViewHolder();
                    holder.title = (TextView) convertView.findViewById(R.id.title);
                    holder.checkbox = (AppMenuItemIcon) convertView.findViewById(R.id.checkbox);
                    holder.button = (ChromeImageButton) convertView.findViewById(R.id.button);

                    convertView.setTag(holder);
                    convertView.setTag(R.id.menu_item_enter_anim_id,
                            buildStandardItemEnterAnimator(convertView, position));
                } else {
                    holder = (TitleButtonMenuItemViewHolder) convertView.getTag();
                }

                holder.title.setText(titleItem.getTitle());
                holder.title.setEnabled(titleItem.isEnabled());
                holder.title.setFocusable(titleItem.isEnabled());
                holder.title.setOnClickListener(v -> mOnClickHandler.onItemClick(titleItem));

                if (TextUtils.isEmpty(titleItem.getTitleCondensed())) {
                    holder.title.setContentDescription(null);
                } else {
                    holder.title.setContentDescription(titleItem.getTitleCondensed());
                }

                if (subItem.isCheckable()) {
                    // Display a checkbox for the MenuItem.
                    holder.checkbox.setVisibility(View.VISIBLE);
                    holder.button.setVisibility(View.GONE);
                    setupCheckBox(holder.checkbox, subItem);
                } else if (subItem.getIcon() != null) {
                    // Display an icon alongside the MenuItem.
                    holder.checkbox.setVisibility(View.GONE);
                    holder.button.setVisibility(View.VISIBLE);
                    setupImageButton(holder.button, subItem);
                } else {
                    // Display just the label of the MenuItem.
                    holder.checkbox.setVisibility(View.GONE);
                    holder.button.setVisibility(View.GONE);
                }

                convertView.setFocusable(false);
                convertView.setEnabled(false);
                break;
            }
            default:
                // If we get into this block, the item must be handled by a custom binder.
                assert mCustomViewBinders != null;

                // Use custom binder.
                boolean bound = false;
                for (int i = 0; i < mCustomViewBinders.size(); i++) {
                    CustomViewBinder binder = mCustomViewBinders.get(i);
                    int customItemViewType = binder.getItemViewType(item.getItemId());
                    if (customItemViewType == CustomViewBinder.NOT_HANDLED) continue;

                    // If the convertView wasn't previously used for the same item view type,
                    // set it back to null so that the custom binder isn't passed a view that it
                    // can't/shouldn't re-use.
                    if (convertView != null
                            && (int) convertView.getTag(R.id.menu_item_view_type) != itemViewType) {
                        convertView = null;
                    }

                    convertView = binder.getView(item, convertView, parent, mInflater);

                    if (binder.supportsEnterAnimation(item.getItemId())) {
                        convertView.setTag(R.id.menu_item_enter_anim_id,
                                buildStandardItemEnterAnimator(convertView, position));
                    }

                    // This will ensure that the item is not highlighted when selected.
                    convertView.setEnabled(item.isEnabled());

                    convertView.setOnClickListener(v -> mOnClickHandler.onItemClick(item));

                    bound = true;
                    break;
                }

                assert bound : "No binder found for item.";

                break;
        }

        if (mHighlightedItemId != null && item.getItemId() == mHighlightedItemId) {
            ViewHighlighter.turnOnHighlight(convertView, false);
        } else {
            ViewHighlighter.turnOffHighlight(convertView);
        }

        convertView.setTag(R.id.menu_item_view_type, itemViewType);

        return convertView;
    }

    private void setupCheckBox(AppMenuItemIcon button, final MenuItem item) {
        button.setChecked(item.isChecked());

        // The checkbox must be tinted to make Android consistently style it across OS versions.
        // http://crbug.com/571445
        ApiCompatibilityUtils.setImageTintList(button,
                AppCompatResources.getColorStateList(button.getContext(), R.color.checkbox_tint));

        setupMenuButton(button, item);
    }

    private void setupImageButton(ImageButton button, final MenuItem item) {
        // Store and recover the level of image as button.setimageDrawable
        // resets drawable to default level.
        int currentLevel = item.getIcon().getLevel();
        button.setImageDrawable(item.getIcon());
        item.getIcon().setLevel(currentLevel);

        if (item.isChecked()) {
            ApiCompatibilityUtils.setImageTintList(button,
                    AppCompatResources.getColorStateList(
                            button.getContext(), R.color.blue_mode_tint));
        }

        setupMenuButton(button, item);
    }

    private void setupMenuButton(View button, final MenuItem item) {
        button.setEnabled(item.isEnabled());
        button.setFocusable(item.isEnabled());
        if (TextUtils.isEmpty(item.getTitleCondensed())) {
            button.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        } else {
            button.setContentDescription(item.getTitleCondensed());
            button.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_AUTO);
        }

        button.setOnClickListener(v -> mOnClickHandler.onItemClick(item));

        button.setOnLongClickListener(v -> mOnClickHandler.onItemLongClick(item, v));

        if (mHighlightedItemId != null && item.getItemId() == mHighlightedItemId) {
            ViewHighlighter.turnOnHighlight(button, true);
        } else {
            ViewHighlighter.turnOffHighlight(button);
        }

        // Menu items may be hidden by command line flags before they get to this point.
        button.setVisibility(item.isVisible() ? View.VISIBLE : View.GONE);
    }

    private void setupStandardMenuItemViewHolder(
            StandardMenuItemViewHolder holder, View convertView, final MenuItem item) {
        // Set up the icon.
        Drawable icon = item.getIcon();
        holder.image.setImageDrawable(icon);
        holder.image.setVisibility(icon == null ? View.GONE : View.VISIBLE);
        holder.image.setChecked(item.isChecked());
        holder.text.setText(item.getTitle());
        holder.text.setContentDescription(item.getTitleCondensed());

        boolean isEnabled = item.isEnabled();
        // Set the text color (using a color state list).
        holder.text.setEnabled(isEnabled);
        // This will ensure that the item is not highlighted when selected.
        convertView.setEnabled(isEnabled);

        convertView.setOnClickListener(v -> mOnClickHandler.onItemClick(item));
    }

    /**
     * This builds an {@link Animator} for the enter animation of a standard menu item.  This means
     * it will animate the alpha from 0 to 1 and translate the view from -10dp to 0dp on the y axis.
     *
     * @param view     The menu item {@link View} to be animated.
     * @param position The position in the menu.  This impacts the start delay of the animation.
     * @return         The {@link Animator}.
     */
    private Animator buildStandardItemEnterAnimator(final View view, int position) {
        final int startDelay = ENTER_ITEM_BASE_DELAY_MS + ENTER_ITEM_ADDL_DELAY_MS * position;

        AnimatorSet animation = new AnimatorSet();
        final float offsetYPx = ENTER_STANDARD_ITEM_OFFSET_Y_DP * mDpToPx;
        animation.playTogether(ObjectAnimator.ofFloat(view, View.ALPHA, 0.f, 1.f),
                ObjectAnimator.ofFloat(view, View.TRANSLATION_Y, offsetYPx, 0.f));
        animation.setStartDelay(startDelay);
        animation.setDuration(ENTER_ITEM_DURATION_MS);
        animation.setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);

        animation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                view.setAlpha(0.f);
            }
        });
        return animation;
    }

    /**
     * This builds an {@link Animator} for the enter animation of icon row menu items.  This means
     * it will animate the alpha from 0 to 1 and translate the views from 10dp to 0dp on the x axis.
     *
     * @param buttons The list of icons in the menu item that should be animated.
     * @return        The {@link Animator}.
     */
    private Animator buildIconItemEnterAnimator(final ImageView[] buttons) {
        final boolean rtl = LocalizationUtils.isLayoutRtl();
        final float offsetXPx = ENTER_STANDARD_ITEM_OFFSET_X_DP * mDpToPx * (rtl ? -1.f : 1.f);
        final int maxViewsToAnimate = buttons.length;

        AnimatorSet animation = new AnimatorSet();
        AnimatorSet.Builder builder = null;
        for (int i = 0; i < maxViewsToAnimate; i++) {
            final int startDelay = ENTER_ITEM_ADDL_DELAY_MS * i;

            ImageView view = buttons[i];
            Animator alpha = ObjectAnimator.ofFloat(view, View.ALPHA, 0.f, 1.f);
            Animator translate = ObjectAnimator.ofFloat(view, View.TRANSLATION_X, offsetXPx, 0);
            alpha.setStartDelay(startDelay);
            translate.setStartDelay(startDelay);
            alpha.setDuration(ENTER_ITEM_DURATION_MS);
            translate.setDuration(ENTER_ITEM_DURATION_MS);

            if (builder == null) {
                builder = animation.play(alpha);
            } else {
                builder.with(alpha);
            }
            builder.with(translate);
        }
        animation.setStartDelay(ENTER_ITEM_BASE_DELAY_MS);
        animation.setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);

        animation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                for (int i = 0; i < maxViewsToAnimate; i++) {
                    buttons[i].setAlpha(0.f);
                }
            }
        });
        return animation;
    }

    private View createMenuItemRow(
            View convertView, ViewGroup parent, MenuItem item, int numItems, int itemViewType) {
        RowItemViewHolder holder = null;
        if (convertView == null
                || (int) convertView.getTag(R.id.menu_item_view_type) != itemViewType) {
            holder = new RowItemViewHolder(numItems);
            convertView = mInflater.inflate(R.layout.icon_row_menu_item, parent, false);

            // Save references to all the buttons.
            for (int i = 0; i < numItems; i++) {
                ImageButton view = convertView.findViewById(BUTTON_IDS[i]);
                holder.buttons[i] = view;
            }

            // Remove unused menu items.
            for (int j = numItems; j < 5; j++) {
                ((ViewGroup) convertView).removeView(convertView.findViewById(BUTTON_IDS[j]));
            }

            convertView.setTag(holder);
            convertView.setTag(
                    R.id.menu_item_enter_anim_id, buildIconItemEnterAnimator(holder.buttons));
        } else {
            holder = (RowItemViewHolder) convertView.getTag();
        }

        for (int i = 0; i < numItems; i++) {
            setupImageButton(holder.buttons[i], item.getSubMenu().getItem(i));
        }
        convertView.setFocusable(false);
        convertView.setEnabled(false);
        return convertView;
    }

    private int getCustomItemViewType(MenuItem item) {
        if (mCustomViewBinders == null) return CustomViewBinder.NOT_HANDLED;

        for (int i = 0; i < mCustomViewBinders.size(); i++) {
            CustomViewBinder binder = mCustomViewBinders.get(i);
            int binderViewType = binder.getItemViewType(item.getItemId());
            if (binderViewType != CustomViewBinder.NOT_HANDLED) {
                return binderViewType + mViewTypeOffsetMap.get(binder);
            }
        }
        return CustomViewBinder.NOT_HANDLED;
    }

    private static class StandardMenuItemViewHolder {
        public TextView text;
        public AppMenuItemIcon image;
    }

    private static class RowItemViewHolder {
        public ImageButton[] buttons;

        RowItemViewHolder(int numButtons) {
            buttons = new ImageButton[numButtons];
        }
    }

    private static class TitleButtonMenuItemViewHolder {
        public TextView title;
        public AppMenuItemIcon checkbox;
        public ImageButton button;
    }

    @VisibleForTesting
    static int getCustomViewTypeCount(@Nullable List<CustomViewBinder> customViewBinders) {
        if (customViewBinders == null) return 0;

        int count = 0;
        for (int i = 0; i < customViewBinders.size(); i++) {
            count += customViewBinders.get(i).getViewTypeCount();
        }
        return count;
    }

    @VisibleForTesting
    static void populateCustomViewBinderOffsetMap(
            @Nullable List<CustomViewBinder> customViewBinders, Map<CustomViewBinder, Integer> map,
            int startingOffset) {
        if (customViewBinders == null) return;

        int currentOffset = startingOffset;
        for (int i = 0; i < customViewBinders.size(); i++) {
            CustomViewBinder binder = customViewBinders.get(i);
            map.put(binder, currentOffset);
            currentOffset += binder.getViewTypeCount();
        }
    }

    @VisibleForTesting
    Map<CustomViewBinder, Integer> getViewTypeOffsetMapForTests() {
        return mViewTypeOffsetMap;
    }
}
