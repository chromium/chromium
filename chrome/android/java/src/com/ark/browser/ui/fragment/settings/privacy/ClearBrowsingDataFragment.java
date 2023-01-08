package com.ark.browser.ui.fragment.settings.privacy;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.collection.ArraySet;

import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.ark.browser.ui.widget.CheckBoxItem;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.fragmentation.dialog.impl.LoadingDialogFragment;
import com.zpj.toast.ZToast;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataCounterBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataTab;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.user_prefs.UserPrefs;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;

public class ClearBrowsingDataFragment extends BaseSwipeBackFragment
        implements BrowsingDataBridge.OnClearBrowsingDataListener {

    private static final String TAG = "ClearBrowsingDataFragment";
    public static final String CLEAR_BROWSING_DATA_FETCHER = "clearBrowsingDataFetcher";
    private static final int IMPORTANT_SITES_DIALOG_CODE = 1;
    private static final int IMPORTANT_SITES_PERCENTAGE_BUCKET_COUNT = 20;
    public static final int CBD_TAB_COUNT = 2;

    private Spinner mSpinner;
    private int mSelectedIndex;
    private ArrayAdapter<Object> mAdapter;

    private Item[] mItems;

    private LoadingDialogFragment mProgressDialog;

    private CheckBoxItem clearHistory;

    private CheckBoxItem clearCookies;

    private CheckBoxItem clearCache;

    private CheckBoxItem clearPasswords;

    private CheckBoxItem clearFormData;

    private CheckBoxItem clearSiteSettings;

//    private CheckBoxItem clearLicenses;

    private ClearBrowsingDataFetcher mFetcher;


    /**
     * The various data types that can be cleared via this screen.
     */
    @IntDef({DialogOption.CLEAR_HISTORY, DialogOption.CLEAR_COOKIES_AND_SITE_DATA,
            DialogOption.CLEAR_CACHE, DialogOption.CLEAR_PASSWORDS, DialogOption.CLEAR_FORM_DATA,
            DialogOption.CLEAR_SITE_SETTINGS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DialogOption {
        // Values used in "for" loop below - should start from 0 and can't have gaps, lowest value
        // is additionally used for starting loop.
        // All updates here must also be reflected in {@link #getDataType(int) getDataType}, {@link
        // #getPreferenceKey(int) getPreferenceKey} and {@link #getIcon(int) getIcon}.
        int CLEAR_HISTORY = 0;
        int CLEAR_COOKIES_AND_SITE_DATA = 1;
        int CLEAR_CACHE = 2;
        int CLEAR_PASSWORDS = 3;
        int CLEAR_FORM_DATA = 4;
        int CLEAR_SITE_SETTINGS = 5;
        int NUM_ENTRIES = 6;
    }

    /**
     * @return All available {@link DialogOption} entries.
     */
    protected static final Set<Integer> getAllOptions() {
        Set<Integer> all = new ArraySet<>();
        for (@DialogOption int i = DialogOption.CLEAR_HISTORY; i < DialogOption.NUM_ENTRIES; i++) {
            all.add(i);
        }
        return all;
    }

    /**
     * Returns the list of supported {@link DialogOption}.
     */
    protected List<Integer> getDialogOptions() {
        return Arrays.asList(DialogOption.CLEAR_HISTORY, DialogOption.CLEAR_COOKIES_AND_SITE_DATA,
                DialogOption.CLEAR_CACHE, DialogOption.CLEAR_PASSWORDS,
                DialogOption.CLEAR_FORM_DATA, DialogOption.CLEAR_SITE_SETTINGS);
    }

    static @BrowsingDataType int getDataType(@DialogOption int type) {
        switch (type) {
            case DialogOption.CLEAR_CACHE:
                return BrowsingDataType.CACHE;
            case DialogOption.CLEAR_COOKIES_AND_SITE_DATA:
                return BrowsingDataType.COOKIES;
            case DialogOption.CLEAR_FORM_DATA:
                return BrowsingDataType.FORM_DATA;
            case DialogOption.CLEAR_HISTORY:
                return BrowsingDataType.HISTORY;
            case DialogOption.CLEAR_PASSWORDS:
                return BrowsingDataType.PASSWORDS;
            case DialogOption.CLEAR_SITE_SETTINGS:
                return BrowsingDataType.SITE_SETTINGS;
            default:
                throw new IllegalArgumentException();
        }
    }

    static String getPreferenceKey(@DialogOption int type) {
        switch (type) {
            case DialogOption.CLEAR_CACHE:
                return "clear_cache_checkbox";
            case DialogOption.CLEAR_COOKIES_AND_SITE_DATA:
                return "clear_cookies_checkbox";
            case DialogOption.CLEAR_FORM_DATA:
                return "clear_form_data_checkbox";
            case DialogOption.CLEAR_HISTORY:
                return "clear_history_checkbox";
            case DialogOption.CLEAR_PASSWORDS:
                return "clear_passwords_checkbox";
            case DialogOption.CLEAR_SITE_SETTINGS:
                return "clear_site_settings_checkbox";
            default:
                throw new IllegalArgumentException();
        }
    }

    static @DrawableRes int getIcon(@DialogOption int type) {
        switch (type) {
            case DialogOption.CLEAR_CACHE:
                return R.drawable.ic_collections_grey;
            case DialogOption.CLEAR_COOKIES_AND_SITE_DATA:
                return R.drawable.permission_cookie;
            case DialogOption.CLEAR_FORM_DATA:
                return R.drawable.ic_edit_24dp;
            case DialogOption.CLEAR_HISTORY:
                return R.drawable.ic_watch_later_24dp;
            case DialogOption.CLEAR_PASSWORDS:
                return R.drawable.ic_vpn_key_grey;
            case DialogOption.CLEAR_SITE_SETTINGS:
                return R.drawable.ic_tv_options_input_settings_rotated_grey;
            default:
                throw new IllegalArgumentException();
        }
    }

    private static class Item implements BrowsingDataCounterBridge.BrowsingDataCounterCallback {
        private static final int MIN_DP_FOR_ICON = 360;
        private final ClearBrowsingDataFragment mParent;
        private final @DialogOption int mOption;
        private final CheckBoxItem mCheckbox;
        private BrowsingDataCounterBridge mCounter;

        public Item(ClearBrowsingDataFragment parent,
                    @DialogOption int option,
                    CheckBoxItem checkbox,
                    boolean selected,
                    boolean enabled) {
            super();
            mParent = parent;
            mOption = option;
            mCheckbox = checkbox;
            mCounter = new BrowsingDataCounterBridge(
                    this, getDataType(option), ClearBrowsingDataTab.ADVANCED);

            mCheckbox.setEnabled(enabled);
            Log.d(TAG, "selected=" + selected);
            mCheckbox.setOnCheckedChangeListener((checkLayout, isChecked) -> {
                TextView clearButton = mParent.findViewById(R.id.btn_clear);
                clearButton.setEnabled(!mParent.getSelectedOptions().isEmpty());
                BrowsingDataBridge.getInstance().setBrowsingDataDeletionPreference(
                        getDataType(mOption), ClearBrowsingDataTab.ADVANCED, isChecked);

            });
            int dp = mParent.getResources().getConfiguration().smallestScreenWidthDp;
            if (dp >= MIN_DP_FOR_ICON) {
                mCheckbox.setIcon(SettingsUtils.getTintedIcon(
                        parent.context, ClearBrowsingDataFragment.getIcon(option)));
            }
        }

        public void destroy() {
            mCounter.destroy();
        }

        @DialogOption
        public int getOption() {
            return mOption;
        }

        public boolean isSelected() {
            return mCheckbox.isChecked();
        }

        public void update() {
            mCheckbox.setChecked(false);
            destroy();
            mCounter = new BrowsingDataCounterBridge(
                    this, getDataType(mOption), ClearBrowsingDataTab.ADVANCED);
        }

        @Override
        public void onCounterFinished(String result) {
            mCheckbox.setContent(result);
        }

        @Override
        public String toString() {
            return "Item{" +
                    ", mOption=" + mOption +
                    ", mCounter=" + mCounter +
                    '}';
        }
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (savedInstanceState == null) {
            mFetcher = new ClearBrowsingDataFetcher();
            mFetcher.fetchImportantSites();
            mFetcher.requestInfoAboutOtherFormsOfBrowsingHistory();
        } else {
            mFetcher = savedInstanceState.getParcelable(
                    ClearBrowsingDataFragment.CLEAR_BROWSING_DATA_FETCHER);
        }
    }

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_setting_clear_browsing_data;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        setToolbarTitle("清除数据");
        mSpinner = view.findViewById(R.id.spinner);
        mSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(
                    AdapterView<?> parent, View view, int position, long id) {
                mSelectedIndex = position;
                BrowsingDataBridge.getInstance().setBrowsingDataDeletionTimePeriod(
                        ClearBrowsingDataTab.ADVANCED,
                        ((TimePeriodSpinnerOption) mSpinner.getSelectedItem()).getTimePeriod());
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
                // No callback. Only update listeners when an actual option is selected.
            }
        });

        TimePeriodSpinnerOption[] spinnerOptions = getTimePeriodSpinnerOptions();
        @TimePeriod
        int selectedTimePeriod = BrowsingDataBridge.getInstance().getBrowsingDataDeletionTimePeriod(
                ClearBrowsingDataTab.ADVANCED);
        for (int i = 0; i < spinnerOptions.length; ++i) {
            if (spinnerOptions[i].getTimePeriod() == selectedTimePeriod) {
                mSelectedIndex = i;
                break;
            }
        }

        int itemLayout;
        itemLayout = android.R.layout.simple_spinner_item;
        mAdapter = new ArrayAdapter<>(getContext(), itemLayout, spinnerOptions);
        mAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mSpinner.setAdapter(mAdapter);
        mSpinner.setSelection(mSelectedIndex);

        TextView clearBtn = view.findViewById(R.id.btn_clear);
        clearBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Log.d(TAG, "getSelectedOptions=" + getSelectedOptions());
                if (getSelectedOptions().isEmpty()) {
                    ZToast.normal("请选择清理项！");
                    return;
                }
                if (shouldShowImportantSitesDialog()) {
                    showImportantDialogThenClear();
                    return;
                }
                // If sites haven't been fetched, just clear the browsing data regularly rather than
                // waiting to show the important sites dialog.
                clearBrowsingData(getSelectedOptions(), null,
                        null, null, null);
            }
        });

        clearHistory = view.findViewById(R.id.layout_clear_history_checkbox);

        clearCookies = view.findViewById(R.id.layout_clear_cookies_checkbox);

        clearCache = view.findViewById(R.id.layout_clear_cache_checkbox);

        clearPasswords = view.findViewById(R.id.layout_clear_passwords_checkbox);

        clearFormData = view.findViewById(R.id.layout_clear_form_data_checkbox);

        clearSiteSettings = view.findViewById(R.id.layout_clear_site_settings_checkbox);

