// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.verify.domain.DomainVerificationManager;
import android.content.pm.verify.domain.DomainVerificationUserState;
import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.AnyThread;
import androidx.annotation.RequiresApi;
import androidx.annotation.UiThread;
import androidx.annotation.VisibleForTesting;
import androidx.browser.trusted.Token;
import androidx.browser.trusted.TrustedWebActivityService;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.StrictModeContext;
import org.chromium.base.metrics.TimingMetric;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.permissiondelegation.InstalledWebappPermissionManager;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.url.GURL;

import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.function.Supplier;

/**
 * Validates whether an installed package or a web origin is handled by a Trusted Web Activity
 * (TWA).
 */
@NullMarked
public class TwaValidator {
    /** Delegate to check domain verification, allowing tests to stub the result. */
    public interface DomainVerificationDelegate {
        boolean isDomainVerified(Context context, String packageName, Origin origin);
    }

    private static volatile @Nullable DomainVerificationDelegate
            sDomainVerificationDelegateForTesting;
    private static volatile @Nullable Boolean sTwaInstalledOverrideForTesting;

    private TwaValidator() {}

    /** Sets a test delegate for domain verification. */
    static void setDomainVerificationDelegateForTesting(DomainVerificationDelegate delegate) {
        sDomainVerificationDelegateForTesting = delegate;
        ResettersForTesting.register(() -> sDomainVerificationDelegateForTesting = null);
    }

    /** Sets a test override for whether a TWA is installed for any URL. */
    public static void setTwaInstalledOverrideForTesting(@Nullable Boolean installed) {
        sTwaInstalledOverrideForTesting = installed;
        ResettersForTesting.register(() -> sTwaInstalledOverrideForTesting = null);
    }

    /**
     * Checks whether the given package is an installed Trusted Web Activity for the specified
     * origin.
     *
     * @param context The Android context.
     * @param packageName The package name to check.
     * @param origin The origin to check against.
     * @return True if the package is a verified or allowlisted TWA for the origin.
     */
    @AnyThread
    @VisibleForTesting
    static boolean isTwaForOrigin(Context context, String packageName, Origin origin) {
        return isTwaForOrigin(
                context,
                packageName,
                origin,
                () -> InstalledWebappPermissionManager.getAllDelegateApps(origin));
    }

    private static boolean isTwaForOrigin(
            Context context,
            String packageName,
            Origin origin,
            Supplier<@Nullable Set<Token>> verifiedDelegateAppsSupplier) {
        if (TextUtils.isEmpty(packageName)) {
            return false;
        }

        // First check if `android.support.customtabs.trusted.TRUSTED_WEB_ACTIVITY_SERVICE` is
        // present for the given package. If not, it is not a TWA and we can exit early.
        Intent serviceIntent =
                new Intent(TrustedWebActivityService.ACTION_TRUSTED_WEB_ACTIVITY_SERVICE);
        serviceIntent.setPackage(packageName);
        List<ResolveInfo> services = queryIntentServices(context, serviceIntent);
        if (services.isEmpty()) {
            return false;
        }

        // 1. For TWAs that have been already launched and are in the
        // InstalledWebappPermissionManager because they passed DAL verification, we can use those
        // results.
        Set<Token> verifiedApps = verifiedDelegateAppsSupplier.get();
        if (verifiedApps != null) {
            PackageManager pm = context.getPackageManager();
            for (Token app : verifiedApps) {
                if (app.matches(packageName, pm)) {
                    return true;
                }
            }
        }

        // 2. On Android 12+ (API 31+), check if Android OS has verified the package for this domain
        // via DomainVerificationManager (App Links). This allows detecting pre-installed TWAs that
        // have not yet been opened in Chrome.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            DomainVerificationDelegate delegate = sDomainVerificationDelegateForTesting;
            if (delegate != null) {
                if (delegate.isDomainVerified(context, packageName, origin)) {
                    return true;
                }
            } else if (isDomainVerifiedByAndroid(context, packageName, origin)) {
                return true;
            }
        }

