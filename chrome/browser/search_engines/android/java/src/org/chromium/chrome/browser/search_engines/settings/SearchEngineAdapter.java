// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.AccessibilityDelegate;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.BaseAdapter;
import android.widget.RadioButton;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.PermissionInfo;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.Iterator;
import java.util.List;

/**
 * A custom adapter for listing search engines.
 */
public class SearchEngineAdapter extends BaseAdapter
        implements TemplateUrlService.LoadListener, TemplateUrlService.TemplateUrlServiceObserver,
                   OnClickListener {
    private static final String TAG = "SearchEngines";

    private static final int VIEW_TYPE_ITEM = 0;
    private static final int VIEW_TYPE_DIVIDER = 1;
    private static final int VIEW_TYPE_COUNT = 2;

    public static final int MAX_RECENT_ENGINE_NUM = 3;
    public static final long MAX_DISPLAY_TIME_SPAN_MS = DateUtils.DAY_IN_MILLIS * 2;

    /**
     * Type for source of search engine. This is needed because if a custom search engine is set as
     * default, it will be moved to the prepopulated list.
     */
    @IntDef({TemplateUrlSourceType.DEFAULT, TemplateUrlSourceType.PREPOPULATED,
            TemplateUrlSourceType.RECENT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TemplateUrlSourceType {
        int DEFAULT = 0;
        int PREPOPULATED = 1;
        int RECENT = 2;
    }

    /** The current context. */
    private final Context mContext;
    private final Profile mProfile;

    /** The layout inflater to use for the custom views. */
    private LayoutInflater mLayoutInflater;

    /** The list of prepopluated and default search engines. */
    private List<TemplateUrl> mPrepopulatedSearchEngines = new ArrayList<>();

    /** The list of recently visited search engines. */
    private List<TemplateUrl> mRecentSearchEngines = new ArrayList<>();

    /**
     * The position (index into mPrepopulatedSearchEngines) of the currently selected search engine.
     * Can be -1 if current search engine is managed and set to something other than the
     * pre-populated values.
     */
    private int mSelectedSearchEnginePosition = -1;

    /** The position of the default search engine before user's action. */
    private int mInitialEnginePosition = -1;

    private boolean mHasLoadObserver;

    private boolean mIsLocationPermissionChanged;

    @Nullable
    private Runnable mDisableAutoSwitchRunnable;

    @Nullable
    private SettingsLauncher mSettingsLauncher;

    /**
     * Construct a SearchEngineAdapter.
     * @param context The current context.
     * @param profile The Profile associated with these settings.
     */
    public SearchEngineAdapter(Context context, Profile profile) {
        mContext = context;
        mProfile = profile;
        mLayoutInflater =
                (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
    }

    /**
     * Start the adapter to gather the available search engines and listen for updates.
     */
    public void start() {
        refreshData();
        TemplateUrlServiceFactory.getForProfile(mProfile).addObserver(this);
    }

    /**
     * Stop the adapter from listening for future search engine updates.
     */
    public void stop() {
        if (mHasLoadObserver) {
            TemplateUrlServiceFactory.getForProfile(mProfile).unregisterLoadListener(this);
            mHasLoadObserver = false;
        }

        TemplateUrlServiceFactory.getForProfile(mProfile).removeObserver(this);
    }

    String getValueForTesting() {
        return Integer.toString(mSelectedSearchEnginePosition);
    }

    String setValueForTesting(String value) {
        return searchEngineSelected(Integer.parseInt(value));
    }

    String getKeywordForTesting(int index) {
        return toKeyword(index);
    }

    /**
     * Initialize the search engine list.
     */
    private void refreshData() {
        TemplateUrlService templateUrlService = TemplateUrlServiceFactory.getForProfile(mProfile);
        if (!templateUrlService.isLoaded()) {
            mHasLoadObserver = true;
            templateUrlService.registerLoadListener(this);
            templateUrlService.load();
            return; // Flow continues in onTemplateUrlServiceLoaded below.
        }

        List<TemplateUrl> templateUrls = templateUrlService.getTemplateUrls();
        TemplateUrl defaultSearchEngineTemplateUrl =
                templateUrlService.getDefaultSearchEngineTemplateUrl();
        sortAndFilterUnnecessaryTemplateUrl(templateUrls, defaultSearchEngineTemplateUrl);
        boolean forceRefresh = mIsLocationPermissionChanged;
        mIsLocationPermissionChanged = false;
        if (!didSearchEnginesChange(templateUrls)) {
            if (forceRefresh) notifyDataSetChanged();
            return;
        }

        mPrepopulatedSearchEngines = new ArrayList<>();
        mRecentSearchEngines = new ArrayList<>();

        for (int i = 0; i < templateUrls.size(); i++) {
            TemplateUrl templateUrl = templateUrls.get(i);
            if (getSearchEngineSourceType(templateUrl, defaultSearchEngineTemplateUrl)
                    == TemplateUrlSourceType.RECENT) {
                mRecentSearchEngines.add(templateUrl);
            } else {
                mPrepopulatedSearchEngines.add(templateUrl);
            }
        }

        // Convert the TemplateUrl index into an index of mSearchEngines.
        mSelectedSearchEnginePosition = -1;
        for (int i = 0; i < mPrepopulatedSearchEngines.size(); ++i) {
            if (mPrepopulatedSearchEngines.get(i).equals(defaultSearchEngineTemplateUrl)) {
                mSelectedSearchEnginePosition = i;
            }
        }

        for (int i = 0; i < mRecentSearchEngines.size(); ++i) {
            if (mRecentSearchEngines.get(i).equals(defaultSearchEngineTemplateUrl)) {
                // Add one to offset the title for the recent search engine list.
                mSelectedSearchEnginePosition = i + computeStartIndexForRecentSearchEngines();
            }
        }

        if (mSelectedSearchEnginePosition == -1) {
            throw new IllegalStateException(String.format(
                    "Default search engine is not found in available search engines:"
                            + " DSE is valid=%b, is managed=%b",
                    defaultSearchEngineTemplateUrl != null,
                    TemplateUrlServiceFactory.getForProfile(mProfile).isDefaultSearchManaged()));
        }

        mInitialEnginePosition = mSelectedSearchEnginePosition;

        notifyDataSetChanged();
    }

    public static void sortAndFilterUnnecessaryTemplateUrl(
            List<TemplateUrl> templateUrls, TemplateUrl defaultSearchEngine) {
        Collections.sort(templateUrls, new Comparator<TemplateUrl>() {
            @Override
            public int compare(TemplateUrl templateUrl1, TemplateUrl templateUrl2) {
                if (templateUrl1.getIsPrepopulated() && templateUrl2.getIsPrepopulated()) {
                    return templateUrl1.getPrepopulatedId() - templateUrl2.getPrepopulatedId();
                } else if (templateUrl1.getIsPrepopulated()) {
                    return -1;
                } else if (templateUrl2.getIsPrepopulated()) {
                    return 1;
                } else if (templateUrl1.equals(templateUrl2)) {
                    return 0;
                } else if (templateUrl1.equals(defaultSearchEngine)) {
                    return -1;
                } else if (templateUrl2.equals(defaultSearchEngine)) {
                    return 1;
                } else {
                    return Long.compare(
                            templateUrl2.getLastVisitedTime(), templateUrl1.getLastVisitedTime());
                }
            }
        });
        int recentEngineNum = 0;
        long displayTime = System.currentTimeMillis() - MAX_DISPLAY_TIME_SPAN_MS;
        Iterator<TemplateUrl> iterator = templateUrls.iterator();
        while (iterator.hasNext()) {
            TemplateUrl templateUrl = iterator.next();
            if (getSearchEngineSourceType(templateUrl, defaultSearchEngine)
                    != TemplateUrlSourceType.RECENT) {
                continue;
            }
            if (recentEngineNum < MAX_RECENT_ENGINE_NUM
                    && templateUrl.getLastVisitedTime() > displayTime) {
                recentEngineNum++;
            } else {
                iterator.remove();
            }
        }
    }

    private static @TemplateUrlSourceType int getSearchEngineSourceType(
            TemplateUrl templateUrl, TemplateUrl defaultSearchEngine) {
        if (templateUrl.getIsPrepopulated()) {
            return TemplateUrlSourceType.PREPOPULATED;
        } else if (templateUrl.equals(defaultSearchEngine)) {
            return TemplateUrlSourceType.DEFAULT;
        } else {
            return TemplateUrlSourceType.RECENT;
        }
    }

    private static boolean containsTemplateUrl(
            List<TemplateUrl> templateUrls, TemplateUrl targetTemplateUrl) {
        for (int i = 0; i < templateUrls.size(); i++) {
            TemplateUrl templateUrl = templateUrls.get(i);
            // Explicitly excluding TemplateUrlSourceType and Index as they might change if a search
            // engine is set as default.
            if (templateUrl.getIsPrepopulated() == targetTemplateUrl.getIsPrepopulated()
                    && TextUtils.equals(templateUrl.getKeyword(), targetTemplateUrl.getKeyword())
                    && TextUtils.equals(
                            templateUrl.getShortName(), targetTemplateUrl.getShortName())) {
                return true;
            }
        }
        return false;
    }

    private boolean didSearchEnginesChange(List<TemplateUrl> templateUrls) {
        if (templateUrls.size()
                != mPrepopulatedSearchEngines.size() + mRecentSearchEngines.size()) {
            return true;
        }
        for (int i = 0; i < templateUrls.size(); i++) {
            TemplateUrl templateUrl = templateUrls.get(i);
            if (!containsTemplateUrl(mPrepopulatedSearchEngines, templateUrl)
                    && !SearchEngineAdapter.containsTemplateUrl(
                            mRecentSearchEngines, templateUrl)) {
                return true;
            }
        }
        return false;
    }

    private String toKeyword(int position) {
        if (position < mPrepopulatedSearchEngines.size()) {
            return mPrepopulatedSearchEngines.get(position).getKeyword();
        } else {
            position -= computeStartIndexForRecentSearchEngines();
            return mRecentSearchEngines.get(position).getKeyword();
        }
    }

    // BaseAdapter:

    @Override
    public int getCount() {
        int size = 0;
        if (mPrepopulatedSearchEngines != null) {
            size += mPrepopulatedSearchEngines.size();
        }
        if (mRecentSearchEngines != null && mRecentSearchEngines.size() != 0) {
            // Account for the header by adding one to the size.
            size += mRecentSearchEngines.size() + 1;
        }
        return size;
    }

    @Override
    public int getViewTypeCount() {
        return VIEW_TYPE_COUNT;
    }

    @Override
    public Object getItem(int pos) {
        if (pos < mPrepopulatedSearchEngines.size()) {
            return mPrepopulatedSearchEngines.get(pos);
        } else if (pos > mPrepopulatedSearchEngines.size()) {
            pos -= computeStartIndexForRecentSearchEngines();
            return mRecentSearchEngines.get(pos);
        }
        return null;
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public int getItemViewType(int position) {
        if (position == mPrepopulatedSearchEngines.size() && mRecentSearchEngines.size() != 0) {
            return VIEW_TYPE_DIVIDER;
        } else {
            return VIEW_TYPE_ITEM;
        }
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        View view = convertView;
        int itemViewType = getItemViewType(position);
        if (convertView == null) {
            view = mLayoutInflater.inflate(
                    itemViewType == VIEW_TYPE_DIVIDER && mRecentSearchEngines.size() != 0
                            ? R.layout.search_engine_recent_title
                            : R.layout.search_engine,
                    null);
        }
        if (itemViewType == VIEW_TYPE_DIVIDER) return view;

        view.setOnClickListener(this);
        view.setTag(position);

        RadioButton radioButton = view.findViewById(R.id.radiobutton);
        final boolean selected = position == mSelectedSearchEnginePosition;
        radioButton.setChecked(selected);

        TextView description = (TextView) view.findViewById(R.id.name);
        Resources resources = mContext.getResources();

        TemplateUrl templateUrl = (TemplateUrl) getItem(position);
        description.setText(templateUrl.getShortName());

        TextView url = (TextView) view.findViewById(R.id.url);
        url.setText(templateUrl.getKeyword());
        if (TextUtils.isEmpty(templateUrl.getKeyword())) {
            url.setVisibility(View.GONE);
        }

        // To improve the explore-by-touch experience, the radio button is hidden from accessibility
        // and instead, "checked" or "not checked" is read along with the search engine's name, e.g.
        // "google.com checked" or "google.com not checked".
        radioButton.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        description.setAccessibilityDelegate(new AccessibilityDelegate() {
            @Override
            public void onInitializeAccessibilityEvent(View host, AccessibilityEvent event) {
                super.onInitializeAccessibilityEvent(host, event);
                event.setChecked(selected);
            }

            @Override
            public void onInitializeAccessibilityNodeInfo(View host, AccessibilityNodeInfo info) {
                super.onInitializeAccessibilityNodeInfo(host, info);
                info.setCheckable(true);
                info.setChecked(selected);
            }
        });

        return view;
    }

    // TemplateUrlService.LoadListener

    @Override
    public void onTemplateUrlServiceLoaded() {
        TemplateUrlServiceFactory.getForProfile(mProfile).unregisterLoadListener(this);
        mHasLoadObserver = false;
        refreshData();
    }

    @Override
    public void onTemplateURLServiceChanged() {
        refreshData();
    }

    // OnClickListener:

    @Override
    public void onClick(View view) {
        searchEngineSelected((int) view.getTag());
    }

    private String searchEngineSelected(int position) {
        // Record the change in search engine.
        mSelectedSearchEnginePosition = position;

        String keyword = toKeyword(mSelectedSearchEnginePosition);
        TemplateUrlServiceFactory.getForProfile(mProfile).setSearchEngine(keyword);

        // If the user has manually set the default search engine, disable auto switching.
        boolean manualSwitch = mSelectedSearchEnginePosition != mInitialEnginePosition;
        if (manualSwitch) {
            RecordUserAction.record("SearchEngine_ManualChange");
            mDisableAutoSwitchRunnable.run();
        }
        notifyDataSetChanged();
        return keyword;
    }

    private String getSearchEngineUrl(TemplateUrl templateUrl) {
        if (templateUrl == null) {
            Log.e(TAG, "Invalid null template URL found");
            assert false;
            return "";
        }

        String url =
                TemplateUrlServiceFactory.getForProfile(mProfile).getSearchEngineUrlFromTemplateUrl(
                        templateUrl.getKeyword());
        if (url == null) {
            Log.e(TAG, "Invalid template URL found: %s", templateUrl);
            assert false;
            return "";
        }

        return url;
    }

    private boolean locationEnabled(TemplateUrl templateUrl) {
        String url = getSearchEngineUrl(templateUrl);
        if (url.isEmpty()) return false;

        PermissionInfo locationSettings =
                new PermissionInfo(ContentSettingsType.GEOLOCATION, url, null, false);
        return locationSettings.getContentSetting(mProfile) == ContentSettingValues.ALLOW;
    }

    private int computeStartIndexForRecentSearchEngines() {
        // If there are custom search engines to show, add 1 for showing the
        // "Recently visited" header.
        if (mRecentSearchEngines.size() > 0) {
            return mPrepopulatedSearchEngines.size() + 1;
        }
        return mPrepopulatedSearchEngines.size();
    }

    void setDisableAutoSwitchRunnable(@NonNull Runnable runnable) {
        mDisableAutoSwitchRunnable = runnable;
    }

    void setSettingsLauncher(@NonNull SettingsLauncher settingsLauncher) {
        mSettingsLauncher = settingsLauncher;
    }
}
