// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.graphics.Bitmap;
import android.net.Uri;
import android.support.customtabs.CustomTabsSessionToken;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.content_public.common.ScreenOrientationValues;

/**
 * Stores info about a web app.
 */
public class WebappInfo {
    private static final String TAG = "WebappInfo";

    /**
     * Parameter for {@link WebappInfo#create} method which allows either a Bitmap or a PNG
     * encoded string to be passed as a parameter.
     */
    public static class Icon {
        private String mEncoded;
        private Bitmap mDecoded;

        public Icon(String encoded) {
            mEncoded = encoded;
        }

        public Icon(Bitmap decoded) {
            mDecoded = decoded;
        }

        public String encoded() {
            if (mEncoded == null) {
                mEncoded = ShortcutHelper.encodeBitmapAsString(mDecoded);
            }
            return mEncoded;
        }

        public Bitmap decoded() {
            if (mDecoded == null) {
                mDecoded = ShortcutHelper.decodeBitmapFromString(mEncoded);
            }
            return mDecoded;
        }
    }

    private boolean mIsInitialized;
    private String mId;
    private Icon mIcon;
    private Uri mUri;
    private Uri mScopeUri;
    private String mName;
    private String mShortName;
    private @WebDisplayMode int mDisplayMode;
    private int mOrientation;
    private int mSource;
    private long mThemeColor;
    private long mBackgroundColor;
    private Uri mSplashScreenUri;
    private boolean mIsIconGenerated;
    private boolean mForceNavigation;

    public static WebappInfo createEmpty() {
        return new WebappInfo();
    }

