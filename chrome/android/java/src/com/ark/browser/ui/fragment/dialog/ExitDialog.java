package com.ark.browser.ui.fragment.dialog;

import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.core.app.ActivityCompat;

import com.ark.browser.tab.TabListManager;
import com.ark.browser.ui.widget.CheckLayout;
import com.ark.browser.ui.widget.DialogHeaderLayout;
import com.zpj.fragmentation.dialog.base.OverDragBottomDialogFragment;
import com.zpj.skin.SkinEngine;
import com.zpj.utils.ContextUtils;
import com.zpj.utils.PrefsHelper;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;

public class ExitDialog extends OverDragBottomDialogFragment<ExitDialog> implements View.OnClickListener {

    private boolean closeTabs = false;
    private boolean clearHistory = false;
    private boolean sendToBackground = false;

    @Override
    protected int getImplLayoutId() {
        return R.layout.fragment_dialog_exit;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        closeTabs = PrefsHelper.with().getBoolean("close_tabs_when_exit", false);
        clearHistory = PrefsHelper.with().getBoolean("clear_history_when_exit", false);
        sendToBackground = PrefsHelper.with().getBoolean("send_background_when_exit", false);

        DialogHeaderLayout headerLayout = findViewById(R.id.layout_dialog_header);
        headerLayout.setOnCloseClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                dismiss();
            }
        });

        CheckLayout close_tabs = findViewById(R.id.close_tabs);
        close_tabs.setChecked(closeTabs);
        CheckLayout clear_history = findViewById(R.id.clear_history);
        clear_history.setChecked(clearHistory);
        CheckLayout send_background = findViewById(R.id.send_background);
        send_background.setChecked(sendToBackground);

        close_tabs.setOnCheckedChangeListener((checkLayout, isChecked) -> closeTabs = isChecked);
        clear_history.setOnCheckedChangeListener((checkLayout, isChecked) -> clearHistory = isChecked);
        send_background.setOnCheckedChangeListener((checkLayout, isChecked) -> sendToBackground = isChecked);

        TextView tvCancel = findViewById(R.id.tv_cancel);
        TextView tvOk = findViewById(R.id.tv_ok);
        SkinEngine.setTextColor(tvCancel, R.attr.textColorMajor);
        SkinEngine.setTextColor(tvOk, android.R.attr.colorPrimary);
        tvCancel.setOnClickListener(this);
        tvOk.setOnClickListener(this);
    }

    @Override
    public void onClick(View v) {
        int id = v.getId();
        if (R.id.tv_ok == id) {
            PrefsHelper.with().putBoolean("close_tabs_when_exit", closeTabs);
            PrefsHelper.with().putBoolean("clear_history_when_exit", clearHistory);
            PrefsHelper.with().putBoolean("send_background_when_exit", sendToBackground);
            if (closeTabs && !sendToBackground) {
                TabListManager.getInstance().getCurrentTabList().closeAllTabs();
            }
            if (clearHistory) {
                //todo something
                int[] dataTypes = {BrowsingDataType.HISTORY};
                BrowsingDataBridge.getInstance().clearBrowsingData(new BrowsingDataBridge.OnClearBrowsingDataListener() {
                    @Override
                    public void onBrowsingDataCleared() {
                        if (sendToBackground) {
                            ContextUtils.getActivity(context).moveTaskToBack(true);
                        } else {
//                                ApplicationLifetime.terminate(false);
                            ActivityCompat.finishAfterTransition(_mActivity);
                        }
                    }
                }, dataTypes, TimePeriod.LAST_DAY);
            } else {
                if (sendToBackground) {
                    ContextUtils.getActivity(context).moveTaskToBack(true);
                } else {
                    ActivityCompat.finishAfterTransition(_mActivity);
//                        ApplicationLifetime.terminate(false);
                }
            }
        }
        dismiss();
    }
}