        return false;
    }

    /**
     * Checks whether a TWA has previously run for the origin (and thus has verified permissions).
     *
     * <p>Note: We should probably use {@link #isTwaInstalledForOrigin(Origin)}, but this method is
     * kept for legacy callers (such as background sync and notification permissions contexts).
     *
     * @param origin The origin to check.
     * @return True if a TWA has been run for the origin.
     */
    @UiThread
    public static boolean hasTwaBeenRunForOrigin(Origin origin) {
        return WebappRegistry.getInstance().getPermissionStore().hasOrigin(origin.toString());
    }

    /**
     * Checks whether there is an installed TWA for the given origin.
     *
     * @param origin The origin to check.
     * @return True if there is an installed TWA for the origin.
     */
    @AnyThread
    public static boolean isTwaInstalledForOrigin(Origin origin) {
        return isTwaInstalledForUrl(new GURL(origin.toString()));
    }

    /**
     * Checks whether there is an installed TWA for the given URL.
     *
     * @param url The GURL to inspect. Must be a valid HTTP or HTTPS URL.
     * @return True if there is an installed TWA matching this URL.
     * @throws IllegalArgumentException If {@code url} is not a valid HTTP or HTTPS URL.
     */
    @AnyThread
    public static boolean isTwaInstalledForUrl(GURL url) {
        Origin origin = Origin.createOrThrow(url.getValidSpecOrEmpty());
        Boolean override = sTwaInstalledOverrideForTesting;
        if (override != null) {
            return override;
        }
        return queryFirstTwaPackage(
                        url,
                        origin,
                        memoize(() -> InstalledWebappPermissionManager.getAllDelegateApps(origin)))
                != null;
    }

    /**
     * Queries for the first installed TWA package that can handle the given URL.
     *
     * @param url The GURL to inspect. Must be a valid HTTP or HTTPS URL.
     * @return The package name of the matching TWA, or null if none found.
     * @throws IllegalArgumentException If {@code url} is not a valid HTTP or HTTPS URL.
     */
    @AnyThread
    public static @Nullable String queryFirstTwaPackage(GURL url) {
        Origin origin = Origin.createOrThrow(url.getValidSpecOrEmpty());
        return queryFirstTwaPackage(
                url,
                origin,
                memoize(() -> InstalledWebappPermissionManager.getAllDelegateApps(origin)));
    }

    private static Supplier<@Nullable Set<Token>> memoize(Supplier<@Nullable Set<Token>> delegate) {
        return new Supplier<@Nullable Set<Token>>() {
            private boolean mLoaded;
            private @Nullable Set<Token> mApps;

            @Override
            public @Nullable Set<Token> get() {
                if (!mLoaded) {
                    mLoaded = true;
                    mApps = delegate.get();
                }
                return mApps;
            }
        };
    }

    private static @Nullable String queryFirstTwaPackage(
            GURL url, Origin origin, Supplier<@Nullable Set<Token>> verifiedDelegateAppsSupplier) {
        try (TimingMetric unused =
                TimingMetric.shortUptime("TrustedWebActivity.QueryFirstTwaPackageTime")) {
            Context context = ContextUtils.getApplicationContext();
            String currentBrowserPackage = context.getPackageName();

            Intent targetIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(url.getSpec()));
            List<ResolveInfo> resolveInfos =
                    PackageManagerUtils.queryIntentActivities(targetIntent, 0);

            for (ResolveInfo info : resolveInfos) {
                if (info.activityInfo == null || info.activityInfo.packageName == null) {
                    continue;
                }
                String packageName = info.activityInfo.packageName;
                // Ignore browsers as a package.
                if (currentBrowserPackage.equals(packageName)) {
                    continue;
                }

                if (isTwaForOrigin(context, packageName, origin, verifiedDelegateAppsSupplier)) {
                    return packageName;
                }
            }

            return null;
        }
    }

    private static List<ResolveInfo> queryIntentServices(Context context, Intent intent) {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            PackageManager pm = context.getPackageManager();
            List<ResolveInfo> services = pm.queryIntentServices(intent, 0);
            return services != null ? services : Collections.emptyList();
        } catch (RuntimeException e) {
            // Unexpected exceptions can be triggered from the PackageManager if it dies
            // (crbug.com/794363), or due to unexpected binder related errors (like
            // https://crbug.com/700505 and https://crbug.com/369574).
            return Collections.emptyList();
        }
    }

    @RequiresApi(Build.VERSION_CODES.S)
    private static boolean isDomainVerifiedByAndroid(
            Context context, String packageName, Origin origin) {
        DomainVerificationManager dvm = context.getSystemService(DomainVerificationManager.class);

        try {
            DomainVerificationUserState userState = dvm.getDomainVerificationUserState(packageName);
            if (userState == null) {
                return false;
            }

            Map<String, Integer> hostStateMap = userState.getHostToStateMap();
            if (hostStateMap == null) {
                return false;
            }

            String host = origin.uri().getHost();
            if (TextUtils.isEmpty(host)) {
                return false;
            }

            Integer state = hostStateMap.get(host);
            return state != null && isVerifiedState(state);
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        } catch (Exception e) {
            // Guard against remote Binder exceptions.
            return false;
        }
    }

    private static boolean isVerifiedState(int state) {
        return state == DomainVerificationUserState.DOMAIN_STATE_VERIFIED
                || state == DomainVerificationUserState.DOMAIN_STATE_SELECTED;
    }
}
