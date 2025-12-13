// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_import;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.util.Base64;

import androidx.annotation.VisibleForTesting;

import io.grpc.Context;
import io.grpc.Contexts;
import io.grpc.Metadata;
import io.grpc.Server;
import io.grpc.ServerInterceptor;
import io.grpc.ServerInterceptors;
import io.grpc.binder.AndroidComponentAddress;
import io.grpc.binder.BinderServerBuilder;
import io.grpc.binder.IBinderReceiver;
import io.grpc.binder.InboundParcelablePolicy;
import io.grpc.binder.ParcelableUtils;
import io.grpc.binder.SecurityPolicies;
import io.grpc.binder.SecurityPolicy;
import io.grpc.binder.ServerSecurityPolicy;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.base.SplitCompatService;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.io.IOException;
import java.util.concurrent.CountDownLatch;

// A service for importing user data coming from other browsers. It implements
// a gRPC API, called the "OS migration system app API".
@NullMarked
public class DataImporterServiceImpl extends SplitCompatService.Impl {
    private static final String TAG = "DataImporterService";

    private static boolean sSkipSecurityPolicyForTesting;
    private static @Nullable CountDownLatch sOnDestroyLatchForTesting;
    private static boolean sFailNextOnBindForTesting;

    /** If true, the gRPC server will not enforce a security policy. Only for use in tests. */
    @VisibleForTesting
    public static void setSkipSecurityPolicyForTesting(boolean skip) {
        ResettersForTesting.register(() -> sSkipSecurityPolicyForTesting = false);
        sSkipSecurityPolicyForTesting = skip;
    }

    @VisibleForTesting
    public static void setOnDestroyLatchForTesting(CountDownLatch latch) {
        ResettersForTesting.register(() -> sOnDestroyLatchForTesting = null);
        sOnDestroyLatchForTesting = latch;
    }

    @VisibleForTesting
    public static void setFailNextOnBindForTesting(boolean fail) {
        ResettersForTesting.register(() -> sFailNextOnBindForTesting = false);
        sFailNextOnBindForTesting = fail;
    }

    private @Nullable IBinderReceiver mBinderReceiver;
    private @Nullable Server mServer;

    @Override
    public @Nullable IBinder onBind(Intent intent) {
        if (sFailNextOnBindForTesting) {
            sFailNextOnBindForTesting = false;
            return null;
        }
        if (!ChromeFeatureList.sAndroidDataImporterService.isEnabled()) {
            Log.w(TAG, "AndroidDataImporterService not enabled");
            return null;
        }
        if (mServer == null) {
            try {
                mBinderReceiver = new IBinderReceiver();
                BinderServerBuilder builder =
                        BinderServerBuilder.forAddress(
                                        AndroidComponentAddress.forContext(getService()),
                                        mBinderReceiver)
                                .addService(
                                        ServerInterceptors.intercept(
                                                new TargetService(),
                                                new ParcelableMetadataInterceptor()))
                                .inboundParcelablePolicy(getInboundParcelablePolicy());
                if (!sSkipSecurityPolicyForTesting) {
                    builder.securityPolicy(getServerSecurityPolicy());
                }
                mServer = builder.build();
                mServer.start();
            } catch (IOException e) {
                Log.e(TAG, "Failed to start grpc server for TargetService", e);
                // The server failed to start. Null out members in case they were partially
                // initialized.
                mServer = null;
                mBinderReceiver = null;
                return null;
            }
        }
        if (mBinderReceiver == null) return null;
        return mBinderReceiver.get();
    }

    @Override
    public void onDestroy() {
        if (mServer != null && !mServer.isShutdown()) {
            mServer.shutdownNow();
            mServer = null;
            mBinderReceiver = null;
        }
        super.onDestroy();
        if (sOnDestroyLatchForTesting != null) {
            sOnDestroyLatchForTesting.countDown();
        }
    }

