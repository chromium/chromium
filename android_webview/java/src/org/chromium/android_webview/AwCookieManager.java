// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.os.Handler;
import android.os.Looper;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.WebViewCachedFlags;
import org.chromium.base.Callback;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.url.GURL;

import java.net.URISyntaxException;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;
import java.util.regex.Pattern;

/**
 * AwCookieManager manages cookies according to RFC2109 spec.
 *
 * <p>Methods in this class are thread safe.
 *
 * <p>The default profile's cookie manager has a singleton lifetime, whereas a non-default profile
 * has a cookie manager that is lifetime scoped to the profile.
 */
@JNINamespace("android_webview")
public final class AwCookieManager {
    private final long mNativeCookieManager;

    /**
     * The class loader will take care of synchronization as each class
     * is only loaded once at the time it is needed. Meaning that the first time
     * {@link AwCookieManager#getDefaultCookieManager()} is called, the static instance
     * of the default cookie manager will be initialized within the holder class.
     */
    private static final class DefaultCookieManagerHolder {
        private static final AwCookieManager sDefaultCookieManager = new AwCookieManager();
    }

    public static AwCookieManager getDefaultCookieManager() {
        return DefaultCookieManagerHolder.sDefaultCookieManager;
    }

    /**
     * Disable cookie partitioning (CHIPS).
     *
     * <p>This is a global setting, and must be called before the browser process is started and the
     * native CookieManager is accessed the first time.
     */
    public static void disablePartitionedCookiesGlobal() {
        AwCookieManagerJni.get().disablePartitionedCookies();
    }

    @VisibleForTesting
    public AwCookieManager() {
        this(AwCookieManagerJni.get().getDefaultCookieManager());
    }

    public AwCookieManager(long nativeCookieManager) {
        LibraryLoader.getInstance().ensureInitialized();
        mNativeCookieManager = nativeCookieManager;
    }

    @CalledByNative
    private static AwCookieManager create(long nativeCookieManager) {
        return new AwCookieManager(nativeCookieManager);
    }

    /**
     * Control whether cookie is enabled or disabled
     *
     * @param accept TRUE if accept cookie
     */
    public void setAcceptCookie(boolean accept) {
        AwCookieManagerJni.get().setShouldAcceptCookies(mNativeCookieManager, accept);
    }

    /**
     * Return whether cookie is enabled
     *
     * @return TRUE if accept cookie
     */
    public boolean acceptCookie() {
        return AwCookieManagerJni.get().getShouldAcceptCookies(mNativeCookieManager);
    }

    /** Synchronous version of setCookie. */
    public void setCookie(String url, String value) {
        AwCookieManagerJni.get().setCookieSync(mNativeCookieManager, url, value);
    }

    /** Deprecated synchronous version of removeSessionCookies. */
    public void removeSessionCookies() {
        AwCookieManagerJni.get().removeSessionCookiesSync(mNativeCookieManager);
    }

    /** Deprecated synchronous version of removeAllCookies. */
    public void removeAllCookies() {
        AwCookieManagerJni.get().removeAllCookiesSync(mNativeCookieManager);
    }

    /**
     * Set cookie for a given url. The old cookie with same host/path/name will be removed. The new
     * cookie will be added if it is not expired or it does not have expiration which implies it is
     * session cookie.
     *
     * @param url The url which cookie is set for.
     * @param value The value for set-cookie: in http response header.
     * @param callback A callback called with the success status after the cookie is set.
     */
    public void setCookie(final String url, final String value, final Callback<Boolean> callback) {
        try {
            AwCookieManagerJni.get()
                    .setCookie(mNativeCookieManager, url, value, new CookieCallback(callback));
        } catch (IllegalStateException e) {
            throw new IllegalStateException(
                    "SetCookie must be called on a thread with a running Looper.");
        }
    }

    /**
     * Get cookie(s) for a given url so that it can be set to "cookie:" in http request header.
     *
     * @param url The url needs cookie
     * @return The cookies in the format of NAME=VALUE [; NAME=VALUE]
     */
    public String getCookie(final String url) {
        String cookie = AwCookieManagerJni.get().getCookie(mNativeCookieManager, url);
        // Return null if the string is empty to match legacy behavior
        return cookie == null || cookie.trim().isEmpty() ? null : cookie;
    }

