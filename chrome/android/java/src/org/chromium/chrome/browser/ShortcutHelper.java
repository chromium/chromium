// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.ShortcutInfo;
import android.content.pm.ShortcutManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;
import android.util.Base64;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.CollectionUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.task.AsyncTask;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.webapps.WebApkInfo;
import org.chromium.chrome.browser.webapps.WebappActivity;
import org.chromium.chrome.browser.webapps.WebappAuthenticator;
import org.chromium.chrome.browser.webapps.WebappDataStorage;
import org.chromium.chrome.browser.webapps.WebappLauncherActivity;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.content_public.common.ScreenOrientationConstants;
import org.chromium.ui.widget.Toast;
import org.chromium.webapk.lib.client.WebApkValidator;

import java.io.ByteArrayOutputStream;
import java.util.ArrayList;
import java.util.List;

/**
 * This class contains functions related to adding shortcuts to the Android Home
 * screen.  These shortcuts are used to either open a page in the main browser
 * or open a web app.
 */
public class ShortcutHelper {
    public static final String EXTRA_ICON = "org.chromium.chrome.browser.webapp_icon";
    public static final String EXTRA_ID = "org.chromium.chrome.browser.webapp_id";
    public static final String EXTRA_MAC = "org.chromium.chrome.browser.webapp_mac";
    // EXTRA_TITLE is present for backward compatibility reasons.
    public static final String EXTRA_TITLE = "org.chromium.chrome.browser.webapp_title";
    public static final String EXTRA_NAME = "org.chromium.chrome.browser.webapp_name";
    public static final String EXTRA_SHORT_NAME = "org.chromium.chrome.browser.webapp_short_name";
    public static final String EXTRA_URL = "org.chromium.chrome.browser.webapp_url";
    public static final String EXTRA_SCOPE = "org.chromium.chrome.browser.webapp_scope";
    public static final String EXTRA_DISPLAY_MODE =
            "org.chromium.chrome.browser.webapp_display_mode";
    public static final String EXTRA_ORIENTATION = ScreenOrientationConstants.EXTRA_ORIENTATION;
    public static final String EXTRA_SOURCE = "org.chromium.chrome.browser.webapp_source";
    public static final String EXTRA_THEME_COLOR = "org.chromium.chrome.browser.theme_color";
    public static final String EXTRA_BACKGROUND_COLOR =
            "org.chromium.chrome.browser.background_color";
    public static final String EXTRA_SPLASH_SCREEN_URL =
            "org.chromium.chrome.browser.webapp_splash_screen_url";
    public static final String EXTRA_IS_ICON_GENERATED =
            "org.chromium.chrome.browser.is_icon_generated";
    public static final String EXTRA_VERSION =
            "org.chromium.chrome.browser.webapp_shortcut_version";
    public static final String REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB =
            "REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB";
    // Whether the webapp should navigate to the URL in {@link EXTRA_URL} if the webapp is already
    // open. Applies to webapps and WebAPKs. Value contains "webapk" for backward compatibility.
    public static final String EXTRA_FORCE_NAVIGATION =
            "org.chromium.chrome.browser.webapk_force_navigation";

    // When a new field is added to the intent, this version should be incremented so that it will
    // be correctly populated into the WebappRegistry/WebappDataStorage.
    public static final int WEBAPP_SHORTCUT_VERSION = 2;

    // This value is equal to kInvalidOrMissingColor in the C++ blink::Manifest struct.
    public static final long MANIFEST_COLOR_INVALID_OR_MISSING = ((long) Integer.MAX_VALUE) + 1;

    private static final String TAG = "ShortcutHelper";

    private static final String INSTALL_SHORTCUT = "com.android.launcher.action.INSTALL_SHORTCUT";

    // These sizes are from the Material spec for icons:
    // https://www.google.com/design/spec/style/icons.html#icons-product-icons
    private static final float MAX_INNER_SIZE_RATIO = 1.25f;
    private static final float ICON_PADDING_RATIO = 2.0f / 44.0f;
    private static final float ICON_CORNER_RADIUS_RATIO = 1.0f / 16.0f;
    private static final float GENERATED_ICON_PADDING_RATIO = 1.0f / 12.0f;
    private static final float GENERATED_ICON_FONT_SIZE_RATIO = 1.0f / 3.0f;

