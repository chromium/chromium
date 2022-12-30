package com.ark.browser.ui.fragment.manager.adblock;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.Nullable;

import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;

import org.chromium.chrome.R;

public class AdblockAbpRuleFragment extends BaseSwipeBackFragment {
    @Override
    protected int getLayoutId() {
        return R.layout.fragment_manager_script;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        setToolbarTitle("ABP规则");
    }
}
