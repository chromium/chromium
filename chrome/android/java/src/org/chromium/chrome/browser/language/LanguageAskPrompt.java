// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language;

import android.content.Context;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.LocaleUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.modaldialog.DialogDismissalCause;
import org.chromium.chrome.browser.modaldialog.ModalDialogManager;
import org.chromium.chrome.browser.modaldialog.ModalDialogView;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.languages.LanguageItem;
import org.chromium.components.language.AndroidLanguageMetricsBridge;
import org.chromium.components.language.GeoLanguageProviderBridge;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;

/**
 * Implements a modal dialog that prompts the user about the languages they can read. Displayed
 * once at browser startup when no other promo or modals are shown.
 */
public class LanguageAskPrompt implements ModalDialogView.Controller {
    // Enum values for the Translate.ExplicitLanguageAsk.Event histogram.
    private static final int PROMPT_EVENT_SHOWN = 0;
    private static final int PROMPT_EVENT_SAVED = 1;
    private static final int PROMPT_EVENT_CANCELLED = 2;
    private static final int PROMPT_EVENT_MAX = PROMPT_EVENT_CANCELLED;

    private void recordPromptEvent(int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "Translate.ExplicitLanguageAsk.Event", event, PROMPT_EVENT_MAX);
    }

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
     * @return Whether the prompt was shown or not.
     */
    public static boolean maybeShowLanguageAskPrompt(ChromeActivity activity) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.EXPLICIT_LANGUAGE_ASK)) return false;
        if (PrefServiceBridge.getInstance().getExplicitLanguageAskPromptShown()) return false;

        LanguageAskPrompt prompt = new LanguageAskPrompt();
        prompt.show(activity);

        PrefServiceBridge.getInstance().setExplicitLanguageAskPromptShown(true);

        return true;
    }

    private ListScrollListener mListScrollListener;
    private ModalDialogManager mModalDialogManager;
    private ModalDialogView mDialog;
    private HashSet<String> mLanguagesUpdate;
    private HashSet<String> mInitialLanguages;

    public LanguageAskPrompt() {}

    /**
     * Mutates the user's accept languages pref so that it reflects which languages are in
     * mLanguagesUpdate, and by extension which languages were checked by the user in the prompt.
     */
    private void saveLanguages() {
        HashSet<String> languagesToAdd = new HashSet<String>(mLanguagesUpdate);
        languagesToAdd.removeAll(mInitialLanguages);

        for (String language : languagesToAdd) {
            PrefServiceBridge.getInstance().updateUserAcceptLanguages(language, true);
            AndroidLanguageMetricsBridge.reportExplicitLanguageAskStateChanged(language, true);
        }

        HashSet<String> languagesToRemove = new HashSet<String>(mInitialLanguages);
        languagesToRemove.removeAll(mLanguagesUpdate);

        for (String language : languagesToRemove) {
            PrefServiceBridge.getInstance().updateUserAcceptLanguages(language, false);
            AndroidLanguageMetricsBridge.reportExplicitLanguageAskStateChanged(language, false);
        }
    }

    /**
     * Displays this prompt inside the specified |activity|.
     * @param activity The current activity to display the prompt into.
     */
    public void show(ChromeActivity activity) {
        if (activity == null) return;

        recordPromptEvent(PROMPT_EVENT_SHOWN);

        List<String> userAcceptLanguagesList =
                PrefServiceBridge.getInstance().getUserLanguageCodes();
        mInitialLanguages = new HashSet<String>();
        mInitialLanguages.addAll(userAcceptLanguagesList);
        mLanguagesUpdate = new HashSet<String>(mInitialLanguages);

        ModalDialogView.Params params = new ModalDialogView.Params();
        params.title = activity.getString(R.string.languages_explicit_ask_title);
        params.positiveButtonTextId = R.string.save;
        params.negativeButtonTextId = R.string.cancel;
        params.cancelOnTouchOutside = true;

        params.customView = LayoutInflater.from(activity).inflate(
                R.layout.language_ask_prompt_content, null, false);
        RecyclerView list = (RecyclerView) params.customView.findViewById(R.id.recycler_view);
        LanguageItemAdapter adapter = new LanguageItemAdapter(activity, mLanguagesUpdate);
        list.setAdapter(adapter);
        LinearLayoutManager linearLayoutManager = new LinearLayoutManager(activity);
        linearLayoutManager.setOrientation(LinearLayoutManager.VERTICAL);
        list.setLayoutManager(linearLayoutManager);
        list.setHasFixedSize(true);

        ImageView topShadow = (ImageView) params.customView.findViewById(R.id.top_shadow);
        ImageView bottomShadow = (ImageView) params.customView.findViewById(R.id.bottom_shadow);
        mListScrollListener = new ListScrollListener(list, topShadow, bottomShadow);

        List<LanguageItem> languages = PrefServiceBridge.getInstance().getChromeLanguageList();
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

        mModalDialogManager = activity.getModalDialogManager();
        mDialog = new ModalDialogView(this, params);
        mModalDialogManager.showDialog(mDialog, ModalDialogManager.ModalDialogType.APP);
    }

    @Override
    public void onClick(int buttonType) {
        if (buttonType == ModalDialogView.ButtonType.NEGATIVE) {
            mModalDialogManager.dismissDialog(
                    mDialog, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        } else {
            saveLanguages();
            mModalDialogManager.dismissDialog(
                    mDialog, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        }
    }

    @Override
    public void onDismiss(@DialogDismissalCause int dismissalCause) {
        if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
            recordPromptEvent(PROMPT_EVENT_SAVED);
        } else {
            recordPromptEvent(PROMPT_EVENT_CANCELLED);
        }
    }
}
