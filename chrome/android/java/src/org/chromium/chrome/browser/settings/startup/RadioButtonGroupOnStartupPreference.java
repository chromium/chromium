// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.startup;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RadioGroup;
import android.widget.RadioGroup.OnCheckedChangeListener;
import android.widget.TextView;

import androidx.preference.PreferenceViewHolder;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.homepage.settings.HomepageSettings;
import org.chromium.chrome.browser.multiwindow.SessionStartupPref;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.ContainedRadioButtonGroupPreference;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;
import org.chromium.components.browser_ui.widget.containment.ContainedLinearLayout;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.ButtonCompat;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A radio button group preference for selecting the session startup behavior ({@link
 * SessionStartupPref#NEW_TAB}, {@link SessionStartupPref#LAST}, or {@link
 * SessionStartupPref#URLS}), along with the configured list of startup pages and per-item overflow
 * actions.
 */
@NullMarked
public class RadioButtonGroupOnStartupPreference extends ContainedRadioButtonGroupPreference
        implements OnCheckedChangeListener {
    private static final int MENU_ID_EDIT = 1;
    private static final int MENU_ID_REMOVE = 2;

    private final Map<String, @Nullable Drawable> mFaviconCache = new HashMap<>();
    private final Map<String, String> mTitleCache = new HashMap<>();

    private @Nullable RadioButtonWithDescription mNewTabPref;
    private @Nullable RadioButtonWithDescription mLastPref;
    private @Nullable RadioButtonWithDescription mUrlsPref;
    private @Nullable LinearLayout mStartupPagesSection;
    private @Nullable LinearLayout mStartupPagesContainer;
    private @Nullable ButtonCompat mAddNewPageButton;
    private @Nullable ButtonCompat mUseCurrentPagesButton;
    private @SessionStartupPref int mStartupType = SessionStartupPref.LAST;
    private List<String> mStartupUrls = new ArrayList<>();
    private @Nullable OnStartupTypeChangeListener mOnStartupTypeChangeListener;
    private @Nullable StartupPageItemActionListener mItemActionListener;
    private @Nullable FaviconLoader mFaviconLoader;
    private @Nullable TitleLoader mTitleLoader;

    /** Constructs a new {@link RadioButtonGroupOnStartupPreference}. */
    public RadioButtonGroupOnStartupPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.radio_button_group_on_startup_preference);
    }

    // ContainedRadioButtonGroupPreference implementation.
    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mNewTabPref = (RadioButtonWithDescription) holder.findViewById(R.id.pref_new_tab);
        mLastPref = (RadioButtonWithDescription) holder.findViewById(R.id.pref_last);
        mUrlsPref = (RadioButtonWithDescription) holder.findViewById(R.id.pref_urls);
        mStartupPagesSection = (LinearLayout) holder.findViewById(R.id.startup_pages_section);
        mStartupPagesContainer = (LinearLayout) holder.findViewById(R.id.startup_pages_container);
        mAddNewPageButton = (ButtonCompat) holder.findViewById(R.id.add_new_page_button);
        if (mAddNewPageButton != null) {
            mAddNewPageButton.setOnClickListener(
                    v -> {
                        if (mItemActionListener != null) {
                            mItemActionListener.onAddNewStartupPage();
                        }
                    });
        }
        mUseCurrentPagesButton = (ButtonCompat) holder.findViewById(R.id.use_current_pages_button);
        if (mUseCurrentPagesButton != null) {
            mUseCurrentPagesButton.setOnClickListener(
                    v -> {
                        if (mItemActionListener != null) {
                            mItemActionListener.onUseCurrentPages();
                        }
                    });
        }

        Context context = getContext();
        mNewTabPref.setDescriptionText(
                SpanApplier.applySpans(
                        context.getString(R.string.on_startup_pref_new_tab_summary),
                        new SpanInfo(
                                "<link>",
                                "</link>",
                                new ChromeClickableSpan(
                                        context,
                                        view ->
                                                SettingsNavigationFactory.createSettingsNavigation(
                                                                context)
                                                        .startSettings(
                                                                context,
                                                                HomepageSettings.class)))));

        ContainedLinearLayout urlsPrefSection =
                (ContainedLinearLayout) holder.findViewById(R.id.urls_pref_section);
        RadioButtonWithDescriptionLayout radioGroup =
                (RadioButtonWithDescriptionLayout) holder.findViewById(R.id.on_startup_radio_group);
        radioGroup.setOnCheckedChangeListener(this);
        if (mUrlsPref.getParent() == radioGroup) {
            // Attach the ContainedLinearLayout to the radio group and move mUrlsPref into it so
            // the radio button and its accessory startup pages list share a single contained card.
            radioGroup.attachAccessoryView(urlsPrefSection, mUrlsPref);
            radioGroup.removeView(mUrlsPref);
            urlsPrefSection.addView(mUrlsPref, /* index= */ 0);
        }

        updateCheckedState();
        updateStartupPagesList();
    }

    // OnCheckedChangeListener implementation.
    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        if (checkedId == R.id.pref_new_tab) {
            mStartupType = SessionStartupPref.NEW_TAB;
        } else if (checkedId == R.id.pref_last) {
            mStartupType = SessionStartupPref.LAST;
        } else if (checkedId == R.id.pref_urls) {
            mStartupType = SessionStartupPref.URLS;
        }
        if (mStartupPagesSection != null) {
            mStartupPagesSection.setVisibility(
                    mStartupType == SessionStartupPref.URLS ? View.VISIBLE : View.GONE);
        }
        if (mOnStartupTypeChangeListener != null) {
            mOnStartupTypeChangeListener.onStartupTypeChanged(mStartupType);
        }
    }

    /* package */ void initialize(
            @SessionStartupPref int startupType,
            List<String> startupUrls,
            OnStartupTypeChangeListener listener,
            StartupPageItemActionListener itemActionListener,
            FaviconLoader faviconLoader,
            TitleLoader titleLoader) {
        mStartupType = startupType;
        mStartupUrls = new ArrayList<>(startupUrls);
        mOnStartupTypeChangeListener = listener;
        mItemActionListener = itemActionListener;
        mFaviconLoader = faviconLoader;
        mTitleLoader = titleLoader;
        updateCheckedState();
        updateStartupPagesList();
    }

    /* package */ void setStartupType(@SessionStartupPref int startupType) {
        mStartupType = startupType;
        updateCheckedState();
    }

    /* package */ void setStartupUrls(List<String> startupUrls) {
        if (mStartupUrls.equals(startupUrls)) {
            return;
        }
        mStartupUrls = new ArrayList<>(startupUrls);
        updateStartupPagesList();
    }

    private void updateCheckedState() {
        if (mNewTabPref == null || mLastPref == null || mUrlsPref == null) {
            return;
        }
        boolean isSpecificPagesSelected = mStartupType == SessionStartupPref.URLS;
        mNewTabPref.setChecked(mStartupType == SessionStartupPref.NEW_TAB);
        mLastPref.setChecked(mStartupType == SessionStartupPref.LAST);
        mUrlsPref.setChecked(isSpecificPagesSelected);
        if (mStartupPagesSection != null) {
            mStartupPagesSection.setVisibility(isSpecificPagesSelected ? View.VISIBLE : View.GONE);
        }
    }

    private void updateStartupPagesList() {
        if (mStartupPagesContainer == null) {
            return;
        }
        // Evict removed URLs while preserving cached metadata for remaining rows.
        mTitleCache.keySet().retainAll(mStartupUrls);
        mFaviconCache.keySet().retainAll(mStartupUrls);

        mStartupPagesContainer.removeAllViews();
        LayoutInflater inflater = LayoutInflater.from(getContext());
        for (int i = 0; i < mStartupUrls.size(); i++) {
            final int index = i;
            final String url = mStartupUrls.get(i);

            // Inflate row view for each configured startup URL.
            View itemView =
                    inflater.inflate(
                            R.layout.startup_page_item,
                            mStartupPagesContainer,
                            /* attachToRoot= */ false);
            ImageView faviconView = itemView.findViewById(R.id.startup_page_favicon);
            TextView titleView = itemView.findViewById(R.id.startup_page_title);
            TextView urlView = itemView.findViewById(R.id.startup_page_url);
            ListMenuButton moreButton = itemView.findViewById(R.id.startup_page_more_button);

            // Display formatted full URL subtext and resolve cached or async website title (falling
            // back to formatted origin).
            String formattedDomain = UrlFormatter.formatUrlForSecurityDisplay(url);
            String displayDomain = !TextUtils.isEmpty(formattedDomain) ? formattedDomain : url;

            String formattedUrl = UrlFormatter.formatUrlForDisplayOmitUsernamePassword(url);
            urlView.setText(!TextUtils.isEmpty(formattedUrl) ? formattedUrl : url);
            moreButton.setContentDescriptionContext(displayDomain);

            // Cache empty title responses as well so unvisited URLs do not re-query HistoryService
            // on subsequent list refreshes.
            if (mTitleCache.containsKey(url)) {
                String cachedTitle = mTitleCache.get(url);
                titleView.setText(!TextUtils.isEmpty(cachedTitle) ? cachedTitle : displayDomain);
            } else {
                titleView.setText(displayDomain);
                if (mTitleLoader != null) {
                    mTitleLoader.loadTitle(
                            url,
                            title -> {
                                mTitleCache.put(url, title);
                                if (!TextUtils.isEmpty(title)) {
                                    titleView.setText(title);
                                }
                            });
                }
            }

            // Cache null favicon responses as well so URLs without favicons do not re-query
            // FaviconHelper on subsequent list refreshes.
            if (mFaviconCache.containsKey(url)) {
                Drawable cachedFavicon = mFaviconCache.get(url);
                if (cachedFavicon != null) {
                    faviconView.setImageDrawable(cachedFavicon);
                }
            } else if (mFaviconLoader != null) {
                mFaviconLoader.loadFavicon(
                        url,
                        drawable -> {
                            mFaviconCache.put(url, drawable);
                            if (drawable != null) {
                                faviconView.setImageDrawable(drawable);
                            }
                        });
            }

            // Populate per-item overflow menu with Edit and Remove actions.
            ModelList modelList = new ModelList();
            modelList.add(
                    new ListItemBuilder()
                            .withTitleRes(R.string.edit)
                            .withMenuId(MENU_ID_EDIT)
                            .build());
            modelList.add(
                    new ListItemBuilder()
                            .withTitleRes(R.string.remove)
                            .withMenuId(MENU_ID_REMOVE)
                            .build());

            // Wire overflow menu item selection to the fragment action listener.
            moreButton.setDelegate(
                    () ->
                            BrowserUiListMenuUtils.getBasicListMenu(
                                    getContext(),
                                    modelList,
                                    (PropertyModel item, View view) -> {
                                        if (mItemActionListener == null) {
                                            return;
                                        }
                                        int menuId = item.get(ListMenuItemProperties.MENU_ITEM_ID);
                                        if (menuId == MENU_ID_EDIT) {
                                            mItemActionListener.onEditStartupPage(index, url);
                                        } else if (menuId == MENU_ID_REMOVE) {
                                            mItemActionListener.onRemoveStartupPage(index, url);
                                        }
                                    }));
            mStartupPagesContainer.addView(itemView);
        }
    }

    /* package */ @Nullable RadioButtonWithDescription getNewTabPrefForTesting() {
        return mNewTabPref;
    }

    /* package */ @Nullable RadioButtonWithDescription getLastPrefForTesting() {
        return mLastPref;
    }

    /* package */ @Nullable RadioButtonWithDescription getUrlsPrefForTesting() {
        return mUrlsPref;
    }

    /* package */ @Nullable LinearLayout getStartupPagesSectionForTesting() {
        return mStartupPagesSection;
    }

    /* package */ @Nullable LinearLayout getStartupPagesContainerForTesting() {
        return mStartupPagesContainer;
    }

    /* package */ @Nullable ButtonCompat getAddNewPageButtonForTesting() {
        return mAddNewPageButton;
    }

    /* package */ @Nullable ButtonCompat getUseCurrentPagesButtonForTesting() {
        return mUseCurrentPagesButton;
    }

    /* package */ interface OnStartupTypeChangeListener {
        void onStartupTypeChanged(@SessionStartupPref int startupType);
    }

    /* package */ interface StartupPageItemActionListener {
        void onAddNewStartupPage();

        void onEditStartupPage(int index, String url);

        void onRemoveStartupPage(int index, String url);

        void onUseCurrentPages();
    }

    /* package */ interface FaviconLoader {
        void loadFavicon(String url, Callback<@Nullable Drawable> callback);
    }

    /* package */ interface TitleLoader {
        void loadTitle(String url, Callback<String> callback);
    }
}
