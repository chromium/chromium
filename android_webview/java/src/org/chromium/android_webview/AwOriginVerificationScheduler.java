// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.components.content_relationship_verification.OriginVerificationScheduler;
import org.chromium.components.content_relationship_verification.OriginVerifier;
import org.chromium.components.content_relationship_verification.OriginVerifierHelper;
import org.chromium.components.embedder_support.util.Origin;

import java.util.Set;

/**
 * Singleton.
 * AwOriginVerificationScheduler provides a WebView specific implementation of {@link
 * OriginVerificationScheduler}.
 *
 * Call {@link AwOriginVerificationScheduler#init} to initialize the statement list and call
 * {@link AwOriginVerificationScheduler#validate} to perform a validation.
 */
public class AwOriginVerificationScheduler extends OriginVerificationScheduler {
    private static final String TAG = "AwOriginVerification";

    /** Lock on creation of sInstance. */
    private static final Object sLock = new Object();

    private static AwOriginVerificationScheduler sInstance;

    private AwOriginVerificationScheduler(
            AwOriginVerifier originVerifier, Set<Origin> pendingOrigins) {
        super(originVerifier, pendingOrigins);
    }

    /**
     * Initializes the AwOriginVerificationScheduler.
     * This should be called exactly only once as it parses the AndroidManifest and statement list.
     *
     * @param packageName the package name of the host application.
     * @param browserContext the browserContext to use for the simpleUrlLoader to download the asset
     *         links file.
     * @param context a context associated with an Activity/Service to load resources.
     */
    public static void init(String packageName, AwBrowserContext browserContext, Context context) {
        ThreadUtils.assertOnUiThread();
        synchronized (sLock) {
            assert sInstance
                    == null
                : "`init(String packageName, Context context)` must only be called once";

            sInstance = new AwOriginVerificationScheduler(
                    new AwOriginVerifier(packageName, OriginVerifier.HANDLE_ALL_URLS,
                            browserContext, AwVerificationResultStore.getInstance()),
                    OriginVerifierHelper.getClaimedOriginsFromManifest(packageName, context));
        }
    }

    public static void initAndScheduleAll(@Nullable Callback<Boolean> callback) {
        synchronized (sLock) {
            if (sInstance == null) {
                Context context = ContextUtils.getApplicationContext();
                init(context.getPackageName(), AwBrowserContext.getDefault(), context);
            }
            sInstance.scheduleAllPendingVerifications(callback);
        }
    }

    public static AwOriginVerificationScheduler getInstance() {
        synchronized (sLock) {
            return sInstance;
        }
    }
}
