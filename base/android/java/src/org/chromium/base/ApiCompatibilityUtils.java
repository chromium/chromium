// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityOptions;
import android.app.Application;
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
import android.os.Bundle;
import android.os.StrictMode;
import android.os.UserManager;
import android.provider.MediaStore;
import android.provider.Settings;
import android.view.Display;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.textclassifier.TextClassifier;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.OptIn;
import androidx.annotation.RequiresApi;
import androidx.core.os.BuildCompat;

import java.io.IOException;
import java.lang.reflect.Field;
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
    private ApiCompatibilityUtils() {
    }

    @RequiresApi(Build.VERSION_CODES.Q)
    private static class ApisQ {
        static boolean isRunningInUserTestHarness() {
            return ActivityManager.isRunningInUserTestHarness();
        }

        static List<Integer> getTargetableDisplayIds(@Nullable Activity activity) {
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
                        && am.isActivityStartAllowedOnDisplay(activity, display.getDisplayId(),
                                new Intent(activity, activity.getClass()))) {
                    displayList.add(display.getDisplayId());
                }
            }
            return displayList;
        }
    }

    @RequiresApi(Build.VERSION_CODES.P)
    private static class ApisP {
        static String getProcessName() {
            return Application.getProcessName();
        }

        static Bitmap getBitmapByUri(ContentResolver cr, Uri uri) throws IOException {
            return ImageDecoder.decodeBitmap(ImageDecoder.createSource(cr, uri));
        }
    }

    @RequiresApi(Build.VERSION_CODES.O)
    private static class ApisO {
        static void initNotificationSettingsIntent(Intent intent, String packageName) {
            intent.setAction(Settings.ACTION_APP_NOTIFICATION_SETTINGS);
            intent.putExtra(Settings.EXTRA_APP_PACKAGE, packageName);
        }

        static void disableSmartSelectionTextClassifier(TextView textView) {
            textView.setTextClassifier(TextClassifier.NO_OP);
        }

        static Bundle createLaunchDisplayIdActivityOptions(int displayId) {
            ActivityOptions options = ActivityOptions.makeBasic();
            options.setLaunchDisplayId(displayId);
            return options.toBundle();
        }
    }

    // This class is sufficiently small that it's fine if it doesn't verify for N devices.
    @RequiresApi(Build.VERSION_CODES.N_MR1)
    private static class ApisNMR1 {
        static boolean isDemoUser() {
            UserManager userManager =
                    (UserManager) ContextUtils.getApplicationContext().getSystemService(
                            Context.USER_SERVICE);
            return userManager.isDemoUser();
        }
    }

    /**
     * Checks that the object reference is not null and throws NullPointerException if it is.
     * See {@link Objects#requireNonNull} which is available since API level 19.
     * @param obj The object to check
     */
    @NonNull
    public static <T> T requireNonNull(T obj) {
        if (obj == null) throw new NullPointerException();
        return obj;
    }

    /**
     * Checks that the object reference is not null and throws NullPointerException if it is.
     * See {@link Objects#requireNonNull} which is available since API level 19.
     * @param obj The object to check
     * @param message The message to put into NullPointerException
     */
    @NonNull
    public static <T> T requireNonNull(T obj, String message) {
        if (obj == null) throw new NullPointerException(message);
        return obj;
    }

    /**
     * {@link String#getBytes()} but specifying UTF-8 as the encoding and capturing the resulting
     * UnsupportedEncodingException.
     */
    public static byte[] getBytesUtf8(String str) {
        return str.getBytes(StandardCharsets.UTF_8);
    }

    /**
     *  Gets an intent to start the Android system notification settings activity for an app.
     *
     */
    public static Intent getNotificationSettingsIntent() {
        Intent intent = new Intent();
        String packageName = ContextUtils.getApplicationContext().getPackageName();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            ApisO.initNotificationSettingsIntent(intent, packageName);
        } else {
            intent.setAction("android.settings.ACTION_APP_NOTIFICATION_SETTINGS");
            intent.putExtra("app_package", packageName);
            intent.putExtra(
                    "app_uid", ContextUtils.getApplicationContext().getApplicationInfo().uid);
        }
        return intent;
    }

    /**
     * @see android.view.Window#setStatusBarColor(int color).
     */
    public static void setStatusBarColor(Window window, int statusBarColor) {
        window.addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        window.setStatusBarColor(statusBarColor);
    }

    /**
     * Sets the status bar icons to dark or light. Note that this is only valid for
     * Android M+.
     *
     * @param rootView The root view used to request updates to the system UI theming.
     * @param useDarkIcons Whether the status bar icons should be dark.
     */
    public static void setStatusBarIconColor(View rootView, boolean useDarkIcons) {
        int systemUiVisibility = rootView.getSystemUiVisibility();
        if (useDarkIcons) {
            systemUiVisibility |= View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
        } else {
            systemUiVisibility &= ~View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
        }
        rootView.setSystemUiVisibility(systemUiVisibility);
    }

    /**
     * @see android.content.res.Resources#getDrawable(int id).
     * TODO(ltian): use {@link AppCompatResources} to parse drawable to prevent fail on
     * {@link VectorDrawable}. (http://crbug.com/792129)
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
     * @see android.content.res.Resources#getColor(int id).
     */
    @SuppressWarnings("deprecation")
    public static int getColor(Resources res, int id) throws NotFoundException {
        return res.getColor(id);
    }

    /**
     * @see android.widget.TextView#setTextAppearance(int id).
     */
    @SuppressWarnings("deprecation")
    public static void setTextAppearance(TextView view, int id) {
        // setTextAppearance(id) is the undeprecated version of this, but it just calls the
        // deprecated one, so there is no benefit to using the non-deprecated one until we can
        // drop support for it entirely (new one was added in M).
        view.setTextAppearance(view.getContext(), id);
    }

    /**
     * @return Whether the device is running in demo mode.
     */
    public static boolean isDemoUser() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.N_MR1 && ApisNMR1.isDemoUser();
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
     * @param activity The {@link Activity} to check.
     * @return Whether or not {@code activity} is currently in Android N+ multi-window mode.
     */
    public static boolean isInMultiWindowMode(Activity activity) {
        return activity.isInMultiWindowMode();
    }

    /**
     * Get a list of ids of targetable displays, including the default display for the
     * current activity. A set of targetable displays can only be determined on Q+. An empty list
     * is returned if called on prior Q.
     * @param activity The {@link Activity} to check.
     * @return A list of display ids. Empty if there is none or version is less than Q, or
     *         windowAndroid does not contain an activity.
     */
    @NonNull
    public static List<Integer> getTargetableDisplayIds(Activity activity) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            return ApisQ.getTargetableDisplayIds(activity);
        }
        return new ArrayList<>();
    }

    /**
     * Disables the Smart Select {@link TextClassifier} for the given {@link TextView} instance.
     * @param textView The {@link TextView} that should have its classifier disabled.
     */
    public static void disableSmartSelectionTextClassifier(TextView textView) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            ApisO.disableSmartSelectionTextClassifier(textView);
        }
    }

    /**
     * Creates an ActivityOptions Bundle with basic options and the LaunchDisplayId set.
     * @param displayId The id of the display to launch into.
     * @return The created bundle, or null if unsupported.
     */
    public static Bundle createLaunchDisplayIdActivityOptions(int displayId) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            return ApisO.createLaunchDisplayIdActivityOptions(displayId);
        }
        return null;
    }

    /**
     * Sets the mode {@link ActivityOptions#MODE_BACKGROUND_ACTIVITY_START_ALLOWED} to the
     * given {@link ActivityOptions}. The options can be used to send {@link PendingIntent}
     * passed to Chrome from a backgrounded app.
     * @param options {@ActivityOptions} to set the required mode to.
     */
    @OptIn(markerClass = androidx.core.os.BuildCompat.PrereleaseSdkCheck.class)
    public static void setActivityOptionsBackgroundActivityStartMode(
            @NonNull ActivityOptions options) {
        if (!BuildCompat.isAtLeastU()) return;

        // options.setPendingIntentBackgroundActivityStartMode(
        //     ActivityOptions.MODE_BACKGROUND_ACTIVITY_START_ALLOWED);
        // TODO(crbug.com/1423489): Replace the reflection with the normal API.
        try {
            Method method = ActivityOptions.class.getMethod(
                    "setPendingIntentBackgroundActivityStartMode", int.class);
            Field field = ActivityOptions.class.getField("MODE_BACKGROUND_ACTIVITY_START_ALLOWED");
            int mode = field.getInt(null);
            method.invoke(options, mode);
        } catch (IllegalAccessException | InvocationTargetException | NoSuchFieldException
                | NoSuchMethodException | RuntimeException e) {
            assert false : "PendingIntent from background activity may fail to run.";
        }
    }

    // Access this via ContextUtils.getProcessName().
    @SuppressWarnings("PrivateApi")
    static String getProcessName() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            return ApisP.getProcessName();
        }
        try {
            Class<?> activityThreadClazz = Class.forName("android.app.ActivityThread");
            return (String) activityThreadClazz.getMethod("currentProcessName").invoke(null);
        } catch (Exception e) {
            // If fallback logic is ever needed, refer to:
            // https://chromium-review.googlesource.com/c/chromium/src/+/905563/1
            throw new RuntimeException(e);
        }
    }

    public static boolean isRunningInUserTestHarness() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            return ApisQ.isRunningInUserTestHarness();
        }
        return false;
    }

    /**
     * Retrieves an image for the given uri as a Bitmap.
     */
    public static Bitmap getBitmapByUri(ContentResolver cr, Uri uri) throws IOException {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            return ApisP.getBitmapByUri(cr, uri);
        }
        return MediaStore.Images.Media.getBitmap(cr, uri);
    }
}
