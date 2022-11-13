package com.ark.browser.settings;

import com.zpj.utils.PrefsHelper;

public final class AppConfig {

    private static final String KEY_NIGHT_MODE = "is_night_mode";
    private static final String KEY_PURE_MODE = "is_pure_mode";
    private static final String KEY_FULLSCREEN_MODE = "is_fullscreen_mode";
    private static final String KEY_SMART_NO_IMAGE_MODE = "is_smart_no_img_mode";
    private static final String KEY_INCOGNITO_MODE = "is_incognito_mode";
    private static final String KEY_SHOW_CONSOLE = "is_show_console";
    private static final String KEY_REFRESH_TIMING = "is_refresh_timing";
    private static final String KEY_EDIT_MODE = "is_edit_mode";

    private static final String KEY_AUTO_SAVE_TRAFFIC = "auto_save_traffic";
    private static final String KEY_SHOW_ORIGINAL_IMAGE = "show_original_image";
    private static final String KEY_COMPRESS_UPLOAD_IMAGE = "compress_upload_image";
    private static final String KEY_SHOW_SPLASH = "show_splash";
    private static final String KEY_SHOW_UPDATE_NOTIFICATION = "show_update_notification";
    private static final String KEY_SHOW_DOWNLOAD_NOTIFICATION = "show_download_notification";
    private static final String KEY_DOWNLOAD_PATH = "download_directory";
    private static final String KEY_MAX_DOWNLOAD_CONCURRENT_COUNT = "max_download_concurrent_count";
    private static final String KEY_MAX_DOWNLOAD_THREAD_COUNT = "max_download_thread_count";
    private static final String KEY_INSTALL_AFTER_DOWNLOADED = "install_after_download";
    private static final String KEY_AUTO_DELETE_AFTER_INSTALLED = "auto_delete_after_installed";
    private static final String KEY_ACCESSIBILITY_INSTALL = "accessibility_install";
    private static final String KEY_ROOT_INSTALL = "root_install";
    private static final String KEY_CHECK_SIGNATURE = "check_signature";

    private AppConfig() {

    }

    public static boolean isNightMode() {
        return PrefsHelper.with().getBoolean(KEY_NIGHT_MODE, false);
    }

    public static void toggleThemeMode() {
        PrefsHelper.with().putBoolean(KEY_NIGHT_MODE, !isNightMode());
    }

    public static boolean isPureMode() {
        return PrefsHelper.with().getBoolean(KEY_PURE_MODE, false);
    }

    public static void togglePureMode() {
        PrefsHelper.with().putBoolean(KEY_PURE_MODE, !isPureMode());
    }

    public static boolean isFullscreenMode() {
        return PrefsHelper.with().getBoolean(KEY_FULLSCREEN_MODE, false);
    }

    public static void toggleFullscreenMode() {
        PrefsHelper.with().putBoolean(KEY_FULLSCREEN_MODE, !isFullscreenMode());
    }

    public static boolean isSmartNoImageMode() {
        return PrefsHelper.with().getBoolean(KEY_SMART_NO_IMAGE_MODE, false);
    }

    public static void toggleSmartNoImageMode() {
        PrefsHelper.with().putBoolean(KEY_SMART_NO_IMAGE_MODE, !isSmartNoImageMode());
    }

    public static boolean isIncognitoMode() {
        return PrefsHelper.with().getBoolean(KEY_INCOGNITO_MODE, false);
    }

    public static void toggleIncognitoMode() {
        PrefsHelper.with().putBoolean(KEY_INCOGNITO_MODE, !isIncognitoMode());
    }

    public static boolean isShowConsole() {
        return PrefsHelper.with().getBoolean(KEY_SHOW_CONSOLE, false);
    }

    public static void toggleShowConsole() {
        PrefsHelper.with().putBoolean(KEY_SHOW_CONSOLE, !isShowConsole());
    }

    public static boolean isRefreshTiming() {
        return PrefsHelper.with().getBoolean(KEY_REFRESH_TIMING, false);
    }

    public static void toggleRefreshTiming() {
        PrefsHelper.with().putBoolean(KEY_REFRESH_TIMING, !isRefreshTiming());
    }

    public static boolean isEditMode() {
        return PrefsHelper.with().getBoolean(KEY_EDIT_MODE, false);
    }

    public static void toggleEditMode() {
        PrefsHelper.with().putBoolean(KEY_EDIT_MODE, !isEditMode());
    }

}