    /** Set cookie for a given url, after applying compatibility fixups to the URL. */
    public void setCookieWithUrlFixup(final String url, final String value)
            throws URISyntaxException {
        UrlValue pair = fixupUrlValue(url, value);
        setCookie(pair.mUrl, pair.mValue);
    }

    /** Set cookie for a given url, after applying compatibility fixups to the URL. */
    public void setCookieWithUrlFixup(
            final String url, final String value, final Callback<Boolean> callback)
            throws URISyntaxException {
        UrlValue pair = fixupUrlValue(url, value);
        setCookie(pair.mUrl, pair.mValue, callback);
    }

    /** Get cookie(s) for a given url, after applying compatibility fixups to the URL. */
    public String getCookieWithUrlFixup(final String url) throws URISyntaxException {
        return getCookie(fixupUrl(url));
    }

    /**
     * Get the attributes of any cookie(s) for a given url.
     *
     * @param url The url for which the cookies are set.
     * @return The cookies as a list of Strings formatted like http set cookie headers.
     */
    public List<String> getCookieInfo(final String url) {
        String[] cookies = AwCookieManagerJni.get().getCookieInfo(mNativeCookieManager, url);
        return Arrays.asList(cookies);
    }

    /**
     * Remove all session cookies, the cookies without an expiration date.
     * The value of the callback is true iff at least one cookie was removed.
     * @param callback A callback called after the cookies (if any) are removed.
     */
    public void removeSessionCookies(Callback<Boolean> callback) {
        try {
            AwCookieManagerJni.get()
                    .removeSessionCookies(mNativeCookieManager, new CookieCallback(callback));

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
            AwCookieManagerJni.get()
                    .removeAllCookies(mNativeCookieManager, new CookieCallback(callback));

        } catch (IllegalStateException e) {
            throw new IllegalStateException(
                    "removeAllCookies must be called on a thread with a running Looper.");
        }
    }

    /** Return true if there are stored cookies. */
    public boolean hasCookies() {
        return AwCookieManagerJni.get().hasCookies(mNativeCookieManager);
    }

    /** Remove all expired cookies */
    public void removeExpiredCookies() {
        AwCookieManagerJni.get().removeExpiredCookies(mNativeCookieManager);
    }

    public void flushCookieStore() {
        AwCookieManagerJni.get().flushCookieStore(mNativeCookieManager);
    }

    /** Whether cookies are accepted for file scheme URLs. */
    public boolean allowFileSchemeCookies() {
        return AwCookieManagerJni.get().getAllowFileSchemeCookies(mNativeCookieManager);
    }

    /**
     * Sets whether cookies are accepted for file scheme URLs.
     *
     * <p>Use of cookies with file scheme URLs is potentially insecure. Do not use this feature
     * unless you can be sure that no unintentional sharing of cookie data can take place.
     *
     * <p>Note that calls to this method will have no effect if made after a WebView or
     * CookieManager instance has been created.
     */
    public void setAcceptFileSchemeCookies(boolean accept) {
        AwCookieManagerJni.get().setAllowFileSchemeCookies(mNativeCookieManager, accept);
    }

