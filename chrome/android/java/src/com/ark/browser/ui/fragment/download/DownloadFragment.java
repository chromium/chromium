package com.ark.browser.ui.fragment.download;

import android.content.Context;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.viewpager.widget.ViewPager;

import com.ark.browser.settings.Keys;
import com.ark.browser.ui.adapter.PageAdapter;
import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.ark.browser.utils.FileUtil;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.skin.SkinEngine;
import com.zpj.toast.ZToast;
import com.zpj.utils.FileUtils;
import com.zpj.utils.ScreenUtils;
import com.zpj.widget.toolbar.ZToolBar;

import net.lucode.hackware.magicindicator.MagicIndicator;
import net.lucode.hackware.magicindicator.ViewPagerHelper;
import net.lucode.hackware.magicindicator.buildins.commonnavigator.CommonNavigator;
import net.lucode.hackware.magicindicator.buildins.commonnavigator.abs.CommonNavigatorAdapter;
import net.lucode.hackware.magicindicator.buildins.commonnavigator.abs.IPagerIndicator;
import net.lucode.hackware.magicindicator.buildins.commonnavigator.abs.IPagerTitleView;
import net.lucode.hackware.magicindicator.buildins.commonnavigator.indicators.LinePagerIndicator;
import net.lucode.hackware.magicindicator.buildins.commonnavigator.titles.ColorTransitionPagerTitleView;

import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.R;
import org.chromium.components.offline_items_collection.ContentId;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

public class DownloadFragment extends BaseSwipeBackFragment {

    private static final String TAG = "DownloadFragment";

    private static final String[] TAB_TITLES = {"下载中", "已完成"};

    private ViewPager mViewPager;

    private boolean mDownloading = true;

