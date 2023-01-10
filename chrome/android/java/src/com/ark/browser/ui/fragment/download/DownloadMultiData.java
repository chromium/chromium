package com.ark.browser.ui.fragment.download;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.graphics.drawable.Drawable;
import android.text.format.Formatter;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import com.ark.browser.ui.widget.AnimProgressBar;
import com.ark.browser.utils.FileUtil;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.recyclerview.EasyViewHolder;
import com.zpj.recyclerview.SingleTypeMultiData;
import com.zpj.recyclerview.layouter.VerticalLayouter;
import com.zpj.skin.SkinEngine;
import com.zpj.statemanager.State;
import com.zpj.toast.ZToast;
import com.zpj.utils.ClickHelper;
import com.zpj.utils.FileUtils;

import org.chromium.base.Log;
import org.chromium.chrome.browser.download.DownloadInfo;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.ui.base.Clipboard;
import org.chromium.chrome.R;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

public class DownloadMultiData extends SingleTypeMultiData<DownloadItem> {

    private final DownloadManagerService downloadManagerService = DownloadManagerService.getDownloadManagerService();

    public DownloadMultiData() {
        super(new VerticalLayouter());
    }

    public void setData(List<DownloadItem> dataSet) {
        this.mData.clear();
        this.mData.addAll(dataSet);
        if (this.mData.isEmpty()) {
            setState(State.STATE_EMPTY);
        } else {
            setState(State.STATE_CONTENT);
        }
    }

    @Override
    public int getLayoutId() {
        return R.layout.item_download;
    }

    @Override
    public boolean loadData() {
        return false;
    }

