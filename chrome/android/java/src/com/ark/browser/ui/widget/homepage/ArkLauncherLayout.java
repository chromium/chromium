package com.ark.browser.ui.widget.homepage;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.util.AttributeSet;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.launcher3.ItemInfo;
import com.android.launcher3.ItemInfoWithIcon;
import com.android.launcher3.LauncherLayout;
import com.android.launcher3.TabItemInfo;
import com.android.launcher3.database.FavoriteItemTable;
import com.android.launcher3.database.SQLite;
import com.android.launcher3.model.FavoriteItem;
import com.android.launcher3.popup.OptionItem;
import com.android.launcher3.popup.OptionsPopupView;
import com.ark.browser.ui.fragment.search.SearchFragment;
import com.ark.browser.ui.fragment.wallpaper.WallpaperSelectFragment;
import com.zpj.utils.Callback;

import org.chromium.chrome.R;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.List;

public class ArkLauncherLayout extends LauncherLayout {

    public ArkLauncherLayout(@NonNull Context context) {
        this(context, null);
    }

    public ArkLauncherLayout(@NonNull Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public ArkLauncherLayout(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        setClickHandler(new LauncherLayout.ClickHandler() {
            @Override
            public void onClickAppShortcut(View v, ItemInfoWithIcon itemInfo) {
                Toast.makeText(context, "title=" + itemInfo.title + " url=" + itemInfo.url, Toast.LENGTH_SHORT).show();
            }

            @Override
            public void onClickTabCard(View v, TabItemInfo itemInfo) {
                Toast.makeText(context, "title=" + itemInfo.title + " url=" + itemInfo.url, Toast.LENGTH_SHORT).show();
            }

            @Override
            public void onClickToSearch(View v) {
                new SearchFragment().show(context);
            }
        });

        setOptionItemProvider(new LauncherLayout.OptionItemProvider() {
            @Override
            public List<OptionItem> createOptions() {
                ArrayList<OptionItem> options = new ArrayList<>();
                options.add(new OptionItem(R.string.wallpaper_button_text, R.drawable.format_picture,
                        new View.OnClickListener() {
                            @Override
                            public void onClick(View v) {
                                Toast.makeText(v.getContext(), "壁纸选择", Toast.LENGTH_SHORT).show();
                                WallpaperSelectFragment.start(context);
                            }
                        }));
                options.add(new OptionItem(R.string.widget_button_text, R.drawable.ic_widgets,
                        new View.OnClickListener() {
                            @Override
                            public void onClick(View v) {
                                Toast.makeText(v.getContext(), "微件", Toast.LENGTH_SHORT).show();
                            }
                        }));
                options.add(new OptionItem(R.string.settings_button_text, R.drawable.ic_setting,
                        OptionsPopupView::startSettings));
                return options;
            }

            @Override
            public List<OptionItem> createOptions(ItemInfo itemInfo) {
                ArrayList<OptionItem> options = new ArrayList<>();
                options.add(new OptionItem(R.string.widget_button_text, R.drawable.ic_widgets,
                        new View.OnClickListener() {
                            @Override
                            public void onClick(View v) {
                                Toast.makeText(v.getContext(), "微件", Toast.LENGTH_SHORT).show();
                            }
                        }));
                options.add(new OptionItem(R.string.app_info_drop_target_label, R.drawable.ic_info_white,
                        new View.OnClickListener() {
                            @Override
                            public void onClick(View v) {
                                Toast.makeText(v.getContext(), "应用信息", Toast.LENGTH_SHORT).show();
                            }
                        }));
                options.add(new OptionItem(R.string.install_drop_target_label, R.drawable.ic_cancel,
                        new View.OnClickListener() {
                            @Override
                            public void onClick(View v) {
                                Toast.makeText(v.getContext(), "安装", Toast.LENGTH_SHORT).show();
                            }
                        }));
                return options;
            }
        });

        setItemLoader(new LauncherLayout.ItemLoader() {
            @Override
            public void onFirstRun() {
                SQLite.with(FavoriteItemTable.class).delete();
                for (ItemInfo info : HomepageUtils.initHomeNav()) {
                    FavoriteItem.from(info).insert();
                }
            }

            @Override
            public void loadIcon(ItemInfo itemInfo, Callback<Bitmap> callback) {
                Resources resources = getResources();
                int resId = R.mipmap.app_icon;
                if (HomepageUtils.isDeepLink(itemInfo.url)) {
                    switch (itemInfo.url) {
                        case HomepageUtils.DEEPLINK_MANAGER:
                            resId = R.drawable.icon_browser_manager;
                            break;
                        case HomepageUtils.DEEPLINK_COLLECTIONS:
                            resId = R.drawable.icon_collections;
                            break;
                        case HomepageUtils.DEEPLINK_BROWSER:
                            resId = R.drawable.icon_browser;
                            break;
                        case HomepageUtils.DEEPLINK_DOWNLOADS:
                            resId = R.drawable.icon_download_manager;
                            break;
                        case HomepageUtils.DEEPLINK_SETTINGS:
                            resId = R.drawable.icon_settings;
                            break;
                    }
                }
                callback.onCallback(BitmapFactory.decodeResource(resources, resId));
            }
        });

    }

}
