package com.ark.browser.ui.fragment.manager;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.Nullable;

import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.ark.browser.ui.fragment.manager.adblock.AdblockManagerFragment;
import com.ark.browser.ui.fragment.manager.extension.ExtensionManagerFragment;
import com.ark.browser.ui.fragment.manager.script.ScriptManagerFragment;
import com.ark.browser.ui.widget.ShadowLayout;

import org.chromium.chrome.R;

public class ManagerFragment extends BaseSwipeBackFragment implements View.OnClickListener {

    private ShadowLayout cleanerItem;
    private ShadowLayout permissionItem;
    private ShadowLayout networkItem;
    private ShadowLayout adsBlockerItem;
    private ShadowLayout addonItem;
    private ShadowLayout scriptItem;

    @Override
    protected void initStatusBar() {
        lightStatusBar();
    }

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_manager;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        cleanerItem = view.findViewById(R.id.item_cleaner);
        permissionItem = view.findViewById(R.id.item_permission);
        networkItem = view.findViewById(R.id.item_network);
        adsBlockerItem = view.findViewById(R.id.item_ads_blocker);
        addonItem = view.findViewById(R.id.item_addon);
        scriptItem = view.findViewById(R.id.item_script);

        cleanerItem.setOnClickListener(this);
        permissionItem.setOnClickListener(this);
        networkItem.setOnClickListener(this);
        adsBlockerItem.setOnClickListener(this);
        addonItem.setOnClickListener(this);
        scriptItem.setOnClickListener(this);
    }

    @Override
    public void onClick(View v) {
        if (v == cleanerItem) {
//            _mActivity.start(new ClearBrowsingDataFragment());
        } else if (v == permissionItem) {
//            _mActivity.start(new WebSiteSettingsFragment());
        } else if (v == networkItem) {
//            _mActivity.start(new DataReductionFragment());
        } else if (v == adsBlockerItem) {
            _mActivity.start(new AdblockManagerFragment());
        } else if (v == addonItem) {
            _mActivity.start(new ExtensionManagerFragment());
        } else if (v == scriptItem) {
            _mActivity.start(new ScriptManagerFragment());
        }
    }
}

