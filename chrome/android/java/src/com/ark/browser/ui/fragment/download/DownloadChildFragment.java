package com.ark.browser.ui.fragment.download;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.text.TextUtils;
import android.text.format.Formatter;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.core.content.FileProvider;
import androidx.recyclerview.widget.LinearLayoutManager;

import com.ark.browser.ui.fragment.base.SkinFragment;
import com.ark.browser.ui.widget.AnimProgressBar;
import com.ark.browser.utils.FileUtil;
import com.ark.browser.utils.ThreadPool;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.recyclerview.EasyRecycler;
import com.zpj.recyclerview.EasyViewHolder;
import com.zpj.recyclerview.IEasy;
import com.zpj.recyclerview.decoration.ShadowItemDecoration;
import com.zpj.skin.SkinEngine;
import com.zpj.toast.ZToast;
import com.zpj.utils.ClickHelper;
import com.zpj.utils.FileUtils;

import org.chromium.base.Log;
import org.chromium.chrome.browser.download.DownloadInfo;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.ui.base.Clipboard;
import org.chromium.chrome.R;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

public abstract class DownloadChildFragment extends SkinFragment
        implements IEasy.OnBindViewHolderListener<DownloadItem>,
        DownloadManagerService.DownloadObserver {

    private static final String TAG = "DownloadChildFragment";

    private final List<DownloadItem> mAllDownloadItems = new ArrayList<>();
    private final List<DownloadItem> downloadMissionList = new ArrayList<>();

    private EasyRecycler<DownloadItem> mRecycler;
    private DownloadManagerService downloadManagerService;

    private EasyRecycler<FilterItem> mFilterRecycler;

    protected int mFilterIndex = 0;

    static abstract class FilterItem {

        String name;

        int count;

        FilterItem(String name) {
            this.name = name;
        }

        abstract List<DownloadItem> filter(List<DownloadItem> items);

    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        downloadManagerService = DownloadManagerService.getDownloadManagerService();
    }

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_download_child;
    }

    public static void openFile(Context context, File file) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
//        intent.addCategory(Intent.CATEGORY_OPENABLE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
//            intent.setFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
//            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
            Uri contentUri = FileProvider.getUriForFile(context, FileUtils.getFileProviderName(context), file);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
//            StrictMode.setThreadPolicy(oldPolicy);

