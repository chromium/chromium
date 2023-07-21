package com.ark.browser.ui.fragment.settings.privacy;

import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.Nullable;

import com.ark.browser.core.UserAgentManager;
import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.zpj.fragmentation.dialog.IDialog;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.toast.ZToast;
import com.zpj.widget.setting.CheckableSettingItem;
import com.zpj.widget.setting.CommonSettingItem;
import com.zpj.widget.setting.OnCheckableItemClickListener;
import com.zpj.widget.setting.OnCommonItemClickListener;
import com.zpj.widget.setting.SwitchSettingItem;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.privacy.secure_dns.SecureDnsBridge;
import org.chromium.net.SecureDnsMode;

import java.util.ArrayList;
import java.util.List;

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

        CommonSettingItem secureDnsItem = view.findViewById(R.id.item_secure_dns);
        secureDnsItem.setInfoText(getSecureDnsConfig());
        secureDnsItem.setOnItemClickListener(this);
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
        } else if (item.getId() == R.id.item_secure_dns) {
            new SecureDnsSelectDialog()
                    .onSingleSelect(new IDialog.OnSingleSelectListener<SecureDnsBridge.Entry, ZDialog.BottomSelectDialogFragmentImpl<SecureDnsBridge.Entry>>() {
                        @Override
                        public void onSelect(ZDialog.BottomSelectDialogFragmentImpl<SecureDnsBridge.Entry> fragment, int i, SecureDnsBridge.Entry entry) {
                            if (i == 0) {
                                item.setInfoText("已关闭");
                            } else {
                                item.setInfoText(entry.name);
                            }
                        }
                    })
                    .show(item.getContext());
        }
    }

    private static String getSecureDnsConfig() {

        int mode = SecureDnsBridge.getMode();
        if (mode == SecureDnsMode.OFF) {
            return "已关闭";
        } else if (mode == SecureDnsMode.AUTOMATIC) {
            return "系统默认";
        } else {
            List<SecureDnsBridge.Entry> allDns = SecureDnsBridge.getProviders();
            String config = SecureDnsBridge.getConfig();
            for (int i = 0; i < allDns.size(); i++) {
                SecureDnsBridge.Entry entry = allDns.get(i);
                if (TextUtils.equals(config, entry.config)) {
                    return entry.name;
                }
            }
        }
        return "已关闭";
    }

    public static class SecureDnsSelectDialog extends ZDialog.BottomSelectDialogFragmentImpl<SecureDnsBridge.Entry> {

        @Override
        protected int getImplLayoutId() {
            return R.layout.fragment_dialog_layout_center_impl_list;
        }

        public SecureDnsSelectDialog() {
            setTitle("使用安全 DNS");


            List<SecureDnsBridge.Entry> allDns = SecureDnsBridge.getProviders();

            int selected = 0;
            int mode = SecureDnsBridge.getMode();
            if (mode == SecureDnsMode.OFF) {
                selected = 0;
            } else if (mode == SecureDnsMode.AUTOMATIC) {
                selected = 1;
            } else {
                // TODO

                String config = SecureDnsBridge.getConfig();
                for (int i = 0; i < allDns.size(); i++) {
                    SecureDnsBridge.Entry entry = allDns.get(i);
                    if (TextUtils.equals(config, entry.config)) {
                        selected = i + 2;
                        break;
                    }
                }

            }

            List<SecureDnsBridge.Entry> entries = new ArrayList<>();
            entries.add(new SecureDnsBridge.Entry("关闭安全 DNS", "开启安全 DNS", null));
            entries.add(new SecureDnsBridge.Entry("系统默认", "使用您当前的服务提供商，安全 DNS 未必一直可用", null));
            entries.addAll(allDns);

            setData(entries);
            setSelected(selected);
            onBindTitle((titleView, item, position) -> titleView.setText(item.name));
            onBindSubtitle((subtitleView, item, position) -> subtitleView.setText(item.config));
        }

        @Override
        protected void initView(View view, @Nullable Bundle savedInstanceState) {
            super.initView(view, savedInstanceState);
            findViewById(R.id.btn_close).setOnClickListener(v -> dismiss());
        }

        @Override
        public ZDialog.BottomSelectDialogFragmentImpl<SecureDnsBridge.Entry> onSingleSelect(IDialog.OnSingleSelectListener<SecureDnsBridge.Entry, ZDialog.BottomSelectDialogFragmentImpl<SecureDnsBridge.Entry>> onSingleSelectListener) {
            return super.onSingleSelect((fragment, i, entry) -> {
                if (i == 0) {
                    SecureDnsBridge.setMode(SecureDnsMode.OFF);
                } else if (i == 1) {
                    SecureDnsBridge.setMode(SecureDnsMode.AUTOMATIC);
                } else {
                    SecureDnsBridge.setMode(SecureDnsMode.SECURE);
                    SecureDnsBridge.setConfig(entry.config);
                }
                if (onSingleSelectListener != null) {
                    onSingleSelectListener.onSelect(fragment, i, entry);
                }
            });
        }
    }
}

