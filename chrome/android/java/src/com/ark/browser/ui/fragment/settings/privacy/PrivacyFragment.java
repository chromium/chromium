package com.ark.browser.ui.fragment.settings.privacy;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.Nullable;

import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.zpj.toast.ZToast;
import com.zpj.widget.setting.CheckableSettingItem;
import com.zpj.widget.setting.CommonSettingItem;
import com.zpj.widget.setting.OnCheckableItemClickListener;
import com.zpj.widget.setting.OnCommonItemClickListener;
import com.zpj.widget.setting.SwitchSettingItem;

import org.chromium.chrome.R;

public class PrivacyFragment extends BaseSwipeBackFragment
        implements OnCheckableItemClickListener, OnCommonItemClickListener {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setToolbarTitle(R.string.text_settings_privacy);
    }

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_setting_privacy;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        setToolbarTitle("隐私设置");
        SwitchSettingItem navigationErrorItem = view.findViewById(R.id.item_navigation_error);
//        navigationErrorItem.setChecked(PrefServiceBridge.getInstance().isResolveNavigationErrorEnabled());
        navigationErrorItem.setOnItemClickListener(this);

        SwitchSettingItem searchSuggestionItem = view.findViewById(R.id.item_search_suggestions);
//        searchSuggestionItem.setChecked(PrefServiceBridge.getInstance().isSearchSuggestEnabled());
        searchSuggestionItem.setOnItemClickListener(this);

        SwitchSettingItem safeBrowsingExtendedReportingItem = view.findViewById(R.id.item_safe_browsing_extended_reporting);
//        safeBrowsingExtendedReportingItem.setChecked(PrefServiceBridge.getInstance().isSafeBrowsingExtendedReportingEnabled());
        safeBrowsingExtendedReportingItem.setOnItemClickListener(this);

        SwitchSettingItem safeBrowsingScoutReportingItem = view.findViewById(R.id.item_safe_browsing_scout_reporting);
//        safeBrowsingScoutReportingItem.setChecked(PrefServiceBridge.getInstance().isSafeBrowsingExtendedReportingEnabled());
        safeBrowsingScoutReportingItem.setOnItemClickListener(this);

//        boolean isSafeBrowsingScoutReportingActive = PrefServiceBridge.getInstance().isSafeBrowsingScoutReportingActive();
        boolean isSafeBrowsingScoutReportingActive = false;
        safeBrowsingExtendedReportingItem.setVisibility(isSafeBrowsingScoutReportingActive ? View.GONE : View.VISIBLE);
        safeBrowsingScoutReportingItem.setVisibility(isSafeBrowsingScoutReportingActive ? View.VISIBLE : View.GONE);

        SwitchSettingItem safeBrowsingItem = view.findViewById(R.id.item_safe_browsing);
//        safeBrowsingItem.setChecked(PrefServiceBridge.getInstance().isSafeBrowsingEnabled());
        safeBrowsingItem.setOnItemClickListener(this);

        SwitchSettingItem networkPredictionsItem = view.findViewById(R.id.item_network_predictions);
//        networkPredictionsItem.setChecked(PrefServiceBridge.getInstance().getNetworkPredictionEnabled());
        networkPredictionsItem.setOnItemClickListener(this);

        SwitchSettingItem usageAndCrashReportsItem = view.findViewById(R.id.item_usage_and_crash_reports);
        usageAndCrashReportsItem.setChecked(false);
        usageAndCrashReportsItem.setOnItemClickListener(this);

        SwitchSettingItem doNotTrackItem = view.findViewById(R.id.item_do_not_track);
//        doNotTrackItem.setChecked(PrefServiceBridge.getInstance().isDoNotTrackEnabled());
        doNotTrackItem.setOnItemClickListener(this);

        SwitchSettingItem contextualSearchItem = view.findViewById(R.id.item_contextual_search);
//        contextualSearchItem.setChecked(!PrefServiceBridge.getInstance().isContextualSearchDisabled());
        contextualSearchItem.setOnItemClickListener(this);

        CommonSettingItem clearBrowsingDataItem = view.findViewById(R.id.item_clear_browsing_data);
        clearBrowsingDataItem.setOnItemClickListener(this);
    }

    @Override
    public void onItemClick(CheckableSettingItem item) {
        boolean isChecked = item.isChecked();
        int id = item.getId();
        if (id == R.id.item_navigation_error) {
//            PrefServiceBridge.getInstance().setResolveNavigationErrorEnabled(isChecked);
        } else if (id == R.id.item_search_suggestions) {
//            PrefServiceBridge.getInstance().setSearchSuggestEnabled(isChecked);
        } else if (id == R.id.item_safe_browsing_extended_reporting || id == R.id.item_safe_browsing_scout_reporting) {
//            PrefServiceBridge.getInstance().setSafeBrowsingExtendedReportingEnabled(isChecked);
        } else if (id == R.id.item_safe_browsing) {
//            PrefServiceBridge.getInstance().setSafeBrowsingEnabled(isChecked);
        } else if (id == R.id.item_network_predictions) {
//            PrefServiceBridge.getInstance().setNetworkPredictionEnabled(isChecked);
        } else if (id == R.id.item_usage_and_crash_reports) {
            ZToast.warning("TODO Usage and crash reports");
        } else if (id == R.id.item_do_not_track) {
//            PrefServiceBridge.getInstance().setDoNotTrackEnabled(isChecked);
        } else if (id == R.id.item_contextual_search) {
//            PrefServiceBridge.getInstance().setContextualSearchState(isChecked);
//            ContextualSearchUma.logPreferenceChange(isChecked);
        }
    }

    @Override
    public void onItemClick(CommonSettingItem item) {
        if (item.getId() == R.id.item_clear_browsing_data) {
            start(new ClearBrowsingDataFragment());
        }
    }
}

