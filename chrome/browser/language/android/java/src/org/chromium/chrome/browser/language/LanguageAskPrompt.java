// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.base.LocaleUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.language.settings.LanguageItem;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.components.language.GeoLanguageProviderBridge;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.TreeSet;

/**
 * Implements a modal dialog that prompts the user about the languages they can read. Displayed
 * once at browser startup when no other promo or modals are shown. Selected languages are added to
 * the user Accept-Languages.
 */
public class LanguageAskPrompt implements ModalDialogProperties.Controller {
    // Enum values for the Translate.ExplicitLanguageAsk.Event histogram.
    private static final int PROMPT_EVENT_CANCELLED = 2;

    private class SeparatorViewHolder extends ViewHolder {
        SeparatorViewHolder(View view) {
            super(view);
        }
    }

    private class LanguageAskPromptRowViewHolder
            extends ViewHolder implements View.OnClickListener {
        private TextView mLanguageNameTextView;
        private TextView mNativeNameTextView;
        private CheckBox mCheckbox;
        private ImageView mDeviceLanguageIcon;
        private String mCode;
        private HashSet<String> mLanguagesUpdate;

        LanguageAskPromptRowViewHolder(View view) {
            super(view);
            view.setOnClickListener(this);
            mLanguageNameTextView =
                    ((TextView) itemView.findViewById(R.id.ui_language_representation));
            mNativeNameTextView =
                    ((TextView) itemView.findViewById(R.id.native_language_representation));
            mCheckbox = ((CheckBox) itemView.findViewById(R.id.language_ask_checkbox));
            mDeviceLanguageIcon = ((ImageView) itemView.findViewById(R.id.device_language_icon));
            mCheckbox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                @Override
                public void onCheckedChanged(CompoundButton button, boolean isChecked) {
                    if (isChecked) {
                        mLanguagesUpdate.add(mCode);
                    } else {
                        mLanguagesUpdate.remove(mCode);
                    }
                }
            });
        }

        @Override
        public void onClick(View v) {
            mCheckbox.setChecked(!mCheckbox.isChecked());
        }

        /**
         * Sets the text in the TextView children of this row to |languageName| and |nativeName|
         * respectively.
         * @param languageName The name of this row's language in the browser's locale.
         * @param nativeLanguage The name of this row's language as written in that language.
         * @param code The standard language code for this row's language.
         * @param languagesUpdate The set of language codes to use to know which languages to
         *        add/remove from the language list.
         */
        public void setLanguage(String languageName, String nativeName, String code,
                HashSet<String> languagesUpdate) {
            mLanguageNameTextView.setText(languageName);
            mNativeNameTextView.setText(nativeName);
            mCode = code;
            mLanguagesUpdate = languagesUpdate;
            mCheckbox.setChecked(mLanguagesUpdate.contains(mCode));
            mDeviceLanguageIcon.setVisibility(LocaleUtils.getDefaultLocaleString().equals(code)
                            ? View.VISIBLE
                            : View.INVISIBLE);
        }
    }

    private class LanguageItemAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {
        private List<LanguageItem> mTopLanguages;
        private List<LanguageItem> mBottomLanguages;
        private HashSet<String> mLanguagesUpdate;

        private static final int TYPE_LANGUAGE_ITEM = 0;
        private static final int TYPE_SEPARATOR = 1;

        /**
         * @param context The context this item's views will be associated with.
         * @param languagesUpdate The set of language codes to use to know which languages to
         *        add/remove from the language list.
         */
        public LanguageItemAdapter(Context context, HashSet<String> languagesUpdate) {
            mTopLanguages = new ArrayList<LanguageItem>();
            mBottomLanguages = new ArrayList<LanguageItem>();
            mLanguagesUpdate = languagesUpdate;
        }

        @Override
        public int getItemViewType(int position) {
            if (position == mTopLanguages.size()) {
                return TYPE_SEPARATOR;
            }

            return TYPE_LANGUAGE_ITEM;
        }

        @Override
        public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            if (viewType == TYPE_LANGUAGE_ITEM) {
                View row = LayoutInflater.from(parent.getContext())
                                   .inflate(R.layout.language_ask_prompt_row, parent, false);

                return new LanguageAskPromptRowViewHolder(row);
            } else if (viewType == TYPE_SEPARATOR) {
                return new SeparatorViewHolder(
                        LayoutInflater.from(parent.getContext())
                                .inflate(
                                        R.layout.language_ask_prompt_row_separator, parent, false));
            }
            // NOTREACHED
            return null;
        }

        @Override
        public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {
            if (getItemViewType(position) == TYPE_LANGUAGE_ITEM) {
                LanguageItem lang = getLanguageItemAt(position);
                ((LanguageAskPromptRowViewHolder) holder)
                        .setLanguage(lang.getDisplayName(), lang.getNativeDisplayName(),
                                lang.getCode(), mLanguagesUpdate);
            }
            // No binding necessary for the separator.
        }

        @Override
        public int getItemCount() {
            // Sum of both lists + a separator.
            return mTopLanguages.size() + mBottomLanguages.size() + 1;
        }

        private LanguageItem getLanguageItemAt(int position) {
            if (position < mTopLanguages.size()) {
                return mTopLanguages.get(position);
            } else if (position > mTopLanguages.size()) {
                return mBottomLanguages.get(position - mTopLanguages.size() - 1);
            }

            // NOTREACHED, this would mean the language item at the separator's position is being
            // requested
            return null;
        }

        /**
         * Sets the list of languages to |languages| and notifies the RecyclerView that the data has
         * changed.
         * @param topLanguages The new list of languages to be displayed above the separator.
         * @param bottomLanguages The new list of languages to be displayed below the separator.
         */
        public void setLanguages(
                List<LanguageItem> topLanguages, List<LanguageItem> bottomLanguages) {
            mTopLanguages.clear();
            mTopLanguages.addAll(topLanguages);
            mBottomLanguages.clear();
            mBottomLanguages.addAll(bottomLanguages);
            notifyDataSetChanged();
        }
    }

    private class ListScrollListener extends RecyclerView.OnScrollListener {
        private RecyclerView mList;
        private ImageView mTopShadow;
        private ImageView mBottomShadow;

        public ListScrollListener(RecyclerView list, ImageView topShadow, ImageView bottomShadow) {
            mList = list;
            mTopShadow = topShadow;
            mBottomShadow = bottomShadow;
            mList.setOnScrollListener(this);
        }

        @Override
        public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
            if (mList.canScrollVertically(-1)) {
                mTopShadow.setVisibility(View.VISIBLE);
            } else {
                mTopShadow.setVisibility(View.GONE);
            }

            if (mList.canScrollVertically(1)) {
                mBottomShadow.setVisibility(View.VISIBLE);
            } else {
                mBottomShadow.setVisibility(View.GONE);
            }
        }
    }

    /**
     * Displays the Explicit Language Ask prompt if the experiment is enabled.
     * @param activity The current activity to display the prompt into.
     * @param modalDialogManagerSupplier Supplier of {@link ModalDialogManager}.
     * @return Whether the prompt was shown or not.
     */
    public static boolean maybeShowLanguageAskPrompt(
            Activity activity, ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier) {
        // Do not show the Accept-Language prompt if the App Language prompt was shown.
        if (TranslateBridge.getAppLanguagePromptShown()) return false;
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.EXPLICIT_LANGUAGE_ASK)) return false;
        if (TranslateBridge.getExplicitLanguageAskPromptShown()) return false;

        LanguageAskPrompt prompt = new LanguageAskPrompt();
        prompt.show(activity, modalDialogManagerSupplier);

        TranslateBridge.setExplicitLanguageAskPromptShown(true);

        return true;
    }

    private ListScrollListener mListScrollListener;
    private ModalDialogManager mModalDialogManager;
    private HashSet<String> mLanguagesUpdate;
    private HashSet<String> mInitialLanguages;

    public LanguageAskPrompt() {}

    /**
     * Mutates the user's accept languages pref so that it reflects which languages are in
     * mLanguagesUpdate, and by extension which languages were checked by the user in the prompt.
     */
    private void saveLanguages() {
        TreeSet<String> languagesToAdd = new TreeSet<String>(mLanguagesUpdate);
        languagesToAdd.removeAll(mInitialLanguages);

        for (String language : languagesToAdd) {
            TranslateBridge.updateUserAcceptLanguages(language, true);
        }

        HashSet<String> languagesToRemove = new HashSet<String>(mInitialLanguages);
        languagesToRemove.removeAll(mLanguagesUpdate);

        for (String language : languagesToRemove) {
            TranslateBridge.updateUserAcceptLanguages(language, false);
        }
    }

    /**
     * Displays this prompt inside the specified |activity|.
     * @param activity The current activity to display the prompt into.
     * @param modalDialogManagerSupplier Supplier of {@link ModalDialogManager}.
     */
    public void show(
            Activity activity, ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier) {
        if (activity == null) return;

        List<String> userAcceptLanguagesList = TranslateBridge.getUserLanguageCodes();
        mInitialLanguages = new HashSet<String>();
        mInitialLanguages.addAll(userAcceptLanguagesList);
        mLanguagesUpdate = new HashSet<String>(mInitialLanguages);

        View customView = LayoutInflater.from(activity).inflate(
                R.layout.language_ask_prompt_content, null, false);
        RecyclerView list = customView.findViewById(R.id.language_ask_prompt_content_recycler_view);
        LanguageItemAdapter adapter = new LanguageItemAdapter(activity, mLanguagesUpdate);
        list.setAdapter(adapter);
        LinearLayoutManager linearLayoutManager = new LinearLayoutManager(activity);
        linearLayoutManager.setOrientation(LinearLayoutManager.VERTICAL);
        list.setLayoutManager(linearLayoutManager);
        list.setHasFixedSize(true);

        ImageView topShadow = customView.findViewById(R.id.top_shadow);
        ImageView bottomShadow = customView.findViewById(R.id.bottom_shadow);
        mListScrollListener = new ListScrollListener(list, topShadow, bottomShadow);

        List<LanguageItem> languages = TranslateBridge.getChromeLanguageList();
        LinkedHashSet<String> currentGeoLanguages =
                GeoLanguageProviderBridge.getCurrentGeoLanguages();

        List<LanguageItem> topLanguages = new ArrayList<LanguageItem>();
        List<LanguageItem> bottomLanguages = new ArrayList<LanguageItem>();
        for (LanguageItem language : languages) {
            if (currentGeoLanguages.contains(language.getCode())
                    || mInitialLanguages.contains(language.getCode())) {
                topLanguages.add(language);
            } else {
                bottomLanguages.add(language);
            }
        }

        Collections.sort(topLanguages, new Comparator<LanguageItem>() {
            private int computeItemScore(LanguageItem item) {
                // Order languages so that the region's languages are on top, followed by the ones
                // already in the user's accept languages.
                if (currentGeoLanguages.contains(item.getCode())) return -2;
                if (mInitialLanguages.contains(item.getCode())) return -1;
                return 0;
            }
            @Override
            public int compare(LanguageItem first, LanguageItem second) {
                return computeItemScore(first) - computeItemScore(second);
            }
        });

        adapter.setLanguages(topLanguages, bottomLanguages);

        Resources resources = activity.getResources();
        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.TITLE, resources,
                                R.string.languages_explicit_ask_title)
                        .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources, R.string.save)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                R.string.cancel)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .build();

        mModalDialogManager = modalDialogManagerSupplier.get();
        mModalDialogManager.showDialog(model, ModalDialogManager.ModalDialogType.APP);
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        } else {
            saveLanguages();
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        }
    }

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {}
}
