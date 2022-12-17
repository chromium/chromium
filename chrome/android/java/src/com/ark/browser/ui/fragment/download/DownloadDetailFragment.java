package com.ark.browser.ui.fragment.download;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;

import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.ark.browser.ui.widget.BlockGraphView;
import com.ark.browser.ui.widget.TitleTextView;
import com.ark.browser.utils.FileUtil;
import com.zpj.fragmentation.SupportActivity;
import com.zpj.utils.ContextUtils;
import com.zpj.widget.toolbar.ZToolBar;

import org.chromium.chrome.browser.download.DownloadInfo;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.R;

import java.util.Date;

public class DownloadDetailFragment extends BaseSwipeBackFragment {

    private DownloadItem downloadMission;
    private DownloadInfo downloadInfo;

    private ImageView fileIcon;
    private TextView taskTitle;
    private TextView fileSize;
    private TextView taskStatus;
    private TitleTextView downloadUrl;
    private TitleTextView oringinUrl;
    private TitleTextView downloadPath;
    private TitleTextView createTime;
    private TitleTextView threadCount;
    private TitleTextView breakpointResume;
    private TitleTextView userAgent;
    private TitleTextView cookie;
    private BlockGraphView mGraph;

    public static DownloadDetailFragment newInstance(DownloadItem mission) {
        DownloadDetailFragment fragment = new DownloadDetailFragment();
        fragment.downloadMission = mission;
        return fragment;
    }

    public DownloadDetailFragment show(Context context) {
        Activity activity = ContextUtils.getActivity(context);
        if (activity instanceof SupportActivity) {
            ((SupportActivity) activity).start(this);
        } else if (activity instanceof FragmentActivity) {
            FragmentManager manager = ((FragmentActivity) activity).getSupportFragmentManager();
            FragmentTransaction ft = manager.beginTransaction();
            ft.add(this, "tag");
            ft.commit();
        } else {
            throw new RuntimeException("the context is not a FragmentActivity object!");
        }
        return this;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (downloadMission == null) {
            popThis();
            return;
        }
        this.downloadInfo = downloadMission.getDownloadInfo();
    }

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_download_detail;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        ZToolBar toolBar = findViewById(R.id.tool_bar);
        toolBar.setCenterText("任务详情");
        fileIcon = view.findViewById(R.id.icon_file);
        taskTitle = view.findViewById(R.id.task_title);
        fileSize = view.findViewById(R.id.file_size);
        taskStatus = view.findViewById(R.id.task_status);

        fileIcon.setImageResource(FileUtil.getFileTypeIconId(downloadInfo.getFileName()));
        taskTitle.setText(downloadInfo.getFileName());
        fileSize.setText(downloadInfo.getBytesReceived() + "");
        taskStatus.setText(downloadInfo.state() + "");

        mGraph = view.findViewById(R.id.info_graph);
        mGraph.setMission(downloadMission);
        mGraph.invalidate();

        downloadUrl = view.findViewById(R.id.download_url);
        oringinUrl = view.findViewById(R.id.oringin_url);
        downloadPath = view.findViewById(R.id.download_path);
        createTime = view.findViewById(R.id.create_time);
        threadCount = view.findViewById(R.id.thread_count);
        breakpointResume = view.findViewById(R.id.breakpoint_resume);
        userAgent = view.findViewById(R.id.user_agent);
        cookie = view.findViewById(R.id.cookie);

        downloadUrl.setText(downloadInfo.getUrl().getSpec());
        if (TextUtils.isEmpty(downloadInfo.getOriginalUrl())) {
            oringinUrl.setVisibility(View.GONE);
        } else {
            oringinUrl.setText(downloadInfo.getOriginalUrl());
        }
        downloadPath.setText(downloadInfo.getFilePath());
        createTime.setText(new Date(downloadMission.getStartTime()).toString());
        threadCount.setText("3");
        breakpointResume.setText(downloadInfo.isResumable() ? "不支持" : "支持");
        userAgent.setText(downloadInfo.getUserAgent());
        String cookieStr = downloadInfo.getCookie();
        cookie.setText(TextUtils.isEmpty(cookieStr) ? "空" : cookieStr);
    }

}

