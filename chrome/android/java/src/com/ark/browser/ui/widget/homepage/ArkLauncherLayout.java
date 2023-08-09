package com.ark.browser.ui.widget.homepage;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.launcher3.ItemInfo;
import com.android.launcher3.LauncherLayout;
import com.android.launcher3.popup.OptionItem;
import com.android.launcher3.popup.OptionsPopupView;
import com.ark.browser.ui.fragment.wallpaper.WallpaperSelectFragment;

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

        setItemLoader(new HomepageItemLoader(getContext()));

    }

}
