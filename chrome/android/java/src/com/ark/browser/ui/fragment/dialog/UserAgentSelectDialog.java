package com.ark.browser.ui.fragment.dialog;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.Nullable;

import com.ark.browser.core.UserAgentManager;
import com.zpj.fragmentation.dialog.ZDialog;

import org.chromium.chrome.R;

public class UserAgentSelectDialog extends ZDialog.BottomSelectDialogFragmentImpl<UserAgentManager.UserAgent> {

    @Override
    protected int getImplLayoutId() {
        return R.layout.fragment_dialog_layout_center_impl_list;
    }

    public UserAgentSelectDialog() {
        setTitle("浏览器标识");
        setData(UserAgentManager.getUserAgentArray());
        setSelected(UserAgentManager.getDefaultUserAgentIndex());
        onBindTitle((titleView, item, position) -> titleView.setText(item.getName()));
        onBindSubtitle((subtitleView, item, position) -> subtitleView.setText(item.getString()));
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        findViewById(R.id.btn_close).setOnClickListener(v -> dismiss());
    }

}