    // True when Android O's ShortcutManager.requestPinShortcut() is supported.
    private static boolean sIsRequestPinShortcutSupported;

    // True when it is already checked if ShortcutManager.requestPinShortcut() is supported.
    private static boolean sCheckedIfRequestPinShortcutSupported;

    private static ShortcutManager sShortcutManager;

    /** Helper for generating home screen shortcuts. */
    public static class Delegate {
        /**
         * Request Android to add a shortcut to the home screen.
         * @param title  Title of the shortcut.
         * @param icon   Image that represents the shortcut.
         * @param intent Intent to fire when the shortcut is activated.
         */
        public void addShortcutToHomescreen(String title, Bitmap icon, Intent shortcutIntent) {
            if (isRequestPinShortcutSupported()) {
                addShortcutWithShortcutManager(title, icon, shortcutIntent);
                return;
            }
            Intent intent = createAddToHomeIntent(title, icon, shortcutIntent);
            ContextUtils.getApplicationContext().sendBroadcast(intent);
        }

        /**
         * Returns the name of the fullscreen Activity to use when launching shortcuts.
         */
        public String getFullscreenAction() {
            return WebappLauncherActivity.ACTION_START_WEBAPP;
        }
    }

    private static Delegate sDelegate = new Delegate();

    /**
     * Sets the delegate to use.
     */
    @VisibleForTesting
    public static void setDelegateForTests(Delegate delegate) {
        sDelegate = delegate;
    }

    /**
     * Adds home screen shortcut which opens in a {@link WebappActivity}. Creates web app
     * home screen shortcut and registers web app asynchronously. Calls
     * ShortcutHelper::OnWebappDataStored() when done.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private static void addWebapp(final String id, final String url, final String scopeUrl,
            final String userTitle, final String name, final String shortName, final String iconUrl,
            final Bitmap icon, @WebDisplayMode final int displayMode, final int orientation,
            final int source, final long themeColor, final long backgroundColor,
            final String splashScreenUrl, final long callbackPointer) {
        new AsyncTask<Intent>() {
            @Override
            protected Intent doInBackground() {
                // Encoding {@link icon} as a string and computing the mac are expensive.

                Context context = ContextUtils.getApplicationContext();

                // Encode the icon as a base64 string (Launcher drops Bitmaps in the Intent).
                String encodedIcon = encodeBitmapAsString(icon);

                String nonEmptyScopeUrl =
                        TextUtils.isEmpty(scopeUrl) ? getScopeFromUrl(url) : scopeUrl;
                Intent shortcutIntent = createWebappShortcutIntent(id,
                        sDelegate.getFullscreenAction(), url, nonEmptyScopeUrl, name, shortName,
                        encodedIcon, WEBAPP_SHORTCUT_VERSION, displayMode, orientation, themeColor,
                        backgroundColor, splashScreenUrl, iconUrl.isEmpty());
                shortcutIntent.putExtra(EXTRA_MAC, getEncodedMac(context, url));
                shortcutIntent.putExtra(EXTRA_SOURCE, source);
                return shortcutIntent;
            }
            @Override
            protected void onPostExecute(final Intent resultIntent) {
                sDelegate.addShortcutToHomescreen(userTitle, icon, resultIntent);

                // Store the webapp data so that it is accessible without the intent. Once this
                // process is complete, call back to native code to start the splash image
                // download.
                WebappRegistry.getInstance().register(
                        id, storage -> {
                            storage.updateFromShortcutIntent(resultIntent);
                            nativeOnWebappDataStored(callbackPointer);
                        });
                if (shouldShowToastWhenAddingShortcut()) {
                    showAddedToHomescreenToast(userTitle);
                }
            }
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Adds home screen shortcut which opens in the browser Activity.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private static void addShortcut(
            String id, String url, String userTitle, Bitmap icon, int source) {
        Context context = ContextUtils.getApplicationContext();
        final Intent shortcutIntent = createShortcutIntent(url);
        shortcutIntent.putExtra(EXTRA_ID, id);
        shortcutIntent.putExtra(EXTRA_SOURCE, source);
        shortcutIntent.setPackage(context.getPackageName());
        sDelegate.addShortcutToHomescreen(userTitle, icon, shortcutIntent);
        if (shouldShowToastWhenAddingShortcut()) {
            showAddedToHomescreenToast(userTitle);
        }
    }

    @TargetApi(Build.VERSION_CODES.O)
    private static void addShortcutWithShortcutManager(
            String title, Bitmap icon, Intent shortcutIntent) {
        String id = shortcutIntent.getStringExtra(ShortcutHelper.EXTRA_ID);
        Context context = ContextUtils.getApplicationContext();

        ShortcutInfo shortcutInfo = new ShortcutInfo.Builder(context, id)
                                            .setShortLabel(title)
                                            .setLongLabel(title)
                                            .setIcon(Icon.createWithBitmap(icon))
                                            .setIntent(shortcutIntent)
                                            .build();
        try {
            sShortcutManager.requestPinShortcut(shortcutInfo, null);
        } catch (IllegalStateException e) {
            Log.d(TAG,
                    "Could not create pinned shortcut: device is locked, or "
                            + "activity is backgrounded.");
        }
    }

    /**
     * Show toast to alert user that the shortcut was added to the home screen.
     */
    private static void showAddedToHomescreenToast(final String title) {
        Context applicationContext = ContextUtils.getApplicationContext();
        String toastText = applicationContext.getString(R.string.added_to_homescreen, title);
        showToast(toastText);
    }

