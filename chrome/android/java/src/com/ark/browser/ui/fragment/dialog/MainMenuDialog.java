package com.ark.browser.ui.fragment.dialog;

import android.os.Bundle;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.Nullable;

import com.ark.browser.ArkBrowserActivity;
import com.ark.browser.settings.AppConfig;
import com.ark.browser.tab.TabListManager;
import com.ark.browser.ui.dialog.BookmarkManagerDialog;
import com.ark.browser.ui.dialog.DownloadManagerDialog;
import com.ark.browser.ui.dialog.HistoryManagerDialog;
import com.ark.browser.ui.widget.DrawableTintTextView;
import com.ark.browser.utils.SkinChangeAnimation;
import com.zpj.fragmentation.dialog.base.OverDragBottomDialogFragment;
import com.zpj.skin.SkinEngine;
import com.zpj.utils.ClickHelper;

import org.chromium.chrome.R;

public class MainMenuDialog extends OverDragBottomDialogFragment<MainMenuDialog> implements View.OnClickListener {

    @Override
    protected int getImplLayoutId() {
        return R.layout.fragment_dialog_main_menu;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        ImageView ibSetting = findViewById(R.id.ib_setting);
        ImageView ibClose = findViewById(R.id.ib_close);
        ImageView ibShare = findViewById(R.id.ib_share);
        DrawableTintTextView tvOffline = findViewById(R.id.tv_offline);
        DrawableTintTextView tvNightMode = findViewById(R.id.tv_night_mode);
        DrawableTintTextView tvPrivateMode = findViewById(R.id.tv_private_mode);
        DrawableTintTextView tvAddon = findViewById(R.id.tv_addon);
        DrawableTintTextView tvSkin = findViewById(R.id.tv_skin);
//        DrawableTintTextView tvTools = findViewById(R.id.tv_tools);
        DrawableTintTextView tvCollection = findViewById(R.id.tv_collection);
        DrawableTintTextView tvDownload = findViewById(R.id.tv_download);
//        DrawableTintTextView tvRefresh = findViewById(R.id.tv_refresh);
        DrawableTintTextView tvManager = findViewById(R.id.tv_manager);
        DrawableTintTextView tvHistory = findViewById(R.id.tv_history);
        DrawableTintTextView tvExit = findViewById(R.id.tv_exit);

        int primaryColor = SkinEngine.getColor(context, android.R.attr.colorPrimary);
        if (AppConfig.isNightMode()) {
            tvNightMode.setTint(primaryColor);
        }

        if (TabListManager.getInstance().getCurrentTabList().isIncognito()) {
            tvPrivateMode.setTint(primaryColor);
        }

        ibSetting.setOnClickListener(this);
        ibClose.setOnClickListener(this);
        ibShare.setOnClickListener(this);
//        tvNightMode.setOnClickListener(this);
        ClickHelper.with(tvNightMode)
                .setOnClickListener((v, x, y) -> {
                    SkinChangeAnimation.with(getContext())
                            .setStartPosition(x, y)
                            .setStartRadius(0)
                            .setDuration(500)
                            .setStartRunnable(() -> {
                                boolean isNightMode = AppConfig.isNightMode();
                                SkinEngine.changeSkin(isNightMode ? R.style.ArkDayTheme : R.style.ArkNightTheme);
                                tvNightMode.setTint(getResources().getColor(isNightMode ? R.color.google_black_400 : R.color.colorPrimary));
//                                IPage page = TabListManager.getInstance().getCurrentPage();
//                                Tab tab = page == null ? null : page.getNativePage();
//                                if (tab != null) {
//                                    if (isNightMode) {
//                                        tab.dayMode();
//                                    } else {
//                                        tab.nightMode();
//                                    }
//                                }
                                AppConfig.toggleThemeMode();
                            })
                            .setDismissRunnable(() -> {
//                                IPage page = TabListManager.getInstance().getCurrentPage();
//                                Tab tab = page == null ? null : page.getNativePage();
//                                if (tab != null) {
//                                    tab.updateThemeColor("MainMenuDialog");
//                                }
                                dismiss();
                            })
                            .start();
                });
        tvOffline.setOnClickListener(this);
        tvPrivateMode.setOnClickListener(this);
        tvAddon.setOnClickListener(this);
        tvSkin.setOnClickListener(this);
//        tvTools.setOnClickListener(this);
        tvCollection.setOnClickListener(this);
        tvDownload.setOnClickListener(this);
//        tvRefresh.setOnClickListener(this);
        tvManager.setOnClickListener(this);
        tvHistory.setOnClickListener(this);
        tvExit.setOnClickListener(this);
    }

    @Override
    public void onClick(View v) {

        int id = v.getId();
        if (R.id.ib_share == id) {
//            ShareDialog.start(context);
        } else if (R.id.ib_close == id) {

        } else if (R.id.ib_setting == id) {
//            new SettingFragment().show(context);
        } else if (R.id.tv_offline == id) {
//            CollectionFragment2.start(context, 2);
        } else if (R.id.tv_night_mode == id) {
//            boolean isNightMode = AppConfig.isNightMode();
//            AppConfig.toggleThemeMode();
//            DrawableTintTextView tvNightMode = (DrawableTintTextView) v;
//            tvNightMode.setTint(getResources().getColor(isNightMode ? R.color.google_black_400 : R.color.colorPrimary));
        } else if (R.id.tv_private_mode == id) {
//            if (!TabListManager.getInstance().getCurrentTabList().isIncognito()) {
//                if (TabListManager.getInstance().getTabList(true).getCount() > 0) {
//                    TabListManager.getInstance().selectTabList(true);
//                } else {
//                    if (PrefServiceBridge.getInstance().isIncognitoModeEnabled()) {
//                        ZToast.normal("隐身模式");
//                    }
//                }
//            } else {
//                TabListManager.getInstance().getTabList(true).closeAllTabs();
//            }
        } else if (R.id.tv_addon == id) {

        } else if (R.id.tv_skin == id) {
//            ADBlockUpdater.UpdateADBlock(context, true);
        } else if (R.id.tv_history == id) {
//            CollectionFragment2.start(context, 1);
            HistoryManagerDialog.show((ArkBrowserActivity) context);
        } else if (R.id.tv_collection == id) {
//            CollectionFragment2.start(context, 0);
            BookmarkManagerDialog.show((ArkBrowserActivity) context);
        } else if (R.id.tv_download == id) {
//            DownloadFragment.newInstance(true).show(context);
            DownloadManagerDialog.show((ArkBrowserActivity) context);
        } else if (R.id.tv_manager == id) {
//            new ManagerFragment().show(context);
        } else if (R.id.tv_exit == id) {
            new ExitDialog().show(context);
        }
        dismiss();
    }
}
