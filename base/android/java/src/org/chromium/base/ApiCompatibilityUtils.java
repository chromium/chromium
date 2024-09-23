// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityOptions;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.content.res.Resources.NotFoundException;
import android.graphics.Bitmap;
import android.graphics.ImageDecoder;
import android.graphics.drawable.Drawable;
import android.hardware.display.DisplayManager;
import android.net.Uri;
import android.os.Build;
import android.os.StrictMode;
import android.os.UserManager;
import android.provider.MediaStore;
import android.view.Display;
import android.view.View;

import androidx.annotation.NonNull;

import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/**
 * Utility class to use APIs not in all supported Android versions.
 *
 * Do not inline because we use many new APIs, and if they are inlined, they could cause dex
 * validation errors on low Android versions.
 */
public class ApiCompatibilityUtils {
    private static final String TAG = "ApiCompatUtil";

    private ApiCompatibilityUtils() {}

    /**
     * {@link String#getBytes()} but specifying UTF-8 as the encoding and capturing the resulting
     * UnsupportedEncodingException.
     */
    public static byte[] getBytesUtf8(String str) {
        return str.getBytes(StandardCharsets.UTF_8);
    }

    /**
     * @see android.content.res.Resources#getDrawable(int id). TODO(ltian): use {@link
     *     AppCompatResources} to parse drawable to prevent fail on {@link VectorDrawable}.
     *     (http://crbug.com/792129)
     */
    public static Drawable getDrawable(Resources res, int id) throws NotFoundException {
        return getDrawableForDensity(res, id, 0);
    }

    /**
     * @see android.content.res.Resources#getDrawableForDensity(int id, int density).
     */
    @SuppressWarnings("deprecation")
    public static Drawable getDrawableForDensity(Resources res, int id, int density) {
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            // For Android Oreo+, Resources.getDrawable(id, null) delegates to
            // Resources.getDrawableForDensity(id, 0, null), but before that the two functions are
            // independent. This check can be removed after Oreo becomes the minimum supported API.
            if (density == 0) {
                return res.getDrawable(id, null);
            }
            return res.getDrawableForDensity(id, density, null);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    /**
     * @return Whether the device is running in demo mode.
     */
    public static boolean isDemoUser() {
        UserManager userManager =
                (UserManager)
                        ContextUtils.getApplicationContext().getSystemService(Context.USER_SERVICE);
        return userManager.isDemoUser();
    }

    /**
     * @see Context#checkPermission(String, int, int)
     */
    public static int checkPermission(Context context, String permission, int pid, int uid) {
        try {
            return context.checkPermission(permission, pid, uid);
        } catch (RuntimeException e) {
            // Some older versions of Android throw odd errors when checking for permissions, so
            // just swallow the exception and treat it as the permission is denied.
            // crbug.com/639099
            return PackageManager.PERMISSION_DENIED;
        }
    }

    /**
     * Get a list of ids of targetable displays, including the default display for the current
     * activity. A set of targetable displays can only be determined on Q+. An empty list is
     * returned if called on prior Q.
     *
     * @param activity The {@link Activity} to check.
     * @return A list of display ids. Empty if there is none or version is less than Q, or
     *     windowAndroid does not contain an activity.
     */
    @NonNull
    public static List<Integer> getTargetableDisplayIds(Activity activity) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            List<Integer> displayList = new ArrayList<>();
            if (activity == null) return displayList;
            DisplayManager displayManager =
                    (DisplayManager) activity.getSystemService(Context.DISPLAY_SERVICE);
            if (displayManager == null) return displayList;
            Display[] displays = displayManager.getDisplays();
            ActivityManager am =
                    (ActivityManager) activity.getSystemService(Context.ACTIVITY_SERVICE);
            for (Display display : displays) {
                if (display.getState() == Display.STATE_ON
                        && am.isActivityStartAllowedOnDisplay(
                                activity,
                                display.getDisplayId(),
                                new Intent(activity, activity.getClass()))) {
                    displayList.add(display.getDisplayId());
                }
            }
            return displayList;
        }
        return new ArrayList<>();
    }

    /**
     * Sets the mode {@link ActivityOptions#MODE_BACKGROUND_ACTIVITY_START_ALLOWED} to the given
     * {@link ActivityOptions}. The options can be used to send {@link PendingIntent} passed to
     * Chrome from a backgrounded app.
     *
     * @param options {@ActivityOptions} to set the required mode to.
     */
    public static void setActivityOptionsBackgroundActivityStartMode(
            @NonNull ActivityOptions options) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.UPSIDE_DOWN_CAKE) return;
        options.setPendingIntentBackgroundActivityStartMode(
                ActivityOptions.MODE_BACKGROUND_ACTIVITY_START_ALLOWED);
    }

    /**
     * Sets the mode {@link ActivityOptions#MODE_BACKGROUND_ACTIVITY_START_ALLOWED} to the given
     * {@link ActivityOptions}. The options can be used to create {@link PendingIntent}.
     *
     * @param options {@ActivityOptions} to set the required mode to.
     */
    public static void setCreatorActivityOptionsBackgroundActivityStartMode(
            @NonNull ActivityOptions options) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.UPSIDE_DOWN_CAKE) return;
        options.setPendingIntentCreatorBackgroundActivityStartMode(
                ActivityOptions.MODE_BACKGROUND_ACTIVITY_START_ALLOWED);
    }

    /**
     * Sets the bottom handwriting bounds offset of the given view to 0. See
     * https://crbug.com/1427112
     *
     * @param view The view on which to set the handwriting bounds.
     */
    public static void clearHandwritingBoundsOffsetBottom(View view) {
        // TODO(crbug.com/40261637): Replace uses of this method with direct calls once the API is
        // available.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.UPSIDE_DOWN_CAKE) return;
        // Set the bottom handwriting bounds offset to 0 so that the view doesn't intercept
        // stylus events meant for the web contents.
        try {
            // float offsetTop = this.getHandwritingBoundsOffsetTop();
            float offsetTop =
                    (float) View.class.getMethod("getHandwritingBoundsOffsetTop").invoke(view);
            // float offsetLeft = this.getHandwritingBoundsOffsetLeft();
            float offsetLeft =
                    (float) View.class.getMethod("getHandwritingBoundsOffsetLeft").invoke(view);
            // float offsetRight = this.getHandwritingBoundsOffsetRight();
            float offsetRight =
                    (float) View.class.getMethod("getHandwritingBoundsOffsetRight").invoke(view);
            // this.setHandwritingBoundsOffsets(offsetLeft, offsetTop, offsetRight, 0);
            Method setHandwritingBoundsOffsets =
                    View.class.getMethod(
                            "setHandwritingBoundsOffsets",
                            float.class,
                            float.class,
                            float.class,
                            float.class);
            setHandwritingBoundsOffsets.invoke(view, offsetLeft, offsetTop, offsetRight, 0);
        } catch (IllegalAccessException
                | InvocationTargetException
                | NoSuchMethodException
                | NullPointerException e) {
            // Do nothing.
        }
    }

    public static boolean isRunningInUserTestHarness() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            return ActivityManager.isRunningInUserTestHarness();
        }
        return false;
    }

    /** Retrieves an image for the given uri as a Bitmap. */
    public static Bitmap getBitmapByUri(ContentResolver cr, Uri uri) throws IOException {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            return ImageDecoder.decodeBitmap(ImageDecoder.createSource(cr, uri));
        }
        return MediaStore.Images.Media.getBitmap(cr, uri);
    }
}