    /**
     * Shows toast notifying user that a WebAPK install is already in progress when user tries to
     * queue a new install for the same WebAPK.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private static void showWebApkInstallInProgressToast() {
        Context applicationContext = ContextUtils.getApplicationContext();
        String toastText = applicationContext.getString(R.string.webapk_install_in_progress);
        showToast(toastText);
    }

    public static void showToast(String text) {
        assert ThreadUtils.runningOnUiThread();
        Toast toast =
                Toast.makeText(ContextUtils.getApplicationContext(), text, Toast.LENGTH_SHORT);
        toast.show();
    }

    /**
     * Stores the specified bitmap as the splash screen for a web app.
     * @param id          ID of the web app which is storing data.
     * @param splashImage Image which should be displayed on the splash screen of
     *                    the web app. This can be null of there is no image to show.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private static void storeWebappSplashImage(final String id, final Bitmap splashImage) {
        final WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage(id);
        if (storage != null) {
            new AsyncTask<String>() {
                @Override
                protected String doInBackground() {
                    return encodeBitmapAsString(splashImage);
                }

                @Override
                protected void onPostExecute(String encodedImage) {
                    storage.updateSplashScreenImage(encodedImage);
                }
            }
                    .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }
    }

    /**
     * Creates an intent that will add a shortcut to the home screen.
     * @param title          Title of the shortcut.
     * @param icon           Image that represents the shortcut.
     * @param shortcutIntent Intent to fire when the shortcut is activated.
     * @return Intent for the shortcut.
     */
    public static Intent createAddToHomeIntent(String title, Bitmap icon, Intent shortcutIntent) {
        Intent i = new Intent(INSTALL_SHORTCUT);
        i.putExtra(Intent.EXTRA_SHORTCUT_INTENT, shortcutIntent);
        i.putExtra(Intent.EXTRA_SHORTCUT_NAME, title);
        i.putExtra(Intent.EXTRA_SHORTCUT_ICON, icon);
        return i;
    }