    /**
     * Sets whether cookies for insecure schemes (http:) are permitted to include the "Secure"
     * directive.
     */
    public void setWorkaroundHttpSecureCookiesForTesting(boolean allow) {
        AwCookieManagerJni.get()
                .setWorkaroundHttpSecureCookiesForTesting(mNativeCookieManager, allow);
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
        @Nullable Callback<Boolean> mCallback;
        @Nullable Handler mHandler;

        public CookieCallback(@Nullable Callback<Boolean> callback) {
            if (callback != null) {
                if (Looper.myLooper() == null) {
                    throw new IllegalStateException(
                            "new CookieCallback should be called on "
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
            mHandler.post(mCallback.bind(result));
        }
    }

    /** A tuple to hold a URL and Value when setting a cookie. */
    @VisibleForTesting
    public static class UrlValue {
        public final String mUrl;
        public final String mValue;

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

    private static String fixupUrlWithWebAddressParser(String url) throws URISyntaxException {
        // WebAddressParser is a copy of the  private API WebAddress in the android framework and a
        // "quirk" of the Classic WebView implementation that allowed embedders to be relaxed about
        // what URLs they passed into the CookieManager, so we do the same normalisation.
        //
        // The implementation of WebAddressParser isn't ideal, we should remove its usage and
        // replace it with UrlFormatter or similar URL parser.
        return new WebAddressParser(url).toString();
    }

    private static UrlValue fixupUrlValueWithWebAddressParser(String url, String value)
            throws URISyntaxException {
        url = fixupUrlWithWebAddressParser(url);

        final String leadingHttpTripleSlashDot = "http:///.";

        // The app passed a domain instead of a real URL (and WebAddressParser "fixed" it into this
        // form). For backwards compatibility, we fix this into a well-formed URL and add a Domain
        // attribute to the cookie value.
        if (url.startsWith(leadingHttpTripleSlashDot)) {
            String domain = url.substring(leadingHttpTripleSlashDot.length() - 1);
            url = "http://" + url.substring(leadingHttpTripleSlashDot.length());
            value = appendDomain(value, domain);
        }
        return new UrlValue(url, value);
    }

    @IntDef({
        GuessedInput.POSSIBLE_URL,
        GuessedInput.POSSIBLE_HOSTNAME,
        GuessedInput.POSSIBLE_HOSTNAME_LEADING_DOT,
    })
    private @interface GuessedInput {
        int POSSIBLE_URL = 0;
        int POSSIBLE_HOSTNAME = 1;
        int POSSIBLE_HOSTNAME_LEADING_DOT = 2;
    }

    // Match any of the special characters that act as delimiters to split URLs into components:
    //   : separates scheme from the rest of the content, and separates host from port
    //   / is part of the hierarchical scheme indicator, and separates authority from path
    //   @ separates userinfo from host
    //   ? separates a query string
    //   # separates a fragment identifier
    // If none of these characters are present, it's very unlikely any parser that's not
    // specifically trying to handle bare hostnames would consider this a meaningful URL.
    private static final Pattern MAYBE_URL_CHARACTER = Pattern.compile("[:/@?#]");

    private static @GuessedInput int guessInputType(String input) {
        // It's likely that many invalid URLs passed by apps are just bare hostnames. This is a
        // plausible misunderstanding of the API: it's easy to think of cookies as being associated
        // with a host/domain, especially when unfamiliar with the many changes to cookie handling
        // in the modern web security model, but a full URL is actually required: e.g.
        //  - Secure cookies are only included when the scheme is `https`, not `http`.
        //  - Cookies can be specific to a particular path prefix within a domain.
        //
        // The legacy behavior here was to use `WebAddressParser` to attempt to fix up the input
        // into a full URL, but this accepts many kinds of malformed input and can produce results
        // that are inconsistent with other URL parsers, creating security issues if the input is
        // from an untrusted source.
        //
        // Instead, we try to *specifically* detect the bare domain case, without touching other
        // kinds of malformed input.

        if (MAYBE_URL_CHARACTER.matcher(input).find()) {
            // If the input contains any URL delimiter characters at all, then a sufficiently
            // tolerant URL parser used by the host app might have attempted to split this into
            // components and interpreted some part of the string as a hostname even if it's not
            // actually a valid URL. In that case, fixing up the input might result in CookieManager
            // using a *different* hostname and defeat the host app's attempt to validate the URL.
            // Leave it alone to be parsed or rejected by GURL according to its normal rules.
            return GuessedInput.POSSIBLE_URL;
        }

        if (input.startsWith(".")) {
            // The `domain` attribute on a cookie, which widens the scope of a cookie to a suffix of
            // the current hostname, accepts an optional leading dot. Some apps assume they can pass
            // values of the `domain` attribute to CookieManager as URLs, and may include this dot.
            return GuessedInput.POSSIBLE_HOSTNAME_LEADING_DOT;
        }

        // Otherwise, this is plausibly a bare hostname.
        return GuessedInput.POSSIBLE_HOSTNAME;
    }

    private static String hostnameToUrl(String hostname) {
        return "http://" + hostname + "/";
    }

    @VisibleForTesting
    public static String fixupPossibleBareHostnameToUrl(String input) {
        @GuessedInput int guessedType = guessInputType(input);
        if (guessedType == GuessedInput.POSSIBLE_HOSTNAME
                || guessedType == GuessedInput.POSSIBLE_HOSTNAME_LEADING_DOT) {
            // When just fixing up a URL (for a getCookie call) the leading dot is not treated
            // specially, as the WebAddressParser-based fixup did not do this either.
            return hostnameToUrl(input);
        } else {
            return input;
        }
    }

    @VisibleForTesting
    public static UrlValue fixupPossibleBareHostnameToUrlValue(String inputUrl, String inputValue) {
        @GuessedInput int guessedType = guessInputType(inputUrl);
        if (guessedType == GuessedInput.POSSIBLE_HOSTNAME) {
            return new UrlValue(hostnameToUrl(inputUrl), inputValue);
        } else if (guessedType == GuessedInput.POSSIBLE_HOSTNAME_LEADING_DOT) {
            return new UrlValue(
                    hostnameToUrl(inputUrl.substring(1)), appendDomain(inputValue, inputUrl));
        } else {
            return new UrlValue(inputUrl, inputValue);
        }
    }

    private static String fixupUrl(String input) throws URISyntaxException {
        boolean useSimplerUrlFixups =
                WebViewCachedFlags.get()
                        .isCachedFeatureEnabled(
                                AwFeatures.WEBVIEW_COOKIE_MANAGER_SIMPLER_URL_FIXUPS);
        String bareHostnameFixedUp = fixupPossibleBareHostnameToUrl(input);

        try {
            String webAddressParserFixedUp = fixupUrlWithWebAddressParser(input);
            compareUrlFixups(input, webAddressParserFixedUp, bareHostnameFixedUp);
            if (useSimplerUrlFixups) {
                return bareHostnameFixedUp;
            } else {
                return webAddressParserFixedUp;
            }
        } catch (URISyntaxException e) {
            compareUrlFixups(input, null, bareHostnameFixedUp);
            if (useSimplerUrlFixups) {
                return bareHostnameFixedUp;
            } else {
                throw e;
            }
        }
    }

    private static UrlValue fixupUrlValue(String inputUrl, String inputValue)
            throws URISyntaxException {
        boolean useSimplerUrlFixups =
                WebViewCachedFlags.get()
                        .isCachedFeatureEnabled(
                                AwFeatures.WEBVIEW_COOKIE_MANAGER_SIMPLER_URL_FIXUPS);
        UrlValue bareHostnameFixedUp = fixupPossibleBareHostnameToUrlValue(inputUrl, inputValue);

        try {
            UrlValue webAddressParserFixedUp =
                    fixupUrlValueWithWebAddressParser(inputUrl, inputValue);
            compareUrlFixups(inputUrl, webAddressParserFixedUp.mUrl, bareHostnameFixedUp.mUrl);
            compareValueFixups(
                    inputValue, webAddressParserFixedUp.mValue, bareHostnameFixedUp.mValue);
            if (useSimplerUrlFixups) {
                return bareHostnameFixedUp;
            } else {
                return webAddressParserFixedUp;
            }
        } catch (URISyntaxException e) {
            compareUrlFixups(inputUrl, null, bareHostnameFixedUp.mUrl);
            if (useSimplerUrlFixups) {
                return bareHostnameFixedUp;
            } else {
                throw e;
            }
        }
    }

    // Used to record the UMA histograms Android.WebView.CookieFixup.*. Since these
    // values are persisted to logs, they should never be renumbered or reused.
    // LINT.IfChange(FixupResult)
    @VisibleForTesting
    @IntDef({
        FixupResult.BOTH_UNCHANGED,
        FixupResult.ONLY_BARE_HOSTNAME_FIXUP_CHANGED,
        FixupResult.BOTH_CHANGED_SAME_RESULT,
        FixupResult.ONLY_WEB_ADDRESS_PARSER_CHANGED,
        FixupResult.BOTH_CHANGED_DIFFERENT_RESULTS,
        FixupResult.WEB_ADDRESS_PARSER_THREW_BARE_HOSTNAME_FIXUP_UNCHANGED,
        FixupResult.WEB_ADDRESS_PARSER_THREW_BARE_HOSTNAME_FIXUP_CHANGED,
    })
    public @interface FixupResult {
        int BOTH_UNCHANGED = 0;
        int ONLY_BARE_HOSTNAME_FIXUP_CHANGED = 1;
        int BOTH_CHANGED_SAME_RESULT = 2;
        int ONLY_WEB_ADDRESS_PARSER_CHANGED = 3;
        int BOTH_CHANGED_DIFFERENT_RESULTS = 4;
        int WEB_ADDRESS_PARSER_THREW_BARE_HOSTNAME_FIXUP_UNCHANGED = 5;
        int WEB_ADDRESS_PARSER_THREW_BARE_HOSTNAME_FIXUP_CHANGED = 6;
        int COUNT = 7;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:FixupResult)

    private static @FixupResult int compareFixups(
            String original, @Nullable String webAddressParser, String bareHostname) {
        if (webAddressParser == null) {
            // WebAddressParser threw URISyntaxException
            if (Objects.equals(original, bareHostname)) {
                return FixupResult.WEB_ADDRESS_PARSER_THREW_BARE_HOSTNAME_FIXUP_UNCHANGED;
            } else {
                return FixupResult.WEB_ADDRESS_PARSER_THREW_BARE_HOSTNAME_FIXUP_CHANGED;
            }
        } else if (Objects.equals(original, webAddressParser)) {
            // WebAddressParser did not change it
            if (Objects.equals(original, bareHostname)) {
                return FixupResult.BOTH_UNCHANGED;
            } else {
                return FixupResult.ONLY_BARE_HOSTNAME_FIXUP_CHANGED;
            }
        } else {
            // WebAddressParser changed it
            if (Objects.equals(webAddressParser, bareHostname)) {
                return FixupResult.BOTH_CHANGED_SAME_RESULT;
            } else if (Objects.equals(original, bareHostname)) {
                return FixupResult.ONLY_WEB_ADDRESS_PARSER_CHANGED;
            } else {
                return FixupResult.BOTH_CHANGED_DIFFERENT_RESULTS;
            }
        }
    }

    private static void compareUrlFixups(
            String original, String webAddressParser, String bareHostname) {
        // Parse and canonicalize each version of the URL via GURL.
        // We use getPossiblyInvalidSpec as the goal is to compare what we would pass on to the next
        // layer below with different fixup strategies, not to actually validate the URL here.
        String canonicalOriginal = new GURL(original).getPossiblyInvalidSpec();
        String canonicalWebAddressParser = new GURL(webAddressParser).getPossiblyInvalidSpec();
        String canonicalBareHostname = new GURL(bareHostname).getPossiblyInvalidSpec();

        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.CookieFixup.Url",
                compareFixups(canonicalOriginal, canonicalWebAddressParser, canonicalBareHostname),
                FixupResult.COUNT);
    }

    private static void compareValueFixups(
            String original, String webAddressParser, String bareHostname) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.CookieFixup.Value",
                compareFixups(original, webAddressParser, bareHostname),
                FixupResult.COUNT);
    }

    @NativeMethods
    interface Natives {
        long getDefaultCookieManager();

        void setShouldAcceptCookies(long nativeCookieManager, boolean accept);

        boolean getShouldAcceptCookies(long nativeCookieManager);

        void setCookie(
                long nativeCookieManager,
                String url,
                @JniType("std::string") String value,
                @JniType("base::OnceCallback<void(bool)>") CookieCallback callback);

        void setCookieSync(
                long nativeCookieManager, String url, @JniType("std::string") String value);

        @JniType("std::string")
        String getCookie(long nativeCookieManager, String url);

        String[] getCookieInfo(long nativeCookieManager, String url);

        void removeSessionCookies(
                long nativeCookieManager,
                @JniType("base::OnceCallback<void(bool)>") CookieCallback callback);

        void removeSessionCookiesSync(long nativeCookieManager);

        void removeAllCookies(
                long nativeCookieManager,
                @JniType("base::OnceCallback<void(bool)>") CookieCallback callback);

        void removeAllCookiesSync(long nativeCookieManager);

        void removeExpiredCookies(long nativeCookieManager);

        void flushCookieStore(long nativeCookieManager);

        boolean hasCookies(long nativeCookieManager);

        boolean getAllowFileSchemeCookies(long nativeCookieManager);

        void setAllowFileSchemeCookies(long nativeCookieManager, boolean allow);

        void setWorkaroundHttpSecureCookiesForTesting(long nativeCookieManager, boolean allow);

        void disablePartitionedCookies();
    }
}