    protected static String urlFromIntent(Intent intent) {
        return IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_URL);
    }

    protected static int sourceFromIntent(Intent intent) {
        int source = IntentUtils.safeGetIntExtra(
                intent, ShortcutHelper.EXTRA_SOURCE, ShortcutSource.UNKNOWN);
        if (source >= ShortcutSource.COUNT) {
            source = ShortcutSource.UNKNOWN;
        }
        return source;
    }

    private static String titleFromIntent(Intent intent) {
        // The reference to title has been kept for reasons of backward compatibility. For intents
        // and shortcuts which were created before we utilized the concept of name and shortName,
        // we set the name and shortName to be the title.
        String title = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_TITLE);
        return title == null ? "" : title;
    }

    private static String nameFromIntent(Intent intent) {
        String name = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_NAME);
        return name == null ? titleFromIntent(intent) : name;
    }

    private static String shortNameFromIntent(Intent intent) {
        String shortName = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_SHORT_NAME);
        return shortName == null ? titleFromIntent(intent) : shortName;
    }

    public static WebappInfo create(Intent intent, CustomTabsSessionToken session) {
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        String url = intent.getDataString();

        PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
        String packageName = connection.getClientPackageNameForSession(session);
        intent.putExtra(ShortcutHelper.EXTRA_ID, packageName);
        intent.putExtra(ShortcutHelper.EXTRA_URL, url);
        // TODO(yusufo): This should be based on scope defined by Android Manifest intent filters.
        intent.putExtra(ShortcutHelper.EXTRA_SCOPE, url);
        intent.putExtra(ShortcutHelper.EXTRA_SOURCE, ShortcutSource.TRUSTED_WEB_ACTIVITY);
        intent.setClassName(ContextUtils.getApplicationContext(), WebappActivity.class.getName());
        ApplicationInfo info = null;
        try {
            info = pm.getApplicationInfo(packageName, 0);
            if (info != null) {
                String label = pm.getApplicationLabel(info).toString();
                intent.putExtra(ShortcutHelper.EXTRA_NAME, label);
                intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, label);
            }
        } catch (NameNotFoundException e) {
            // Failing gracefully below. This is a best effort.
        }

        return create(intent);
    }

    /**
     * Construct a WebappInfo.
     * @param intent Intent containing info about the app.
     */
    public static WebappInfo create(Intent intent) {
        String id = idFromIntent(intent);
        String url = urlFromIntent(intent);

        String icon = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_ICON);
        String scope = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_SCOPE);
        @WebDisplayMode
        int displayMode = IntentUtils.safeGetIntExtra(
                intent, ShortcutHelper.EXTRA_DISPLAY_MODE, WebDisplayMode.STANDALONE);
        int orientation = IntentUtils.safeGetIntExtra(
                intent, ShortcutHelper.EXTRA_ORIENTATION, ScreenOrientationValues.DEFAULT);
        int source = sourceFromIntent(intent);
        long themeColor = IntentUtils.safeGetLongExtra(intent,
                ShortcutHelper.EXTRA_THEME_COLOR,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING);
        long backgroundColor = IntentUtils.safeGetLongExtra(intent,
                ShortcutHelper.EXTRA_BACKGROUND_COLOR,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING);
        String splashScreenUrl =
                IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_SPLASH_SCREEN_URL);
        boolean isIconGenerated = IntentUtils.safeGetBooleanExtra(intent,
                ShortcutHelper.EXTRA_IS_ICON_GENERATED, false);
        boolean forceNavigation = IntentUtils.safeGetBooleanExtra(
                intent, ShortcutHelper.EXTRA_FORCE_NAVIGATION, false);

        String name = nameFromIntent(intent);
        String shortName = shortNameFromIntent(intent);

        return create(id, url, scope, new Icon(icon), name, shortName, displayMode, orientation,
                source, themeColor, backgroundColor, splashScreenUrl, isIconGenerated,
                forceNavigation);
    }

    /**
     * Construct a WebappInfo.
     * @param id              ID for the webapp.
     * @param url             URL for the webapp.
     * @param scope           Scope for the webapp.
     * @param icon            Icon to show for the webapp.
     * @param name            Name of the webapp.
     * @param shortName       The short name of the webapp.
     * @param displayMode     Display mode of the webapp.
     * @param orientation     Orientation of the webapp.
     * @param source          Source where the webapp was added from.
     * @param themeColor      The theme color of the webapp.
     * @param backgroundColor The background color of the webapp.
     * @param splashScreenUrl URL of the HTML splash screen.
     * @param isIconGenerated Whether the |icon| was generated by Chromium.
     * @param forceNavigation Whether the webapp should navigate to {@link url} if the
     *                        webapp is already open.
     */
    public static WebappInfo create(String id, String url, String scope, Icon icon, String name,
            String shortName, @WebDisplayMode int displayMode, int orientation, int source,
            long themeColor, long backgroundColor, String splashScreenUrl, boolean isIconGenerated,
            boolean forceNavigation) {
        if (id == null || url == null) {
            Log.e(TAG, "Incomplete data provided: " + id + ", " + url);
            return null;
        }

        return new WebappInfo(id, url, scope, icon, name, shortName, displayMode, orientation,
                source, themeColor, backgroundColor, splashScreenUrl, isIconGenerated,
                forceNavigation);
    }

    protected WebappInfo(String id, String url, String scope, Icon icon, String name,
            String shortName, @WebDisplayMode int displayMode, int orientation, int source,
            long themeColor, long backgroundColor, String splashScreenUrl, boolean isIconGenerated,
            boolean forceNavigation) {
        Uri uri = Uri.parse(url);
        if (TextUtils.isEmpty(scope)) {
            scope = ShortcutHelper.getScopeFromUrl(url);
        }
        Uri scopeUri = Uri.parse(scope);

        mIcon = icon;
        mId = id;
        mName = name;
        mShortName = shortName;
        mUri = uri;
        mScopeUri = scopeUri;
        mDisplayMode = displayMode;
        mOrientation = orientation;
        mSource = source;
        mThemeColor = themeColor;
        mBackgroundColor = backgroundColor;
        mSplashScreenUri = Uri.parse(splashScreenUrl != null ? splashScreenUrl : "");
        mIsIconGenerated = isIconGenerated;
        mForceNavigation = forceNavigation;
        mIsInitialized = mUri != null;
    }

    protected WebappInfo() {
    }

    public boolean isInitialized() {
        return mIsInitialized;
    }

    public String id() {
        return mId;
    }

    public Uri uri() {
        return mUri;
    }

    /**
     * Whether the webapp should be navigated to {@link uri()} if the webapp is already open when
     * Chrome receives a ACTION_START_WEBAPP intent.
     */
    public boolean shouldForceNavigation() {
        return mForceNavigation;
    }

    // TODO(yusufo) : Plumb the scope for the Webapp through the support library/client Android
    // manifest for TrustedWebActivity.
    public Uri scopeUri() {
        return mScopeUri;
    }

    public String name() {
        return mName;
    }

    public String shortName() {
        return mShortName;
    }

    public @WebDisplayMode int displayMode() {
        return mDisplayMode;
    }

    public String apkPackageName() {
        return null;
    }

    public int orientation() {
        return mOrientation;
    }

    public int source() {
        return mSource;
    }

    /**
     * Theme color is actually a 32 bit unsigned integer which encodes a color
     * in ARGB format. mThemeColor is a long because we also need to encode the
     * error state of ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING.
     */
    public long themeColor() {
        return mThemeColor;
    }

    /**
     * Returns whether the theme color specified in the Intent is valid.
     * A theme color isn't valid if its value is ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING;
     */
    public boolean hasValidThemeColor() {
        return mThemeColor != ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING;
    }

    /**
     * Background color is actually a 32 bit unsigned integer which encodes a color
     * in ARGB format. mBackgroundColor is a long because we also need to encode the
     * error state of ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING.
     */
    public long backgroundColor() {
        return mBackgroundColor;
    }

    /**
     * Returns whether the background color specified in the Intent is valid.
     * A background color isn't valid if its value is
     * ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING.
     */
    public boolean hasValidBackgroundColor() {
        return mBackgroundColor != ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING;
    }

    /**
     * Returns the background color specified by {@link #backgroundColor()} if
     * the value is valid. Returns the specified fallback color otherwise.
     */
    public int backgroundColor(int fallback) {
        return hasValidBackgroundColor() ? (int) mBackgroundColor : fallback;
    }

    /**
     * Returns the Splash Screen URL, or empty if not specified.
     */
    public Uri splashScreenUri() {
        return mSplashScreenUri;
    }

    /**
     * Returns whether a non-empty Splash Screen URL has been specified.
     */
    public boolean hasSplashScreenUri() {
        return !mSplashScreenUri.equals(Uri.EMPTY);
    }

    // This is needed for clients that want to send the icon through an intent.
    public String encodedIcon() {
        return (mIcon == null) ? null : mIcon.encoded();
    }

    /**
     * Returns the icon in Bitmap form.
     */
    public Bitmap icon() {
        // TODO(yusufo) : Add a way to plumb this through for Trusted Web Activity.
        return (mIcon == null) ? null : mIcon.decoded();
    }

    /**
     * Returns whether the icon was generated by Chromium.
     */
    public boolean isIconGenerated() {
        return mIsIconGenerated;
    }

    /** Returns whether the WebappActivity should be transparent while the page is loading. */
    public boolean useTransparentSplash() {
        return false;
    }

    /**
     * Sets extras on an Intent that will launch a WebappActivity.
     * @param intent Intent that will be used to launch a WebappActivity.
     */
    public void setWebappIntentExtras(Intent intent) {
        intent.putExtra(ShortcutHelper.EXTRA_ID, id());
        intent.putExtra(ShortcutHelper.EXTRA_URL, uri().toString());
        intent.putExtra(ShortcutHelper.EXTRA_FORCE_NAVIGATION, shouldForceNavigation());
        intent.putExtra(ShortcutHelper.EXTRA_SCOPE, scopeUri().toString());
        intent.putExtra(ShortcutHelper.EXTRA_ICON, encodedIcon());
        intent.putExtra(ShortcutHelper.EXTRA_VERSION, ShortcutHelper.WEBAPP_SHORTCUT_VERSION);
        intent.putExtra(ShortcutHelper.EXTRA_NAME, name());
        intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName());
        intent.putExtra(ShortcutHelper.EXTRA_DISPLAY_MODE, displayMode());
        intent.putExtra(ShortcutHelper.EXTRA_ORIENTATION, orientation());
        intent.putExtra(ShortcutHelper.EXTRA_SOURCE, source());
        intent.putExtra(ShortcutHelper.EXTRA_THEME_COLOR, themeColor());
        intent.putExtra(ShortcutHelper.EXTRA_BACKGROUND_COLOR, backgroundColor());
        intent.putExtra(ShortcutHelper.EXTRA_SPLASH_SCREEN_URL, splashScreenUri().toString());
        intent.putExtra(ShortcutHelper.EXTRA_IS_ICON_GENERATED, isIconGenerated());
    }

    public static String idFromIntent(Intent intent) {
        return IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_ID);
    }
    /**
     * Returns true if the WebappInfo was created for an Intent fired from a launcher shortcut (as
     * opposed to an intent from a push notification or other internal source).
     */
    public boolean isLaunchedFromHomescreen() {
        int source = source();
        return source != ShortcutSource.NOTIFICATION && source != ShortcutSource.EXTERNAL_INTENT
                && source != ShortcutSource.EXTERNAL_INTENT_FROM_CHROME
                && source != ShortcutSource.WEBAPK_SHARE_TARGET;
    }
}