    private static final String RESTORE_PACKAGE_NAME = "com.google.android.apps.restore";
    private static final String RESTORE_SHA_HASH_PROD =
            "Vr4TK3gGVv4kRM00Mm6116rJHSCWq/D+ZzqZJwYi7Ic=";
    private static final String RESTORE_SHA_HASH_DEV =
            "znRkbMxkfBZAxFcQI+HPcbkOTR0HFnwYS+KRNkSeTH8=";

    private static final String DEV_APP_PACKAGE_NAME =
            "com.google.android.apps.restore.experimental.osmigration.devapp";
    private static final String DEV_APP_SHA_HASH = "EDk47kU35Z6O55L2VFBPuDRvxrNG0LvEQV/DOfz8jsE=";

    private ServerSecurityPolicy getServerSecurityPolicy() {
        return ServerSecurityPolicy.newBuilder()
                .servicePolicy(TargetServiceGrpc.SERVICE_NAME, getSecurityPolicy())
                .build();
    }

    private SecurityPolicy getSecurityPolicy() {
        PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
        if (VersionInfo.isLocalBuild() || VersionInfo.isCanaryBuild() || VersionInfo.isDevBuild()) {
            // In local (aka developer) builds, as well as in Canary and Dev channel, allow
            // development apps to call the API.
            return SecurityPolicies.anyOf(isRestoreAppProd(pm), isRestoreAppDev(pm), isDevApp(pm));
        } else {
            // In Stable and Beta channel, only allow the production Restore app.
            return isRestoreAppProd(pm);
        }
    }

    private static SecurityPolicy isRestoreAppProd(PackageManager pm) {
        return SecurityPolicies.hasSignatureSha256Hash(
                pm, RESTORE_PACKAGE_NAME, Base64.decode(RESTORE_SHA_HASH_PROD, Base64.DEFAULT));
    }

    private static SecurityPolicy isRestoreAppDev(PackageManager pm) {
        return SecurityPolicies.hasSignatureSha256Hash(
                pm, RESTORE_PACKAGE_NAME, Base64.decode(RESTORE_SHA_HASH_DEV, Base64.DEFAULT));
    }

    private static SecurityPolicy isDevApp(PackageManager pm) {
        return SecurityPolicies.hasSignatureSha256Hash(
                pm, DEV_APP_PACKAGE_NAME, Base64.decode(DEV_APP_SHA_HASH, Base64.DEFAULT));
    }

    private InboundParcelablePolicy getInboundParcelablePolicy() {
        return InboundParcelablePolicy.newBuilder().setAcceptParcelableMetadataValues(true).build();
    }

    // Key under which the ParcelFileDescriptor (aka input file) is stored in the request metadata
    // and, after the interceptor has copied it, in the grpc context.
    static final String PFD_KEY = "pfd-keys-bin";
    static final Context.Key<ParcelFileDescriptor> PFD_CONTEXT_KEY = Context.key(PFD_KEY);

    // Helper class which copies the ParcelFileDescriptor (input file) from request metadata into
    // the grpc context, where individual RPC implementations can access it.
    static class ParcelableMetadataInterceptor implements ServerInterceptor {
        static final Metadata.Key<ParcelFileDescriptor> PFD_METADATA_KEY =
                ParcelableUtils.metadataKey(PFD_KEY, ParcelFileDescriptor.CREATOR);

        @Override
        public <ReqT, RespT> io.grpc.ServerCall.Listener<ReqT> interceptCall(
                io.grpc.ServerCall<ReqT, RespT> call,
                io.grpc.Metadata headers,
                io.grpc.ServerCallHandler<ReqT, RespT> next) {
            Context context = Context.current();
            if (headers.containsKey(PFD_METADATA_KEY)) {
                context = context.withValue(PFD_CONTEXT_KEY, headers.get(PFD_METADATA_KEY));
            }
            return Contexts.interceptCall(context, call, headers, next);
        }
    }
}
