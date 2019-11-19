// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.os.Handler;
import android.os.Looper;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;

/**
 * AwCookieManager manages cookies according to RFC2109 spec.
 *
 * Methods in this class are thread safe.
 */
@JNINamespace("android_webview")
public final class AwCookieManager {
    private long mNativeCookieManager;

    public AwCookieManager() {
        this(AwCookieManagerJni.get().getDefaultCookieManager());
    }

    public AwCookieManager(long nativeCookieManager) {
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_WEBVIEW);
        mNativeCookieManager = nativeCookieManager;
    }

    /**
     * Control whether cookie is enabled or disabled
     * @param accept TRUE if accept cookie
     */
    public void setAcceptCookie(boolean accept) {
        AwCookieManagerJni.get().setShouldAcceptCookies(
                mNativeCookieManager, AwCookieManager.this, accept);
    }

    /**
     * Return whether cookie is enabled
     * @return TRUE if accept cookie
     */
    public boolean acceptCookie() {
        return AwCookieManagerJni.get().getShouldAcceptCookies(
                mNativeCookieManager, AwCookieManager.this);
    }

    /**
     * Synchronous version of setCookie.
     */
    public void setCookie(String url, String value) {
        UrlValue pair = fixupUrlValue(url, value);
        AwCookieManagerJni.get().setCookieSync(
                mNativeCookieManager, AwCookieManager.this, pair.mUrl, pair.mValue);
    }

    /**
     * Deprecated synchronous version of removeSessionCookies.
     */
    public void removeSessionCookies() {
        AwCookieManagerJni.get().removeSessionCookiesSync(
                mNativeCookieManager, AwCookieManager.this);
    }

    /**
     * Deprecated synchronous version of removeAllCookies.
     */
    public void removeAllCookies() {
        AwCookieManagerJni.get().removeAllCookiesSync(mNativeCookieManager, AwCookieManager.this);
    }

    /**
     * Set cookie for a given url. The old cookie with same host/path/name will
     * be removed. The new cookie will be added if it is not expired or it does
     * not have expiration which implies it is session cookie.
     * @param url The url which cookie is set for.
     * @param value The value for set-cookie: in http response header.
     * @param callback A callback called with the success status after the cookie is set.
     */
    public void setCookie(final String url, final String value, final Callback<Boolean> callback) {
        try {
            UrlValue pair = fixupUrlValue(url, value);
            AwCookieManagerJni.get().setCookie(mNativeCookieManager, AwCookieManager.this,
                    pair.mUrl, pair.mValue, new CookieCallback(callback));
        } catch (IllegalStateException e) {
            throw new IllegalStateException(
                    "SetCookie must be called on a thread with a running Looper.");
        }
    }

    /**
     * Get cookie(s) for a given url so that it can be set to "cookie:" in http
     * request header.
     * @param url The url needs cookie
     * @return The cookies in the format of NAME=VALUE [; NAME=VALUE]
     */
    public String getCookie(final String url) {
        String cookie =
                AwCookieManagerJni.get().getCookie(mNativeCookieManager, AwCookieManager.this, url);
        // Return null if the string is empty to match legacy behavior
        return cookie == null || cookie.trim().isEmpty() ? null : cookie;
    }

    /**
     * Remove all session cookies, the cookies without an expiration date.
     * The value of the callback is true iff at least one cookie was removed.
     * @param callback A callback called after the cookies (if any) are removed.
     */
    public void removeSessionCookies(Callback<Boolean> callback) {
        try {
            AwCookieManagerJni.get().removeSessionCookies(
                    mNativeCookieManager, AwCookieManager.this, new CookieCallback(callback));
        } catch (IllegalStateException e) {
            throw new IllegalStateException(
                    "removeSessionCookies must be called on a thread with a running Looper.");
        }
    }

    /**
     * Remove all cookies.
     * The value of the callback is true iff at least one cookie was removed.
     * @param callback A callback called after the cookies (if any) are removed.
     */
    public void removeAllCookies(Callback<Boolean> callback) {
        try {
            AwCookieManagerJni.get().removeAllCookies(
                    mNativeCookieManager, AwCookieManager.this, new CookieCallback(callback));
        } catch (IllegalStateException e) {
            throw new IllegalStateException(
                    "removeAllCookies must be called on a thread with a running Looper.");
        }
    }

    /**
     *  Return true if there are stored cookies.
     */
    public boolean hasCookies() {
        return AwCookieManagerJni.get().hasCookies(mNativeCookieManager, AwCookieManager.this);
    }

    /**
     * Remove all expired cookies
     */
    public void removeExpiredCookies() {
        AwCookieManagerJni.get().removeExpiredCookies(mNativeCookieManager, AwCookieManager.this);
    }

    public void flushCookieStore() {
        AwCookieManagerJni.get().flushCookieStore(mNativeCookieManager, AwCookieManager.this);
    }

    /**
     * Whether cookies are accepted for file scheme URLs.
     */
    public boolean allowFileSchemeCookies() {
        return AwCookieManagerJni.get().getAllowFileSchemeCookies(
                mNativeCookieManager, AwCookieManager.this);
    }

    /**
     * Sets whether cookies are accepted for file scheme URLs.
     *
     * Use of cookies with file scheme URLs is potentially insecure. Do not use this feature unless
     * you can be sure that no unintentional sharing of cookie data can take place.
     * <p>
     * Note that calls to this method will have no effect if made after a WebView or CookieManager
     * instance has been created.
     */
    public void setAcceptFileSchemeCookies(boolean accept) {
        AwCookieManagerJni.get().setAllowFileSchemeCookies(
                mNativeCookieManager, AwCookieManager.this, accept);
    }

    /**
     * CookieCallback is a bridge that knows how to call a Callback on its original thread.
     * We need to arrange for the users Callback#onResult to be called on the original
     * thread after the work is done. When the API is called we construct a CookieCallback which
     * remembers the handler of the current thread. Later the native code uses
     * the native method |RunBooleanCallbackAndroid| to call CookieCallback#onResult which posts a
     * Runnable on the handler of the original thread which in turn calls Callback#onResult.
     */
    static class CookieCallback implements Callback<Boolean> {
        @Nullable
        Callback<Boolean> mCallback;
        @Nullable
        Handler mHandler;

        public CookieCallback(@Nullable Callback<Boolean> callback) {
            if (callback != null) {
                if (Looper.myLooper() == null) {
                    throw new IllegalStateException("new CookieCallback should be called on "
                            + "a thread with a running Looper.");
                }
                mCallback = callback;
                mHandler = new Handler();
            }
        }

        @Override
        public void onResult(final Boolean result) {
            if (mHandler == null) return;
            assert mCallback != null;
            mHandler.post(() -> mCallback.onResult(result));
        }
    }

    /**
     * A tuple to hold a URL and Value when setting a cookie.
     */
    private static class UrlValue {
        public String mUrl;
        public String mValue;

        public UrlValue(String url, String value) {
            mUrl = url;
            mValue = value;
        }
    }

    private static String appendDomain(String value, String domain) {
        // Prefer the explicit Domain attribute, if available. We allow any case for "Domain".
        if (value.matches("^.*(?i);[\\t ]*Domain[\\t ]*=.*$")) {
            return value;
        } else if (value.matches("^.*;\\s*$")) {
            return value + " Domain=" + domain;
        }
        return value + "; Domain=" + domain;
    }

    private static UrlValue fixupUrlValue(String url, String value) {
        final String leadingHttpTripleSlashDot = "http:///.";

        // The app passed a domain instead of a real URL (and the glue layer "fixed" it into this
        // form). For backwards compatibility, we fix this into a well-formed URL and add a Domain
        // attribute to the cookie value.
        if (url.startsWith(leadingHttpTripleSlashDot)) {
            String domain = url.substring(leadingHttpTripleSlashDot.length() - 1);
            url = "http://" + url.substring(leadingHttpTripleSlashDot.length());
            value = appendDomain(value, domain);
        }
        return new UrlValue(url, value);
    }

    @NativeMethods
    interface Natives {
        long getDefaultCookieManager();
        void setShouldAcceptCookies(
                long nativeCookieManager, AwCookieManager caller, boolean accept);
        boolean getShouldAcceptCookies(long nativeCookieManager, AwCookieManager caller);
        void setCookie(long nativeCookieManager, AwCookieManager caller, String url, String value,
                CookieCallback callback);
        void setCookieSync(
                long nativeCookieManager, AwCookieManager caller, String url, String value);
        String getCookie(long nativeCookieManager, AwCookieManager caller, String url);
        void removeSessionCookies(
                long nativeCookieManager, AwCookieManager caller, CookieCallback callback);
        void removeSessionCookiesSync(long nativeCookieManager, AwCookieManager caller);
        void removeAllCookies(
                long nativeCookieManager, AwCookieManager caller, CookieCallback callback);
        void removeAllCookiesSync(long nativeCookieManager, AwCookieManager caller);
        void removeExpiredCookies(long nativeCookieManager, AwCookieManager caller);
        void flushCookieStore(long nativeCookieManager, AwCookieManager caller);
        boolean hasCookies(long nativeCookieManager, AwCookieManager caller);
        boolean getAllowFileSchemeCookies(long nativeCookieManager, AwCookieManager caller);
        void setAllowFileSchemeCookies(
                long nativeCookieManager, AwCookieManager caller, boolean allow);
    }
}