    @Override
    public void onBindViewHolder(EasyViewHolder holder, List<DownloadItem> list, int position, List<Object> payloads) {
        Context context = holder.getContext();
        DownloadItem mission = list.get(position);
        DownloadInfo info = mission.getDownloadInfo();

        ClickHelper.with(holder.getItemView())
                .setOnClickListener((v, x, y) -> {
                    if (mission.isComplete()) {
                        File file = new File(info.getFilePath());
                        ZToast.success("mimetype=" + FileUtils.getMIMEType(file));
                        if (file.exists()) {
                            FileUtils.openFile(context, file);
//                            openFile(context, file);
                        } else {
                            // TODO
//                            DownloadDetailFragment.newInstance(mission).show(context);
                        }
                    } else {
                        // TODO
//                        DownloadDetailFragment.newInstance(mission).show(context);
                    }
                })
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

        if (mission.isComplete()) {
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
                tvSize.setText(Formatter.formatFileSize(context, info.getBytesReceived()));
                tvSize.setTextColor(SkinEngine.getColor(context, R.attr.textColorMinor));
                ivIcon.setAlpha(1f);
                tvName.setTextColor(SkinEngine.getColor(context, R.attr.textColorMajor));
            } else {
                tvSize.setText("文件已移除");
                tvSize.setTextColor(context.getResources().getColor(R.color.red5));
                ivIcon.setAlpha(0.5f);
                tvName.setTextColor(SkinEngine.getColor(context, R.attr.textColorMinor));
            }
        } else {
            holder.setImageResource(R.id.iv_icon, FileUtil.getFileTypeIconId(info.getFileName()));
//            holder.setText(R.id.item_size, Formatter.formatFileSize(getContext(), info.getBytesReceived()));
            if (info.getProgress().isIndeterminate()) {
                holder.setText(R.id.tv_size, Formatter.formatFileSize(context, info.getBytesReceived()));
            } else {
                holder.setText(R.id.tv_size, Formatter.formatFileSize(context, info.getBytesReceived()) + " / " + Formatter.formatFileSize(context, info.getBytesTotalSize()));
            }
            downloadBtn.setTag(!info.isPaused());
        }
        downloadBtn.setVisibility(View.VISIBLE);
        downloadBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Log.d("onClickonClickonClick", "canPause=" + mission.canPause() + " canResume=" + mission.canResume() + " isPaused=" + mission.getDownloadInfo().isPaused() + " isDownloading=" + mission.isDownloading());
                if (mission.canPause()) {
                    downloadManagerService.pauseDownload(mission.getContentId(), null);
                    downloadBtn.setImageResource(R.drawable.download_item_resume_icon_style2);
                } else if (mission.canResume()) {
                    downloadManagerService.resumeDownload(mission.getContentId(), mission, false);
                    downloadBtn.setImageResource(R.drawable.download_item_pause_icon_style2);
                }
//                updateStatus(holder, mission);
            }
        });
        updateStatus(holder, mission);
    }

    private void updateStatus(EasyViewHolder holder, DownloadItem item) {
        DownloadInfo info = item.getDownloadInfo();
        Context context = holder.getContext();
        ImageView downloadBtn = holder.getView(R.id.btn_action);
        TextView status = holder.getTextView(R.id.tv_status);
        AnimProgressBar progressBar = holder.getView(R.id.progress_bar);
        Log.d("DownloadChildFragment", "updateStatus isFinished=" + item.isComplete() + " info=" + info);
        if (item.isDownloading()) {
            progressBar.setVisibility(View.VISIBLE);
            if (info.getProgress().isIndeterminate()) {
                progressBar.setProgress(100, true);
                holder.setText(R.id.tv_size, Formatter.formatFileSize(context, info.getBytesReceived()));
            } else {
                progressBar.setProgress(info.getProgress().getPercentage(), true);
                holder.setText(R.id.tv_size, Formatter.formatFileSize(context, info.getBytesReceived()) + " / " + Formatter.formatFileSize(context, info.getBytesTotalSize()));
            }
            status.setText(info.getProgress().getPercentage() + "%");
            downloadBtn.setVisibility(View.VISIBLE);
            downloadBtn.setImageResource(R.drawable.download_item_pause_icon_style2);
        } else if (item.isComplete()) {
            progressBar.setVisibility(View.GONE);
            status.setText("已完成");
            status.setVisibility(View.GONE);
            downloadBtn.setVisibility(View.GONE);
        } else { // if (mission.isPaused())
            progressBar.setVisibility(View.VISIBLE);
            progressBar.setProgress(info.getProgress().getPercentage(), false);
            status.setText("已暂停");
            downloadBtn.setVisibility(View.VISIBLE);
            downloadBtn.setImageResource(R.drawable.download_item_resume_icon_style2);
        }
    }

    private void buildPopup(View view, final EasyViewHolder h, DownloadItem mission, float x, float y) {
        Context context = view.getContext();
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
//                            mission.start();
                            downloadManagerService.resumeDownload(mission.getContentId(), mission, false);
                            h.getImageView(R.id.btn_action).setImageResource(R.drawable.download_item_pause_icon_style2);
                            break;
                        case "暂停":
//                            mission.pause();
                            downloadManagerService.pauseDownload(mission.getContentId(), null);
                            h.getImageView(R.id.btn_action).setImageResource(R.drawable.download_item_resume_icon_style2);
                            break;
                        case "打开":
//                            mission.openFile(getContext());
                            FileUtils.openFile(context, new File(mission.getDownloadInfo().getFilePath()));
                            break;
                        case "重命名":
//                            ZEditDialog.with(getContext())
//                                    .setTitle("重命名")
//                                    .setEditText(mission.getTaskName())
//                                    .setEmptyable(false)
//                                    .setHint("请输入文件名")
//                                    .setPositiveButton((dialog, text) -> {
//                                        dialog.dismiss();
//                                        ZDownloader.rename(mission, text);
//                                        recyclerLayout.notifyItemChanged(h.getAdapterPosition());
//                                    })
//                                    .show();
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
                            // TODO
//                            DownloadDetailFragment.newInstance(mission).show(context);
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

}

