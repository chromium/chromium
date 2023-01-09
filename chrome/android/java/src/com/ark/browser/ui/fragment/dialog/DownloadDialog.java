package com.ark.browser.ui.fragment.dialog;

import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.SeekBar;
import android.widget.TextView;

import androidx.annotation.Nullable;

import com.ark.browser.utils.FileUtil;
import com.zpj.fragmentation.dialog.base.OverDragBottomDialogFragment;
import com.zpj.skin.SkinEngine;
import com.zpj.toast.ZToast;
import com.zpj.utils.PrefsHelper;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadDialogBridge;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.ui.base.Clipboard;

import java.io.File;

public class DownloadDialog extends OverDragBottomDialogFragment<DownloadDialog> {

    private static final String TAG = "cr_DownloadDialog";


    //    private WindowAndroid windowAndroid;
    private boolean flag = false;

    private String fileName;
    private String url;
    private String fileSize;
    private String downloadPath;

    private boolean isDismissed = false;

    private DownloadDialogBridge downloadDialogBridge;


//    public static DownloadDialog create(WindowAndroid windowAndroid, String fileName, String url, long fileSize, String downloadPath) {
//        DownloadDialog popup = new DownloadDialog();
//        popup.activity = windowAndroid.getActivity().get();
//        popup.setFileName(fileName)
//                .setUrl(url)
//                .setFileSize(FileUtils.formatFileSize(fileSize))
//                .setDownloadPath(downloadPath);
//        return popup;
//    }

    @Override
    protected int getImplLayoutId() {
        return R.layout.fragment_dialog_download;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (downloadDialogBridge == null) {
            popThis();
        }
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {

        if (downloadDialogBridge == null) {
            popThis();
            return;
        }

        super.initView(view, savedInstanceState);
        ImageView fileIcon = findViewById(R.id.file_icon);
        TextView fileSizeTextView = findViewById(R.id.file_size);
        final EditText etName = findViewById(R.id.et_file_name);
        EditText etLink = findViewById(R.id.et_link);
        TextView copyLink = findViewById(R.id.copy_link);
        final TextView tCount = findViewById(R.id.threads_count);
        final SeekBar threads = findViewById(R.id.threads);

        final TextView ok = findViewById(R.id.tv_ok);
        final TextView cancel = findViewById(R.id.tv_cancel);

        SkinEngine.setTextColor(cancel, R.attr.textColorMajor);
        SkinEngine.setTextColor(ok, R.attr.colorPrimary);

//        fileSizeTextView.setText(Utility.formatSize(contentLength));
        fileSizeTextView.setText(fileSize);
        copyLink.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Clipboard.getInstance().setTextFromUser(url);
                ZToast.success("已复制到剪贴板");
            }
        });

        etName.setText(fileName);
        fileIcon.setImageResource(FileUtil.getFileTypeIconId(etName.getText().toString()));
        etLink.setText(url);


        threads.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {

            @Override
            public void onProgressChanged(SeekBar seekbar, int progress, boolean fromUser) {
                tCount.setText("下载线程: " + (progress + 1));
            }

            @Override
            public void onStartTrackingTouch(SeekBar p1) {

            }

            @Override
            public void onStopTrackingTouch(SeekBar p1) {

            }

        });

        int def = PrefsHelper.with().getInt("threads", 4);
        threads.setProgress(def - 1);
        tCount.setText("下载线程: " + def);

        cancel.setOnClickListener(v -> {
            flag = false;
            dismiss();
        });

        ok.setOnClickListener(v -> {
            flag = true;
            File targetFile = new File(downloadPath);
            fileName = etName.getText().toString();


            if (TextUtils.isEmpty(fileName)) {
                fileName = targetFile.getName();
            }
            File parentFile = targetFile.getParentFile();
            if (parentFile == null) {
                parentFile = new File(context.getExternalCacheDir(), "download");
            }
            if (!parentFile.exists()) {
                parentFile.mkdirs();
            }

            if (targetFile.exists()) {
                String parent = parentFile.getAbsolutePath();
                int i = fileName.lastIndexOf('.');
                if (i < 0) {
                    downloadPath = generateNewFilePath(parent, fileName, null);
                } else {
                    downloadPath = generateNewFilePath(parent, fileName.substring(0, i), fileName.substring(i));
                }
            }
            dismiss();
        });
    }

    @Override
    protected void onDismiss() {
        if (isDismissed) {
            return;
        }
        isDismissed = true;
        super.onDismiss();
        if (flag) {
            DownloadManagerService.getDownloadManagerService();
            downloadDialogBridge.onComplete(downloadPath);
//            nativeAccept(nativeDownloadDialog, fileName, Environment.getExternalStorageDirectory().getAbsolutePath() + "/QXBrowser/download/" + fileName);
        } else {
            downloadDialogBridge.onCancel();
        }
        Log.d(TAG, "onDismiss flag=" + flag);
    }

    public DownloadDialog setFileName(String fileName) {
        this.fileName = fileName;
        return this;
    }

    public DownloadDialog setUrl(String url) {
        this.url = url;
        return this;
    }

    public DownloadDialog setDownloadPath(String downloadPath) {
        this.downloadPath = downloadPath;
        return this;
    }

    public DownloadDialog setFileSize(String fileSize) {
        this.fileSize = fileSize;
        return this;
    }

    public DownloadDialog setDownloadDialogBridge(DownloadDialogBridge downloadDialogBridge) {
        this.downloadDialogBridge = downloadDialogBridge;
        return this;
    }

    @Override
    protected void onCancel() {
        super.onCancel();
        downloadDialogBridge.onCancel();
    }



    private static String generateNewFilePath(String parent, String name, @Nullable String suffix) {
        if (suffix == null) {
            suffix = "";
        }
        int index = name.length() - 1;
        char last = name.charAt(index);
        String newName = name;
        int num = 1;
        if (last == ')') {
            while (--index >= 0) {
                last = name.charAt(index);
                if (last == '(') {
                    try {
                        num = Integer.parseInt(name.substring(index + 1, name.length() - 1)) + 1;
                        newName = name.substring(0, index);
                    } catch (Exception ignore) {
                    }
                } else if (!Character.isDigit(last)) {
                    break;
                }
            }
        }

        File newFile;
        do {
            String newNameWithSuffix = String.format("%s(%s)%s", newName, num++, suffix);
            newFile = new File(parent, newNameWithSuffix);
        } while (newFile.exists());
        return newFile.getAbsolutePath();
    }


}

