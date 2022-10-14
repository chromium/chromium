package com.ark.browser.ui.dialog;

import android.view.Display;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.lifecycle.DefaultLifecycleObserver;
import androidx.lifecycle.LifecycleOwner;

import com.ark.browser.ArkBrowserActivity;
import com.ark.browser.ui.widget.SmartSearchPanel;

import org.chromium.chrome.R;

import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.tab.Tab;

public class MainMenuDialog {

    public static void show(ArkBrowserActivity activity) {
        View view = LayoutInflater.from(activity).inflate(R.layout.layout_menu, null);

        AlertDialog dialog = new AlertDialog.Builder(activity)
                .setView(view)
                .create();

        TextView userAgentButton = view.findViewById(R.id.btn_user_agent);
        userAgentButton.setOnClickListener(v -> {
            dialog.dismiss();
            UserAgentSelector.show(activity);
        });

        TextView refreshButton = view.findViewById(R.id.btn_refresh);
        refreshButton.setOnClickListener(v -> {
            Tab tab = activity.getActivityTab();
            if (tab != null) {
                tab.reload();
            }
            dialog.dismiss();
        });

        TextView downloadManagerButton = view.findViewById(R.id.btn_download_manager);
        downloadManagerButton.setOnClickListener(v -> {
            DownloadManagerDialog.show(activity);
            dialog.dismiss();
        });

        TextView test = view.findViewById(R.id.btn_refresh2);
        test.setOnClickListener(v -> {
//            SmartSearchPanel smartSearchPanel = activity.findViewById(R.id.layout_smart_search);
//            smartSearchPanel.updateKeyword("test");
//            smartSearchPanel.show();


            SmartSearchDialog smartSearchDialog = new SmartSearchDialog(activity);
            smartSearchDialog.show();


//            SmartSearchPanel smartSearchPanel = (SmartSearchPanel) LayoutInflater.from(activity).inflate(R.layout.layout_smart_search, null, false);
//
//            AlertDialog selector = new AlertDialog.Builder(activity)
//                    .setTitle("Test")
//                    .setView(smartSearchPanel)
//                    .create();
//
//
//            smartSearchPanel.attachWindow(selector.getWindow());
//
//            selector.show();
//
//
//            smartSearchPanel.updateKeyword("test");
//            smartSearchPanel.show();


            dialog.dismiss();
        });

        dialog.show();

        //设置弹窗在底部
        Window window = dialog.getWindow();
        window.setGravity(Gravity.BOTTOM);

        WindowManager m = activity.getWindowManager();
        Display d = m.getDefaultDisplay(); //为获取屏幕宽、高
        WindowManager.LayoutParams p = dialog.getWindow().getAttributes(); //获取对话框当前的参数值
        p.width = d.getWidth(); //宽度设置为屏幕
        dialog.getWindow().setAttributes(p); //设置生效

    }

}