    /**
     * Creates a shortcut to launch a web app on the home screen.
     * @param id              Id of the web app.
     * @param action          Intent action to open a full screen activity.
     * @param url             Url of the web app.
     * @param scope           Url scope of the web app.
     * @param name            Name of the web app.
     * @param shortName       Short name of the web app.
     * @param encodedIcon     Base64 encoded icon of the web app.
     * @param version         Version number of the shortcut.
     * @param displayMode     Display mode of the web app.
     * @param orientation     Orientation of the web app.
     * @param themeColor      Theme color of the web app.
     * @param backgroundColor Background color of the web app.
     * @param splashScreenUrl Url of the HTML splash screen.
     * @param isIconGenerated True if the icon is generated by Chromium.
     * @return Intent for onclick action of the shortcut.
     * This method must not be called on the UI thread.
     */
    public static Intent createWebappShortcutIntent(String id, String action, String url,
            String scope, String name, String shortName, String encodedIcon, int version,
            @WebDisplayMode int displayMode, int orientation, long themeColor, long backgroundColor,
            String splashScreenUrl, boolean isIconGenerated) {
        // Create an intent as a launcher icon for a full-screen Activity.
        Intent shortcutIntent = new Intent();
        shortcutIntent.setPackage(ContextUtils.getApplicationContext().getPackageName())
                .setAction(action)
                .putExtra(EXTRA_ID, id)
                .putExtra(EXTRA_URL, url)
                .putExtra(EXTRA_SCOPE, scope)
                .putExtra(EXTRA_NAME, name)
                .putExtra(EXTRA_SHORT_NAME, shortName)
                .putExtra(EXTRA_ICON, encodedIcon)
                .putExtra(EXTRA_VERSION, version)
                .putExtra(EXTRA_DISPLAY_MODE, displayMode)
                .putExtra(EXTRA_ORIENTATION, orientation)
                .putExtra(EXTRA_THEME_COLOR, themeColor)
                .putExtra(EXTRA_BACKGROUND_COLOR, backgroundColor)
                .putExtra(EXTRA_SPLASH_SCREEN_URL, splashScreenUrl)
                .putExtra(EXTRA_IS_ICON_GENERATED, isIconGenerated);
        return shortcutIntent;
    }

    /**
     * Creates an intent with mostly empty parameters for launching a web app on the homescreen.
     * @param id              Id of the web app.
     * @param url             Url of the web app.
     * @return the Intent
     * This method must not be called on the UI thread.
     */
    public static Intent createWebappShortcutIntentForTesting(String id, String url) {
        return createWebappShortcutIntent(id, null, url, getScopeFromUrl(url), null, null, null,
                WEBAPP_SHORTCUT_VERSION, WebDisplayMode.STANDALONE, 0, 0, 0, null, false);
    }

