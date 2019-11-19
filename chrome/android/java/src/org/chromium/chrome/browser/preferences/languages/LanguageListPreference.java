// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.languages;

import android.content.Context;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceViewHolder;
import android.support.v7.widget.DividerItemDecoration;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.util.AttributeSet;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.chrome.browser.ui.widget.ListMenuButton;
import org.chromium.chrome.browser.ui.widget.ListMenuButton.Item;
import org.chromium.chrome.browser.ui.widget.TintedDrawable;

import java.util.ArrayList;

/**
 * A preference that displays the current accept language list.
 */
public class LanguageListPreference extends Preference {
    private static class LanguageListAdapter
            extends LanguageListBaseAdapter implements LanguagesManager.AcceptLanguageObserver {

        LanguageListAdapter(Context context) {
            super(context);
        }

        @Override
        public void onBindViewHolder(ViewHolder holder, int position) {
            super.onBindViewHolder(holder, position);

            final LanguageItem info = getItemByPosition(position);

            showDragIndicatorInRow((LanguageRowViewHolder) holder);
            ((LanguageRowViewHolder) holder).setMenuButtonDelegate(new ListMenuButton.Delegate() {
                @Override
                public Item[] getItems() {
                    ArrayList<Item> menuItems = new ArrayList<>();

                    // Show "Offer to translate" option if "Chrome Translate" is enabled.
                    if (PrefServiceBridge.getInstance().getBoolean(Pref.OFFER_TRANSLATE_ENABLED)) {
                        // Set this row checked if the language is unblocked.
                        int endIconResId = TranslateBridge.isBlockedLanguage(info.getCode())
                                ? 0
                                : R.drawable.ic_check_googblue_24dp;
                        // Add checked icon at the end.
                        menuItems.add(new Item(mContext,
                                R.string.languages_item_option_offer_to_translate, endIconResId,
                                info.isSupported()));
                    }

                    int languageCount = getItemCount();
                    // Enable "Remove" option if there are multiple accept languages.
                    menuItems.add(new Item(mContext, R.string.remove, languageCount > 1));

                    // Add some appropriate options for moving the language when the list is not
                    // draggable. E.g. in the accessibility mode.
                    if (!mDragStateDelegate.getDragEnabled()) {
                        // Add "Move to top" and "Move up" menu when it's not the first one.
                        if (position > 0) {
                            menuItems.add(new Item(mContext, R.string.menu_item_move_to_top, true));
                            menuItems.add(new Item(mContext, R.string.menu_item_move_up, true));
                        }

                        // Add "Move down" menu when it's not the last one.
                        if (position < (languageCount - 1)) {
                            menuItems.add(new Item(mContext, R.string.menu_item_move_down, true));
                        }
                    }

                    return menuItems.toArray(new Item[menuItems.size()]);
                }

                @Override
                public void onItemSelected(Item item) {
                    if (item.getTextId() == R.string.languages_item_option_offer_to_translate) {
                        // Toggle current blocked state of this language.
                        boolean state = (item.getEndIconId() == 0);
                        TranslateBridge.setLanguageBlockedState(info.getCode(), !state);
                        LanguagesManager.recordAction(state
                                        ? LanguagesManager.LanguageSettingsActionType
                                                  .ENABLE_TRANSLATE_FOR_SINGLE_LANGUAGE
                                        : LanguagesManager.LanguageSettingsActionType
                                                  .DISABLE_TRANSLATE_FOR_SINGLE_LANGUAGE);
                    } else if (item.getTextId() == R.string.remove) {
                        LanguagesManager.getInstance().removeFromAcceptLanguages(info.getCode());
                        LanguagesManager.recordAction(
                                LanguagesManager.LanguageSettingsActionType.LANGUAGE_REMOVED);
                    } else if (item.getTextId() == R.string.menu_item_move_up) {
                        LanguagesManager.getInstance().moveLanguagePosition(
                                info.getCode(), -1, true);
                    } else if (item.getTextId() == R.string.menu_item_move_down) {
                        LanguagesManager.getInstance().moveLanguagePosition(
                                info.getCode(), 1, true);
                    } else if (item.getTextId() == R.string.menu_item_move_to_top) {
                        LanguagesManager.getInstance().moveLanguagePosition(
                                info.getCode(), -position, true);
                    }
                }
            });
        }

        @Override
        public void onDataUpdated() {
            if (mDragStateDelegate.getDragActive()) {
                enableDrag();
            } else {
                disableDrag();
            }
            setDisplayedLanguages(LanguagesManager.getInstance().getUserAcceptLanguageItems());
        }
    }

    private TextView mAddLanguageButton;
    private RecyclerView mRecyclerView;
    private LanguageListAdapter mAdapter;
    private AddLanguageFragment.Launcher mLauncher;

    public LanguageListPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mAdapter = new LanguageListAdapter(context);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        assert mLauncher != null;

        mAddLanguageButton = (TextView) holder.findViewById(R.id.add_language);
        mAddLanguageButton.setCompoundDrawablesRelativeWithIntrinsicBounds(
                TintedDrawable.constructTintedDrawable(
                        getContext(), R.drawable.plus, R.color.pref_accent_color),
                null, null, null);
        mAddLanguageButton.setOnClickListener(view -> {
            mLauncher.launchAddLanguage();
            LanguagesManager.recordAction(
                    LanguagesManager.LanguageSettingsActionType.CLICK_ON_ADD_LANGUAGE);
        });

        mRecyclerView = (RecyclerView) holder.findViewById(R.id.language_list);
        LinearLayoutManager layoutMangager = new LinearLayoutManager(getContext());
        mRecyclerView.setLayoutManager(layoutMangager);
        mRecyclerView.addItemDecoration(
                new DividerItemDecoration(getContext(), layoutMangager.getOrientation()));

        // We do not want the RecyclerView to be announced by screen readers every time
        // the view is bound.
        if (mRecyclerView.getAdapter() != mAdapter) {
            mRecyclerView.setAdapter(mAdapter);
            LanguagesManager.getInstance().setAcceptLanguageObserver(mAdapter);
            // Initialize accept language list.
            mAdapter.onDataUpdated();
        }
    }

    /**
     * Register a launcher for AddLanguageFragment. Preference's host fragment should call
     * this in its onCreate().
     */
    void registerActivityLauncher(AddLanguageFragment.Launcher launcher) {
        mLauncher = launcher;
    }
}
