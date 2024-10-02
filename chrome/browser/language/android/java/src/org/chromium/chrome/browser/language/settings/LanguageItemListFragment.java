// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

import static org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils.buildMenuListItem;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.DividerItemDecoration;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.language.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ProfileDependentSetting;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.Collection;

/**
 * Base Fragment for an editable list of LanguageItems. Languages can be removed via the overflow
 * menu and added with the `Add Language` button. Subclasses will override makeFragmentListDelegate
 * to populate the LanguageItem list and provide callbacks for adding and removing items.
 */
public abstract class LanguageItemListFragment extends Fragment
        implements EmbeddableSettingsPage, ProfileDependentSetting {
    // Request code for returning from Select Language Fragment
    private static final int REQUEST_CODE_SELECT_LANGUAGE = 1;

    /**
     * Interface for helper functions to populate the LanguageItem list and used by
     * {@link LanguageItemListPreference} to make a summary and launch the correct Fragment.
     */
    public interface ListDelegate {
        /** Return LanguageItems to show in LanguageItemListFragment. */
        Collection<LanguageItem> getLanguageItems();

        /** Return class to launch this LanguageItemListFragment from an Intent. */
        Class<? extends Fragment> getFragmentClass();
    }

    private class ListAdapter extends LanguageListBaseAdapter {
        ListAdapter(Context context, Profile profile) {
            super(context, profile);
        }

        @Override
        public void onBindViewHolder(ViewHolder holder, int position) {
            super.onBindViewHolder(holder, position);

            final LanguageItem currentLanguageItem = getItemByPosition(position);

            ModelList menuItems = new ModelList();
            menuItems.add(buildMenuListItem(R.string.remove, 0, 0));

            // ListMenu.Delegate handles return from three dot menu.
            ListMenu.Delegate delegate =
                    (model) -> {
                        int textId = model.get(ListMenuItemProperties.TITLE_ID);
                        if (textId == R.string.remove) {
                            onLanguageRemoved(currentLanguageItem.getCode());
                            onDataUpdated();
                            recordRemoveAction();
                        }
                    };
            ((LanguageRowViewHolder) holder)
                    .setMenuButtonDelegate(
                            () ->
                                    BrowserUiListMenuUtils.getBasicListMenu(
                                            getContext(), menuItems, delegate));
        }

        public void onDataUpdated() {
            setDisplayedLanguages(mListDelegate.getLanguageItems());
        }
    }

    private Profile mProfile;
    private ListAdapter mAdapter;
    private ListDelegate mListDelegate;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mListDelegate = makeFragmentListDelegate();
        mPageTitle.set(getLanguageListTitle(getContext()));
        recordFragmentImpression();
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        // Inflate the layout for this fragment.
        View inflatedView =
                inflater.inflate(R.layout.language_list_with_add_button, container, false);
        final Activity activity = getActivity();

        RecyclerView mRecyclerView = (RecyclerView) inflatedView.findViewById(R.id.language_list);
        LinearLayoutManager layoutManager = new LinearLayoutManager(activity);
        mRecyclerView.setLayoutManager(layoutManager);
        mRecyclerView.addItemDecoration(
                new DividerItemDecoration(activity, layoutManager.getOrientation()));

        mAdapter = new ListAdapter(activity, mProfile);
        mRecyclerView.setAdapter(mAdapter);
        mAdapter.onDataUpdated();
        ScrollView scrollView = inflatedView.findViewById(R.id.scroll_view);
        scrollView
                .getViewTreeObserver()
                .addOnScrollChangedListener(
                        SettingsUtils.getShowShadowOnScrollListener(
                                scrollView, inflatedView.findViewById(R.id.shadow)));

        TextView addLanguageButton = (TextView) inflatedView.findViewById(R.id.add_language);
        final TintedDrawable tintedDrawable =
                TintedDrawable.constructTintedDrawable(getContext(), R.drawable.plus);
        tintedDrawable.setTint(SemanticColorUtils.getDefaultControlColorActive(getContext()));
        addLanguageButton.setCompoundDrawablesRelativeWithIntrinsicBounds(
                tintedDrawable, null, null, null);

        addLanguageButton.setOnClickListener(
                view -> { // Lambda for View.OnClickListener
                    recordAddLanguageImpression();
                    Intent intent =
                            SettingsNavigationFactory.createSettingsNavigation()
                                    .createSettingsIntent(
                                            getActivity(), SelectLanguageFragment.class);
                    intent.putExtra(
                            SelectLanguageFragment.INTENT_POTENTIAL_LANGUAGES,
                            getPotentialLanguageType());
                    startActivityForResult(intent, REQUEST_CODE_SELECT_LANGUAGE);
                });

        return inflatedView;
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, requestCode, data);
        if (requestCode == REQUEST_CODE_SELECT_LANGUAGE && resultCode == Activity.RESULT_OK) {
            String code = data.getStringExtra(SelectLanguageFragment.INTENT_SELECTED_LANGUAGE);
            onLanguageAdded(code);
            mAdapter.onDataUpdated();
            recordAddAction();
        }
    }

    @Override
    public void setProfile(Profile profile) {
        mProfile = profile;
    }

    /** Return the {@link Profile} associated with this language item. */
    public Profile getProfile() {
        return mProfile;
    }

    /**
     * Return the ListDelegate that will be used to populate the LanguageItemList for a subclass.
     */
    protected abstract LanguageItemListFragment.ListDelegate makeFragmentListDelegate();

    /** Return title for LanguageItemListFragment. */
    protected abstract String getLanguageListTitle(Context context);

    /** Return the type of potential languages to populate the add language fragment with. */
    protected abstract @LanguagesManager.LanguageListType int getPotentialLanguageType();

    /**
     * Records the {@link LanguagesManager.LanguageSettingsPageType} impression for viewing this
     * LanguageItemListFragment.
     */
    protected abstract void recordFragmentImpression();

    /**
     * Records the {@link LanguagesManager.LanguageSettingsPageType} impression for viewing the
     * Add Language page from this LanguageItemListFragment.
     */
    protected abstract void recordAddLanguageImpression();

    /**
     * Records the {@link LanguagesManager.LanguageSettingsActionType} for adding to this
     * LanguageItemListFragment.
     */
    protected abstract void recordAddAction();

    /**
     * Records the {@link LanguagesManager.LanguageSettingsActionType} for removing from this
     * LanguageItemListFragment.
     */
    protected abstract void recordRemoveAction();

    /** Callback for when a language is added to the LanguageItemList. */
    protected abstract void onLanguageAdded(String code);

    /** Callback for when a language is removed to the LanguageItemList. */
    protected abstract void onLanguageRemoved(String code);
}
