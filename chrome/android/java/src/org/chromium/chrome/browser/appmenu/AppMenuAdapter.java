// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.graphics.drawable.Drawable;
import android.support.annotation.IntDef;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.AppCompatImageButton;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.widget.ViewHighlighter;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

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
    @IntDef({MenuItemType.STANDARD, MenuItemType.TITLE_BUTTON, MenuItemType.THREE_BUTTON,
            MenuItemType.FOUR_BUTTON, MenuItemType.FIVE_BUTTON, MenuItemType.UPDATE})
    @Retention(RetentionPolicy.SOURCE)
    private @interface MenuItemType {
        /**
         * Regular Android menu item that contains a title and an icon if icon is specified.
         */
        int STANDARD = 0;
        /**
         * Menu item for updating Chrome; uses a custom layout.
         */
        int UPDATE = 1;
        /**
         * Menu item that has two buttons, the first one is a title and the second one is an icon.
         * It is different from the regular menu item because it contains two separate buttons.
         */
        int TITLE_BUTTON = 2;
        /**
         * Menu item that has three buttons. Every one of these buttons is displayed as an icon.
         */
        int THREE_BUTTON = 3;
        /**
         * Menu item that has four buttons. Every one of these buttons is displayed as an icon.
         */
        int FOUR_BUTTON = 4;
        /**
         * Menu item that has five buttons. Every one of these buttons is displayed as an icon.
         */
        int FIVE_BUTTON = 5;
        /**
         * The number of view types specified above.  If you add a view type you MUST increment
         * this.
         */
        int NUM_ENTRIES = 6;
    }

    /** IDs of all of the buttons in icon_row_menu_item.xml. */
    private static final int[] BUTTON_IDS = {
        R.id.button_one,
        R.id.button_two,
        R.id.button_three,
        R.id.button_four,
        R.id.button_five
    };

    /** MenuItem Animation Constants */
    private static final int ENTER_ITEM_DURATION_MS = 350;
    private static final int ENTER_ITEM_BASE_DELAY_MS = 80;
    private static final int ENTER_ITEM_ADDL_DELAY_MS = 30;
    private static final float ENTER_STANDARD_ITEM_OFFSET_Y_DP = -10.f;
    private static final float ENTER_STANDARD_ITEM_OFFSET_X_DP = 10.f;

    private final AppMenu mAppMenu;
    private final LayoutInflater mInflater;
    private final List<MenuItem> mMenuItems;
    private final int mNumMenuItems;
    private final Integer mHighlightedItemId;
    private final float mDpToPx;

    public AppMenuAdapter(AppMenu appMenu, List<MenuItem> menuItems, LayoutInflater inflater,
            Integer highlightedItemId) {
        mAppMenu = appMenu;
        mMenuItems = menuItems;
        mInflater = inflater;
        mHighlightedItemId = highlightedItemId;
        mNumMenuItems = menuItems.size();
        mDpToPx = inflater.getContext().getResources().getDisplayMetrics().density;
    }

    @Override
    public int getCount() {
        return mNumMenuItems;
    }

    @Override
    public int getViewTypeCount() {
        return MenuItemType.NUM_ENTRIES;
    }

    @Override
    public @MenuItemType int getItemViewType(int position) {
        MenuItem item = getItem(position);
        int viewCount = item.hasSubMenu() ? item.getSubMenu().size() : 1;

        if (item.getItemId() == R.id.update_menu_id) {
            return MenuItemType.UPDATE;
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
        switch (getItemViewType(position)) {
            case MenuItemType.STANDARD: {
                StandardMenuItemViewHolder holder = null;
                if (convertView == null
                        || !(convertView.getTag() instanceof StandardMenuItemViewHolder)) {
                    holder = new StandardMenuItemViewHolder();
                    convertView = mInflater.inflate(R.layout.menu_item, parent, false);
                    holder.text = (TextView) convertView.findViewById(R.id.menu_item_text);
                    holder.image = (AppMenuItemIcon) convertView.findViewById(R.id.menu_item_icon);
                    convertView.setTag(holder);
                    convertView.setTag(R.id.menu_item_enter_anim_id,
                            buildStandardItemEnterAnimator(convertView, position));
                    convertView.setTag(
                            R.id.menu_item_original_background, convertView.getBackground());
                } else {
                    holder = (StandardMenuItemViewHolder) convertView.getTag();
                }
                setupStandardMenuItemViewHolder(holder, convertView, item);
                break;
            }
            case MenuItemType.UPDATE: {
                CustomMenuItemViewHolder holder = null;
                if (convertView == null
                        || !(convertView.getTag() instanceof CustomMenuItemViewHolder)) {
                    holder = new CustomMenuItemViewHolder();
                    convertView = mInflater.inflate(R.layout.update_menu_item, parent, false);
                    holder.text = (TextView) convertView.findViewById(R.id.menu_item_text);
                    holder.image = (AppMenuItemIcon) convertView.findViewById(R.id.menu_item_icon);
                    holder.summary = (TextView) convertView.findViewById(R.id.menu_item_summary);
                    convertView.setTag(holder);
                    convertView.setTag(R.id.menu_item_enter_anim_id,
                            buildStandardItemEnterAnimator(convertView, position));
                    convertView.setTag(
                            R.id.menu_item_original_background, convertView.getBackground());
                } else {
                    holder = (CustomMenuItemViewHolder) convertView.getTag();
                }
                setupStandardMenuItemViewHolder(holder, convertView, item);
                UpdateMenuItemHelper.getInstance().decorateMenuItemViews(
                        mInflater.getContext(), holder.text, holder.image, holder.summary);
                break;
            }
            case MenuItemType.THREE_BUTTON:
                convertView = createMenuItemRow(convertView, parent, item, 3);
                break;
            case MenuItemType.FOUR_BUTTON:
                convertView = createMenuItemRow(convertView, parent, item, 4);
                break;
            case MenuItemType.FIVE_BUTTON:
                convertView = createMenuItemRow(convertView, parent, item, 5);
                break;
            case MenuItemType.TITLE_BUTTON: {
                assert item.hasSubMenu();
                final MenuItem titleItem = item.getSubMenu().getItem(0);
                final MenuItem subItem = item.getSubMenu().getItem(1);

                TitleButtonMenuItemViewHolder holder = null;
                if (convertView == null
                        || !(convertView.getTag() instanceof TitleButtonMenuItemViewHolder)) {
                    convertView = mInflater.inflate(R.layout.title_button_menu_item, parent, false);

                    holder = new TitleButtonMenuItemViewHolder();
                    holder.title = (TextView) convertView.findViewById(R.id.title);
                    holder.checkbox = (AppMenuItemIcon) convertView.findViewById(R.id.checkbox);
                    holder.button = (AppCompatImageButton) convertView.findViewById(R.id.button);
                    holder.button.setTag(
                            R.id.menu_item_original_background, holder.button.getBackground());

                    convertView.setTag(holder);
                    convertView.setTag(R.id.menu_item_enter_anim_id,
                            buildStandardItemEnterAnimator(convertView, position));
                    convertView.setTag(
                            R.id.menu_item_original_background, convertView.getBackground());
                } else {
                    holder = (TitleButtonMenuItemViewHolder) convertView.getTag();
                }

                holder.title.setText(titleItem.getTitle());
                holder.title.setEnabled(titleItem.isEnabled());
                holder.title.setFocusable(titleItem.isEnabled());
                holder.title.setOnClickListener(v -> mAppMenu.onItemClick(titleItem));
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
                assert false : "Unexpected MenuItem type";
        }

        if (mHighlightedItemId != null && item.getItemId() == mHighlightedItemId) {
            ViewHighlighter.turnOnHighlight(convertView, false);
        } else {
            ViewHighlighter.turnOffHighlight(convertView);
        }

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

    private void setupImageButton(AppCompatImageButton button, final MenuItem item) {
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

        button.setOnClickListener(v -> mAppMenu.onItemClick(item));

        button.setOnLongClickListener(v -> mAppMenu.onItemLongClick(item, v));

        if (mHighlightedItemId != null && item.getItemId() == mHighlightedItemId) {
            ViewHighlighter.turnOnHighlight(button, true);
        } else {
            ViewHighlighter.turnOffHighlight(button);
        }

        // Menu items may be hidden by command line flags before they get to this point.
        button.setVisibility(item.isVisible() ? View.VISIBLE : View.GONE);
    }

    private void setupStandardMenuItemViewHolder(StandardMenuItemViewHolder holder,
            View convertView, final MenuItem item) {
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

        convertView.setOnClickListener(v -> mAppMenu.onItemClick(item));
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
            View convertView, ViewGroup parent, MenuItem item, int numItems) {
        RowItemViewHolder holder = null;
        if (convertView == null
                || !(convertView.getTag() instanceof RowItemViewHolder)
                || ((RowItemViewHolder) convertView.getTag()).buttons.length != numItems) {
            holder = new RowItemViewHolder(numItems);
            convertView = mInflater.inflate(R.layout.icon_row_menu_item, parent, false);
            convertView.setTag(R.id.menu_item_original_background, convertView.getBackground());

            // Save references to all the buttons.
            for (int i = 0; i < numItems; i++) {
                AppCompatImageButton view =
                        (AppCompatImageButton) convertView.findViewById(BUTTON_IDS[i]);
                holder.buttons[i] = view;
                holder.buttons[i].setTag(
                        R.id.menu_item_original_background, holder.buttons[i].getBackground());
            }

            // Remove unused menu items.
            for (int j = numItems; j < 5; j++) {
                ((ViewGroup) convertView).removeView(convertView.findViewById(BUTTON_IDS[j]));
            }

            convertView.setTag(holder);
            convertView.setTag(R.id.menu_item_enter_anim_id,
                    buildIconItemEnterAnimator(holder.buttons));
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

    static class StandardMenuItemViewHolder {
        public TextView text;
        public AppMenuItemIcon image;
    }

    static class CustomMenuItemViewHolder extends StandardMenuItemViewHolder {
        public TextView summary;
    }

    private static class RowItemViewHolder {
        public AppCompatImageButton[] buttons;

        RowItemViewHolder(int numButtons) {
            buttons = new AppCompatImageButton[numButtons];
        }
    }

    static class TitleButtonMenuItemViewHolder {
        public TextView title;
        public AppMenuItemIcon checkbox;
        public AppCompatImageButton button;
    }
}
