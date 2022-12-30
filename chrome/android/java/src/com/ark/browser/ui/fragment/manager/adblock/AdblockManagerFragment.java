package com.ark.browser.ui.fragment.manager.adblock;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.Nullable;

import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.zpj.toast.ZToast;
import com.zpj.widget.setting.CommonSettingItem;
import com.zpj.widget.setting.SwitchSettingItem;

import org.chromium.chrome.R;

public class AdblockManagerFragment extends BaseSwipeBackFragment {

    private SwitchSettingItem adblockItem;
    private SwitchSettingItem adblockTipItem;
    private CommonSettingItem domainRulesItem;
    private CommonSettingItem domRulesItem;
    private CommonSettingItem abpRulesItem;

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_manager_adblock;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        setToolbarTitle("广告拦截");

        adblockItem = view.findViewById(R.id.item_block_ads);
        adblockTipItem = view.findViewById(R.id.item_block_ads_tip);
        domainRulesItem = view.findViewById(R.id.item_domain_rules);
        domRulesItem = view.findViewById(R.id.item_dom_rules);
        abpRulesItem = view.findViewById(R.id.item_abp_rules);

        adblockItem.setOnItemClickListener(item -> ZToast.normal("广告拦截"));

        adblockTipItem.setOnItemClickListener(item -> ZToast.normal("广告拦截提示"));

        domainRulesItem.setOnItemClickListener(item -> _mActivity.start(new AdblockDomainRuleFragment()));
        domRulesItem.setOnItemClickListener(item -> _mActivity.start(new AdblockDomWebsitesFragment()));
        abpRulesItem.setOnItemClickListener(item -> _mActivity.start(new AdblockAbpRuleFragment()));
    }
}