    /**
     * Shortcut intent for icon on home screen.
     * @param url Url of the shortcut.
     * @return Intent for onclick action of the shortcut.
     */
    public static Intent createShortcutIntent(String url) {
        Intent shortcutIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        shortcutIntent.putExtra(REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        return shortcutIntent;
    }

    /**
     * Utility method to check if a shortcut can be added to the home screen.
     * @return if a shortcut can be added to the home screen under the current profile.
     */
    // TODO(crbug.com/635567): Fix this properly.
    @SuppressLint("WrongConstant")
    public static boolean isAddToHomeIntentSupported() {
        if (isRequestPinShortcutSupported()) return true;
        PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
        Intent i = new Intent(INSTALL_SHORTCUT);
        List<ResolveInfo> receivers =
                pm.queryBroadcastReceivers(i, PackageManager.GET_INTENT_FILTERS);
        return !receivers.isEmpty();
    }

    /**
     * Returns whether the given icon matches the size requirements to be used on the home screen.
     * @param width  Icon width, in pixels.
     * @param height Icon height, in pixels.
     * @return whether the given icon matches the size requirements to be used on the home screen.
     */
    @CalledByNative
    public static boolean isIconLargeEnoughForLauncher(int width, int height) {
        Context context = ContextUtils.getApplicationContext();
        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        final int minimalSize = am.getLauncherLargeIconSize() / 2;
        return width >= minimalSize && height >= minimalSize;
    }

    /**
     * Adapts a website's icon (e.g. favicon or touch icon) to make it suitable for the home screen.
     * This involves adding padding if the icon is a full sized square.
     *
     * @param context Context used to create the intent.
     * @param webIcon The website's favicon or touch icon.
     * @return Bitmap Either the touch-icon or the newly created favicon.
     */
    @CalledByNative
    public static Bitmap createHomeScreenIconFromWebIcon(Bitmap webIcon) {
        // getLauncherLargeIconSize() is just a guess at the launcher icon size, and is often
        // wrong -- the launcher can show icons at any size it pleases. Instead of resizing the
        // icon to the supposed launcher size and then having the launcher resize the icon again,
        // just leave the icon at its original size and let the launcher do a single rescaling.
        // Unless the icon is much too big; then scale it down here too.
        Context context = ContextUtils.getApplicationContext();
        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        int maxInnerSize = Math.round(am.getLauncherLargeIconSize() * MAX_INNER_SIZE_RATIO);
        int innerSize = Math.min(maxInnerSize, Math.max(webIcon.getWidth(), webIcon.getHeight()));

        int outerSize = innerSize;
        Rect innerBounds = new Rect(0, 0, innerSize, innerSize);

        // Draw the icon with padding around it if all four corners are not transparent. Otherwise,
        // don't add padding.
        if (shouldPadIcon(webIcon)) {
            int padding = Math.round(ICON_PADDING_RATIO * innerSize);
            outerSize += 2 * padding;
            innerBounds.offset(padding, padding);
        }

        Bitmap bitmap = null;
        try {
            bitmap = Bitmap.createBitmap(outerSize, outerSize, Bitmap.Config.ARGB_8888);
        } catch (OutOfMemoryError e) {
            Log.e(TAG, "OutOfMemoryError while creating bitmap for home screen icon.");
            return webIcon;
        }

        Canvas canvas = new Canvas(bitmap);
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setFilterBitmap(true);
        canvas.drawBitmap(webIcon, null, innerBounds, paint);

        return bitmap;
    }

    /**
     * Generates a generic icon to be used in the launcher. This is just a rounded rectangle with
     * a letter in the middle taken from the website's domain name.
     *
     * @param url   URL of the shortcut.
     * @param red   Red component of the dominant icon color.
     * @param green Green component of the dominant icon color.
     * @param blue  Blue component of the dominant icon color.
     * @return Bitmap Either the touch-icon or the newly created favicon.
     */
    @CalledByNative
    public static Bitmap generateHomeScreenIcon(String url, int red, int green, int blue) {
        Context context = ContextUtils.getApplicationContext();
        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        final int outerSize = am.getLauncherLargeIconSize();
        final int iconDensity = am.getLauncherLargeIconDensity();

        Bitmap bitmap = null;
        try {
            bitmap = Bitmap.createBitmap(outerSize, outerSize, Bitmap.Config.ARGB_8888);
        } catch (OutOfMemoryError e) {
            Log.w(TAG, "OutOfMemoryError while trying to draw bitmap on canvas.");
            return null;
        }

        Canvas canvas = new Canvas(bitmap);

        // Draw the drop shadow.
        int padding = (int) (GENERATED_ICON_PADDING_RATIO * outerSize);
        Rect outerBounds = new Rect(0, 0, outerSize, outerSize);
        Bitmap iconShadow =
                getBitmapFromResourceId(context, R.mipmap.shortcut_icon_shadow, iconDensity);
        Paint paint = new Paint(Paint.FILTER_BITMAP_FLAG);
        canvas.drawBitmap(iconShadow, null, outerBounds, paint);

        // Draw the rounded rectangle and letter.
        int innerSize = outerSize - 2 * padding;
        int cornerRadius = Math.round(ICON_CORNER_RADIUS_RATIO * outerSize);
        int fontSize = Math.round(GENERATED_ICON_FONT_SIZE_RATIO * outerSize);
        int color = Color.rgb(red, green, blue);
        RoundedIconGenerator generator = new RoundedIconGenerator(
                innerSize, innerSize, cornerRadius, color, fontSize);
        Bitmap icon = generator.generateIconForUrl(url);
        if (icon == null) return null; // Bookmark URL does not have a domain.
        canvas.drawBitmap(icon, padding, padding, null);

        return bitmap;
    }

    /**
     * Returns the package name of the WebAPK if WebAPKs are enabled and there is an installed
     * WebAPK which can handle {@link url}. Returns null otherwise.
     */
    @CalledByNative
    private static String queryWebApkPackage(String url) {
        return WebApkValidator.queryWebApkPackage(ContextUtils.getApplicationContext(), url);
    }

    /**
     * Compresses a bitmap into a PNG and converts into a Base64 encoded string.
     * The encoded string can be decoded using {@link decodeBitmapFromString(String)}.
     * @param bitmap The Bitmap to compress and encode.
     * @return the String encoding the Bitmap.
     */
    public static String encodeBitmapAsString(Bitmap bitmap) {
        if (bitmap == null) return "";
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.PNG, 100, output);
        return Base64.encodeToString(output.toByteArray(), Base64.DEFAULT);
    }