//        clearLicenses = view.findViewById(R.id.layout_media_licenses_checkbox);

        CheckBoxItem[] checkLayouts = new CheckBoxItem[] {
                clearHistory, clearCookies, clearCache,
                clearPasswords, clearFormData, clearSiteSettings
        };
        List<Integer> options = Arrays.asList(
                DialogOption.CLEAR_HISTORY,
                DialogOption.CLEAR_COOKIES_AND_SITE_DATA,
                DialogOption.CLEAR_CACHE, DialogOption.CLEAR_PASSWORDS,
                DialogOption.CLEAR_FORM_DATA, DialogOption.CLEAR_SITE_SETTINGS
        );
        mItems = new Item[options.size()];
        for (int i = 0; i < options.size(); i++) {
            @DialogOption
            int option = options.get(i);
            boolean enabled = true;

            // It is possible to disable the deletion of browsing history.
            if (option == DialogOption.CLEAR_HISTORY
                    && !UserPrefs.get(Profile.getLastUsedRegularProfile())
                    .getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY)) {
                enabled = false;
                BrowsingDataBridge.getInstance().setBrowsingDataDeletionPreference(
                        getDataType(DialogOption.CLEAR_HISTORY), ClearBrowsingDataTab.BASIC, false);
                BrowsingDataBridge.getInstance().setBrowsingDataDeletionPreference(
                        getDataType(DialogOption.CLEAR_HISTORY), ClearBrowsingDataTab.ADVANCED,
                        false);
            }

            mItems[i] = new Item(
                    this,
                    option,
                    checkLayouts[i],
                    isOptionSelectedByDefault(option), enabled);
        }
    }

    private boolean isOptionSelectedByDefault(@DialogOption int option) {
        return BrowsingDataBridge.getInstance().getBrowsingDataDeletionPreference(
                getDataType(option), ClearBrowsingDataTab.ADVANCED);
    }

    @Override
    public void onBrowsingDataCleared() {
        if (getActivity() == null) {
            return;
        }
        if (MultiWindowUtils.isActivityVisible(getActivity())
                && getSelectedOptions().contains(DialogOption.CLEAR_HISTORY)
                && mFetcher.isDialogAboutOtherFormsOfBrowsingHistoryEnabled()
                && !OtherFormsOfHistoryDialogFragment.wasDialogShown()) {
            new OtherFormsOfHistoryDialogFragment().show(context);
        } else {
            ZToast.success("清除完成！");
            for (Item item : getSelectedItems()) {
                item.update();
            }
        }
        dismissProgressDialog();
    }

    private boolean shouldShowImportantSitesDialog() {
        Set<Integer> selectedOptions = getSelectedOptions();
        if (!selectedOptions.contains(DialogOption.CLEAR_CACHE)
                && !selectedOptions.contains(DialogOption.CLEAR_COOKIES_AND_SITE_DATA)) {
            return false;
        }
        boolean haveImportantSites = mFetcher.getSortedImportantDomains() != null
                && mFetcher.getSortedImportantDomains().length != 0;
        RecordHistogram.recordBooleanHistogram(
                "History.ClearBrowsingData.ImportantDialogShown", haveImportantSites);
        return haveImportantSites;
    }

    private int[] toIntArray(List<Integer> boxedList) {
        int[] result = new int[boxedList.size()];
        for (int i = 0; i < result.length; i++) {
            result[i] = boxedList.get(i);
        }
        return result;
    }

    private void showImportantDialogThenClear() {
        ConfirmImportantSitesDialogFragment fragment = ConfirmImportantSitesDialogFragment.newInstance(
                mFetcher.getSortedImportantDomains(), mFetcher.getSortedImportantDomainReasons(),
                mFetcher.getSortedExampleOrigins());
        fragment.setCallback(this::onConfirmResult);
        fragment.show(context);
    }

    private void onConfirmResult(Intent data) {
        // Deselected means that the user is excluding the domain from being cleared.
        String[] deselectedDomains = data.getStringArrayExtra(
                ConfirmImportantSitesDialogFragment.DESELECTED_DOMAINS_TAG);
        int[] deselectedDomainReasons = data.getIntArrayExtra(
                ConfirmImportantSitesDialogFragment.DESELECTED_DOMAIN_REASONS_TAG);
        String[] ignoredDomains = data.getStringArrayExtra(
                ConfirmImportantSitesDialogFragment.IGNORED_DOMAINS_TAG);
        int[] ignoredDomainReasons = data.getIntArrayExtra(
                ConfirmImportantSitesDialogFragment.IGNORED_DOMAIN_REASONS_TAG);
        if (deselectedDomains != null && mFetcher.getSortedImportantDomains() != null) {
            // mMaxImportantSites is a constant on the C++ side.
            RecordHistogram.recordCustomCountHistogram(
                    "History.ClearBrowsingData.ImportantDeselectedNum",
                    deselectedDomains.length, 1, mFetcher.getMaxImportantSites() + 1,
                    mFetcher.getMaxImportantSites() + 1);
            RecordHistogram.recordCustomCountHistogram(
                    "History.ClearBrowsingData.ImportantIgnoredNum", ignoredDomains.length, 1,
                    mFetcher.getMaxImportantSites() + 1, mFetcher.getMaxImportantSites() + 1);
            // We put our max at 20 instead of 100 to reduce the number of empty buckets (as
            // our maximum denominator is 5).
            RecordHistogram.recordEnumeratedHistogram(
                    "History.ClearBrowsingData.ImportantDeselectedPercent",
                    deselectedDomains.length * IMPORTANT_SITES_PERCENTAGE_BUCKET_COUNT
                            / mFetcher.getSortedImportantDomains().length,
                    IMPORTANT_SITES_PERCENTAGE_BUCKET_COUNT + 1);
            RecordHistogram.recordEnumeratedHistogram(
                    "History.ClearBrowsingData.ImportantIgnoredPercent",
                    ignoredDomains.length * IMPORTANT_SITES_PERCENTAGE_BUCKET_COUNT
                            / mFetcher.getSortedImportantDomains().length,
                    IMPORTANT_SITES_PERCENTAGE_BUCKET_COUNT + 1);
        }
        clearBrowsingData(getSelectedOptions(), deselectedDomains, deselectedDomainReasons,
                ignoredDomains, ignoredDomainReasons);
    }

    protected final Set<Integer> getSelectedOptions() {
        Set<Integer> selected = new ArraySet<>();
        for (Item item : mItems) {
            if (item.isSelected()) selected.add(item.getOption());
        }
        return selected;
    }

    protected final Set<Item> getSelectedItems() {
        Set<Item> selected = new ArraySet<>();
        for (Item item : mItems) {
            if (item.isSelected()) {
                selected.add(item);
            }
        }
        return selected;
    }

    private void clearBrowsingData(Set<Integer> options,
                                   @Nullable String[] blacklistedDomains, @Nullable int[] blacklistedDomainReasons,
                                   @Nullable String[] ignoredDomains, @Nullable int[] ignoredDomainReasons) {
        showProgressDialog();

        int[] dataTypes = new int[options.size()];
        int i = 0;
        for (@DialogOption int option : options) {
            dataTypes[i] = getDataType(option);
            ++i;
        }

        int timePeriod = ((TimePeriodSpinnerOption) mSpinner.getSelectedItem()).getTimePeriod();
        if (blacklistedDomains != null && blacklistedDomains.length != 0) {
            BrowsingDataBridge.getInstance().clearBrowsingDataExcludingDomains(this, dataTypes,
                    timePeriod, blacklistedDomains, blacklistedDomainReasons, ignoredDomains,
                    ignoredDomainReasons);
        } else {
            BrowsingDataBridge.getInstance().clearBrowsingData(this, dataTypes, timePeriod);
        }

    }

    private void showProgressDialog() {
        if (getActivity() == null) return;
        mProgressDialog = ZDialog.loading()
                .setTitle(getActivity().getString(R.string.clear_browsing_data_progress_title))
                .show(getActivity());
    }

    private void dismissProgressDialog() {
        if (mProgressDialog != null && mProgressDialog.isVisible()) {
            mProgressDialog.dismiss();
        }
        mProgressDialog = null;
    }

    protected static class TimePeriodSpinnerOption {
        private int mTimePeriod;
        private String mTitle;

        /**
         * Constructs this time period spinner option.
         * @param timePeriod The time period represented as an int from the shared enum
         *     {@link TimePeriod}.
         * @param title The text that will be used to represent this item in the spinner.
         */
        public TimePeriodSpinnerOption(int timePeriod, String title) {
            mTimePeriod = timePeriod;
            mTitle = title;
        }

        /**
         * @return The time period represented as an int from the shared enum {@link TimePeriod}
         */
        public int getTimePeriod() {
            return mTimePeriod;
        }

        @Override
        public String toString() {
            return mTitle;
        }
    }

    private TimePeriodSpinnerOption[] getTimePeriodSpinnerOptions() {
        Activity activity = getActivity();
        List<TimePeriodSpinnerOption> options = new ArrayList<>();
        options.add(new TimePeriodSpinnerOption(TimePeriod.LAST_HOUR,
                activity.getString(R.string.clear_browsing_data_tab_period_hour)));
        options.add(new TimePeriodSpinnerOption(TimePeriod.LAST_DAY,
                activity.getString(R.string.clear_browsing_data_tab_period_24_hours)));
        options.add(new TimePeriodSpinnerOption(TimePeriod.LAST_WEEK,
                activity.getString(R.string.clear_browsing_data_tab_period_7_days)));
        options.add(new TimePeriodSpinnerOption(TimePeriod.FOUR_WEEKS,
                activity.getString(R.string.clear_browsing_data_tab_period_four_weeks)));
        options.add(new TimePeriodSpinnerOption(TimePeriod.ALL_TIME,
                activity.getString(R.string.clear_browsing_data_tab_period_everything)));
        return options.toArray(new TimePeriodSpinnerOption[0]);
    }
}

