//package com.ark.browser.ui.fragment.settings.privacy;
//
//import android.app.Activity;
//import android.os.Bundle;
//import android.os.SystemClock;
//import android.view.View;
//import android.widget.AdapterView;
//import android.widget.ArrayAdapter;
//import android.widget.Spinner;
//import android.widget.TextView;
//
//import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
//import com.zpj.fragmentation.dialog.ZDialog;
//import com.zpj.fragmentation.dialog.impl.LoadingDialogFragment;
//import com.zpj.toast.ZToast;
//
//import org.chromium.base.ActivityState;
//import org.chromium.base.ApplicationStatus;
//import org.chromium.base.Log;
//import org.chromium.base.metrics.RecordHistogram;
//import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
//
//import java.util.ArrayList;
//import java.util.Arrays;
//import java.util.EnumSet;
//import java.util.List;
//import java.util.concurrent.TimeUnit;
//
//public class ClearBrowsingDataFragment extends BaseSwipeBackFragment
//        implements BrowsingDataBridge.ImportantSitesCallback,
//        BrowsingDataBridge.OnClearBrowsingDataListener,
//        BrowsingDataBridge.OtherFormsOfBrowsingHistoryListener {
//
//    private static final String TAG = "ClearBrowsingDataFragment";
//    private static final int IMPORTANT_SITES_DIALOG_CODE = 1;
//    private static final int IMPORTANT_SITES_PERCENTAGE_BUCKET_COUNT = 20;
//    public static final int CBD_TAB_COUNT = 2;
//
//    private List<String> deselectedDomainList = new ArrayList<>();
//    private List<Integer> deselectedDomainReasonList = new ArrayList<>();
//    private List<String> ignoredDomainList = new ArrayList<>();
//    private List<Integer> ignoredDomainReasonList = new ArrayList<>();
//
//    private Spinner mSpinner;
//    private int mSelectedIndex;
//    private ArrayAdapter<Object> mAdapter;
//
//    private Item[] mItems;
//
//    private OtherFormsOfHistoryDialogFragment mDialogAboutOtherFormsOfBrowsingHistory;
//    private boolean mIsDialogAboutOtherFormsOfBrowsingHistoryEnabled;
//    private LoadingDialogFragment mProgressDialog;
//
//    // This is a constant on the C++ side.
//    private int mMaxImportantSites;
//    // This is the sorted list of important registerable domains. If null, then we haven't finished
//    // fetching them yet.
//    private String[] mSortedImportantDomains;
//    // These are the reasons the above domains were chosen as important.
//    private int[] mSortedImportantDomainReasons;
//    // These are full url examples of the domains above. We use them for favicons.
//    private String[] mSortedExampleOrigins;
//    // This is the dialog we show to the user that lets them 'uncheck' (or exclude) the above
//    // important domains from being cleared.
//    private ConfirmImportantSitesDialogFragment mConfirmImportantSitesDialog;
//    // Time in ms, when the dialog was created.
//    private long mDialogOpened;
//
//    private CheckLayout clearHistory;
//
//    private CheckLayout clearCookies;
//
//    private CheckLayout clearCache;
//
//    private CheckLayout clearPasswords;
//
//    private CheckLayout clearFormData;
//
//    private CheckLayout clearSiteSettings;
//
//    private CheckLayout clearLicenses;
//
//
//
//    public enum DialogOption {
//        CLEAR_HISTORY(BrowsingDataType.HISTORY, R.id.item_1, R.drawable.ic_watch_later_24dp, true),
//        CLEAR_COOKIES_AND_SITE_DATA(
//                BrowsingDataType.COOKIES, R.id.item_1, R.drawable.permission_cookie, true),
//        CLEAR_CACHE(BrowsingDataType.CACHE, R.id.item_1, R.drawable.ic_collections_grey, false),
//        CLEAR_PASSWORDS(
//                BrowsingDataType.PASSWORDS, R.id.item_1, R.drawable.ic_vpn_key_grey, false),
//        CLEAR_FORM_DATA(BrowsingDataType.FORM_DATA, R.id.item_1, R.drawable.ic_edit_24dp, true),
//        CLEAR_SITE_SETTINGS(BrowsingDataType.SITE_SETTINGS, R.id.item_1,
//                R.drawable.ic_tv_options_input_settings_rotated_grey, false),
//        CLEAR_MEDIA_LICENSES(BrowsingDataType.MEDIA_LICENSES, R.id.item_1,
//                R.drawable.permission_protected_media, true);
//
//        private final int mDataType;
//        @IdRes
//        private final int mResId;
//        private final int mIcon;
//        private final boolean mIsBitmap;
//
//        DialogOption(int dataType, @IdRes int resId, int icon, boolean isBitmap) {
//            mDataType = dataType;
//            mResId = resId;
//            mIcon = icon;
//            mIsBitmap = isBitmap;
//        }
//
//        /**
//         * @return The {@link BrowsingDataType} this option represents.
//         */
//        public int getDataType() {
//            return mDataType;
//        }
//
//        /**
//         * @return The key of the corresponding preference.
//         */
//        @IdRes
//        public int getResId() {
//            return mResId;
//        }
//
//        /**
//         * @return The resource id for the icon that is used to display this option.
//         */
//        public int getIcon() {
//            return mIcon;
//        }
//
//        /**
//         * @return Whether the icon is a bitmap. Otherwise it's a vector.
//         */
//        public boolean iconIsBitmap() {
//            return mIsBitmap;
//        }
//
//        @Override
//        public String toString() {
//            return "DialogOption{" +
//                    "mDataType=" + mDataType +
//                    ", mResId=" + mResId +
//                    ", mIcon=" + mIcon +
//                    ", mIsBitmap=" + mIsBitmap +
//                    '}';
//        }
//    }
//
//    private static class Item implements BrowsingDataCounterBridge.BrowsingDataCounterCallback {
//        private static final int MIN_DP_FOR_ICON = 360;
//        private final ClearBrowsingDataFragment mParent;
//        private final DialogOption mOption;
//        private final CheckLayout mCheckbox;
//        private BrowsingDataCounterBridge mCounter;
//
//        public Item(ClearBrowsingDataFragment parent,
//                    DialogOption option,
//                    CheckLayout checkbox,
//                    boolean selected,
//                    boolean enabled) {
//            super();
//            mParent = parent;
//            mOption = option;
//            mCheckbox = checkbox;
//            mCounter = new BrowsingDataCounterBridge(
//                    this, mOption.getDataType(), ClearBrowsingDataTab.ADVANCED);
//
////            mCheckbox.setEnabled(enabled);
//            Log.d(TAG, "selected=" + selected);
////            if (selected) {
////                checkbox.performClick();
////            }
//            mCheckbox.setOnCheckedChangeListener(new CheckLayout.OnCheckedChangeListener() {
//                @Override
//                public void onCheckedChanged(CheckLayout checkLayout, boolean isChecked) {
//                    TextView clearButton = mParent.getView().findViewById(R.id.btn_clear);
//                    clearButton.setEnabled(!mParent.getSelectedOptions().isEmpty());
//                    PrefServiceBridge.getInstance().setBrowsingDataDeletionPreference(
//                            mOption.getDataType(), ClearBrowsingDataTab.ADVANCED, isChecked);
//                }
//            });
//            mCheckbox.setIcon(option.getIcon());
//
//
////            int dp = mParent.getResources().getConfiguration().smallestScreenWidthDp;
////            if (dp >= MIN_DP_FOR_ICON) {
////                if (option.iconIsBitmap()) {
////                    Drawable icon = TintedDrawable.constructTintedDrawable(
////                            mParent.getResources(), option.getIcon(), R.color.google_grey_600);
//////                    mCheckbox.setIcon(icon);
////                } else {
////                    Drawable icon = VectorDrawableCompat.create(mParent.getResources(),
////                            option.getIcon(), mParent.getActivity().getTheme());
//////                    mCheckbox.setIcon(icon);
////                }
////            }
//        }
//
//        public void destroy() {
//            mCounter.destroy();
//        }
//
//        public DialogOption getOption() {
//            return mOption;
//        }
//
//        public boolean isSelected() {
//            return mCheckbox.isChecked();
//        }
//
//        @Override
//        public void onCounterFinished(String result) {
//            mCheckbox.setContent(result);
//        }
//
//        @Override
//        public String toString() {
//            return "Item{" +
//                    ", mOption=" + mOption +
//                    ", mCounter=" + mCounter +
//                    '}';
//        }
//    }
//
//    @Override
//    public void onCreate(@Nullable Bundle savedInstanceState) {
//        super.onCreate(savedInstanceState);
//        mDialogOpened = SystemClock.elapsedRealtime();
//        mMaxImportantSites = BrowsingDataBridge.getMaxImportantSites();
//        BrowsingDataBridge.getInstance().requestInfoAboutOtherFormsOfBrowsingHistory(this);
//        if (ChromeFeatureList.isEnabled(ChromeFeatureList.IMPORTANT_SITES_IN_CBD)) {
//            BrowsingDataBridge.fetchImportantSites(this);
//        }
//    }
//
//    @Override
//    protected int getLayoutId() {
//        return R.layout.fragment_setting_clear_browsing_data;
//    }
//
//    @Override
//    protected void initView(View view, @Nullable Bundle savedInstanceState) {
//        super.initView(view, savedInstanceState);
//        setToolbarTitle("清除数据");
//        mSpinner = view.findViewById(R.id.spinner);
//        mSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
//            @Override
//            public void onItemSelected(
//                    AdapterView<?> parent, View view, int position, long id) {
//                mSelectedIndex = position;
//                PrefServiceBridge.getInstance().setBrowsingDataDeletionTimePeriod(
//                        ClearBrowsingDataTab.ADVANCED, ((TimePeriodSpinnerOption) mSpinner.getSelectedItem()).getTimePeriod());
//            }
//
//            @Override
//            public void onNothingSelected(AdapterView<?> parent) {
//                // No callback. Only update listeners when an actual option is selected.
//            }
//        });
//
//        TimePeriodSpinnerOption[] spinnerOptions = getTimePeriodSpinnerOptions();
//        int selectedTimePeriod = PrefServiceBridge.getInstance().getBrowsingDataDeletionTimePeriod(
//                ClearBrowsingDataTab.ADVANCED);
//        for (int i = 0; i < spinnerOptions.length; ++i) {
//            if (spinnerOptions[i].getTimePeriod() == selectedTimePeriod) {
//                mSelectedIndex = i;
//                break;
//            }
//        }
//
//        int itemLayout;
////        if (mSingleLine) {
////            itemLayout = R.layout.preference_spinner_single_line_item;
////        } else {
////            itemLayout = android.R.layout.simple_spinner_item;
////        }
//        itemLayout = android.R.layout.simple_spinner_item;
//        mAdapter = new ArrayAdapter<>(getContext(), itemLayout, spinnerOptions);
//        mAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
//        mSpinner.setAdapter(mAdapter);
//        mSpinner.setSelection(mSelectedIndex);
//
//        TextView clearBtn = view.findViewById(R.id.btn_clear);
//        clearBtn.setOnClickListener(new View.OnClickListener() {
//            @Override
//            public void onClick(View v) {
//                Log.d(TAG, "getSelectedOptions=" + getSelectedOptions());
//                if (getSelectedOptions().isEmpty()) {
//                    ZToast.normal("请选择清理项！");
//                    return;
//                }
//                if (shouldShowImportantSitesDialog()) {
//                    showImportantDialogThenClear();
//                    return;
//                }
//                // If sites haven't been fetched, just clear the browsing data regularly rather than
//                // waiting to show the important sites dialog.
//                clearBrowsingData(getSelectedOptions(), null, null, null, null);
//            }
//        });
//
//        clearHistory = view.findViewById(R.id.layout_clear_history_checkbox);
//
//        clearCookies = view.findViewById(R.id.layout_clear_cookies_checkbox);
//
//        clearCache = view.findViewById(R.id.layout_clear_cache_checkbox);
//
//        clearPasswords = view.findViewById(R.id.layout_clear_passwords_checkbox);
//
//        clearFormData = view.findViewById(R.id.layout_clear_form_data_checkbox);
//
//        clearSiteSettings = view.findViewById(R.id.layout_clear_site_settings_checkbox);
//
//        clearLicenses = view.findViewById(R.id.layout_media_licenses_checkbox);
//
//        CheckLayout[] checkLayouts = new CheckLayout[] {
//                clearHistory, clearCookies, clearCache,
//                clearPasswords, clearFormData, clearSiteSettings, clearLicenses
//        };
//        DialogOption[] options = new DialogOption[] {DialogOption.CLEAR_HISTORY,
//                DialogOption.CLEAR_COOKIES_AND_SITE_DATA, DialogOption.CLEAR_CACHE,
//                DialogOption.CLEAR_PASSWORDS, DialogOption.CLEAR_FORM_DATA,
//                DialogOption.CLEAR_SITE_SETTINGS, DialogOption.CLEAR_MEDIA_LICENSES};
//        mItems = new Item[checkLayouts.length];
//        for (int i = 0; i < checkLayouts.length; i++) {
//            Log.d(TAG, "item" + i + "=" + mItems[i]);
//            boolean enabled = true;
//
//            // It is possible to disable the deletion of browsing history.
//            if (checkLayouts[i] == clearHistory
//                    && !PrefServiceBridge.getInstance().getBoolean(
//                    Pref.ALLOW_DELETING_BROWSER_HISTORY)) {
//                enabled = false;
//                PrefServiceBridge.getInstance().setBrowsingDataDeletionPreference(
//                        DialogOption.CLEAR_HISTORY.getDataType(), ClearBrowsingDataTab.BASIC,
//                        false);
//                PrefServiceBridge.getInstance().setBrowsingDataDeletionPreference(
//                        DialogOption.CLEAR_HISTORY.getDataType(), ClearBrowsingDataTab.ADVANCED,
//                        false);
//            }
//
//            mItems[i] = new Item(
//                    this,
//                    options[i],
//                    checkLayouts[i],
//                    PrefServiceBridge.getInstance().getBrowsingDataDeletionPreference(
//                            options[i].getDataType(), ClearBrowsingDataTab.ADVANCED),
//                    enabled);
//        }
//    }
//
//    @Override
//    public void onBrowsingDataCleared() {
//        if (getActivity() == null) {
//            return;
//        }
//        if (isActivityVisible(getActivity())
//                && getSelectedOptions().contains(DialogOption.CLEAR_HISTORY)
//                && mIsDialogAboutOtherFormsOfBrowsingHistoryEnabled
//                && !OtherFormsOfHistoryDialogFragment.wasDialogShown(getActivity())) {
//            mDialogAboutOtherFormsOfBrowsingHistory = new OtherFormsOfHistoryDialogFragment();
//            mDialogAboutOtherFormsOfBrowsingHistory.show(getActivity());
//        } else {
//            ZToast.normal("清除完成！");
//        }
//        dismissProgressDialog();
//    }
//
//    @Override
//    public void onImportantRegisterableDomainsReady(String[] domains, String[] exampleOrigins, int[] importantReasons, boolean dialogDisabled) {
//        if (domains == null || dialogDisabled) return;
//        // mMaxImportantSites is a constant on the C++ side. While 0 is valid, use 1 as the minimum
//        // because histogram code assumes a min >= 1; the underflow bucket will record the 0s.
//        RecordHistogram.recordLinearCountHistogram("History.ClearBrowsingData.NumImportant",
//                domains.length, 1, mMaxImportantSites + 1, mMaxImportantSites + 1);
//        mSortedImportantDomains = Arrays.copyOf(domains, domains.length);
//        mSortedImportantDomainReasons = Arrays.copyOf(importantReasons, importantReasons.length);
//        mSortedExampleOrigins = Arrays.copyOf(exampleOrigins, exampleOrigins.length);
////        deselectedDomainList.addAll(Arrays.asList(domains));
////        for (int importantReason : importantReasons) {
////            deselectedDomainReasonList.add(importantReason);
////        }
//    }
//
//    @Override
//    public void enableDialogAboutOtherFormsOfBrowsingHistory() {
//        if (getActivity() == null) return;
//
//        mIsDialogAboutOtherFormsOfBrowsingHistoryEnabled = true;
//    }
//
//    private boolean shouldShowImportantSitesDialog() {
//        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.IMPORTANT_SITES_IN_CBD)) return false;
//        EnumSet<DialogOption> selectedOptions = getSelectedOptions();
//        if (!selectedOptions.contains(DialogOption.CLEAR_CACHE)
//                && !selectedOptions.contains(DialogOption.CLEAR_COOKIES_AND_SITE_DATA)) {
//            return false;
//        }
//        boolean haveImportantSites =
//                mSortedImportantDomains != null && mSortedImportantDomains.length != 0;
//        RecordHistogram.recordBooleanHistogram(
//                "History.ClearBrowsingData.ImportantDialogShown", haveImportantSites);
//        return haveImportantSites;
//    }
//
//    private int[] toIntArray(List<Integer> boxedList) {
//        int[] result = new int[boxedList.size()];
//        for (int i = 0; i < result.length; i++) {
//            result[i] = boxedList.get(i);
//        }
//        return result;
//    }
//
//    private void showImportantDialogThenClear() {
//
//        ZDialog.alert()
//                .setTitle("确定清除？")
//                .setContent("这将清除所选中网站的数据！")
//                .setPositiveButton((fragment, which) -> {
//                    clearBrowsingData(getSelectedOptions(), null, null, null, null);
//                })
//                .show(context);
//
//
////        ZPopup.alert(getActivity())
////                .setTitle("确定清除？")
////                .setContent("这将清除所选中网站的数据！")
////                .setConfirmButton(popup -> {
////                    //                        String[] deselectedDomains = deselectedDomainList.toArray(new String[0]);
//////                        int[] deselectedDomainReasons = toIntArray(deselectedDomainReasonList);
//////                        String[] ignoredDomains = ignoredDomainList.toArray(new String[0]);
//////                        int[] ignoredDomainReasons = toIntArray(ignoredDomainReasonList);
//////                        if (deselectedDomains != null && mSortedImportantDomains != null) {
//////                            // mMaxImportantSites is a constant on the C++ side.
//////                            RecordHistogram.recordCustomCountHistogram(
//////                                    "History.ClearBrowsingData.ImportantDeselectedNum",
//////                                    deselectedDomains.length, 1, mMaxImportantSites + 1,
//////                                    mMaxImportantSites + 1);
//////                            RecordHistogram.recordCustomCountHistogram(
//////                                    "History.ClearBrowsingData.ImportantIgnoredNum", ignoredDomains.length, 1,
//////                                    mMaxImportantSites + 1, mMaxImportantSites + 1);
//////                            // We put our max at 20 instead of 100 to reduce the number of empty buckets (as
//////                            // our maximum denominator is 5).
//////                            RecordHistogram.recordEnumeratedHistogram(
//////                                    "History.ClearBrowsingData.ImportantDeselectedPercent",
//////                                    deselectedDomains.length * IMPORTANT_SITES_PERCENTAGE_BUCKET_COUNT
//////                                            / mSortedImportantDomains.length,
//////                                    IMPORTANT_SITES_PERCENTAGE_BUCKET_COUNT + 1);
//////                            RecordHistogram.recordEnumeratedHistogram(
//////                                    "History.ClearBrowsingData.ImportantIgnoredPercent",
//////                                    ignoredDomains.length * IMPORTANT_SITES_PERCENTAGE_BUCKET_COUNT
//////                                            / mSortedImportantDomains.length,
//////                                    IMPORTANT_SITES_PERCENTAGE_BUCKET_COUNT + 1);
//////                        }
//////                        clearBrowsingData(getSelectedOptions(), deselectedDomains, deselectedDomainReasons,
//////                                ignoredDomains, ignoredDomainReasons);
////
////                    clearBrowsingData(getSelectedOptions(), null, null, null, null);
////                })
////                .show();
//
//
////        Profile mProfile = Profile.getLastUsedProfile().getOriginalProfile();
////        LargeIconBridge mLargeIconBridge = new LargeIconBridge(mProfile);
////        ActivityManager activityManager =
////                ((ActivityManager) ContextUtils.getApplicationContext().getSystemService(
////                        Context.ACTIVITY_SERVICE));
////        int maxSize = Math.min(
////                activityManager.getMemoryClass() / 16 * 25 * 1024, 100 * 1024);
////        mLargeIconBridge.createCache(maxSize);
////
////        int mFaviconSize = getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
////        int mCornerRadius = getResources().getDimensionPixelSize(R.dimen.default_favicon_corner_radius);
////        int textSize = getResources().getDimensionPixelSize(R.dimen.default_favicon_icon_text_size);
////        int iconColor = ApiCompatibilityUtils.getColor(
////                getResources(), R.color.default_favicon_background_color);
////        RoundedIconGenerator mIconGenerator = new RoundedIconGenerator(
////                mFaviconSize, mFaviconSize, mCornerRadius, iconColor, textSize);
////        ZListDialog<String> dialog = new ZListDialog<>(getActivity());
////        dialog.setItemList(Arrays.asList(mSortedImportantDomains))
////                .setItemRes(R.layout.layout_check)
////                .setTitle(getString(R.string.clear_browsing_data_important_dialog_text))
////                .setAdapterCallback(new EasyAdapter.IEasy<String>() {
////
////                    private Drawable getFaviconDrawable(Bitmap icon, int fallbackColor, String url) {
////                        if (icon == null) {
////                            mIconGenerator.setBackgroundColor(fallbackColor);
////                            icon = mIconGenerator.generateIconForUrl(url);
////                            return new BitmapDrawable(getResources(), icon);
////                        } else {
////                            RoundedBitmapDrawable roundedIcon =
////                                    RoundedBitmapDrawableFactory.create(getResources(),
////                                            Bitmap.createScaledBitmap(icon, mFaviconSize, mFaviconSize, false));
////                            roundedIcon.setCornerRadius(mCornerRadius);
////                            return roundedIcon;
////                        }
////                    }
////
////                    @Override
////                    public EasyAdapter.ViewHolder onCreateViewHolder(List<String> list, View itemView, int i) {
////                        return null;
////                    }
////
////                    @Override
////                    public void onBindViewHolder(List<String> list, View itemView, int i) {
////                        TintedImageView imageView = itemView.getView(R.id.icon_view);
////                        TextView title = itemView.getView(R.id.title_view);
////                        TextView content = itemView.getView(R.id.content_view);
////                        SmoothCheckBox checkBox = itemView.getView(R.id.check_box);
////                        checkBox.setChecked(true, true);
////                        title.setText(list.get(i));
////                        content.setText(mSortedExampleOrigins[i]);
////                        mLargeIconBridge.getLargeIconForUrl(mSortedExampleOrigins[i], mFaviconSize, new LargeIconBridge.LargeIconCallback() {
////                            @Override
////                            public void onLargeIconAvailable(@Nullable Bitmap icon, int fallbackColor, boolean isFallbackColorDefault, int iconType) {
////                                imageView.setImageDrawable(getFaviconDrawable(icon, fallbackColor, mSortedExampleOrigins[i]));
////                            }
////                        });
////                        itemView.setOnClickListener(new View.OnClickListener() {
////                            @Override
////                            public void onClick(View v) {
////
////                            }
////                        });
////                    }
////                })
////                .show();
//
//
////        mConfirmImportantSitesDialog = ConfirmImportantSitesDialogFragment.newInstance(
////                mSortedImportantDomains, mSortedImportantDomainReasons, mSortedExampleOrigins);
////        mConfirmImportantSitesDialog.setTargetFragment(this, IMPORTANT_SITES_DIALOG_CODE);
////        getChildFragmentManager().beginTransaction().show(mConfirmImportantSitesDialog).commit();
////        mConfirmImportantSitesDialog.show(getChildFragmentManager(), ConfirmImportantSitesDialogFragment.FRAGMENT_TAG);
//    }
//
//    protected final EnumSet<DialogOption> getSelectedOptions() {
//        EnumSet<DialogOption> selected = EnumSet.noneOf(DialogOption.class);
//        for (Item item : mItems) {
//            if (item.isSelected()) {
//                selected.add(item.getOption());
//            }
//        }
//        return selected;
//    }
//
//    private void clearBrowsingData(EnumSet<DialogOption> options,
//                                   @Nullable String[] blacklistedDomains, @Nullable int[] blacklistedDomainReasons,
//                                   @Nullable String[] ignoredDomains, @Nullable int[] ignoredDomainReasons) {
//        showProgressDialog();
//
//        RecordHistogram.recordMediumTimesHistogram("History.ClearBrowsingData.TimeSpentInDialog",
//                SystemClock.elapsedRealtime() - mDialogOpened, TimeUnit.MILLISECONDS);
//
//        int[] dataTypes = new int[options.size()];
//        int i = 0;
//        for (DialogOption option : options) {
//            dataTypes[i] = option.getDataType();
//            ++i;
//        }
//
//        int timePeriod = ((TimePeriodSpinnerOption) mSpinner.getSelectedItem()).getTimePeriod();
//        if (blacklistedDomains != null && blacklistedDomains.length != 0) {
//            BrowsingDataBridge.getInstance().clearBrowsingDataExcludingDomains(this, dataTypes,
//                    timePeriod, blacklistedDomains, blacklistedDomainReasons, ignoredDomains,
//                    ignoredDomainReasons);
//        } else {
//            BrowsingDataBridge.getInstance().clearBrowsingData(this, dataTypes, timePeriod);
//        }
//
//    }
//
//    private void showProgressDialog() {
//        if (getActivity() == null) return;
//        mProgressDialog = ZDialog.loading()
//                .setTitle(getActivity().getString(R.string.clear_browsing_data_progress_title))
//                .show(getActivity());
//    }
//
//    private void dismissProgressDialog() {
//        if (mProgressDialog != null && mProgressDialog.isVisible()) {
//            mProgressDialog.dismiss();
//        }
//        mProgressDialog = null;
//    }
//
//    protected static class TimePeriodSpinnerOption {
//        private int mTimePeriod;
//        private String mTitle;
//
//        /**
//         * Constructs this time period spinner option.
//         * @param timePeriod The time period represented as an int from the shared enum
//         *     {@link TimePeriod}.
//         * @param title The text that will be used to represent this item in the spinner.
//         */
//        public TimePeriodSpinnerOption(int timePeriod, String title) {
//            mTimePeriod = timePeriod;
//            mTitle = title;
//        }
//
//        /**
//         * @return The time period represented as an int from the shared enum {@link TimePeriod}
//         */
//        public int getTimePeriod() {
//            return mTimePeriod;
//        }
//
//        @Override
//        public String toString() {
//            return mTitle;
//        }
//    }
//
//    private TimePeriodSpinnerOption[] getTimePeriodSpinnerOptions() {
//        Activity activity = getActivity();
//
//        List<TimePeriodSpinnerOption> options = new ArrayList<>();
//        options.add(new TimePeriodSpinnerOption(TimePeriod.LAST_HOUR,
//                activity.getString(R.string.clear_browsing_data_tab_period_hour)));
//        options.add(new TimePeriodSpinnerOption(TimePeriod.LAST_DAY,
//                activity.getString(R.string.clear_browsing_data_tab_period_24_hours)));
//        options.add(new TimePeriodSpinnerOption(TimePeriod.LAST_WEEK,
//                activity.getString(R.string.clear_browsing_data_tab_period_7_days)));
//        options.add(new TimePeriodSpinnerOption(TimePeriod.FOUR_WEEKS,
//                activity.getString(R.string.clear_browsing_data_tab_period_four_weeks)));
//        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CLEAR_OLD_BROWSING_DATA)) {
//            options.add(new TimePeriodSpinnerOption(TimePeriod.OLDER_THAN_30_DAYS,
//                    activity.getString(
//                            R.string.clear_browsing_data_tab_period_older_than_30_days)));
//        }
//        options.add(new TimePeriodSpinnerOption(TimePeriod.ALL_TIME,
//                activity.getString(R.string.clear_browsing_data_tab_period_everything)));
//        return options.toArray(new TimePeriodSpinnerOption[0]);
//    }
//
//    private static boolean isActivityVisible(Activity activity) {
//        if (activity == null) return false;
//        int activityState = ApplicationStatus.getStateForActivity(activity);
//        // In Android N multi-window mode, only one activity is resumed at a time. The other
//        // activity visible on the screen will be in the paused state. Activities not visible on
//        // the screen will be stopped or destroyed.
//        return activityState == ActivityState.RESUMED || activityState == ActivityState.PAUSED;
//    }
//}
//
