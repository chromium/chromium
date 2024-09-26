// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.ActivityManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;
import android.util.TypedValue;
import android.view.Display;
import android.view.Surface;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.TextView;

import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;

import java.util.ArrayList;

/** Contains utility methods for interacting with WebAPKs. */
public class WebApkUtils {
    private static final String TAG = "cr_WebApkUtils";
    private static final float CONTRAST_LIGHT_ITEM_THRESHOLD = 3f;

    /** Returns whether the application is installed and enabled. */
    public static boolean isInstalled(PackageManager packageManager, String packageName) {
        if (TextUtils.isEmpty(packageName)) return false;

        ApplicationInfo info;
        try {
            info = packageManager.getApplicationInfo(packageName, 0);
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
        return info.enabled;
    }

    /** Returns the <meta-data> value in the Android Manifest for {@link key}. */
    public static String readMetaDataFromManifest(Context context, String key) {
        Bundle metadata = readMetaData(context);
        if (metadata == null) return null;

        return metadata.getString(key);
    }

    /** Returns the <meta-data> in the Android Manifest. */
    public static Bundle readMetaData(Context context) {
        ApplicationInfo ai = null;
        try {
            ai =
                    context.getPackageManager()
                            .getApplicationInfo(
                                    context.getPackageName(), PackageManager.GET_META_DATA);
        } catch (NameNotFoundException e) {
            return null;
        }
        return ai.metaData;
    }

    /**
     * Returns the new intent url, rewrite if |loggedIntentUrlParam| is set. A query parameter with
     * the original URL is appended to the URL. Note: if the intent url has been rewritten before,
     * we don't rewrite it again.
     */
    public static String rewriteIntentUrlIfNecessary(String intentStartUrl, Bundle metadata) {
        String startUrl = metadata.getString(WebApkMetaDataKeys.START_URL);
        String loggedIntentUrlParam =
                metadata.getString(WebApkMetaDataKeys.LOGGED_INTENT_URL_PARAM);

        if (TextUtils.isEmpty(loggedIntentUrlParam)) return intentStartUrl;

        if (intentStartUrl.startsWith(startUrl)
                && !TextUtils.isEmpty(
                        Uri.parse(intentStartUrl).getQueryParameter(loggedIntentUrlParam))) {
            return intentStartUrl;
        }

        Uri.Builder returnUrlBuilder = Uri.parse(startUrl).buildUpon();
        returnUrlBuilder.appendQueryParameter(loggedIntentUrlParam, intentStartUrl);
        return returnUrlBuilder.toString();
    }

    /** Returns the package name for the passed-in ResolveInfo. */
    public static String getPackageNameFromResolveInfo(ResolveInfo resolveInfo) {
        return (resolveInfo != null && resolveInfo.activityInfo != null)
                ? resolveInfo.activityInfo.packageName
                : null;
    }

    /** Builds a context for the passed in remote package name. */
    public static Context fetchRemoteContext(Context context, String remotePackageName) {
        try {
            return context.getApplicationContext().createPackageContext(remotePackageName, 0);
        } catch (NameNotFoundException e) {
            e.printStackTrace();
        }
        return null;
    }

    /** Returns the uid for the passed in remote package name. */
    public static int getRemotePackageUid(Context context, String remotePackageName) {
        if (remotePackageName == null) {
            return -1;
        }
        try {
            PackageManager packageManager = context.getPackageManager();
            ApplicationInfo appInfo =
                    packageManager.getApplicationInfo(
                            remotePackageName, PackageManager.GET_META_DATA);
            return appInfo.uid;
        } catch (NameNotFoundException e) {
            e.printStackTrace();
        }
        return -1;
    }

    /**
     * Imitates Chrome's @style/AlertDialogContent. We set the style via Java instead of via
     * specifying the style in the XML to avoid having layout files in both layout-v17/ and in
     * layout/.
     */
    public static void applyAlertDialogContentStyle(
            Context context, View contentView, TextView titleView) {
        Resources res = context.getResources();
        titleView.setTextColor(getColor(res, R.color.webapk_black_alpha_87));
        titleView.setTextSize(
                TypedValue.COMPLEX_UNIT_PX, res.getDimension(R.dimen.headline_size_medium));
        int dialogContentPadding = res.getDimensionPixelSize(R.dimen.dialog_content_padding);
        int titleBottomPadding = res.getDimensionPixelSize(R.dimen.title_bottom_padding);
        titleView.setPaddingRelative(
                dialogContentPadding,
                dialogContentPadding,
                dialogContentPadding,
                titleBottomPadding);

        int dialogContentTopPadding = res.getDimensionPixelSize(R.dimen.dialog_content_top_padding);
        contentView.setPaddingRelative(
                dialogContentPadding,
                dialogContentTopPadding,
                dialogContentPadding,
                dialogContentPadding);
    }

    /**
     * @see android.content.res.Resources#getColor(int id).
     */
    public static int getColor(Resources res, int id) throws Resources.NotFoundException {
        return res.getColor(id, null);
    }

    /**
     * Calculates the contrast between the given color and white, using the algorithm provided by
     * the WCAG v1 in http://www.w3.org/TR/WCAG20/#contrast-ratiodef.
     */
    private static float getContrastForColor(int color) {
        float bgR = Color.red(color) / 255f;
        float bgG = Color.green(color) / 255f;
        float bgB = Color.blue(color) / 255f;
        bgR = (bgR < 0.03928f) ? bgR / 12.92f : (float) Math.pow((bgR + 0.055f) / 1.055f, 2.4f);
        bgG = (bgG < 0.03928f) ? bgG / 12.92f : (float) Math.pow((bgG + 0.055f) / 1.055f, 2.4f);
        bgB = (bgB < 0.03928f) ? bgB / 12.92f : (float) Math.pow((bgB + 0.055f) / 1.055f, 2.4f);
        float bgL = 0.2126f * bgR + 0.7152f * bgG + 0.0722f * bgB;
        return Math.abs(1.05f / (bgL + 0.05f));
    }

    /**
     * Check whether lighter or darker foreground elements (i.e. text, drawables etc.) should be
     * used depending on the given background color.
     *
     * @param backgroundColor The background color value which is being queried.
     * @return Whether light colored elements should be used.
     */
    public static boolean shouldUseLightForegroundOnBackground(int backgroundColor) {
        return getContrastForColor(backgroundColor) >= CONTRAST_LIGHT_ITEM_THRESHOLD;
    }

    /**
     * Decodes bitmap drawable from WebAPK's resources. This should also be used for XML aliases.
     */
    @SuppressWarnings("deprecation")
    public static Bitmap decodeBitmapFromDrawable(Resources resources, int resourceId) {
        if (resourceId == 0) {
            return null;
        }
        try {
            Drawable drawable = resources.getDrawable(resourceId, null);
            return drawable != null ? ((BitmapDrawable) drawable).getBitmap() : null;
        } catch (Resources.NotFoundException e) {
            return null;
        }
    }

    /**
     * Sets the status bar icons to dark or light.
     *
     * <p>TODO: migrate to WindowInsetsController API for Android R+ (API 30+)
     *
     * @param rootView The root view used to request updates to the system UI theming.
     * @param useDarkIcons Whether the status bar icons should be dark.
     */
    public static void setStatusBarIconColor(View rootView, boolean useDarkIcons, Context context) {
        int systemUiVisibility = rootView.getSystemUiVisibility();
        // The status bar should always be black in automotive devices to match the black back
        // button toolbar, so we should not use dark icons.
        if (useDarkIcons && !isAutomotive(context)) {
            systemUiVisibility |= View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
        } else {
            systemUiVisibility &= ~View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
        }
        rootView.setSystemUiVisibility(systemUiVisibility);
    }

    /**
     * @see android.view.Window#setStatusBarColor(int color).
     */
    public static void setStatusBarColor(Activity activity, int statusBarColor) {
        Window window = activity.getWindow();
        window.addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        // The status bar should always be black in automotive devices to match the black back
        // button toolbar.
        if (isAutomotive(activity)) {
            window.setStatusBarColor(Color.BLACK);
        } else {
            window.setStatusBarColor(statusBarColor);
        }
    }

    /** Returns the Intent to query a list of installed browser apps. */
    public static Intent getQueryInstalledBrowsersIntent() {
        return new Intent()
                .setAction(Intent.ACTION_VIEW)
                .addCategory(Intent.CATEGORY_BROWSABLE)
                .setData(Uri.parse("http://"));
    }

    public static String getNotificationChannelName(Context context) {
        return context.getString(R.string.notification_channel_name);
    }

    public static int getNotificationSmallIconId() {
        return R.drawable.notification_badge;
    }

    /** Computes the screen lock orientation from the passed-in metadata and the display size. */
    public static int computeNaturalScreenLockOrientationFromMetaData(
            Context context, Bundle metadata) {
        String orientation = metadata.getString(WebApkMetaDataKeys.ORIENTATION);
        if (orientation == null || !orientation.equals("natural")) {
            return ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
        }

        WindowManager windowManager =
                (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        Display display = windowManager.getDefaultDisplay();
        int rotation = display.getRotation();
        if (rotation == Surface.ROTATION_0 || rotation == Surface.ROTATION_180) {
            if (display.getHeight() >= display.getWidth()) {
                return ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
            }
            return ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
        }
        if (display.getHeight() < display.getWidth()) {
            return ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
        }
        return ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
    }

    /** Grants the host browser permission to the shared files if any. */
    public static void grantUriPermissionToHostBrowserIfShare(
            Context context, HostBrowserLauncherParams params) {
        if (params.getSelectedShareTargetActivityClassName() == null) return;

        Intent originalIntent = params.getOriginalIntent();
        ArrayList<Uri> uris = originalIntent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
        if (uris == null) {
            uris = new ArrayList<>();
            Uri uri = originalIntent.getParcelableExtra(Intent.EXTRA_STREAM);
            if (uri != null) {
                uris.add(uri);
            }
        }
        for (Uri uri : uris) {
            context.grantUriPermission(
                    params.getHostBrowserPackageName(), uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }
    }

    /** Returns the ComponentName for the top activity in {@link taskId}'s task stack. */
    @SuppressLint("NewApi") // See crbug.com/1081331 for context.
    public static ComponentName fetchTopActivityComponent(Context context, int taskId) {
        ActivityManager manager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        for (ActivityManager.AppTask task : manager.getAppTasks()) {
            try {
                ActivityManager.RecentTaskInfo taskInfo = task.getTaskInfo();
                if (taskInfo != null && taskInfo.id == taskId) {
                    return taskInfo.topActivity;
                }
            } catch (IllegalArgumentException e) {
            }
        }
        return null;
    }

    public static boolean isSplashIconAdaptive(Context context) {
        try {
            return context.getResources().getBoolean(R.bool.is_splash_icon_maskable);
        } catch (Resources.NotFoundException e) {
        }
        return false;
    }

    /** Returns whether the system is in dark mode */
    public static boolean inDarkMode(Context context) {
        return (context.getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK)
                == Configuration.UI_MODE_NIGHT_YES;
    }

    private static boolean isAutomotive(Context context) {
        boolean isAutomotive;
        try {
            isAutomotive =
                    context.getApplicationContext()
                            .getPackageManager()
                            .hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE);
        } catch (SecurityException e) {
            Log.e(TAG, "Unable to query for Automotive system feature", e);

            // `hasSystemFeature` can possibly throw an exception on modified instances of
            // Android. In this case, assume the device is not a car since automotive vehicles
            // should not have such a modification.
            isAutomotive = false;
        }
        return isAutomotive;
    }
}
