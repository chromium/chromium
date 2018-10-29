// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.TextView;

import org.chromium.webapk.lib.common.WebApkMetaDataKeys;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Contains utility methods for interacting with WebAPKs.
 */
public class WebApkUtils {
    private static final String TAG = "cr_WebApkUtils";

    /** Percentage to darken a color by when setting the status bar color. */
    private static final float DARKEN_COLOR_FRACTION = 0.6f;

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
            ai = context.getPackageManager().getApplicationInfo(
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

    /** Returns a set of ResolveInfo for all of the installed browsers. */
    public static Set<ResolveInfo> getInstalledBrowserResolveInfos(PackageManager packageManager) {
        Intent browserIntent = getQueryInstalledBrowsersIntent();
        // Note: {@link PackageManager#queryIntentActivities()} does not return ResolveInfos for
        // disabled browsers.
        Set<ResolveInfo> result = new HashSet<>();
        List<ResolveInfo> resolveInfosAll =
                packageManager.queryIntentActivities(browserIntent, PackageManager.MATCH_ALL);
        List<ResolveInfo> resolveInfosDefaultOnly = packageManager.queryIntentActivities(
                browserIntent, PackageManager.MATCH_DEFAULT_ONLY);

        result.addAll(resolveInfosAll);
        result.addAll(resolveInfosDefaultOnly);
        return result;
    }

    /**
     * Android uses padding_left under API level 17 and uses padding_start after that.
     * If we set the padding in resource file, android will create duplicated resource xml
     * with the padding to be different.
     */
    public static void setPaddingInPixel(View view, int start, int top, int end, int bottom) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            view.setPaddingRelative(start, top, end, bottom);
        } else {
            view.setPadding(start, top, end, bottom);
        }
    }

    /**
     * Imitates Chrome's @style/AlertDialogContent. We set the style via Java instead of via
     * specifying the style in the XML to avoid having layout files in both layout-v17/ and in
     * layout/.
     */
    public static void applyAlertDialogContentStyle(
            Context context, View contentView, TextView titleView) {
        Resources res = context.getResources();
        titleView.setTextColor(getColor(res, R.color.black_alpha_87));
        titleView.setTextSize(
                TypedValue.COMPLEX_UNIT_PX, res.getDimension(R.dimen.headline_size_medium));
        int dialogContentPadding = res.getDimensionPixelSize(R.dimen.dialog_content_padding);
        int titleBottomPadding = res.getDimensionPixelSize(R.dimen.title_bottom_padding);
        setPaddingInPixel(titleView, dialogContentPadding, dialogContentPadding,
                dialogContentPadding, titleBottomPadding);

        int dialogContentTopPadding = res.getDimensionPixelSize(R.dimen.dialog_content_top_padding);
        setPaddingInPixel(contentView, dialogContentPadding, dialogContentTopPadding,
                dialogContentPadding, dialogContentPadding);
    }

    /**
     * @see android.content.res.Resources#getColor(int id).
     */
    @SuppressWarnings("deprecation")
    public static int getColor(Resources res, int id) throws Resources.NotFoundException {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            return res.getColor(id, null);
        } else {
            return res.getColor(id);
        }
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
        return Math.abs((1.05f) / (bgL + 0.05f));
    }

    /**
     * Darkens the given color to use on the status bar.
     * @param color Color which should be darkened.
     * @return Color that should be used for Android status bar.
     */
    public static int getDarkenedColorForStatusBar(int color) {
        return getDarkenedColor(color, DARKEN_COLOR_FRACTION);
    }

    /**
     * Darken a color to a fraction of its current brightness.
     * @param color The input color.
     * @param darkenFraction The fraction of the current brightness the color should be.
     * @return The new darkened color.
     */
    public static int getDarkenedColor(int color, float darkenFraction) {
        float[] hsv = new float[3];
        Color.colorToHSV(color, hsv);
        hsv[2] *= darkenFraction;
        return Color.HSVToColor(hsv);
    }

    /**
     * Check whether lighter or darker foreground elements (i.e. text, drawables etc.)
     * should be used depending on the given background color.
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
            Drawable drawable = null;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                drawable = resources.getDrawable(resourceId, null);
            } else {
                drawable = resources.getDrawable(resourceId);
            }
            return drawable != null ? ((BitmapDrawable) drawable).getBitmap() : null;
        } catch (Resources.NotFoundException e) {
            return null;
        }
    }

    /**
     * @see android.view.Window#setStatusBarColor(int color).
     */
    public static void setStatusBarColor(Window window, int statusBarColor) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        window.addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        window.setStatusBarColor(statusBarColor);
    }

    /**
     * Returns the Intent to query a list of installed browser apps.
     */
    static Intent getQueryInstalledBrowsersIntent() {
        return new Intent()
                .setAction(Intent.ACTION_VIEW)
                .addCategory(Intent.CATEGORY_BROWSABLE)
                .setData(Uri.parse("http://"));
    }
}
