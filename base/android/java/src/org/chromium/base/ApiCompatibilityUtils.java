// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityOptions;
import android.app.Application;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.content.res.Resources.NotFoundException;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.ImageDecoder;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.hardware.display.DisplayManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.StrictMode;
import android.os.UserManager;
import android.provider.MediaStore;
import android.provider.Settings;
import android.text.Html;
import android.text.Spanned;
import android.text.TextUtils;
import android.view.Display;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodSubtype;
import android.view.textclassifier.TextClassifier;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.annotations.VerifiesOnLollipopMR1;
import org.chromium.base.annotations.VerifiesOnM;
import org.chromium.base.annotations.VerifiesOnN;
import org.chromium.base.annotations.VerifiesOnO;
import org.chromium.base.annotations.VerifiesOnP;
import org.chromium.base.annotations.VerifiesOnQ;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
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

    @VerifiesOnQ
    @TargetApi(Build.VERSION_CODES.Q)
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

    @VerifiesOnP
    @TargetApi(Build.VERSION_CODES.P)
    private static class ApisP {
        static String getProcessName() {
            return Application.getProcessName();
        }

        static Bitmap getBitmapByUri(ContentResolver cr, Uri uri) throws IOException {
            return ImageDecoder.decodeBitmap(ImageDecoder.createSource(cr, uri));
        }
    }

    @VerifiesOnO
    @TargetApi(Build.VERSION_CODES.O)
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

    @VerifiesOnN
    @TargetApi(Build.VERSION_CODES.N)
    private static class ApisN {
        static String toHtml(Spanned spanned, int option) {
            return Html.toHtml(spanned, option);
        }

        // This class is sufficiently small that it's fine if it doesn't verify for N devices.
        @TargetApi(Build.VERSION_CODES.N_MR1)
        static boolean isDemoUser() {
            UserManager userManager =
                    (UserManager) ContextUtils.getApplicationContext().getSystemService(
                            Context.USER_SERVICE);
            return userManager.isDemoUser();
        }

        static String getLocale(InputMethodSubtype inputMethodSubType) {
            return inputMethodSubType.getLanguageTag();
        }

        static boolean isInMultiWindowMode(Activity activity) {
            return activity.isInMultiWindowMode();
        }
    }

    @VerifiesOnM
    @TargetApi(Build.VERSION_CODES.M)
    private static class ApisM {
        public static void setStatusBarIconColor(View rootView, boolean useDarkIcons) {
            int systemUiVisibility = rootView.getSystemUiVisibility();
            if (useDarkIcons) {
                systemUiVisibility |= View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
            } else {
                systemUiVisibility &= ~View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
            }
            rootView.setSystemUiVisibility(systemUiVisibility);
        }
    }

    @VerifiesOnLollipopMR1
    @TargetApi(Build.VERSION_CODES.LOLLIPOP_MR1)
    private static class ApisLmr1 {
        static void setAccessibilityTraversalBefore(View view, int viewFocusedAfter) {
            view.setAccessibilityTraversalBefore(viewFocusedAfter);
        }
    }

    /**
     * Compares two long values numerically. The value returned is identical to what would be
     * returned by {@link Long#compare(long, long)} which is available since API level 19.
     */
    public static int compareLong(long lhs, long rhs) {
        return lhs < rhs ? -1 : (lhs == rhs ? 0 : 1);
    }

    /**
     * Compares two boolean values. The value returned is identical to what would be returned by
     * {@link Boolean#compare(boolean, boolean)} which is available since API level 19.
     */
    public static int compareBoolean(boolean lhs, boolean rhs) {
        return lhs == rhs ? 0 : lhs ? 1 : -1;
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
        try {
            return str.getBytes("UTF-8");
        } catch (UnsupportedEncodingException e) {
            throw new IllegalStateException(e);
        }
    }

    /**
     * @see android.text.Html#toHtml(Spanned, int)
     * @param option is ignored on below N
     */
    @SuppressWarnings("deprecation")
    public static String toHtml(Spanned spanned, int option) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            return ApisN.toHtml(spanned, option);
        }
        return Html.toHtml(spanned);
    }

    // These methods have a new name, and the old name is deprecated.

    /**
     * @see android.app.Activity#finishAndRemoveTask()
     */
    public static void finishAndRemoveTask(Activity activity) {
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.LOLLIPOP) {
            activity.finishAndRemoveTask();
        } else {
            assert Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP;
            // crbug.com/395772 : Fallback for Activity.finishAndRemoveTask() failing.
            new FinishAndRemoveTaskWithRetry(activity).run();
        }
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

    private static class FinishAndRemoveTaskWithRetry implements Runnable {
        private static final long RETRY_DELAY_MS = 500;
        private static final long MAX_TRY_COUNT = 3;
        private final Activity mActivity;
        private int mTryCount;

        FinishAndRemoveTaskWithRetry(Activity activity) {
            mActivity = activity;
        }

        @Override
        public void run() {
            mActivity.finishAndRemoveTask();
            mTryCount++;
            if (!mActivity.isFinishing()) {
                if (mTryCount < MAX_TRY_COUNT) {
                    ThreadUtils.postOnUiThreadDelayed(this, RETRY_DELAY_MS);
                } else {
                    mActivity.finish();
                }
            }
        }
    }

    /**
     * @see android.view.Window#setStatusBarColor(int color).
     */
    public static void setStatusBarColor(Window window, int statusBarColor) {
        // If both system bars are black, we can remove these from our layout,
        // removing or shrinking the SurfaceFlinger overlay required for our views.
        // This benefits battery usage on L and M.  However, this no longer provides a battery
        // benefit as of N and starts to cause flicker bugs on O, so don't bother on O and up.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O && statusBarColor == Color.BLACK
                && window.getNavigationBarColor() == Color.BLACK) {
            window.clearFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        } else {
            window.addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        }
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
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            ApisM.setStatusBarIconColor(rootView, useDarkIcons);
        }
    }

    /**
     * @see android.content.res.Resources#getDrawable(int id).
     * TODO(ltian): use {@link AppCompatResources} to parse drawable to prevent fail on
     * {@link VectorDrawable}. (http://crbug.com/792129)
     */
    public static Drawable getDrawable(Resources res, int id) throws NotFoundException {
        return getDrawableForDensity(res, id, 0);
    }

    public static void setImageTintList(ImageView view, @Nullable ColorStateList tintList) {
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP) {
            // Work around broken workaround in ImageViewCompat, see
            // https://crbug.com/891609#c3.
            if (tintList != null && view.getImageTintMode() == null) {
                view.setImageTintMode(PorterDuff.Mode.SRC_IN);
            }
        }
        ImageViewCompat.setImageTintList(view, tintList);

        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP) {
            // Work around that the tint list is not cleared when setting tint list to null on L
            // in some cases. See https://crbug.com/983686.
            if (tintList == null) view.refreshDrawableState();
        }
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
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.N_MR1 && ApisN.isDemoUser();
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
     * @see android.view.inputmethod.InputMethodSubType#getLocate()
     */
    @SuppressWarnings("deprecation")
    public static String getLocale(InputMethodSubtype inputMethodSubType) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            return ApisN.getLocale(inputMethodSubType);
        }
        return inputMethodSubType.getLocale();
    }

    /**
     * @param activity The {@link Activity} to check.
     * @return Whether or not {@code activity} is currently in Android N+ multi-window mode.
     */
    public static boolean isInMultiWindowMode(Activity activity) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            return ApisN.isInMultiWindowMode(activity);
        }
        return false;
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
     * @see View#setAccessibilityTraversalBefore(int)
     */
    public static void setAccessibilityTraversalBefore(View view, int viewFocusedAfter) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP_MR1) {
            ApisLmr1.setAccessibilityTraversalBefore(view, viewFocusedAfter);
        }
    }

    /**
     * Adds a content description to the provided EditText password field on versions of Android
     * where the hint text is not used for accessibility. Does nothing if the EditText field does
     * not have a password input type or the hint text is empty.  See https://crbug.com/911762.
     *
     * @param view The EditText password field.
     */
    public static void setPasswordEditTextContentDescription(EditText view) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) return;

        if (isPasswordInputType(view.getInputType()) && !TextUtils.isEmpty(view.getHint())) {
            view.setContentDescription(view.getHint());
        }
    }

    private static boolean isPasswordInputType(int inputType) {
        final int variation =
                inputType & (EditorInfo.TYPE_MASK_CLASS | EditorInfo.TYPE_MASK_VARIATION);
        return variation == (EditorInfo.TYPE_CLASS_TEXT | EditorInfo.TYPE_TEXT_VARIATION_PASSWORD)
                || variation
                == (EditorInfo.TYPE_CLASS_TEXT | EditorInfo.TYPE_TEXT_VARIATION_WEB_PASSWORD)
                || variation
                == (EditorInfo.TYPE_CLASS_NUMBER | EditorInfo.TYPE_NUMBER_VARIATION_PASSWORD);
    }

    // Access this via ContextUtils.getProcessName().
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