    /**
     * Decodes a Base64 string into a Bitmap. Used to decode Bitmaps encoded by
     * {@link encodeBitmapAsString(Bitmap)}.
     * @param encodedString the Base64 String to decode.
     * @return the Bitmap which was encoded by the String.
     */
    public static Bitmap decodeBitmapFromString(String encodedString) {
        if (TextUtils.isEmpty(encodedString)) return null;
        byte[] decoded = Base64.decode(encodedString, Base64.DEFAULT);
        return BitmapFactory.decodeByteArray(decoded, 0, decoded.length);
    }

    /**
     * Returns the ideal size for an icon representing a web app.  This size is used on app banners,
     * the Android Home screen, and in Android's recent tasks list, among other places.
     * @param context Context to pull resources from.
     * @return the dimensions in pixels which the icon should have.
     */
    public static int getIdealHomescreenIconSizeInPx(Context context) {
        return getSizeFromResourceInPx(context, R.dimen.webapp_home_screen_icon_size);
    }

    /**
     * Returns the minimum size for an icon representing a web app.  This size is used on app
     * banners, the Android Home screen, and in Android's recent tasks list, among other places.
     * @param context Context to pull resources from.
     * @return the lower bound of the size which the icon should have in pixels.
     */
    public static int getMinimumHomescreenIconSizeInPx(Context context) {
        float sizeInPx = context.getResources().getDimension(R.dimen.webapp_home_screen_icon_size);
        float density = context.getResources().getDisplayMetrics().density;
        float idealIconSizeInDp = sizeInPx / density;

        return Math.round(idealIconSizeInDp * (density - 1));
    }

    /**
     * Returns the ideal size for an image displayed on a web app's splash screen.
     * @param context Context to pull resources from.
     * @return the dimensions in pixels which the image should have.
     */
    public static int getIdealSplashImageSizeInPx(Context context) {
        return getSizeFromResourceInPx(context, R.dimen.webapp_splash_image_size_ideal);
    }

    /**
     * Returns the minimum size for an image displayed on a web app's splash screen.
     * @param context Context to pull resources from.
     * @return the lower bound of the size which the image should have in pixels.
     */
    public static int getMinimumSplashImageSizeInPx(Context context) {
        return getSizeFromResourceInPx(context, R.dimen.webapp_splash_image_size_minimum);
    }

    /**
     * Returns the ideal size for a badge icon of a WebAPK.
     * @param context Context to pull resources from.
     * @return the dimensions in pixels which the badge icon should have.
     */
    public static int getIdealBadgeIconSizeInPx(Context context) {
        return getSizeFromResourceInPx(context, R.dimen.webapk_badge_icon_size);
    }

    /**
     * @return String that can be used to verify that a WebappActivity is being started by Chrome.
     */
    public static String getEncodedMac(Context context, String url) {
        // The only reason we convert to a String here is because Android inexplicably eats a
        // byte[] when adding the shortcut -- the Bundle received by the launched Activity even
        // lacks the key for the extra.
        byte[] mac = WebappAuthenticator.getMacForUrl(context, url);
        return Base64.encodeToString(mac, Base64.DEFAULT);
    }

