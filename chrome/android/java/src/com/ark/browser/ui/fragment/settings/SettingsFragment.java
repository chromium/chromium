package com.ark.browser.ui.fragment.settings;

import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.appcompat.widget.Toolbar;
import androidx.core.graphics.drawable.DrawableCompat;

import com.ark.browser.core.UserAgentManager;
import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.ark.browser.ui.fragment.dialog.AboutMeDialog;
import com.ark.browser.ui.fragment.dialog.SearchEngineSelectDialog;
import com.ark.browser.ui.fragment.dialog.UserAgentSelectDialog;
import com.google.android.material.appbar.CollapsingToolbarLayout;
import com.zpj.skin.SkinEngine;
import com.zpj.toast.ZToast;
import com.zpj.widget.setting.CheckableSettingItem;
import com.zpj.widget.setting.CommonSettingItem;
import com.zpj.widget.setting.OnCheckableItemClickListener;
import com.zpj.widget.setting.OnCommonItemClickListener;
import com.zpj.widget.setting.SwitchSettingItem;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.search_engines.TemplateUrl;

public class SettingsFragment extends BaseSwipeBackFragment
        implements OnCommonItemClickListener, OnCheckableItemClickListener {

    private CommonSettingItem itemSearchEngine;
    private CommonSettingItem itemUserAgent;

//    public static void start() {
//        ZBus.post(new SettingFragment2());
//    }

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_settings;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
//        ZToolBar toolBar = findViewById(R.id.tool_bar);
//        toolBar.setCenterText("设置");

        Toolbar toolbar = findViewById(R.id.toolbar);
        toolbar.setTitle("设置");

        int textColor = SkinEngine.getColor(context, R.attr.textColorMajor);
        toolbar.setTitleTextColor(textColor);

        final Drawable wrappedDrawable = DrawableCompat.wrap(toolbar.getNavigationIcon());
        DrawableCompat.setTintList(wrappedDrawable, ColorStateList.valueOf(SkinEngine.getColor(context, R.attr.textColorMajor)));
        toolbar.setNavigationIcon(wrappedDrawable);

        CollapsingToolbarLayout collapsingToolbarLayout = findViewById(R.id.collapsingToolbar);
        collapsingToolbarLayout.setExpandedTitleColor(textColor);
        collapsingToolbarLayout.setCollapsedTitleTextColor(textColor);

        itemSearchEngine = findViewById(R.id.item_search_engine);
        itemSearchEngine.setOnItemClickListener(this);

        TemplateUrlServiceFactory.get().runWhenLoaded(() -> {
            TemplateUrl defaultTemplateUrl = TemplateUrlServiceFactory.get().getDefaultSearchEngineTemplateUrl();
            itemSearchEngine.setInfoText(defaultTemplateUrl.getShortName());
        });

        itemUserAgent = findViewById(R.id.item_user_agent);
        itemUserAgent.setOnItemClickListener(this);

        itemUserAgent.setInfoText(UserAgentManager.getDefaultUserAgent().getName());

        SwitchSettingItem translateItem = findViewById(R.id.item_translate);
//        translateItem.setChecked(PrefServiceBridge.getInstance().isTranslateEnabled());
//        translateItem.setOnItemClickListener(new OnCheckableItemClickListener() {
//            @Override
//            public void onItemClick(CheckableSettingItem item) {
//                PrefServiceBridge.getInstance().setTranslateEnabled(item.isChecked());
//            }
//        });

        CommonSettingItem itemAutoFill = view.findViewById(R.id.item_auto_fill);
        itemAutoFill.setOnItemClickListener(this);

        CommonSettingItem itemSavePassword = view.findViewById(R.id.item_save_password);
        itemSavePassword.setOnItemClickListener(this);

        CommonSettingItem itemNotification = view.findViewById(R.id.item_notification);
        itemNotification.setOnItemClickListener(this);


        CommonSettingItem itemPrivacy = view.findViewById(R.id.item_privacy);
        itemPrivacy.setOnItemClickListener(this);

        CommonSettingItem itemAccessibility = view.findViewById(R.id.item_accessibility);
        itemAccessibility.setOnItemClickListener(this);

        CommonSettingItem itemWebsite = view.findViewById(R.id.item_website);
        itemWebsite.setOnItemClickListener(this);

        CommonSettingItem itemDataReduction = view.findViewById(R.id.item_data_reduction);
        itemDataReduction.setOnItemClickListener(this);

        CommonSettingItem itemAboutMe = view.findViewById(R.id.item_about_me);
        itemAboutMe.setOnItemClickListener(this);

        CommonSettingItem itemCheckUpdate = view.findViewById(R.id.item_check_update);
        itemCheckUpdate.setOnItemClickListener(this);
    }

    private void showSearchEngineSelectDialog() {
        new SearchEngineSelectDialog()
                .onSingleSelect((fragment, position, item) -> {
                    TemplateUrlServiceFactory.get().setSearchEngine(item.getKeyword());
                    itemSearchEngine.setInfoText(item.getShortName());
                })
                .show(context);
    }

    private void showUserAgentSelectDialog() {
        new UserAgentSelectDialog()
                .onSingleSelect((fragment, position, item) -> {
                    UserAgentManager.setDefaultUserAgentIndex(position);
                    itemUserAgent.setInfoText(item.getName());
                })
                .show(context);
    }

    private void goNotificationSetting() {
        Intent intent = new Intent();
        if (Build.VERSION.SDK_INT >= 26) {
            // android 8.0引导
            intent.setAction("android.settings.APP_NOTIFICATION_SETTINGS");
            intent.putExtra("android.provider.extra.APP_PACKAGE", getContext().getPackageName());
        } else if (Build.VERSION.SDK_INT >= 21) {
            // android 5.0-7.0
            intent.setAction("android.settings.APP_NOTIFICATION_SETTINGS");
            intent.putExtra("app_package", getContext().getPackageName());
            intent.putExtra("app_uid", getContext().getApplicationInfo().uid);
        } else {
            // 其他
            intent.setAction("android.settings.APPLICATION_DETAILS_SETTINGS");
            intent.setData(Uri.fromParts("package", getContext().getPackageName(), null));
        }
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(intent);
    }

    @Override
    public void onItemClick(CommonSettingItem item) {
        int id = item.getId();
        if (id == R.id.item_search_engine) {
            showSearchEngineSelectDialog();
        } else if (id == R.id.item_user_agent) {
            showUserAgentSelectDialog();
        } else if (id == R.id.item_auto_fill) {
//            _mActivity.start(new AutofillAndPaymentsFragment());
        } else if (id == R.id.item_save_password) {
//            _mActivity.start(new SavePasswordsFragment());
        } else if (id == R.id.item_notification) {
            goNotificationSetting();
        } else if (id == R.id.item_privacy) {
//            _mActivity.start(new PrivacyFragment());
        } else if (id == R.id.item_accessibility) {
//            _mActivity.start(new AccessibilityFragment());
        } else if (id == R.id.item_website) {
//            _mActivity.start(new WebSiteSettingsFragment());
        } else if (id == R.id.item_data_reduction) {
//            _mActivity.start(new DataReductionFragment());
        } else if (id == R.id.item_about_me) {
            new AboutMeDialog().show(context);
        } else if (id == R.id.item_check_update) {
            ZToast.normal("检查更新");
        }
    }

    @Override
    public void onItemClick(CheckableSettingItem item) {

    }
}
