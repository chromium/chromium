package com.ark.browser.utils;

import com.zpj.utils.FileUtils;

import org.chromium.chrome.R;

public class FileUtil {

    public static int getFileTypeIconId(String fileName) {
        FileUtils.FileType fileType = FileUtils.getFileType(fileName);
        if (fileType.equals(FileUtils.FileType.TORRENT)) {
            return R.drawable.wechat_icon_bt;
        } else if (fileType.equals(FileUtils.FileType.TXT)) {
            return R.drawable.wechat_icon_txt;
        } else if (fileType.equals(FileUtils.FileType.APK)) {
            return R.drawable.wechat_icon_apk;
        } else if (fileType.equals(FileUtils.FileType.PDF)) {
            return R.drawable.wechat_icon_pdf;
        } else if (fileType.equals(FileUtils.FileType.DOC)) {
            return R.drawable.wechat_icon_word;
        } else if (fileType.equals(FileUtils.FileType.PPT)) {
            return R.drawable.wechat_icon_ppt;
        } else if (fileType.equals(FileUtils.FileType.XLS)) {
            return R.drawable.wechat_icon_excel;
        } else if (fileType.equals(FileUtils.FileType.HTML)) {
            return R.drawable.wechat_icon_html;
        } else if (fileType.equals(FileUtils.FileType.SWF)) {
            return R.drawable.format_flash;
        } else if (fileType.equals(FileUtils.FileType.CHM)) {
            return R.drawable.format_chm;
        } else if (fileType.equals(FileUtils.FileType.IMAGE)) {
            return R.drawable.format_picture;
        } else if (fileType.equals(FileUtils.FileType.VIDEO)) {
            return R.drawable.format_media;
        } else if (fileType.equals(FileUtils.FileType.ARCHIVE)) {
            return R.drawable.wechat_icon_zip;
        } else if (fileType.equals(FileUtils.FileType.MUSIC)) {
            return R.drawable.wechat_icon_music;
        } else if (fileType.equals(FileUtils.FileType.EBOOK)) {
            return R.drawable.wechat_icon_txt;
        }
        return R.drawable.wechat_icon_others;
    }

}