    /**
     * Generates a scope URL based on the passed in URL. It should be used if the Web Manifest
     * does not specify a scope URL.
     * @param url The url to convert to a scope.
     * @return The scope.
     */
    @CalledByNative
    public static String getScopeFromUrl(String url) {
        // Scope URL is generated by:
        // - Removing last component of the URL if it does not end with a slash.
        // - Clearing the URL's query and fragment.

        Uri uri = Uri.parse(url);
        String path = uri.getEncodedPath();

        // Remove the last path element if there is at least one path element, *and* the path does
        // not end with a slash. This means that URLs to specific files have the file component
        // removed, but URLs to directories retain the directory.
        int lastSlashIndex = (path == null) ? -1 : path.lastIndexOf("/");
        if (lastSlashIndex < 0) {
            path = "/";
        } else if (lastSlashIndex < path.length() - 1) {
            path = path.substring(0, lastSlashIndex + 1);
        }

        Uri.Builder builder = uri.buildUpon();
        builder.encodedPath(path);
        builder.fragment("");
        builder.query("");
        return builder.build().toString();
    }

    /**
     * Returns an array of sizes which describe the ideal size and minimum size of the Home screen
     * icon and the ideal and minimum sizes of the splash screen image in that order.
     */
    @CalledByNative
    private static int[] getHomeScreenIconAndSplashImageSizes() {
        Context context = ContextUtils.getApplicationContext();
        // This ordering must be kept up to date with the C++ ShortcutHelper.
        return new int[] {
            getIdealHomescreenIconSizeInPx(context),
            getMinimumHomescreenIconSizeInPx(context),
            getIdealSplashImageSizeInPx(context),
            getMinimumSplashImageSizeInPx(context),
            getIdealBadgeIconSizeInPx(context)
        };
    }

    /**
     * Returns true if we should add padding to this icon. We use a heuristic that if the pixels in
     * all four corners of the icon are not transparent, we assume the icon is square and maximally
     * sized, i.e. in need of padding. Otherwise, no padding is added.
     */
    private static boolean shouldPadIcon(Bitmap icon) {
        int maxX = icon.getWidth() - 1;
        int maxY = icon.getHeight() - 1;

        if ((Color.alpha(icon.getPixel(0, 0)) != 0) && (Color.alpha(icon.getPixel(maxX, maxY)) != 0)
                && (Color.alpha(icon.getPixel(0, maxY)) != 0)
                && (Color.alpha(icon.getPixel(maxX, 0)) != 0)) {
            return true;
        }
        return false;
    }

    private static boolean shouldShowToastWhenAddingShortcut() {
        return !isRequestPinShortcutSupported();
    }

