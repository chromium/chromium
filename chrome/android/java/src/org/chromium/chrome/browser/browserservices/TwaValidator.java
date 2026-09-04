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

import androidx.annotation.RequiresApi;
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
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.url.GURL;

import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Set;

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

    private static @Nullable DomainVerificationDelegate sDomainVerificationDelegateForTesting;

    private TwaValidator() {}

    /** Sets a test delegate for domain verification. */
    static void setDomainVerificationDelegateForTesting(DomainVerificationDelegate delegate) {
        sDomainVerificationDelegateForTesting = delegate;
        ResettersForTesting.register(() -> sDomainVerificationDelegateForTesting = null);
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
    @VisibleForTesting
    static boolean isTwaForOrigin(Context context, String packageName, Origin origin) {
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
        // InstalledWebappPermissionManager because they passed DAL verification, we can read those
        // results.
        Set<Token> verifiedApps = InstalledWebappPermissionManager.getAllDelegateApps(origin);
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
            if (sDomainVerificationDelegateForTesting != null) {
                if (sDomainVerificationDelegateForTesting.isDomainVerified(
                        context, packageName, origin)) {
                    return true;
                }
            } else if (isDomainVerifiedByAndroid(context, packageName, origin)) {
                return true;
            }
        }

        return false;
    }

    /**
     * Checks whether there is an installed TWA for the given origin.
     *
     * @param origin The origin to check.
     * @return True if there is an installed TWA for the origin.
     */
    @VisibleForTesting
    static boolean isTwaInstalledForOrigin(Origin origin) {
        return isTwaInstalledForUrl(new GURL(origin.toString()));
    }

    /**
     * Checks whether there is an installed TWA for the given URL.
     *
     * @param url The GURL to inspect.
     * @return True if there is an installed TWA matching this URL.
     */
    @VisibleForTesting
    static boolean isTwaInstalledForUrl(GURL url) {
        if (GURL.isEmptyOrInvalid(url)) {
            return false;
        }
        return queryFirstTwaPackage(url) != null;
    }

    /**
     * Queries for the first installed TWA package that can handle the given URL.
     *
     * @param url The GURL to inspect.
     * @return The package name of the matching TWA, or null if none found.
     */
    private static @Nullable String queryFirstTwaPackage(GURL url) {
        if (GURL.isEmptyOrInvalid(url)) {
            return null;
        }
        Origin origin = Origin.createOrThrow(url.getSpec());

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

                if (isTwaForOrigin(context, packageName, origin)) {
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