    public static DownloadFragment newInstance(boolean downloading) {
        Bundle args = new Bundle();
        args.putBoolean(Keys.KEY_TYPE, downloading);
        DownloadFragment fragment = new DownloadFragment();
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (savedInstanceState == null) {
            if (getArguments() != null) {
                mDownloading = getArguments().getBoolean(Keys.KEY_TYPE, true);
            }
        } else {
            mDownloading = savedInstanceState.getBoolean(Keys.KEY_TYPE, true);
        }
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putBoolean(Keys.KEY_TYPE, mDownloading);
    }

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_download;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        this.mViewPager = findViewById(R.id.view_pager);
        ZToolBar toolBar = findViewById(R.id.tool_bar);
        toolBar.getRightImageButton().setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                ZDialog.attach()
                        .addItems("全部暂停", "全部开始", "多选", "下载设置")
                        .setOnSelectListener((fragment, position, title) -> {
                            ZToast.normal(title);
                            switch (position) {
                                case 0:
//										ZDownloader.pauseAll();
                                    break;
                                case 1:
//										ZDownloader.resumeAll();
                                    break;
                                case 2:
                                    break;
                                case 3:
                                    break;
                            }
                            fragment.dismiss();
                        })
                        .setAttachView(v)
                        .show(context);
            }
        });

        MagicIndicator magicIndicator = findViewById(R.id.magic_indicator);
        CommonNavigator navigator = new CommonNavigator(getContext());
        navigator.setAdjustMode(true);

        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(SkinEngine.getColor(context, R.attr.backgroundColorAccent));
        drawable.setCornerRadius(ScreenUtils.dp2px(12));
        magicIndicator.setBackground(drawable);

        navigator.setAdapter(new CommonNavigatorAdapter() {
            @Override
            public int getCount() {
                return TAB_TITLES.length;
            }

            @Override
            public IPagerTitleView getTitleView(Context context, int index) {
                ColorTransitionPagerTitleView titleView = new ColorTransitionPagerTitleView(context);
                titleView.setNormalColor(SkinEngine.getColor(context, R.attr.textColorMinor));
                titleView.setSelectedColor(SkinEngine.getColor(context, R.attr.textColorMajor));
                titleView.getPaint().setFakeBoldText(true);
                titleView.setText(TAB_TITLES[index]);
                titleView.setOnClickListener(view1 -> mViewPager.setCurrentItem(index));
                return titleView;
            }

            @Override
            public IPagerIndicator getIndicator(Context context) {
                LinePagerIndicator indicator = new LinePagerIndicator(context);
                float navigatorHeight = ScreenUtils.dp2px(48);
                float borderSize = ScreenUtils.dp2px(context, 5);
                float lineHeight = navigatorHeight - 2 * borderSize;
                indicator.setLineHeight(lineHeight);
                indicator.setRoundRadius(borderSize * 2);
                indicator.setXOffset(borderSize);
                indicator.setYOffset(borderSize);
                indicator.setColors(SkinEngine.getColor(context, R.attr.backgroundColor));
                return indicator;
            }
        });
        magicIndicator.setNavigator(navigator);

        final List<Fragment> fragments = new ArrayList<>();
        DownloadingFragment downloadingFragment = findChildFragment(DownloadingFragment.class);
        if (downloadingFragment == null) {
            downloadingFragment = new DownloadingFragment();
        }

        DownloadedFragment downloadedFragment = findChildFragment(DownloadedFragment.class);
        if (downloadedFragment == null) {
            downloadedFragment = new DownloadedFragment();
        }

        fragments.add(downloadingFragment);
        fragments.add(downloadedFragment);

        PageAdapter pageAdapter = new PageAdapter(getChildFragmentManager(), fragments, TAB_TITLES);
        mViewPager.setOffscreenPageLimit(fragments.size());

        mViewPager.setAdapter(pageAdapter);
        ViewPagerHelper.bind(magicIndicator, mViewPager);

        mViewPager.setCurrentItem(mDownloading ? 0 : 1);
    }

    public static class DownloadingFragment extends DownloadChildFragment {

        @Override
        public boolean isDownloading() {
            return true;
        }

        @Override
        List<FilterItem> getFilterItems() {
            List<FilterItem> items = new ArrayList<>();
            items.add(new FilterItem("全部") {
                @Override
                List<DownloadItem> filter(List<DownloadItem> items) {
                    return new ArrayList<>(items);
                }
            });
            items.add(new FileTypeFilterItem("视频", FileUtils.FileType.VIDEO));
            items.add(new FileTypeFilterItem("图片", FileUtils.FileType.IMAGE));
            items.add(new FileTypeFilterItem("文本", FileUtils.FileType.TXT));
            items.add(new FileTypeFilterItem("压缩包", FileUtils.FileType.ARCHIVE));
            items.add(new FileTypeFilterItem("安装包", FileUtils.FileType.APK));
            items.add(new FileTypeFilterItem("其它", FileUtils.FileType.UNKNOWN));
            return items;
        }

    }

    public static class DownloadedFragment extends DownloadChildFragment {

        @Override
        public boolean isDownloading() {
            return false;
        }

        @Override
        List<FilterItem> getFilterItems() {
            mFilterIndex = 1;
            List<FilterItem> items = new ArrayList<>();
            items.add(new FilterItem("全部") {
                @Override
                List<DownloadItem> filter(List<DownloadItem> items) {
                    return new ArrayList<>(items);
                }
            });
            items.add(new FilterItem("已完成") {
                @Override
                List<DownloadItem> filter(List<DownloadItem> items) {

                    List<DownloadItem> filterItems = new ArrayList<>();
                    for (DownloadItem item : items) {
                        if (!item.isRemoved()) {
                            filterItems.add(item);
                        }
                    }

                    return filterItems;
                }
            });
            items.add(new FilterItem("已移除") {
                @Override
                List<DownloadItem> filter(List<DownloadItem> items) {

                    List<DownloadItem> filterItems = new ArrayList<>();
                    for (DownloadItem item : items) {
                        if (item.isRemoved()) {
                            filterItems.add(item);
                        }
                    }

                    return filterItems;
                }
            });
            items.add(new FileTypeFilterItem("视频", FileUtils.FileType.VIDEO));
            items.add(new FileTypeFilterItem("图片", FileUtils.FileType.IMAGE));
            items.add(new FileTypeFilterItem("文本", FileUtils.FileType.TXT));
            items.add(new FileTypeFilterItem("压缩包", FileUtils.FileType.ARCHIVE));
            items.add(new FileTypeFilterItem("安装包", FileUtils.FileType.APK));
            items.add(new FileTypeFilterItem("其它", FileUtils.FileType.UNKNOWN));
            return items;
        }
    }

    private static class FileTypeFilterItem extends DownloadChildFragment.FilterItem {

        private final FileUtils.FileType mFileType;

        FileTypeFilterItem(String name, FileUtils.FileType fileType) {
            super(name);
            mFileType = fileType;
        }

        @Override
        List<DownloadItem> filter(List<DownloadItem> items) {
            List<DownloadItem> filterItems = new ArrayList<>();
            for (DownloadItem item : items) {
                String name = item.getDownloadInfo().getFileName();
                if (FileUtils.getFileType(name) == mFileType) {
                    filterItems.add(item);
                }
            }

            return filterItems;
        }
    }


}