//            context.grantUriPermission(context.getPackageName(), contentUri, Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            intent.setDataAndTypeAndNormalize(contentUri, FileUtils.getMIMEType(file));
        } else {
            Uri uri = Uri.fromFile(file);
            intent.setDataAndType(uri, FileUtils.getMIMEType(file));
        }

        context.startActivity(intent);
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        ImageView ivExpand = findViewById(R.id.iv_expand);
        ivExpand.setOnClickListener(v -> {
            List<String> titles = new ArrayList<>();
            for (FilterItem item : mFilterRecycler.getItems()) {
                titles.add(item.name);
            }
            int oldIndex = mFilterIndex;
            new DownloadFilterDialog()
                    .addItems(titles)
                    .setSelectedItem(oldIndex)
                    .setOnItemClickListener((view12, title, position) -> {
                        mFilterIndex = position;
                        mFilterRecycler.notifyItemChanged(oldIndex);
                        mFilterRecycler.notifyItemChanged(mFilterIndex);
                        mFilterRecycler.scrollToPosition(mFilterIndex);
                        filterDownloadItems(mFilterIndex);
                    })
                    .setOnDismissListener(dialog -> {
                        ivExpand.animate()
                                .rotation(0)
                                .setDuration(250)
                                .start();
                        mFilterRecycler.getRecyclerView().animate().alpha(1f).setDuration(250).start();
                    })
                    .show(v);

            ivExpand.animate()
                    .rotation(90)
                    .setDuration(250)
                    .start();
            mFilterRecycler.getRecyclerView().animate().alpha(0f).setDuration(250).start();

        });

        mFilterRecycler = new EasyRecycler<>(findViewById(R.id.rv_filter));
        mFilterRecycler.setItemRes(R.layout.item_download_filter)
                .setLayoutManager(new LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false))
                .setItems(getFilterItems())
                .onBindViewHolder((holder, list, position, payloads) -> {
                    FilterItem item = list.get(position);
                    if (position == mFilterIndex) {
                        holder.setText(R.id.tv_title, item.name + " | " + item.count);
                        holder.setTextColor(R.id.tv_title, Color.WHITE);
                        holder.setBackgroundResource(R.id.tv_title, R.drawable.grey_shape_select);
                    } else {
                        holder.setText(R.id.tv_title, item.name);
                        holder.setTextColor(R.id.tv_title, SkinEngine.getColor(context, R.attr.textColorNormal));
                        holder.setBackgroundResource(R.id.tv_title, R.drawable.grey_shape2);
                    }
                })
                .setLoadMoreEnabled(false)
                .onItemClick((holder, v, item) -> {
                    if (mFilterIndex == holder.getRealPosition()) {
                        return;
                    }
                    mFilterIndex = holder.getRealPosition();
                    mFilterRecycler.notifyDataSetChanged();
                    filterDownloadItems(mFilterIndex);
                })
                .build();


        mRecycler = new EasyRecycler<>(findViewById(R.id.recycler_view), downloadMissionList);
        mRecycler.setItemRes(R.layout.item_download)
                .onBindViewHolder(this)
                .addItemDecoration(new ShadowItemDecoration())
                .setLoadMoreEnabled(false)
                .onItemClick((holder, view1, item) -> {
                    if (item.isComplete()) {
                        File file = new File(item.getDownloadInfo().getFilePath());
                        ZToast.success("mimetype=" + FileUtils.getMIMEType(file));
                        if (file.exists()) {
                            openFile(context, file);
                        } else {
                            DownloadDetailFragment.newInstance(item).show(context);
                        }
                    } else {
                        DownloadDetailFragment.newInstance(item).show(context);
                    }
                })
                .build();
        mRecycler.showLoading();
        downloadManagerService.addDownloadObserver(this);
        downloadManagerService.getAllDownloads(null);
    }

    @Override
    public void onDestroyView() {
        downloadManagerService.removeDownloadObserver(this);
        super.onDestroyView();
    }

    public void refresh() {
        mRecycler.notifyDataSetChanged();
    }

    public abstract boolean isDownloading();

    @Override
    public void onBindViewHolder(EasyViewHolder holder, List<DownloadItem> list, int position, List<Object> payloads) {
        DownloadItem mission = list.get(position);
        DownloadInfo info = mission.getDownloadInfo();

        ClickHelper.with(holder.getItemView())
                .setOnLongClickListener((v, x, y) -> {
                    buildPopup(v, holder, mission, x, y);
                    return true;
                });

        for (Object object : payloads) {
            if ("update".equals(object)) {
                updateStatus(holder, mission);

                return;
            }
        }

        holder.setText(R.id.tv_name, info.getFileName());

        ImageView downloadBtn = holder.getView(R.id.btn_action);
        if (isDownloading()) {
            holder.setImageResource(R.id.iv_icon, FileUtil.getFileTypeIconId(info.getFileName()));
//            holder.setText(R.id.item_size, Formatter.formatFileSize(getContext(), info.getBytesReceived()));
            if (info.getProgress().isIndeterminate()) {
                holder.setText(R.id.tv_size, Formatter.formatFileSize(context, info.getBytesReceived()));
            } else {
                holder.setText(R.id.tv_size, Formatter.formatFileSize(context, info.getBytesReceived()) + " / " + Formatter.formatFileSize(context, info.getBytesTotalSize()));
            }
            downloadBtn.setTag(!info.isPaused());
        } else {
            ImageView ivIcon = holder.getView(R.id.iv_icon);
            if (info.getFileName().toLowerCase().endsWith(".apk")) {
                PackageManager pm = context.getPackageManager();
                PackageInfo pkgInfo = pm.getPackageArchiveInfo(info.getFilePath(), PackageManager.GET_ACTIVITIES);
                if (pkgInfo != null) {
                    ApplicationInfo appInfo = pkgInfo.applicationInfo;
                    appInfo.sourceDir = info.getFilePath();
                    appInfo.publicSourceDir = info.getFilePath();
//                        Drawable icon = pm.getApplicationIcon(appInfo);// 得到图标信息
                    Drawable icon = appInfo.loadIcon(pm);
                    ivIcon.setImageDrawable(icon);
                } else {
                    ivIcon.setImageResource(FileUtil.getFileTypeIconId(info.getFileName()));
                }
            } else {
                ivIcon.setImageResource(FileUtil.getFileTypeIconId(info.getFileName()));
            }
            TextView tvSize = holder.getView(R.id.tv_size);
            TextView tvName = holder.getView(R.id.tv_name);
            if (new File(info.getFilePath()).exists()) {
                tvSize.setText(Formatter.formatFileSize(getContext(), info.getBytesReceived()));
                tvSize.setTextColor(SkinEngine.getColor(context, R.attr.textColorMinor));
                ivIcon.setAlpha(1f);
                tvName.setTextColor(SkinEngine.getColor(context, R.attr.textColorMajor));
            } else {
                tvSize.setText("文件已移除");
                tvSize.setTextColor(context.getResources().getColor(R.color.red5));
                ivIcon.setAlpha(0.5f);
                tvName.setTextColor(SkinEngine.getColor(context, R.attr.textColorMinor));
            }
        }
        downloadBtn.setVisibility(View.VISIBLE);
        downloadBtn.setOnClickListener(v -> {
            Log.d("onClickonClickonClick", "canPause=" + mission.canPause() + " canStart=" + mission.canResume() + " isPaused=" + mission.getDownloadInfo().isPaused() + " isRunning=" + mission.isDownloading());
            if (mission.canPause()) {
                downloadManagerService.pauseDownload(mission.getContentId(), null);
                downloadBtn.setImageResource(R.drawable.download_item_resume_icon_style2);
            } else if (mission.canResume()) {
                downloadManagerService.resumeDownload(mission.getContentId(), mission, false);
                downloadBtn.setImageResource(R.drawable.download_item_pause_icon_style2);
            }
//                updateStatus(holder, mission);
        });
        updateStatus(holder, mission);
    }

    private void updateStatus(EasyViewHolder holder, DownloadItem item) {
        if (item == null) return;
        ImageView downloadBtn = holder.getView(R.id.btn_action);
        TextView status = holder.getTextView(R.id.tv_status);
        AnimProgressBar progressBar = holder.getView(R.id.progress_bar);

        Log.d(TAG, "updateStatus downloading=" + isDownloading() + " item=" + item);

        DownloadInfo info = item.getDownloadInfo();
        OfflineItem.Progress progress = info.getProgress();
        if (item.isDownloading()) {
            progressBar.setVisibility(View.VISIBLE);
            if (progress.isIndeterminate()) {
                progressBar.setProgress(100, true);
                status.setText("下载中...");
                holder.setText(R.id.tv_size, Formatter.formatFileSize(context, info.getBytesReceived()));
            } else {
                progressBar.setProgress(progress.getPercentage(), true);
                status.setText(progress.getPercentage() + "%");
                holder.setText(R.id.tv_size, Formatter.formatFileSize(context, info.getBytesReceived()) + " / " + Formatter.formatFileSize(context, info.getBytesTotalSize()));
            }
            status.setText(progress.getPercentage() + "%");
            downloadBtn.setVisibility(View.VISIBLE);
            downloadBtn.setImageResource(R.drawable.download_item_pause_icon_style2);
        } else if (item.isComplete()) {
            progressBar.setVisibility(View.GONE);
            status.setText("已完成");
            status.setVisibility(View.GONE);
            downloadBtn.setVisibility(View.GONE);
        } else { // if (mission.isPaused())
            progressBar.setVisibility(View.VISIBLE);
            progressBar.setProgress(progress.getPercentage(), false);
            status.setText("已暂停");
            downloadBtn.setVisibility(View.VISIBLE);
            downloadBtn.setImageResource(R.drawable.download_item_resume_icon_style2);
        }
    }

    private void buildPopup(View view, final EasyViewHolder h, DownloadItem mission, float x, float y) {
        DownloadInfo info = mission.getDownloadInfo();
        ArrayList<String> titleList = new ArrayList<>();
        if (!mission.isComplete()) {
            if (!mission.isDownloading()) {
//                if (mission.getErrCode() == -1) {
//                    titleList.add("开始");
//                }
                titleList.add("删除");
            } else {
                titleList.add("暂停");
            }
            titleList.add("复制链接");
            titleList.add("任务详情");
        } else {
            titleList.add("打开");
            titleList.add("重命名");
            titleList.add("删除");
            titleList.add("复制链接");
            titleList.add("任务详情");
            titleList.add("分享");
        }

        ZDialog.attach()
                .addItems(titleList)
                .setOnSelectListener((fragment, position, title) -> {
                    switch (title) {
                        case "开始":
                            downloadManagerService.resumeDownload(mission.getContentId(), mission, false);
                            h.getImageView(R.id.btn_action).setImageResource(R.drawable.download_item_pause_icon_style2);
                            break;
                        case "暂停":
                            downloadManagerService.pauseDownload(mission.getContentId(), null);
                            h.getImageView(R.id.btn_action).setImageResource(R.drawable.download_item_resume_icon_style2);
                            break;
                        case "打开":
                            FileUtils.openFile(context, new File(mission.getDownloadInfo().getFilePath()));
                            break;
                        case "重命名":
                            ZToast.normal("重命名");
                            break;
                        case "删除":
                            ZDialog.check()
                                    .setChecked(true)
                                    .setCheckTitle("删除已下载的文件")
                                    .setTitle("确定删除？")
                                    .setContent("你将删除下载任务：" + info.getFileName())
                                    .setPositiveButton((fragment1, which) -> {
                                        mission.setHasBeenExternallyRemoved(fragment1.isChecked());
                                        downloadManagerService.removeDownload(mission.getId(), null, fragment1.isChecked());
                                        if (mission.isComplete() && fragment1.isChecked()) {
                                            FileUtils.deleteFile(mission.getDownloadInfo().getFilePath());
                                        }
                                    })
                                    .show(context);
                            break;
                        case "复制链接":
                            Clipboard.getInstance().setTextFromUser(info.getOriginalUrl());
                            break;
                        case "任务详情":
                            DownloadDetailFragment.newInstance(mission).show(context);
                            break;
                        case "分享":
                            ZToast.normal("分享");
                            break;
                        case "检验":
                            ZToast.normal("检验");
                            break;
                    }
                    fragment.dismiss();
                })
                .setTouchPoint(x, y)
                .show(context);
    }

    @Override
    public void onAllDownloadsRetrieved(List<DownloadItem> list, ProfileKey profileKey) {
        postOnLazyInit(() -> {
            mAllDownloadItems.clear();
            for (DownloadItem item : list) {
                if (item.isComplete() != isDownloading()) {
                    mAllDownloadItems.add(item);
                }
            }

            filterDownloadItems(mFilterIndex);
        });
    }

    @Override
    public void onDownloadItemFinished(DownloadItem item) {
        Log.d(TAG, "onDownloadItemFinished item=" + item);
        if (isDownloading()) {
            for (int i = 0; i < mAllDownloadItems.size(); i++) {
                if (item.getId().equals(mAllDownloadItems.get(i).getId())) {
                    mAllDownloadItems.remove(i);
                    return;
                }
            }
            for (int i = 0; i < downloadMissionList.size(); i++) {
                if (item.getId().equals(downloadMissionList.get(i).getId())) {
                    downloadMissionList.remove(i);
                    mRecycler.notifyItemRemoved(i);
                    return;
                }
            }
        } else {
            for (DownloadItem downloadItem : mAllDownloadItems) {
                if (TextUtils.equals(item.getId(), downloadItem.getId())) {
                    return;
                }
            }
            mAllDownloadItems.add(0, item);
            if (mFilterIndex == 0 || mFilterIndex == 1) {
                downloadMissionList.add(0, item);
                mRecycler.notifyItemInserted(0);
            } else {
                downloadMissionList.clear();
                downloadMissionList.addAll(mFilterRecycler.getItemAt(mFilterIndex).filter(mAllDownloadItems));
                mRecycler.notifyDataSetChanged();
            }
        }

    }

    @Override
    public void onDownloadItemUpdated(DownloadItem item) {
        if (isDownloading()) {
            int i = 0;
            for (DownloadItem downloadItem : downloadMissionList) {
                if (item.getId().equals(downloadItem.getId())) {
                    if (item.getDownloadInfo().isPaused() == downloadItem.getDownloadInfo().isPaused()
                            && item.getDownloadInfo().state() == downloadItem.getDownloadInfo().state()
                            && item.getDownloadInfo().getBytesReceived() == downloadItem.getDownloadInfo().getBytesReceived()) {
                        return;
                    }
                    boolean isPartUpdate = TextUtils.equals(item.getDownloadInfo().getFileName(), downloadItem.getDownloadInfo().getFileName());
                    downloadItem.setDownloadInfo(item.getDownloadInfo());


                    if (isPartUpdate) {
                        mRecycler.notifyItemChanged(i, "update");
                    } else {
                        mRecycler.notifyItemChanged(i);
                    }
                    return;
                }
                i++;
            }
        }
    }

    @Override
    public void onDownloadItemCreated(DownloadItem item) {
        if (isDownloading()) {
            mAllDownloadItems.add(0, item);
            // TODO filter
            if (mFilterIndex == 0) {
                downloadMissionList.add(0, item);
                mRecycler.notifyItemInserted(0);
            }
        }
    }

    @Override
    public void onDownloadItemRemoved(String guid) {
        int i = 0;
        for (DownloadItem downloadItem : mAllDownloadItems) {
            if (TextUtils.equals(guid, downloadItem.getId())) {
                mAllDownloadItems.remove(downloadItem);
                break;
            }
            i++;
        }
        i = 0;
        for (DownloadItem downloadItem : downloadMissionList) {
            if (TextUtils.equals(guid, downloadItem.getId())) {
                downloadMissionList.remove(downloadItem);
                mRecycler.notifyItemRemoved(i);
                break;
            }
            i++;
        }
    }

    @Override
    public void onAddOrReplaceDownloadSharedPreferenceEntry(ContentId id) {

    }

    private void filterDownloadItems(int index) {
        FilterItem item = mFilterRecycler.getItemAt(index);
        ThreadPool.execute(() -> {
            List<DownloadItem> filterItems = item.filter(mAllDownloadItems);
            mRecycler.post(() -> {
                downloadMissionList.clear();
                downloadMissionList.addAll(filterItems);
                if (downloadMissionList.isEmpty()) {
                    mRecycler.showEmpty();
                } else {
                    mRecycler.showContent();
                }

                mFilterRecycler.getItemAt(index).count = downloadMissionList.size();
                mFilterRecycler.post(() -> mFilterRecycler.notifyItemChanged(index));
            });
        });
    }

    abstract List<FilterItem> getFilterItems();

}

