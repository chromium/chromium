// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.Message;
import android.os.Process;
import android.provider.Settings;
import android.webkit.WebSettings;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.client_hints.AwUserAgentMetadata;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.AwSwitches;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.MediaIntegrityApiStatus;
import org.chromium.android_webview.metrics.BackForwardCacheNotRestoredReason;
import org.chromium.android_webview.safe_browsing.AwSafeBrowsingConfigHelper;
import org.chromium.android_webview.settings.AttributionBehavior;
import org.chromium.android_webview.settings.ForceDarkBehavior;
import org.chromium.android_webview.settings.ForceDarkMode;
import org.chromium.android_webview.settings.SpeculativeLoadingAllowedFlags;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.webauthn.WebauthnMode;
import org.chromium.components.webauthn.WebauthnModeProvider;
import org.chromium.content_public.browser.WebContents;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Stores Android WebView specific settings that does not need to be synced to WebKit.
 *
 * <p>Methods in this class can be called from any thread, including threads created by the client
 * of WebView.
 *
 * <p>Flushing the BFCache is required if a settings property is changed.
 */
@Lifetime.WebView
@JNINamespace("android_webview")
public class AwSettings {
    private static final String TAG = "AwSettings";
    private static final boolean TRACE = false;

    /* See {@link android.webkit.WebSettings}. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        LAYOUT_ALGORITHM_NORMAL,
        /* See {@link android.webkit.WebSettings}. */
        LAYOUT_ALGORITHM_SINGLE_COLUMN,
        /* See {@link android.webkit.WebSettings}. */
        LAYOUT_ALGORITHM_NARROW_COLUMNS,
        LAYOUT_ALGORITHM_TEXT_AUTOSIZING
    })
    public @interface LayoutAlgorithm {}

    public static final int LAYOUT_ALGORITHM_NORMAL = 0;
    /* See {@link android.webkit.WebSettings}. */
    public static final int LAYOUT_ALGORITHM_SINGLE_COLUMN = 1;
    /* See {@link android.webkit.WebSettings}. */
    public static final int LAYOUT_ALGORITHM_NARROW_COLUMNS = 2;
    public static final int LAYOUT_ALGORITHM_TEXT_AUTOSIZING = 3;

    public static final int FORCE_DARK_OFF = ForceDarkMode.FORCE_DARK_OFF;
    public static final int FORCE_DARK_AUTO = ForceDarkMode.FORCE_DARK_AUTO;
    public static final int FORCE_DARK_ON = ForceDarkMode.FORCE_DARK_ON;
    public static final int FORCE_DARK_MODES_COUNT = 3;

    @ForceDarkMode private int mForceDarkMode = ForceDarkMode.FORCE_DARK_AUTO;

    private boolean mAlgorithmicDarkeningAllowed;

    public static final int FORCE_DARK_ONLY = ForceDarkBehavior.FORCE_DARK_ONLY;
    public static final int MEDIA_QUERY_ONLY = ForceDarkBehavior.MEDIA_QUERY_ONLY;
    // This option requires RuntimeEnabledFeatures::MetaColorSchemeEnabled()
    public static final int PREFER_MEDIA_QUERY_OVER_FORCE_DARK =
            ForceDarkBehavior.PREFER_MEDIA_QUERY_OVER_FORCE_DARK;
    public static final int FORCE_DARK_STRATEGY_COUNT = 3;

    @ForceDarkBehavior
    private int mForceDarkBehavior = ForceDarkBehavior.PREFER_MEDIA_QUERY_OVER_FORCE_DARK;

    @AttributionBehavior
    public static final int ATTRIBUTION_DISABLED = AttributionBehavior.DISABLED;

    @AttributionBehavior
    public static final int ATTRIBUTION_APP_SOURCE_AND_WEB_TRIGGER =
            AttributionBehavior.APP_SOURCE_AND_WEB_TRIGGER;

    @AttributionBehavior
    public static final int ATTRIBUTION_WEB_SOURCE_AND_WEB_TRIGGER =
            AttributionBehavior.WEB_SOURCE_AND_WEB_TRIGGER;

    @AttributionBehavior
    public static final int ATTRIBUTION_APP_SOURCE_AND_APP_TRIGGER =
            AttributionBehavior.APP_SOURCE_AND_APP_TRIGGER;

    private Set<String> mRequestedWithHeaderAllowedOriginRules;

    private Context mContext;
    private WebContents mWebContents;

    // This class must be created on the UI thread. Afterwards, it can be
    // used from any thread. Internally, the class uses a message queue
    // to call native code on the UI thread only.

    // Values passed in on construction.
    private final boolean mHasInternetPermission;

    private ZoomSupportChangeListener mZoomChangeListener;
    private double mDIPScale = 1.0;

    // Lock to protect all settings.
    private final Object mAwSettingsLock = new Object();

    @LayoutAlgorithm private int mLayoutAlgorithm = LAYOUT_ALGORITHM_NARROW_COLUMNS;
    private int mTextSizePercent = 100;
    private String mStandardFontFamily = "sans-serif";
    private String mFixedFontFamily = "monospace";
    private String mSansSerifFontFamily = "sans-serif";
    private String mSerifFontFamily = "serif";
    private String mCursiveFontFamily = "cursive";
    private String mFantasyFontFamily = "fantasy";
    private String mDefaultTextEncoding = "UTF-8";
    private String mUserAgent;
    private AwUserAgentMetadata mAwUserAgentMetadata;
    private boolean mHasUserAgentMetadataOverrides;
    private int mMinimumFontSize = 8;
    private int mMinimumLogicalFontSize = 8;
    private int mDefaultFontSize = 16;
    private int mDefaultFixedFontSize = 13;
    private boolean mLoadsImagesAutomatically = true;
    private boolean mImagesEnabled = true;
    private boolean mJavaScriptEnabled;
    private boolean mAllowUniversalAccessFromFileURLs;
    private boolean mAllowFileAccessFromFileURLs;
    private boolean mJavaScriptCanOpenWindowsAutomatically;
    private boolean mSupportMultipleWindows;
    private boolean mDomStorageEnabled;
    private boolean mDatabaseEnabled;
    private boolean mUseWideViewport;
    private boolean mZeroLayoutHeightDisablesViewportQuirk;
    private boolean mForceZeroLayoutHeight;
    private boolean mLoadWithOverviewMode;
    private boolean mMediaPlaybackRequiresUserGesture = true;
    private String mDefaultVideoPosterURL;
    private float mInitialPageScalePercent;
    private boolean mSpatialNavigationEnabled; // Default depends on device features.
    private boolean mEnableSupportedHardwareAcceleratedFeatures;
    private int mMixedContentMode = WebSettings.MIXED_CONTENT_NEVER_ALLOW;
    private int mAttributionBehavior = AttributionBehavior.APP_SOURCE_AND_WEB_TRIGGER;

    @SpeculativeLoadingAllowedFlags
    private int mSpeculativeLoadingAllowedFlags =
            SpeculativeLoadingAllowedFlags.SPECULATIVE_LOADING_DISABLED;

    private boolean mHasCalledSetSpeculativeLoadingAllowedBefore;

    // Enabling this setting or the kWebViewBackForwardCache feature will enable BFCache
    // in WebView.
    private boolean mBackForwardCacheEnabled;
    private boolean mHasCalledSetBackForwardCacheEnabledBefore;

    private boolean mCSSHexAlphaColorEnabled;
    private boolean mScrollTopLeftInteropEnabled;
    private boolean mWillSuppressErrorPage;

    private boolean mOffscreenPreRaster;
    private int mDisabledMenuItems = WebSettings.MENU_ITEM_NONE;

    // Although this bit is stored on AwSettings it is actually controlled via the CookieManager.
    private boolean mAcceptThirdPartyCookies;

    // if null, default to AwSafeBrowsingConfigHelper.getSafeBrowsingEnabledByManifest()
    private Boolean mSafeBrowsingEnabled;

    private final boolean mSupportLegacyQuirks;
    private final boolean mAllowEmptyDocumentPersistence;
    private final boolean mAllowGeolocationOnInsecureOrigins;
    private final boolean mDoNotUpdateSelectionOnMutatingSelectionRange;

    private final boolean mPasswordEchoEnabled;

    // Not accessed by the native side.
    private boolean mBlockSpecialFileUrls;
    private boolean mBlockNetworkLoads; // Default depends on permission of embedding APK.
    private boolean mAllowContentUrlAccess = true;
    private boolean mAllowFileUrlAccess;
    private int mCacheMode = WebSettings.LOAD_DEFAULT;
    private boolean mShouldFocusFirstNode = true;
    private boolean mGeolocationEnabled = true;
    private boolean mFullscreenSupported;
    private boolean mSupportZoom = true;
    private boolean mBuiltInZoomControls;
    private boolean mDisplayZoomControls = true;
    private final AwMediaIntegrityApiStatusConfig mIntegrityApiStatusConfig;

    private @WebauthnMode int mWebauthnMode = WebauthnMode.NONE;

    // Cache default user agent string obtained through JNI, since it will not change during the
    // process lifetime. This saves a JNI call when creating new AwSettings objects after the first
    // one in the process, and when client code asks for the default UA.
    static class LazyDefaultUserAgent {
        // Lazy Holder pattern
        private static final String sInstance = AwSettingsJni.get().getDefaultUserAgent();
    }

    // Cache default user agent metadata obtained through JNI.
    static class LazyDefaultUserAgentMetadata {
        // Lazy Holder pattern
        private static final AwUserAgentMetadata sInstance =
                AwSettingsJni.get().getDefaultUserAgentMetadata();
    }

    // The native side of this object. It's lifetime is bounded by the WebContent it is attached to.
    private long mNativeAwSettings;

    // Custom handler that queues messages to call native code on the UI thread.
    private final EventHandler mEventHandler;

    private static final int MINIMUM_FONT_SIZE = 1;
    private static final int MAXIMUM_FONT_SIZE = 72;

    // Class to handle messages to be processed on the UI thread.
    private class EventHandler {
        // Message id for running a Runnable with mAwSettingsLock held.
        private static final int RUN_RUNNABLE_BLOCKING = 0;
        // Actual UI thread handler
        private Handler mHandler;
        // Synchronization flag.
        private boolean mSynchronizationPending;

        EventHandler() {}

        @SuppressLint("HandlerLeak")
        void bindUiThread() {
            if (mHandler != null) return;
            mHandler =
                    new Handler(ThreadUtils.getUiThreadLooper()) {
                        @Override
                        public void handleMessage(Message msg) {
                            switch (msg.what) {
                                case RUN_RUNNABLE_BLOCKING:
                                    synchronized (mAwSettingsLock) {
                                        if (mNativeAwSettings != 0) {
                                            ((Runnable) msg.obj).run();
                                        }
                                        mSynchronizationPending = false;
                                        mAwSettingsLock.notifyAll();
                                    }
                                    break;
                            }
                        }
                    };
        }

        void runOnUiThreadBlockingAndLocked(Runnable r) {
            assert Thread.holdsLock(mAwSettingsLock);
            if (mHandler == null) return;
            if (ThreadUtils.runningOnUiThread()) {
                r.run();
            } else {
                assert !mSynchronizationPending;
                mSynchronizationPending = true;
                mHandler.sendMessage(Message.obtain(null, RUN_RUNNABLE_BLOCKING, r));
                try {
                    while (mSynchronizationPending) {
                        mAwSettingsLock.wait();
                    }
                } catch (InterruptedException e) {
                    Log.e(TAG, "Interrupted waiting a Runnable to complete", e);
                    mSynchronizationPending = false;
                }
            }
        }

        void maybePostOnUiThread(Runnable r) {
            if (mHandler != null) {
                mHandler.post(r);
            }
        }

        void updateWebkitPreferencesLocked() {
            runOnUiThreadBlockingAndLocked(
                    AwSettings.this::updateWebkitPreferencesOnUiThreadLocked);
        }

        void updateCookiePolicyLocked() {
            runOnUiThreadBlockingAndLocked(AwSettings.this::updateCookiePolicyOnUiThreadLocked);
        }

        void updateAllowFileAccessLocked() {
            runOnUiThreadBlockingAndLocked(AwSettings.this::updateAllowFileAccessOnUiThreadLocked);
        }

        void updateSpeculativeLoadingAllowedLocked() {
            runOnUiThreadBlockingAndLocked(
                    AwSettings.this::updateSpeculativeLoadingAllowedOnUiThreadLocked);
        }

        void updateBackForwardCacheEnabled() {
            runOnUiThreadBlockingAndLocked(
                    AwSettings.this::updateBackForwardCacheEnabledOnUiThreadLocked);
        }

        void updateGeolocationEnabled() {
            runOnUiThreadBlockingAndLocked(
                    AwSettings.this::updateGeolocationEnabledOnUiThreadLocked);
        }
    }

    interface ZoomSupportChangeListener {
        public void onGestureZoomSupportChanged(
                boolean supportsDoubleTapZoom, boolean supportsMultiTouchZoom);
    }

    public AwSettings(
            Context context,
            boolean isAccessFromFileURLsGrantedByDefault,
            boolean supportsLegacyQuirks,
            boolean allowEmptyDocumentPersistence,
            boolean allowGeolocationOnInsecureOrigins,
            boolean doNotUpdateSelectionOnMutatingSelectionRange) {
        mContext = context;
        boolean hasInternetPermission =
                context.checkPermission(
                                android.Manifest.permission.INTERNET,
                                Process.myPid(),
                                Process.myUid())
                        == PackageManager.PERMISSION_GRANTED;
        synchronized (mAwSettingsLock) {
            mHasInternetPermission = hasInternetPermission;
            mBlockNetworkLoads = !hasInternetPermission;
            mEventHandler = new EventHandler();
            if (isAccessFromFileURLsGrantedByDefault) {
                mAllowUniversalAccessFromFileURLs = true;
                mAllowFileAccessFromFileURLs = true;
            }

            mUserAgent = LazyDefaultUserAgent.sInstance;
            mAwUserAgentMetadata = LazyDefaultUserAgentMetadata.sInstance.shallowCopy();

            // Best-guess a sensible initial value based on the features supported on the device.
            mSpatialNavigationEnabled =
                    !context.getPackageManager()
                            .hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN);

            // Respect the system setting for password echoing.
            mPasswordEchoEnabled =
                    Settings.System.getInt(
                                    context.getContentResolver(),
                                    Settings.System.TEXT_SHOW_PASSWORD,
                                    1)
                            == 1;

            // By default, scale the text size by the system font scale factor. Embedders
            // may override this by invoking setTextZoom().
            mTextSizePercent =
                    (int) (mTextSizePercent * context.getResources().getConfiguration().fontScale);

            mSupportLegacyQuirks = supportsLegacyQuirks;
            mAllowEmptyDocumentPersistence = allowEmptyDocumentPersistence;
            mAllowGeolocationOnInsecureOrigins = allowGeolocationOnInsecureOrigins;
            mDoNotUpdateSelectionOnMutatingSelectionRange =
                    doNotUpdateSelectionOnMutatingSelectionRange;

            // The application context we receive in the sdk runtime is a separate
            // context from the context that actual SDKs receive (and contains asset
            // file links). This means file urls will not work in this environment.
            // Explicitly block this to cause confusion in the case of accidentally
            // hitting assets in the application context.
            mBlockSpecialFileUrls = ContextUtils.isSdkSandboxProcess();

            mAllowFileUrlAccess =
                    ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion
                            < Build.VERSION_CODES.R;
            mRequestedWithHeaderAllowedOriginRules =
                    ManifestMetadataUtil.getXRequestedWithAllowList();
            mIntegrityApiStatusConfig = new AwMediaIntegrityApiStatusConfig();
            mSpeculativeLoadingAllowedFlags =
                    SpeculativeLoadingAllowedFlags.SPECULATIVE_LOADING_DISABLED;
            mHasCalledSetSpeculativeLoadingAllowedBefore = false;
            mBackForwardCacheEnabled = false;
            mHasCalledSetBackForwardCacheEnabledBefore = false;
        }
        // Defer initializing the native side until a native WebContents instance is set.
    }

    /** Get the AwSettings for the WebView with the given WebContents */
    @Nullable
    public static AwSettings fromWebContents(@NonNull WebContents webContents) {
        return AwSettingsJni.get().fromWebContents(webContents);
    }

    public int getUiModeNight() {
        return mContext.getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK;
    }

    @CalledByNative
    private void nativeAwSettingsGone(long nativeAwSettings) {
        assert mNativeAwSettings != 0 && mNativeAwSettings == nativeAwSettings;
        mNativeAwSettings = 0;
    }

    @CalledByNative
    private double getDIPScaleLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mDIPScale;
    }

    void setDIPScale(double dipScale) {
        synchronized (mAwSettingsLock) {
            mDIPScale = dipScale;
            // TODO(joth): This should also be synced over to native side, but right now
            // the setDIPScale call is always followed by a setWebContents() which covers this.
        }
    }

    void setZoomListener(ZoomSupportChangeListener zoomChangeListener) {
        synchronized (mAwSettingsLock) {
            mZoomChangeListener = zoomChangeListener;
        }
    }

    private void flushBackForwardCacheOnUiThreadLocked() {
        synchronized (mAwSettingsLock) {
            WebContents contents = mWebContents;
            Boolean backForwardCacheEnabled = mBackForwardCacheEnabled;
            mEventHandler.maybePostOnUiThread(
                    () -> flushBackForwardCache(contents, backForwardCacheEnabled));
        }
    }

    private void flushBackForwardCache() {
        assert Thread.holdsLock(mAwSettingsLock);
        flushBackForwardCache(mWebContents, mBackForwardCacheEnabled);
    }

    private void flushBackForwardCache(WebContents contents, boolean backForwardCacheEnabled) {
        ThreadUtils.assertOnUiThread();
        backForwardCacheEnabled =
                AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_BACK_FORWARD_CACHE)
                        || backForwardCacheEnabled;
        if (contents != null && backForwardCacheEnabled) {
            AwContents awContents = AwContents.fromWebContents(contents);
            if (awContents != null) {
                awContents.flushBackForwardCache(
                        BackForwardCacheNotRestoredReason.WEBVIEW_SETTINGS_CHANGED);
            }
        }
    }

    void setWebContents(WebContents webContents) {
        synchronized (mAwSettingsLock) {
            if (mNativeAwSettings != 0) {
                AwSettingsJni.get().destroy(mNativeAwSettings, AwSettings.this);
                assert mNativeAwSettings == 0; // nativeAwSettingsGone should have been called.
            }
            if (webContents != null) {
                mEventHandler.bindUiThread();
                mNativeAwSettings = AwSettingsJni.get().init(AwSettings.this, webContents);
                updateEverythingLocked();
                setRequestedWithHeaderOriginAllowListLocked(mRequestedWithHeaderAllowedOriginRules);
                WebauthnModeProvider.getInstance()
                        .setWebauthnModeForWebContents(webContents, mWebauthnMode);
                flushBackForwardCacheOnUiThreadLocked();
            }
            mWebContents = webContents;
        }
    }

    private void updateEverythingLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        assert mNativeAwSettings != 0;
        AwSettingsJni.get().updateEverythingLocked(mNativeAwSettings, AwSettings.this);
        onGestureZoomSupportChanged(supportsDoubleTapZoomLocked(), supportsMultiTouchZoomLocked());
    }

    /** See {@link android.webkit.WebSettings#setBlockNetworkLoads}. */
    public void setBlockNetworkLoads(boolean flag) {
        if (TRACE) Log.i(TAG, "setBlockNetworkLoads=" + flag);
        synchronized (mAwSettingsLock) {
            if (!flag && !mHasInternetPermission) {
                throw new SecurityException(
                        "Permission denied - " + "application missing INTERNET permission");
            }
            if (mBlockNetworkLoads != flag) {
                flushBackForwardCacheOnUiThreadLocked();
            }
            mBlockNetworkLoads = flag;
        }
    }

    /** See {@link android.webkit.WebSettings#getBlockNetworkLoads}. */
    public boolean getBlockNetworkLoads() {
        synchronized (mAwSettingsLock) {
            return mBlockNetworkLoads;
        }
    }

    /**
     * Enable/disable third party cookies for an AwContents
     * @param accept true if we should accept third party cookies
     */
    public void setAcceptThirdPartyCookies(boolean accept) {
        if (CommandLine.getInstance().hasSwitch(AwSwitches.WEBVIEW_FORCE_DISABLE3PCS)) {
            if (TRACE) Log.i(TAG, "setAcceptThirdPartyCookies force disabled");
            return;
        }
        if (TRACE) Log.i(TAG, "setAcceptThirdPartyCookies=" + accept);
        RecordHistogram.recordBooleanHistogram(
                "Android.WebView.SetAcceptThirdPartyCookies", accept);
        synchronized (mAwSettingsLock) {
            mAcceptThirdPartyCookies = accept;
            mEventHandler.updateCookiePolicyLocked();
        }
    }

    /**
     * Enable/Disable SafeBrowsing per WebView
     *
     * @param enabled true if this WebView should have SafeBrowsing
     */
    public void setSafeBrowsingEnabled(boolean enabled) {
        synchronized (mAwSettingsLock) {
            if (mSafeBrowsingEnabled == null || mSafeBrowsingEnabled != enabled) {
                flushBackForwardCacheOnUiThreadLocked();
            }
            mSafeBrowsingEnabled = enabled;
        }
    }

    /**
     * Return whether third party cookies are enabled for an AwContents
     * @return true if accept third party cookies
     */
    public boolean getAcceptThirdPartyCookies() {
        synchronized (mAwSettingsLock) {
            return mAcceptThirdPartyCookies;
        }
    }

    @CalledByNative
    private boolean getAcceptThirdPartyCookiesLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mAcceptThirdPartyCookies;
    }

    /**
     * Return whether Safe Browsing has been enabled for the current WebView
     * @return true if SafeBrowsing is enabled
     */
    public boolean getSafeBrowsingEnabled() {
        synchronized (mAwSettingsLock) {
            if (mSafeBrowsingEnabled == null) {
                return AwSafeBrowsingConfigHelper.getSafeBrowsingEnabledByManifest();
            }
            return mSafeBrowsingEnabled;
        }
    }

    /** See {@link android.webkit.WebSettings#setAllowFileAccess}. */
    public void setAllowFileAccess(boolean allow) {
        if (TRACE) Log.i(TAG, "setAllowFileAccess=" + allow);
        synchronized (mAwSettingsLock) {
            mAllowFileUrlAccess = allow;
            mEventHandler.updateAllowFileAccessLocked();
        }
    }

    /** See {@link android.webkit.WebSettings#getAllowFileAccess}. */
    @CalledByNative
    public boolean getAllowFileAccess() {
        synchronized (mAwSettingsLock) {
            return mAllowFileUrlAccess;
        }
    }

    /** See {@link android.webkit.WebSettings#setAllowContentAccess}. */
    public void setAllowContentAccess(boolean allow) {
        if (TRACE) Log.i(TAG, "setAllowContentAccess=" + allow);
        synchronized (mAwSettingsLock) {
            if (mAllowContentUrlAccess != allow) {
                flushBackForwardCacheOnUiThreadLocked();
            }
            mAllowContentUrlAccess = allow;
        }
    }

    /** See {@link android.webkit.WebSettings#getAllowContentAccess}. */
    public boolean getAllowContentAccess() {
        synchronized (mAwSettingsLock) {
            return mAllowContentUrlAccess;
        }
    }

    /** See {@link android.webkit.WebSettings#setCacheMode}. */
    public void setCacheMode(int mode) {
        if (TRACE) Log.i(TAG, "setCacheMode=" + mode);
        synchronized (mAwSettingsLock) {
            if (mCacheMode != mode) {
                flushBackForwardCacheOnUiThreadLocked();
            }
            mCacheMode = mode;
        }
    }

    /** See {@link android.webkit.WebSettings#getCacheMode}. */
    public int getCacheMode() {
        synchronized (mAwSettingsLock) {
            return mCacheMode;
        }
    }

    /** See {@link android.webkit.WebSettings#setNeedInitialFocus}. */
    public void setShouldFocusFirstNode(boolean flag) {
        if (TRACE) Log.i(TAG, "setNeedInitialFocus=" + flag);
        synchronized (mAwSettingsLock) {
            if (mShouldFocusFirstNode != flag) {
                flushBackForwardCacheOnUiThreadLocked();
            }
            mShouldFocusFirstNode = flag;
        }
    }

    /** See {@link android.webkit.WebView#setInitialScale}. */
    public void setInitialPageScale(final float scaleInPercent) {
        if (TRACE) Log.i(TAG, "setInitialScale=" + scaleInPercent);
        synchronized (mAwSettingsLock) {
            if (mInitialPageScalePercent != scaleInPercent) {
                mInitialPageScalePercent = scaleInPercent;
                mEventHandler.runOnUiThreadBlockingAndLocked(
                        () -> updateInitialPageScaleOnUiThreadLocked());
            }
        }
    }

    @CalledByNative
    private float getInitialPageScalePercentLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mInitialPageScalePercent;
    }

    @VisibleForTesting
    public void setSpatialNavigationEnabled(boolean enable) {
        synchronized (mAwSettingsLock) {
            if (mSpatialNavigationEnabled != enable) {
                mSpatialNavigationEnabled = enable;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    @CalledByNative
    private boolean getSpatialNavigationLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mSpatialNavigationEnabled;
    }

    @VisibleForTesting
    public void setEnableSupportedHardwareAcceleratedFeatures(boolean enable) {
        synchronized (mAwSettingsLock) {
            if (mEnableSupportedHardwareAcceleratedFeatures != enable) {
                mEnableSupportedHardwareAcceleratedFeatures = enable;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    @CalledByNative
    private boolean getEnableSupportedHardwareAcceleratedFeaturesLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mEnableSupportedHardwareAcceleratedFeatures;
    }

    public void setFullscreenSupported(boolean supported) {
        synchronized (mAwSettingsLock) {
            if (mFullscreenSupported != supported) {
                mFullscreenSupported = supported;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    @CalledByNative
    private boolean getFullscreenSupportedLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mFullscreenSupported;
    }

    /** See {@link android.webkit.WebSettings#setNeedInitialFocus}. */
    public boolean shouldFocusFirstNode() {
        synchronized (mAwSettingsLock) {
            return mShouldFocusFirstNode;
        }
    }

    /** See {@link android.webkit.WebSettings#setGeolocationEnabled}. */
    public void setGeolocationEnabled(boolean flag) {
        if (TRACE) Log.i(TAG, "setGeolocationEnabled=" + flag);
        synchronized (mAwSettingsLock) {
            if (mGeolocationEnabled != flag) {
                flushBackForwardCacheOnUiThreadLocked();
            }
            mGeolocationEnabled = flag;
            mEventHandler.updateGeolocationEnabled();
        }
    }

    /**
     * @return Returns if geolocation is currently enabled.
     */
    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public boolean getGeolocationEnabled() {
        synchronized (mAwSettingsLock) {
            return mGeolocationEnabled;
        }
    }

    public void setUserAgent(int ua) {
        // Minimal implementation for backwards compatibility: just supports resetting to default.
        if (ua == 0) {
            setUserAgentString(null);
        } else {
            Log.w(TAG, "setUserAgent not supported, ua=" + ua);
        }
    }

    /**
     * @return the default User-Agent used by each WebContents instance, i.e. unless overridden by
     *     {@link #setUserAgentString()}
     */
    public static String getDefaultUserAgent() {
        return LazyDefaultUserAgent.sInstance;
    }

    /**
     * @return the default metadata for user-agent client hints used by each WebContents instance,
     *     i.e. unless overridden by {@link #setUserAgentMetadata()}
     */
    public static AwUserAgentMetadata getDefaultUserAgentMetadata() {
        return LazyDefaultUserAgentMetadata.sInstance;
    }

    @CalledByNative
    private static boolean getAllowSniffingFileUrls() {
        // Don't allow sniffing file:// URLs for MIME type if the application targets P or later.
        return ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion
                < Build.VERSION_CODES.P;
    }

    /** See {@link android.webkit.WebSettings#setUserAgentString}. */
    public void setUserAgentString(String ua) {
        if (TRACE) Log.i(TAG, "setUserAgentString=" + ua);
        synchronized (mAwSettingsLock) {
            final String oldUserAgent = mUserAgent;
            if (ua == null || ua.length() == 0) {
                mUserAgent = LazyDefaultUserAgent.sInstance;
            } else {
                mUserAgent = ua;
            }
            if (!oldUserAgent.equals(mUserAgent)) {
                if (ua != null
                        && ua.length() > 0
                        && AwContents.BAD_HEADER_CHAR.matcher(ua).find()) {
                    throw new IllegalArgumentException(
                            AwContents.BAD_HEADER_MSG + "Invalid User-Agent '" + ua + "'");
                }
                mEventHandler.runOnUiThreadBlockingAndLocked(
                        () -> updateUserAgentOnUiThreadLocked());
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getUserAgentString}. */
    public String getUserAgentString() {
        synchronized (mAwSettingsLock) {
            return getUserAgentLocked();
        }
    }

    @CalledByNative
    private String getUserAgentLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mUserAgent;
    }

    /**
     * See {@link androidx.webkit.WebSettingsCompat#setUserAgentMetadata}.
     * Map<String, Object> represents the priorities name its value for AwUserAgentMetadata.
     */
    public void setUserAgentMetadataFromMap(Map<String, Object> uaMetadataMap) {
        if (TRACE) Log.i(TAG, "setUserAgentMetadata=" + uaMetadataMap);
        synchronized (mAwSettingsLock) {
            final AwUserAgentMetadata overrideUaMetadata =
                    AwUserAgentMetadata.fromMap(
                            uaMetadataMap, LazyDefaultUserAgentMetadata.sInstance);
            if (!mAwUserAgentMetadata.equals(overrideUaMetadata)) {
                mAwUserAgentMetadata = overrideUaMetadata;
                // We only consider it has override when the input is not empty and has difference
                // with the existing user-agent metadata. e.g. user overrides the user-agent with a
                // totally different value, initially they provide the user-agent metadata
                // overrides, we should only generate low-entropy user-agent client hints once users
                // clear the user-agent metadata overrides.
                mHasUserAgentMetadataOverrides =
                        (uaMetadataMap != null && !uaMetadataMap.isEmpty());
                mEventHandler.runOnUiThreadBlockingAndLocked(
                        () -> updateUserAgentOnUiThreadLocked());
            }
        }
    }

    /** See {@link androidx.webkit.WebSettingsCompat#getUserAgentMetadata}. */
    public Map<String, Object> getUserAgentMetadataMap() {
        synchronized (mAwSettingsLock) {
            return getUserAgentMetadataLocked().toMapObject();
        }
    }

    @CalledByNative
    private AwUserAgentMetadata getUserAgentMetadataLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mAwUserAgentMetadata;
    }

    @CalledByNative
    private boolean getHasUserAgentMetadataOverridesLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mHasUserAgentMetadataOverrides;
    }

    /** See {@link android.webkit.WebSettings#setLoadWithOverviewMode}. */
    public void setLoadWithOverviewMode(boolean overview) {
        if (TRACE) Log.i(TAG, "setLoadWithOverviewMode=" + overview);
        synchronized (mAwSettingsLock) {
            if (mLoadWithOverviewMode != overview) {
                mLoadWithOverviewMode = overview;
                mEventHandler.runOnUiThreadBlockingAndLocked(
                        () -> {
                            if (mNativeAwSettings != 0) {
                                updateWebkitPreferencesOnUiThreadLocked();
                                AwSettingsJni.get()
                                        .resetScrollAndScaleState(
                                                mNativeAwSettings, AwSettings.this);
                            }
                        });
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getLoadWithOverviewMode}. */
    public boolean getLoadWithOverviewMode() {
        synchronized (mAwSettingsLock) {
            return getLoadWithOverviewModeLocked();
        }
    }

    @CalledByNative
    private boolean getLoadWithOverviewModeLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mLoadWithOverviewMode;
    }

    /** See {@link android.webkit.WebSettings#setTextZoom}. */
    public void setTextZoom(final int textZoom) {
        if (TRACE) Log.i(TAG, "setTextZoom=" + textZoom);
        synchronized (mAwSettingsLock) {
            if (mTextSizePercent != textZoom) {
                mTextSizePercent = textZoom;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getTextZoom}. */
    public int getTextZoom() {
        synchronized (mAwSettingsLock) {
            return getTextSizePercentLocked();
        }
    }

    @CalledByNative
    private int getTextSizePercentLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mTextSizePercent;
    }

    /** See {@link android.webkit.WebSettings#setStandardFontFamily}. */
    public void setStandardFontFamily(String font) {
        if (TRACE) Log.i(TAG, "setStandardFontFamily=" + font);
        synchronized (mAwSettingsLock) {
            if (font != null && !mStandardFontFamily.equals(font)) {
                mStandardFontFamily = font;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getStandardFontFamily}. */
    public String getStandardFontFamily() {
        synchronized (mAwSettingsLock) {
            return getStandardFontFamilyLocked();
        }
    }

    @CalledByNative
    private String getStandardFontFamilyLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mStandardFontFamily;
    }

    /** See {@link android.webkit.WebSettings#setFixedFontFamily}. */
    public void setFixedFontFamily(String font) {
        if (TRACE) Log.i(TAG, "setFixedFontFamily=" + font);
        synchronized (mAwSettingsLock) {
            if (font != null && !mFixedFontFamily.equals(font)) {
                mFixedFontFamily = font;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getFixedFontFamily}. */
    public String getFixedFontFamily() {
        synchronized (mAwSettingsLock) {
            return getFixedFontFamilyLocked();
        }
    }

    @CalledByNative
    private String getFixedFontFamilyLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mFixedFontFamily;
    }

    /** See {@link android.webkit.WebSettings#setSansSerifFontFamily}. */
    public void setSansSerifFontFamily(String font) {
        if (TRACE) Log.i(TAG, "setSansSerifFontFamily=" + font);
        synchronized (mAwSettingsLock) {
            if (font != null && !mSansSerifFontFamily.equals(font)) {
                mSansSerifFontFamily = font;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getSansSerifFontFamily}. */
    public String getSansSerifFontFamily() {
        synchronized (mAwSettingsLock) {
            return getSansSerifFontFamilyLocked();
        }
    }

    @CalledByNative
    private String getSansSerifFontFamilyLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mSansSerifFontFamily;
    }

    /** See {@link android.webkit.WebSettings#setSerifFontFamily}. */
    public void setSerifFontFamily(String font) {
        if (TRACE) Log.i(TAG, "setSerifFontFamily=" + font);
        synchronized (mAwSettingsLock) {
            if (font != null && !mSerifFontFamily.equals(font)) {
                mSerifFontFamily = font;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getSerifFontFamily}. */
    public String getSerifFontFamily() {
        synchronized (mAwSettingsLock) {
            return getSerifFontFamilyLocked();
        }
    }

    @CalledByNative
    private String getSerifFontFamilyLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mSerifFontFamily;
    }

    /** See {@link android.webkit.WebSettings#setCursiveFontFamily}. */
    public void setCursiveFontFamily(String font) {
        if (TRACE) Log.i(TAG, "setCursiveFontFamily=" + font);
        synchronized (mAwSettingsLock) {
            if (font != null && !mCursiveFontFamily.equals(font)) {
                mCursiveFontFamily = font;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getCursiveFontFamily}. */
    public String getCursiveFontFamily() {
        synchronized (mAwSettingsLock) {
            return getCursiveFontFamilyLocked();
        }
    }

    @CalledByNative
    private String getCursiveFontFamilyLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mCursiveFontFamily;
    }

    /** See {@link android.webkit.WebSettings#setFantasyFontFamily}. */
    public void setFantasyFontFamily(String font) {
        if (TRACE) Log.i(TAG, "setFantasyFontFamily=" + font);
        synchronized (mAwSettingsLock) {
            if (font != null && !mFantasyFontFamily.equals(font)) {
                mFantasyFontFamily = font;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getFantasyFontFamily}. */
    public String getFantasyFontFamily() {
        synchronized (mAwSettingsLock) {
            return getFantasyFontFamilyLocked();
        }
    }

    @CalledByNative
    private String getFantasyFontFamilyLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mFantasyFontFamily;
    }

    /** See {@link android.webkit.WebSettings#setMinimumFontSize}. */
    public void setMinimumFontSize(int size) {
        if (TRACE) Log.i(TAG, "setMinimumFontSize=" + size);
        synchronized (mAwSettingsLock) {
            size = clipFontSize(size);
            if (mMinimumFontSize != size) {
                mMinimumFontSize = size;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getMinimumFontSize}. */
    public int getMinimumFontSize() {
        synchronized (mAwSettingsLock) {
            return getMinimumFontSizeLocked();
        }
    }

    @CalledByNative
    private int getMinimumFontSizeLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mMinimumFontSize;
    }

    /** See {@link android.webkit.WebSettings#setMinimumLogicalFontSize}. */
    public void setMinimumLogicalFontSize(int size) {
        if (TRACE) Log.i(TAG, "setMinimumLogicalFontSize=" + size);
        synchronized (mAwSettingsLock) {
            size = clipFontSize(size);
            if (mMinimumLogicalFontSize != size) {
                mMinimumLogicalFontSize = size;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getMinimumLogicalFontSize}. */
    public int getMinimumLogicalFontSize() {
        synchronized (mAwSettingsLock) {
            return getMinimumLogicalFontSizeLocked();
        }
    }

    @CalledByNative
    private int getMinimumLogicalFontSizeLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mMinimumLogicalFontSize;
    }

    /** See {@link android.webkit.WebSettings#setDefaultFontSize}. */
    public void setDefaultFontSize(int size) {
        if (TRACE) Log.i(TAG, "setDefaultFontSize=" + size);
        synchronized (mAwSettingsLock) {
            size = clipFontSize(size);
            if (mDefaultFontSize != size) {
                mDefaultFontSize = size;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getDefaultFontSize}. */
    public int getDefaultFontSize() {
        synchronized (mAwSettingsLock) {
            return getDefaultFontSizeLocked();
        }
    }

    @CalledByNative
    private int getDefaultFontSizeLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mDefaultFontSize;
    }

    /** See {@link android.webkit.WebSettings#setDefaultFixedFontSize}. */
    public void setDefaultFixedFontSize(int size) {
        if (TRACE) Log.i(TAG, "setDefaultFixedFontSize=" + size);
        synchronized (mAwSettingsLock) {
            size = clipFontSize(size);
            if (mDefaultFixedFontSize != size) {
                mDefaultFixedFontSize = size;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getDefaultFixedFontSize}. */
    public int getDefaultFixedFontSize() {
        synchronized (mAwSettingsLock) {
            return getDefaultFixedFontSizeLocked();
        }
    }

    @CalledByNative
    private int getDefaultFixedFontSizeLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mDefaultFixedFontSize;
    }

    /** See {@link android.webkit.WebSettings#setJavaScriptEnabled}. */
    public void setJavaScriptEnabled(boolean flag) {
        if (TRACE) Log.i(TAG, "setJavaScriptEnabled=" + flag);
        synchronized (mAwSettingsLock) {
            if (mJavaScriptEnabled != flag) {
                mJavaScriptEnabled = flag;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#setAllowUniversalAccessFromFileURLs}. */
    public void setAllowUniversalAccessFromFileURLs(boolean flag) {
        if (TRACE) Log.i(TAG, "setAllowUniversalAccessFromFileURLs=" + flag);
        synchronized (mAwSettingsLock) {
            if (mAllowUniversalAccessFromFileURLs != flag) {
                mAllowUniversalAccessFromFileURLs = flag;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#setAllowFileAccessFromFileURLs}. */
    public void setAllowFileAccessFromFileURLs(boolean flag) {
        if (TRACE) Log.i(TAG, "setAllowFileAccessFromFileURLs=" + flag);
        synchronized (mAwSettingsLock) {
            if (mAllowFileAccessFromFileURLs != flag) {
                mAllowFileAccessFromFileURLs = flag;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#setLoadsImagesAutomatically}. */
    public void setLoadsImagesAutomatically(boolean flag) {
        if (TRACE) Log.i(TAG, "setLoadsImagesAutomatically=" + flag);
        synchronized (mAwSettingsLock) {
            if (mLoadsImagesAutomatically != flag) {
                mLoadsImagesAutomatically = flag;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getLoadsImagesAutomatically}. */
    public boolean getLoadsImagesAutomatically() {
        synchronized (mAwSettingsLock) {
            return getLoadsImagesAutomaticallyLocked();
        }
    }

    @CalledByNative
    private boolean getLoadsImagesAutomaticallyLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mLoadsImagesAutomatically;
    }

    /** See {@link android.webkit.WebSettings#setBlockNetworkImage}. */
    public void setImagesEnabled(boolean flag) {
        if (TRACE) Log.i(TAG, "setBlockNetworkImage=" + !flag);
        synchronized (mAwSettingsLock) {
            if (mImagesEnabled != flag) {
                mImagesEnabled = flag;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getBlockNetworkImage}. */
    public boolean getImagesEnabled() {
        synchronized (mAwSettingsLock) {
            return mImagesEnabled;
        }
    }

    @CalledByNative
    private boolean getImagesEnabledLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mImagesEnabled;
    }

    /** See {@link android.webkit.WebSettings#getJavaScriptEnabled}. */
    public boolean getJavaScriptEnabled() {
        synchronized (mAwSettingsLock) {
            return mJavaScriptEnabled;
        }
    }

    @CalledByNative
    private boolean getJavaScriptEnabledLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mJavaScriptEnabled;
    }

    /** See {@link android.webkit.WebSettings#getAllowUniversalAccessFromFileURLs}. */
    public boolean getAllowUniversalAccessFromFileURLs() {
        synchronized (mAwSettingsLock) {
            return getAllowUniversalAccessFromFileURLsLocked();
        }
    }

    @CalledByNative
    private boolean getAllowUniversalAccessFromFileURLsLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mAllowUniversalAccessFromFileURLs;
    }

    /** See {@link android.webkit.WebSettings#getAllowFileAccessFromFileURLs}. */
    public boolean getAllowFileAccessFromFileURLs() {
        synchronized (mAwSettingsLock) {
            return getAllowFileAccessFromFileURLsLocked();
        }
    }

    @CalledByNative
    private boolean getAllowFileAccessFromFileURLsLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mAllowFileAccessFromFileURLs;
    }

    /** See {@link android.webkit.WebSettings#setJavaScriptCanOpenWindowsAutomatically}. */
    public void setJavaScriptCanOpenWindowsAutomatically(boolean flag) {
        if (TRACE) Log.i(TAG, "setJavaScriptCanOpenWindowsAutomatically=" + flag);
        synchronized (mAwSettingsLock) {
            if (mJavaScriptCanOpenWindowsAutomatically != flag) {
                mJavaScriptCanOpenWindowsAutomatically = flag;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getJavaScriptCanOpenWindowsAutomatically}. */
    public boolean getJavaScriptCanOpenWindowsAutomatically() {
        synchronized (mAwSettingsLock) {
            return getJavaScriptCanOpenWindowsAutomaticallyLocked();
        }
    }

    @CalledByNative
    private boolean getJavaScriptCanOpenWindowsAutomaticallyLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mJavaScriptCanOpenWindowsAutomatically;
    }

    /** See {@link android.webkit.WebSettings#setLayoutAlgorithm}. */
    public void setLayoutAlgorithm(@LayoutAlgorithm int l) {
        if (TRACE) Log.i(TAG, "setLayoutAlgorithm=" + l);
        synchronized (mAwSettingsLock) {
            if (mLayoutAlgorithm != l) {
                mLayoutAlgorithm = l;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getLayoutAlgorithm}. */
    @LayoutAlgorithm
    public int getLayoutAlgorithm() {
        synchronized (mAwSettingsLock) {
            return mLayoutAlgorithm;
        }
    }

    public void setRequestedWithHeaderOriginAllowList(Set<String> allowedOriginRules) {
        // Even though clients shouldn't pass in null, it's better to guard against it
        allowedOriginRules =
                allowedOriginRules != null ? allowedOriginRules : Collections.emptySet();
        AwWebContentsMetricsRecorder.recordRequestedWithHeaderModeAPIUsage(allowedOriginRules);
        synchronized (mAwSettingsLock) {
            setRequestedWithHeaderOriginAllowListLocked(allowedOriginRules);
        }
    }

    private void setRequestedWithHeaderOriginAllowListLocked(final Set<String> allowedOriginRules) {
        assert Thread.holdsLock(mAwSettingsLock);
        if (mNativeAwSettings == 0) {
            return;
        }

        // Final set to be updated by the Runnable on the UI thread.
        final Set<String> rejectedRules = new HashSet<>();

        mEventHandler.runOnUiThreadBlockingAndLocked(
                () -> {
                    flushBackForwardCache();
                    String[] rejected =
                            AwSettingsJni.get()
                                    .updateXRequestedWithAllowListOriginMatcher(
                                            mNativeAwSettings,
                                            allowedOriginRules.toArray(new String[0]));
                    rejectedRules.addAll(java.util.Arrays.asList(rejected));
                });

        if (!rejectedRules.isEmpty()) {
            throw new IllegalArgumentException("Malformed origin match rules: " + rejectedRules);
        }
        mRequestedWithHeaderAllowedOriginRules = allowedOriginRules;
    }

    public Set<String> getRequestedWithHeaderOriginAllowList() {
        synchronized (mAwSettingsLock) {
            return mRequestedWithHeaderAllowedOriginRules;
        }
    }

    /**
     * Gets whether Text Auto-sizing layout algorithm is enabled.
     *
     * @return true if Text Auto-sizing layout algorithm is enabled
     */
    @CalledByNative
    private boolean getTextAutosizingEnabledLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mLayoutAlgorithm == LAYOUT_ALGORITHM_TEXT_AUTOSIZING;
    }

    /** See {@link android.webkit.WebSettings#setSupportMultipleWindows}. */
    public void setSupportMultipleWindows(boolean support) {
        if (TRACE) Log.i(TAG, "setSupportMultipleWindows=" + support);
        synchronized (mAwSettingsLock) {
            if (mSupportMultipleWindows != support) {
                mSupportMultipleWindows = support;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#supportMultipleWindows}. */
    public boolean supportMultipleWindows() {
        synchronized (mAwSettingsLock) {
            return mSupportMultipleWindows;
        }
    }

    public void setBlockSpecialFileUrls(boolean block) {
        if (TRACE) Log.i(TAG, "setBlockSpecialFileUrls=" + block);
        synchronized (mAwSettingsLock) {
            if (mBlockSpecialFileUrls != block) {
                flushBackForwardCacheOnUiThreadLocked();
            }
            mBlockSpecialFileUrls = block;
        }
    }

    public boolean getBlockSpecialFileUrls() {
        synchronized (mAwSettingsLock) {
            return mBlockSpecialFileUrls;
        }
    }

    @CalledByNative
    private boolean getSupportMultipleWindowsLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mSupportMultipleWindows;
    }

    @CalledByNative
    private boolean getCSSHexAlphaColorEnabledLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mCSSHexAlphaColorEnabled;
    }

    public void setCSSHexAlphaColorEnabled(boolean enabled) {
        synchronized (mAwSettingsLock) {
            if (mCSSHexAlphaColorEnabled != enabled) {
                mCSSHexAlphaColorEnabled = enabled;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    @CalledByNative
    private boolean getScrollTopLeftInteropEnabledLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mScrollTopLeftInteropEnabled;
    }

    public void setScrollTopLeftInteropEnabled(boolean enabled) {
        synchronized (mAwSettingsLock) {
            if (mScrollTopLeftInteropEnabled != enabled) {
                mScrollTopLeftInteropEnabled = enabled;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    @CalledByNative
    private boolean getWillSuppressErrorPageLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mWillSuppressErrorPage;
    }

    public boolean getWillSuppressErrorPage() {
        synchronized (mAwSettingsLock) {
            return getWillSuppressErrorPageLocked();
        }
    }

    public void setWillSuppressErrorPage(boolean suppressed) {
        synchronized (mAwSettingsLock) {
            if (mWillSuppressErrorPage == suppressed) return;

            mWillSuppressErrorPage = suppressed;
            updateWillSuppressErrorStateLocked();
        }
    }

    private void updateWillSuppressErrorStateLocked() {
        mEventHandler.runOnUiThreadBlockingAndLocked(
                () -> updateWillSuppressErrorStateOnUiThreadLocked());
    }

    @CalledByNative
    private boolean getSupportLegacyQuirksLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mSupportLegacyQuirks;
    }

    @CalledByNative
    private boolean getAllowEmptyDocumentPersistenceLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mAllowEmptyDocumentPersistence;
    }

    @CalledByNative
    private boolean getAllowGeolocationOnInsecureOrigins() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mAllowGeolocationOnInsecureOrigins;
    }

    @CalledByNative
    private boolean getDoNotUpdateSelectionOnMutatingSelectionRange() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mDoNotUpdateSelectionOnMutatingSelectionRange;
    }

    /** See {@link android.webkit.WebSettings#setUseWideViewPort}. */
    public void setUseWideViewPort(boolean use) {
        if (TRACE) Log.i(TAG, "setUseWideViewPort=" + use);
        synchronized (mAwSettingsLock) {
            if (mUseWideViewport != use) {
                mUseWideViewport = use;
                onGestureZoomSupportChanged(
                        supportsDoubleTapZoomLocked(), supportsMultiTouchZoomLocked());
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getUseWideViewPort}. */
    public boolean getUseWideViewPort() {
        synchronized (mAwSettingsLock) {
            return getUseWideViewportLocked();
        }
    }

    @CalledByNative
    private boolean getUseWideViewportLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mUseWideViewport;
    }

    public void setZeroLayoutHeightDisablesViewportQuirk(boolean enabled) {
        synchronized (mAwSettingsLock) {
            if (mZeroLayoutHeightDisablesViewportQuirk != enabled) {
                mZeroLayoutHeightDisablesViewportQuirk = enabled;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    public boolean getZeroLayoutHeightDisablesViewportQuirk() {
        synchronized (mAwSettingsLock) {
            return getZeroLayoutHeightDisablesViewportQuirkLocked();
        }
    }

    @CalledByNative
    private boolean getZeroLayoutHeightDisablesViewportQuirkLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mZeroLayoutHeightDisablesViewportQuirk;
    }

    public void setForceZeroLayoutHeight(boolean enabled) {
        synchronized (mAwSettingsLock) {
            if (mForceZeroLayoutHeight != enabled) {
                mForceZeroLayoutHeight = enabled;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    public boolean getForceZeroLayoutHeight() {
        synchronized (mAwSettingsLock) {
            return getForceZeroLayoutHeightLocked();
        }
    }

    @CalledByNative
    private boolean getForceZeroLayoutHeightLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mForceZeroLayoutHeight;
    }

    @CalledByNative
    private boolean getPasswordEchoEnabledLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mPasswordEchoEnabled;
    }

    /** See {@link android.webkit.WebSettings#setDomStorageEnabled}. */
    public void setDomStorageEnabled(boolean flag) {
        if (TRACE) Log.i(TAG, "setDomStorageEnabled=" + flag);
        synchronized (mAwSettingsLock) {
            if (mDomStorageEnabled != flag) {
                mDomStorageEnabled = flag;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getDomStorageEnabled}. */
    public boolean getDomStorageEnabled() {
        synchronized (mAwSettingsLock) {
            return mDomStorageEnabled;
        }
    }

    @CalledByNative
    private boolean getDomStorageEnabledLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mDomStorageEnabled;
    }

    /** See {@link android.webkit.WebSettings#setDatabaseEnabled}. */
    public void setDatabaseEnabled(boolean flag) {
        if (TRACE) Log.i(TAG, "setDatabaseEnabled=" + flag);
        synchronized (mAwSettingsLock) {
            if (mDatabaseEnabled != flag) {
                mDatabaseEnabled = flag;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getDatabaseEnabled}. */
    public boolean getDatabaseEnabled() {
        synchronized (mAwSettingsLock) {
            return mDatabaseEnabled;
        }
    }

    @CalledByNative
    private boolean getDatabaseEnabledLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mDatabaseEnabled;
    }

    /** See {@link android.webkit.WebSettings#setDefaultTextEncodingName}. */
    public void setDefaultTextEncodingName(String encoding) {
        if (TRACE) Log.i(TAG, "setDefaultTextEncodingName=" + encoding);
        synchronized (mAwSettingsLock) {
            if (encoding != null && !mDefaultTextEncoding.equals(encoding)) {
                mDefaultTextEncoding = encoding;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getDefaultTextEncodingName}. */
    public String getDefaultTextEncodingName() {
        synchronized (mAwSettingsLock) {
            return getDefaultTextEncodingLocked();
        }
    }

    @CalledByNative
    private String getDefaultTextEncodingLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mDefaultTextEncoding;
    }

    /** See {@link android.webkit.WebSettings#setMediaPlaybackRequiresUserGesture}. */
    public void setMediaPlaybackRequiresUserGesture(boolean require) {
        if (TRACE) Log.i(TAG, "setMediaPlaybackRequiresUserGesture=" + require);
        synchronized (mAwSettingsLock) {
            if (mMediaPlaybackRequiresUserGesture != require) {
                mMediaPlaybackRequiresUserGesture = require;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getMediaPlaybackRequiresUserGesture}. */
    public boolean getMediaPlaybackRequiresUserGesture() {
        synchronized (mAwSettingsLock) {
            return getMediaPlaybackRequiresUserGestureLocked();
        }
    }

    @CalledByNative
    private boolean getMediaPlaybackRequiresUserGestureLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mMediaPlaybackRequiresUserGesture;
    }

    /** See {@link android.webkit.WebSettings#setDefaultVideoPosterURL}. */
    public void setDefaultVideoPosterURL(String url) {
        synchronized (mAwSettingsLock) {
            if ((mDefaultVideoPosterURL != null && !mDefaultVideoPosterURL.equals(url))
                    || (mDefaultVideoPosterURL == null && url != null)) {
                mDefaultVideoPosterURL = url;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getDefaultVideoPosterURL}. */
    public String getDefaultVideoPosterURL() {
        synchronized (mAwSettingsLock) {
            return getDefaultVideoPosterURLLocked();
        }
    }

    @CalledByNative
    private String getDefaultVideoPosterURLLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mDefaultVideoPosterURL;
    }

    private void onGestureZoomSupportChanged(
            final boolean supportsDoubleTapZoom, final boolean supportsMultiTouchZoom) {
        // Always post asynchronously here, to avoid doubling back onto the caller.
        mEventHandler.maybePostOnUiThread(
                () -> {
                    synchronized (mAwSettingsLock) {
                        if (mZoomChangeListener != null) {
                            mZoomChangeListener.onGestureZoomSupportChanged(
                                    supportsDoubleTapZoom, supportsMultiTouchZoom);
                        }
                    }
                });
    }

    /** See {@link android.webkit.WebSettings#setSupportZoom}. */
    public void setSupportZoom(boolean support) {
        if (TRACE) Log.i(TAG, "setSupportZoom=" + support);
        synchronized (mAwSettingsLock) {
            if (mSupportZoom != support) {
                flushBackForwardCacheOnUiThreadLocked();
                mSupportZoom = support;
                onGestureZoomSupportChanged(
                        supportsDoubleTapZoomLocked(), supportsMultiTouchZoomLocked());
            }
        }
    }

    /** See {@link android.webkit.WebSettings#supportZoom}. */
    public boolean supportZoom() {
        synchronized (mAwSettingsLock) {
            return mSupportZoom;
        }
    }

    /** See {@link android.webkit.WebSettings#setBuiltInZoomControls}. */
    public void setBuiltInZoomControls(boolean enabled) {
        if (TRACE) Log.i(TAG, "setBuiltInZoomControls=" + enabled);
        synchronized (mAwSettingsLock) {
            if (mBuiltInZoomControls != enabled) {
                flushBackForwardCacheOnUiThreadLocked();
                mBuiltInZoomControls = enabled;
                onGestureZoomSupportChanged(
                        supportsDoubleTapZoomLocked(), supportsMultiTouchZoomLocked());
            }
        }
    }

    /** See {@link android.webkit.WebSettings#getBuiltInZoomControls}. */
    public boolean getBuiltInZoomControls() {
        synchronized (mAwSettingsLock) {
            return mBuiltInZoomControls;
        }
    }

    /** See {@link android.webkit.WebSettings#setDisplayZoomControls}. */
    public void setDisplayZoomControls(boolean enabled) {
        if (TRACE) Log.i(TAG, "setDisplayZoomControls=" + enabled);
        synchronized (mAwSettingsLock) {
            if (mDisplayZoomControls != enabled) {
                flushBackForwardCacheOnUiThreadLocked();
            }
            mDisplayZoomControls = enabled;
        }
    }

    /** See {@link android.webkit.WebSettings#getDisplayZoomControls}. */
    public boolean getDisplayZoomControls() {
        synchronized (mAwSettingsLock) {
            return mDisplayZoomControls;
        }
    }

    public void setMixedContentMode(int mode) {
        // Using explicit max count for the histogram since enum is defined in Android code. The
        // values can be trusted to remain stable since they are defined in the Android API.
        RecordHistogram.recordEnumeratedHistogram("Android.WebView.MixedContent.Mode", mode, 3);
        synchronized (mAwSettingsLock) {
            if (mMixedContentMode != mode) {
                mMixedContentMode = mode;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    @CalledByNative
    public int getMixedContentMode() {
        synchronized (mAwSettingsLock) {
            return mMixedContentMode;
        }
    }

    public void setAttributionBehavior(@AttributionBehavior int behavior) {
        synchronized (mAwSettingsLock) {
            if (mAttributionBehavior != behavior) {
                mAttributionBehavior = behavior;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    @CalledByNative
    @AttributionBehavior
    public int getAttributionBehavior() {
        synchronized (mAwSettingsLock) {
            return mAttributionBehavior;
        }
    }

    public void setSpeculativeLoadingAllowed(@SpeculativeLoadingAllowedFlags int flags) {
        synchronized (mAwSettingsLock) {
            // Only trigger an update if the value changed, or this is the first time we call this
            // function. The latter is important to make sure every embedder that calls this
            // function explicitly will be assigned a synthetic field trial group.
            if (mSpeculativeLoadingAllowedFlags != flags
                    || !mHasCalledSetSpeculativeLoadingAllowedBefore) {
                mSpeculativeLoadingAllowedFlags = flags;
                mHasCalledSetSpeculativeLoadingAllowedBefore = true;
                mEventHandler.updateSpeculativeLoadingAllowedLocked();
            }
        }
    }

    @CalledByNative
    @SpeculativeLoadingAllowedFlags
    public int getSpeculativeLoadingAllowed() {
        synchronized (mAwSettingsLock) {
            return mSpeculativeLoadingAllowedFlags;
        }
    }

    public void setBackForwardCacheEnabled(boolean enabled) {
        if (TRACE) Log.i(TAG, "setBackForwardCacheEnabled = " + enabled);
        synchronized (mAwSettingsLock) {
            // Only trigger an update if the value changed, or this is the first time we call this
            // function. The latter is important to make sure every embedder that calls this
            // function explicitly will be assigned a synthetic field trial group.
            if (mBackForwardCacheEnabled != enabled
                    || !mHasCalledSetBackForwardCacheEnabledBefore) {
                mBackForwardCacheEnabled = enabled;
                mHasCalledSetBackForwardCacheEnabledBefore = true;
                mEventHandler.updateBackForwardCacheEnabled();
            }
        }
    }

    @CalledByNative
    public boolean getBackForwardCacheEnabled() {
        synchronized (mAwSettingsLock) {
            return mBackForwardCacheEnabled;
        }
    }

    @ForceDarkMode
    public int getForceDarkMode() {
        synchronized (mAwSettingsLock) {
            return getForceDarkModeLocked();
        }
    }

    @CalledByNative
    @ForceDarkMode
    private int getForceDarkModeLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mForceDarkMode;
    }

    public void setForceDarkMode(@ForceDarkMode int forceDarkMode) {
        AwWebContentsMetricsRecorder.recordForceDarkModeAPIUsage(mContext, forceDarkMode);
        synchronized (mAwSettingsLock) {
            if (mForceDarkMode != forceDarkMode) {
                mForceDarkMode = forceDarkMode;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    public boolean isAlgorithmicDarkeningAllowed() {
        synchronized (mAwSettingsLock) {
            return isAlgorithmicDarkeningAllowedLocked();
        }
    }

    @CalledByNative
    private boolean isAlgorithmicDarkeningAllowedLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mAlgorithmicDarkeningAllowed;
    }

    public void setAlgorithmicDarkeningAllowed(boolean allow) {
        synchronized (mAwSettingsLock) {
            if (mAlgorithmicDarkeningAllowed != allow) {
                mAlgorithmicDarkeningAllowed = allow;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    public boolean isForceDarkApplied() {
        synchronized (mAwSettingsLock) {
            assert mNativeAwSettings != 0;
            return AwSettingsJni.get().isForceDarkApplied(mNativeAwSettings, AwSettings.this);
        }
    }

    public boolean prefersDarkFromTheme() {
        synchronized (mAwSettingsLock) {
            assert mNativeAwSettings != 0;
            return AwSettingsJni.get().prefersDarkFromTheme(mNativeAwSettings, AwSettings.this);
        }
    }

    @ForceDarkBehavior
    public int getForceDarkBehavior() {
        synchronized (mAwSettingsLock) {
            return getForceDarkBehaviorLocked();
        }
    }

    @CalledByNative
    @ForceDarkBehavior
    public int getForceDarkBehaviorLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mForceDarkBehavior;
    }

    public void setForceDarkBehavior(@ForceDarkBehavior int forceDarkBehavior) {
        AwWebContentsMetricsRecorder.recordForceDarkBehaviorAPIUsage(forceDarkBehavior);
        synchronized (mAwSettingsLock) {
            if (mForceDarkBehavior != forceDarkBehavior) {
                mForceDarkBehavior = forceDarkBehavior;
                mEventHandler.updateWebkitPreferencesLocked();
            }
        }
    }

    @CalledByNative
    private boolean getAllowRunningInsecureContentLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mMixedContentMode == WebSettings.MIXED_CONTENT_ALWAYS_ALLOW;
    }

    @CalledByNative
    private boolean getUseStricMixedContentCheckingLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mMixedContentMode == WebSettings.MIXED_CONTENT_NEVER_ALLOW;
    }

    @CalledByNative
    private boolean getAllowMixedContentAutoupgradesLocked() {
        if (AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_MIXED_CONTENT_AUTOUPGRADES)) {
            // We only allow mixed content autoupgrades (upgrading HTTP subresources to HTTPS in
            // HTTPS sites) when the mixed content mode is set to MIXED_CONTENT_COMPATIBILITY,
            // which keeps it in line with the behavior in Chrome. With
            // MIXED_CONTENT_ALWAYS_ALLOW, we disable autoupgrades since the developer is
            // explicitly allowing mixed content, whereas with MIXED_CONTENT_NEVER_ALLOW, there
            // is no need to autoupgrade since the content will be blocked.
            assert Thread.holdsLock(mAwSettingsLock);
            return mMixedContentMode == WebSettings.MIXED_CONTENT_COMPATIBILITY_MODE;
        }
        return false;
    }

    public boolean getOffscreenPreRaster() {
        synchronized (mAwSettingsLock) {
            return getOffscreenPreRasterLocked();
        }
    }

    @CalledByNative
    private boolean getOffscreenPreRasterLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mOffscreenPreRaster;
    }

    /**
     * Sets whether this WebView should raster tiles when it is
     * offscreen but attached to window. Turning this on can avoid
     * rendering artifacts when animating an offscreen WebView on-screen.
     * In particular, insertVisualStateCallback requires this mode to function.
     * Offscreen WebViews in this mode uses more memory. Please follow
     * these guidelines to limit memory usage:
     * - Webview size should be not be larger than the device screen size.
     * - Limit simple mode to a small number of webviews. Use it for
     *   visible webviews and webviews about to be animated to visible.
     */
    public void setOffscreenPreRaster(boolean enabled) {
        synchronized (mAwSettingsLock) {
            if (enabled != mOffscreenPreRaster) {
                mOffscreenPreRaster = enabled;
                mEventHandler.runOnUiThreadBlockingAndLocked(
                        () -> updateOffscreenPreRasterOnUiThreadLocked());
            }
        }
    }

    public int getDisabledActionModeMenuItems() {
        synchronized (mAwSettingsLock) {
            return mDisabledMenuItems;
        }
    }

    public void setDisabledActionModeMenuItems(int menuItems) {
        synchronized (mAwSettingsLock) {
            if (mDisabledMenuItems != menuItems) {
                flushBackForwardCacheOnUiThreadLocked();
            }
            mDisabledMenuItems = menuItems;
        }
    }

    public void updateAcceptLanguages() {
        synchronized (mAwSettingsLock) {
            mEventHandler.runOnUiThreadBlockingAndLocked(
                    () -> updateRendererPreferencesOnUiThreadLocked());
        }
    }

    @CalledByNative
    private boolean supportsDoubleTapZoomLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mSupportZoom && mBuiltInZoomControls && mUseWideViewport;
    }

    private boolean supportsMultiTouchZoomLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        return mSupportZoom && mBuiltInZoomControls;
    }

    boolean supportsMultiTouchZoom() {
        synchronized (mAwSettingsLock) {
            return supportsMultiTouchZoomLocked();
        }
    }

    boolean shouldDisplayZoomControls() {
        synchronized (mAwSettingsLock) {
            return supportsMultiTouchZoomLocked() && mDisplayZoomControls;
        }
    }

    private int clipFontSize(int size) {
        if (size < MINIMUM_FONT_SIZE) {
            return MINIMUM_FONT_SIZE;
        } else if (size > MAXIMUM_FONT_SIZE) {
            return MAXIMUM_FONT_SIZE;
        }
        return size;
    }

    @CalledByNative
    private boolean getRecordFullDocument() {
        assert Thread.holdsLock(mAwSettingsLock);
        return AwContentsStatics.getRecordFullDocument();
    }

    @CalledByNative
    private void updateEverything() {
        synchronized (mAwSettingsLock) {
            updateEverythingLocked();
        }
    }

    @CalledByNative
    private void populateWebPreferences(long webPrefsPtr) {
        synchronized (mAwSettingsLock) {
            assert mNativeAwSettings != 0;
            AwSettingsJni.get()
                    .populateWebPreferencesLocked(mNativeAwSettings, AwSettings.this, webPrefsPtr);
        }
    }

    private void updateInitialPageScaleOnUiThreadLocked() {
        assert mEventHandler.mHandler != null;
        ThreadUtils.assertOnUiThread();
        flushBackForwardCache();
        if (mNativeAwSettings != 0) {
            AwSettingsJni.get().updateInitialPageScaleLocked(mNativeAwSettings, AwSettings.this);
        }
    }

    private void updateUserAgentOnUiThreadLocked() {
        assert mEventHandler.mHandler != null;
        ThreadUtils.assertOnUiThread();
        flushBackForwardCache();
        if (mNativeAwSettings != 0) {
            AwSettingsJni.get().updateUserAgentLocked(mNativeAwSettings, AwSettings.this);
        }
    }

    private void updateWebkitPreferencesOnUiThreadLocked() {
        assert mEventHandler.mHandler != null;
        ThreadUtils.assertOnUiThread();
        flushBackForwardCache();
        if (mNativeAwSettings != 0) {
            AwSettingsJni.get().updateWebkitPreferencesLocked(mNativeAwSettings, AwSettings.this);
        }
    }

    private void updateRendererPreferencesOnUiThreadLocked() {
        assert mEventHandler.mHandler != null;
        ThreadUtils.assertOnUiThread();
        flushBackForwardCache();
        if (mNativeAwSettings != 0) {
            AwSettingsJni.get().updateRendererPreferencesLocked(mNativeAwSettings, AwSettings.this);
        }
    }

    private void updateOffscreenPreRasterOnUiThreadLocked() {
        assert mEventHandler.mHandler != null;
        ThreadUtils.assertOnUiThread();
        flushBackForwardCache();
        if (mNativeAwSettings != 0) {
            AwSettingsJni.get().updateOffscreenPreRasterLocked(mNativeAwSettings, AwSettings.this);
        }
    }

    private void updateWillSuppressErrorStateOnUiThreadLocked() {
        assert mEventHandler.mHandler != null;
        ThreadUtils.assertOnUiThread();
        flushBackForwardCache();
        if (mNativeAwSettings != 0) {
            AwSettingsJni.get()
                    .updateWillSuppressErrorStateLocked(mNativeAwSettings, AwSettings.this);
        }
    }

    private void updateCookiePolicyOnUiThreadLocked() {
        assert mEventHandler.mHandler != null;
        ThreadUtils.assertOnUiThread();
        flushBackForwardCache();
        if (mNativeAwSettings != 0) {
            AwSettingsJni.get().updateCookiePolicyLocked(mNativeAwSettings, AwSettings.this);
        }
    }

    private void updateAllowFileAccessOnUiThreadLocked() {
        assert mEventHandler.mHandler != null;
        ThreadUtils.assertOnUiThread();
        flushBackForwardCache();
        if (mNativeAwSettings != 0) {
            AwSettingsJni.get().updateAllowFileAccessLocked(mNativeAwSettings, AwSettings.this);
        }
    }

    private void updateSpeculativeLoadingAllowedOnUiThreadLocked() {
        assert mEventHandler.mHandler != null;
        ThreadUtils.assertOnUiThread();
        if (mNativeAwSettings != 0) {
            AwSettingsJni.get()
                    .updateSpeculativeLoadingAllowedLocked(mNativeAwSettings, AwSettings.this);
        }
    }

    private void updateBackForwardCacheEnabledOnUiThreadLocked() {
        assert mEventHandler.mHandler != null;
        ThreadUtils.assertOnUiThread();
        if (mNativeAwSettings != 0) {
            AwSettingsJni.get()
                    .updateBackForwardCacheEnabledLocked(mNativeAwSettings, AwSettings.this);
        }
    }

    private void updateGeolocationEnabledOnUiThreadLocked() {
        assert mEventHandler.mHandler != null;
        ThreadUtils.assertOnUiThread();
        if (mNativeAwSettings != 0) {
            AwSettingsJni.get().updateGeolocationEnabledLocked(mNativeAwSettings, AwSettings.this);
        }
    }

    public void setEnterpriseAuthenticationAppLinkPolicyEnabled(boolean enabled) {
        synchronized (mAwSettingsLock) {
            mEventHandler.runOnUiThreadBlockingAndLocked(
                    () -> {
                        flushBackForwardCache();
                        if (mNativeAwSettings != 0) {
                            AwSettingsJni.get()
                                    .setEnterpriseAuthenticationAppLinkPolicyEnabled(
                                            mNativeAwSettings, AwSettings.this, enabled);
                        }
                    });
        }
    }

    public boolean getEnterpriseAuthenticationAppLinkPolicyEnabled() {
        synchronized (mAwSettingsLock) {
            assert mNativeAwSettings != 0;
            return AwSettingsJni.get()
                    .getEnterpriseAuthenticationAppLinkPolicyEnabled(
                            mNativeAwSettings, AwSettings.this);
        }
    }

    public void setWebViewIntegrityApiStatus(
            @MediaIntegrityApiStatus int defaultStatus,
            Map<String, @MediaIntegrityApiStatus Integer> permissionConfig) {
        synchronized (mAwSettingsLock) {
            mIntegrityApiStatusConfig.setApiAvailabilityRules(defaultStatus, permissionConfig);
        }
    }

    public @MediaIntegrityApiStatus int getWebViewIntegrityApiDefaultStatus() {
        synchronized (mAwSettingsLock) {
            return mIntegrityApiStatusConfig.getDefaultStatus();
        }
    }

    public Map<String, @MediaIntegrityApiStatus Integer> getWebViewIntegrityApiOverrideRules() {
        synchronized (mAwSettingsLock) {
            return mIntegrityApiStatusConfig.getOverrideRules();
        }
    }

    public @MediaIntegrityApiStatus int getWebViewIntegrityApiStatusForUri(Uri uri) {
        synchronized (mAwSettingsLock) {
            return mIntegrityApiStatusConfig.getStatusForUri(uri);
        }
    }

    public void setWebauthnSupport(@WebauthnMode int support) {
        synchronized (mAwSettingsLock) {
            if (mWebauthnMode != support && AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_WEBAUTHN)) {
                mWebauthnMode = support;
                mEventHandler.updateWebkitPreferencesLocked();
                WebauthnModeProvider.getInstance()
                        .setWebauthnModeForWebContents(mWebContents, support);
            }
        }
    }

    @CalledByNative
    public @WebauthnMode int getWebauthnSupportLocked() {
        assert Thread.holdsLock(mAwSettingsLock);
        // TODO(crbug.com/40210253): Consider supporting a NOT_SUPPORTED case.
        return AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_WEBAUTHN)
                ? mWebauthnMode
                : WebauthnMode.NONE;
    }

    public int getWebauthnSupport() {
        synchronized (mAwSettingsLock) {
            return getWebauthnSupportLocked();
        }
    }

    @NativeMethods
    interface Natives {
        long init(AwSettings caller, WebContents webContents);

        void destroy(long nativeAwSettings, AwSettings caller);

        AwSettings fromWebContents(WebContents webContents);

        void populateWebPreferencesLocked(
                long nativeAwSettings, AwSettings caller, long webPrefsPtr);

        void resetScrollAndScaleState(long nativeAwSettings, AwSettings caller);

        void updateEverythingLocked(long nativeAwSettings, AwSettings caller);

        void updateInitialPageScaleLocked(long nativeAwSettings, AwSettings caller);

        void updateUserAgentLocked(long nativeAwSettings, AwSettings caller);

        void updateWebkitPreferencesLocked(long nativeAwSettings, AwSettings caller);

        String getDefaultUserAgent();

        AwUserAgentMetadata getDefaultUserAgentMetadata();

        void updateRendererPreferencesLocked(long nativeAwSettings, AwSettings caller);

        void updateOffscreenPreRasterLocked(long nativeAwSettings, AwSettings caller);

        void updateWillSuppressErrorStateLocked(long nativeAwSettings, AwSettings caller);

        void updateCookiePolicyLocked(long nativeAwSettings, AwSettings caller);

        void updateAllowFileAccessLocked(long nativeAwSettings, AwSettings caller);

        void updateSpeculativeLoadingAllowedLocked(long nativeAwSettings, AwSettings caller);

        void updateBackForwardCacheEnabledLocked(long nativeAwSettings, AwSettings caller);

        boolean isForceDarkApplied(long nativeAwSettings, AwSettings caller);

        boolean prefersDarkFromTheme(long nativeAwSettings, AwSettings caller);

        void setEnterpriseAuthenticationAppLinkPolicyEnabled(
                long nativeAwSettings, AwSettings caller, boolean enabled);

        boolean getEnterpriseAuthenticationAppLinkPolicyEnabled(
                long nativeAwSettings, AwSettings caller);

        String[] updateXRequestedWithAllowListOriginMatcher(long nativeAwSettings, String[] rules);

        void updateGeolocationEnabledLocked(long nativeAwSettings, AwSettings caller);
    }
}