    private static boolean isRequestPinShortcutSupported() {
        if (!sCheckedIfRequestPinShortcutSupported) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                checkIfRequestPinShortcutSupported();
            }
            sCheckedIfRequestPinShortcutSupported = true;
        }
        return sIsRequestPinShortcutSupported;
    }

    @TargetApi(Build.VERSION_CODES.O)
    private static void checkIfRequestPinShortcutSupported() {
        sShortcutManager =
                ContextUtils.getApplicationContext().getSystemService(ShortcutManager.class);
        try (StrictModeContext unused = StrictModeContext.allowDiskReads()) {
            sIsRequestPinShortcutSupported = sShortcutManager.isRequestPinShortcutSupported();
        }
    }

    private static int getSizeFromResourceInPx(Context context, int resource) {
        return Math.round(context.getResources().getDimension(resource));
    }

    private static Bitmap getBitmapFromResourceId(Context context, int id, int density) {
        Drawable drawable = ApiCompatibilityUtils.getDrawableForDensity(
                context.getResources(), id, density);

        if (drawable instanceof BitmapDrawable) {
            BitmapDrawable bd = (BitmapDrawable) drawable;
            return bd.getBitmap();
        }
        assert false : "The drawable was not a bitmap drawable as expected";
        return null;
    }

    /**
     * Calls the native |callbackPointer| with lists of information on all installed WebAPKs.
     *
     * @param callbackPointer Callback to call with the information on the WebAPKs found.
     */
    @CalledByNative
    public static void retrieveWebApks(long callbackPointer) {
        List<String> names = new ArrayList<>();
        List<String> shortNames = new ArrayList<>();
        List<String> packageNames = new ArrayList<>();
        List<Integer> shellApkVersions = new ArrayList<>();
        List<Integer> versionCodes = new ArrayList<>();
        List<String> uris = new ArrayList<>();
        List<String> scopes = new ArrayList<>();
        List<String> manifestUrls = new ArrayList<>();
        List<String> manifestStartUrls = new ArrayList<>();
        List<Integer> displayModes = new ArrayList<>();
        List<Integer> orientations = new ArrayList<>();
        List<Long> themeColors = new ArrayList<>();
        List<Long> backgroundColors = new ArrayList<>();
        List<Long> lastUpdateCheckTimesMs = new ArrayList<>();
        List<Boolean> relaxUpdates = new ArrayList<>();

        Context context = ContextUtils.getApplicationContext();
        PackageManager packageManager = context.getPackageManager();
        for (PackageInfo packageInfo : packageManager.getInstalledPackages(0)) {
            if (WebApkValidator.isValidWebApk(context, packageInfo.packageName)) {
                // Pass non-null URL parameter so that {@link WebApkInfo#create()}
                // return value is non-null
                WebApkInfo webApkInfo =
                        WebApkInfo.create(packageInfo.packageName, "", ShortcutSource.UNKNOWN,
                                false /* forceNavigation */, false /* useTransparentSplash */);
                if (webApkInfo != null) {
                    names.add(webApkInfo.name());
                    shortNames.add(webApkInfo.shortName());
                    packageNames.add(webApkInfo.apkPackageName());
                    shellApkVersions.add(webApkInfo.shellApkVersion());
                    versionCodes.add(packageInfo.versionCode);
                    uris.add(webApkInfo.uri().toString());
                    scopes.add(webApkInfo.scopeUri().toString());
                    manifestUrls.add(webApkInfo.manifestUrl());
                    manifestStartUrls.add(webApkInfo.manifestStartUrl());
                    displayModes.add(webApkInfo.displayMode());
                    orientations.add(webApkInfo.orientation());
                    themeColors.add(webApkInfo.themeColor());
                    backgroundColors.add(webApkInfo.backgroundColor());

                    WebappDataStorage storage =
                            WebappRegistry.getInstance().getWebappDataStorage(webApkInfo.id());
                    long lastUpdateCheckTimeMsForStorage = 0;
                    boolean relaxUpdatesForStorage = false;
                    if (storage != null) {
                        lastUpdateCheckTimeMsForStorage =
                                storage.getLastCheckForWebManifestUpdateTimeMs();
                        relaxUpdatesForStorage = storage.shouldRelaxUpdates();
                    }
                    lastUpdateCheckTimesMs.add(lastUpdateCheckTimeMsForStorage);
                    relaxUpdates.add(relaxUpdatesForStorage);
                }
            }
        }
        nativeOnWebApksRetrieved(callbackPointer, names.toArray(new String[0]),
                shortNames.toArray(new String[0]), packageNames.toArray(new String[0]),
                CollectionUtil.integerListToIntArray(shellApkVersions),
                CollectionUtil.integerListToIntArray(versionCodes), uris.toArray(new String[0]),
                scopes.toArray(new String[0]), manifestUrls.toArray(new String[0]),
                manifestStartUrls.toArray(new String[0]),
                CollectionUtil.integerListToIntArray(displayModes),
                CollectionUtil.integerListToIntArray(orientations),
                CollectionUtil.longListToLongArray(themeColors),
                CollectionUtil.longListToLongArray(backgroundColors),
                CollectionUtil.longListToLongArray(lastUpdateCheckTimesMs),
                CollectionUtil.booleanListToBooleanArray(relaxUpdates));
    }

    private static native void nativeOnWebappDataStored(long callbackPointer);
    private static native void nativeOnWebApksRetrieved(long callbackPointer, String[] names,
            String[] shortNames, String[] packageName, int[] shellApkVersions, int[] versionCodes,
            String[] uris, String[] scopes, String[] manifestUrls, String[] manifestStartUrls,
            int[] displayModes, int[] orientations, long[] themeColors, long[] backgroundColors,
            long[] lastUpdateCheckTimesMs, boolean[] relaxUpdates);
}
