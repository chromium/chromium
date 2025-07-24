// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_import;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.util.Base64;

import io.grpc.Context;
import io.grpc.Contexts;
import io.grpc.Metadata;
import io.grpc.Server;
import io.grpc.ServerInterceptor;
import io.grpc.ServerInterceptors;
import io.grpc.Status;
import io.grpc.StatusRuntimeException;
import io.grpc.binder.AndroidComponentAddress;
import io.grpc.binder.BinderServerBuilder;
import io.grpc.binder.IBinderReceiver;
import io.grpc.binder.InboundParcelablePolicy;
import io.grpc.binder.ParcelableUtils;
import io.grpc.binder.SecurityPolicies;
import io.grpc.binder.SecurityPolicy;
import io.grpc.binder.ServerSecurityPolicy;
import io.grpc.stub.StreamObserver;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.io.IOException;

// A service for importing user data coming from other browsers. It implements
// a gRPC API, called the "OS migration system app API".
@NullMarked
public class DataImporterServiceImpl extends DataImporterService.Impl {
    private static final String TAG = "DataImporterService";

    private @Nullable IBinderReceiver mBinderReceiver;
    private @Nullable Server mServer;
    private boolean mStarted;

    @Override
    public void onCreate() {
        if (!ChromeFeatureList.sAndroidDataImporterService.isEnabled()) {
            Log.w(TAG, "AndroidDataImporterService not enabled");
            return;
        }
        mBinderReceiver = new IBinderReceiver();
        mServer =
                BinderServerBuilder.forAddress(
                                AndroidComponentAddress.forContext(getService()), mBinderReceiver)
                        .addService(
                                ServerInterceptors.intercept(
                                        new TargetService(), new ParcelableMetadataInterceptor()))
                        .securityPolicy(getServerSecurityPolicy())
                        .inboundParcelablePolicy(getInboundParcelablePolicy())
                        .build();
    }

    @Override
    public @Nullable IBinder onBind(Intent intent) {
        if (!ChromeFeatureList.sAndroidDataImporterService.isEnabled()) {
            Log.w(TAG, "AndroidDataImporterService not enabled");
            return null;
        }
        // `mServer` and `mBinderReceiver` were created by `onCreate()` earlier.
        assert (mServer != null);
        assert (mBinderReceiver != null);

        if (!mStarted) {
            try {
                mStarted = true;
                mServer.start();
                // TODO(crbug.com/431218724): Figure out when to shut down the server again (and
                // then handle the server being in the shutdown process here).
            } catch (IOException e) {
                Log.e(TAG, "Failed to start grpc server for TargetService", e);
                return null;
            }
        }
        return mBinderReceiver.get();
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

    static class TargetService extends TargetServiceGrpc.TargetServiceImplBase {
        @Override
        public void handshake(
                TargetHandshakeRequest request,
                StreamObserver<TargetHandshakeResponse> responseObserver) {
            TargetHandshakeResponse.Builder response = TargetHandshakeResponse.newBuilder();
            switch (request.getItemType()) {
                case SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA:
                    response.setSupported(true);
                    response.setDataFormatVersion(1);
                    break;
                case SYSTEM_APP_API_ITEM_TYPE_UNSPECIFIED:
                case UNRECOGNIZED:
                    response.setSupported(false);
                    break;
            }
            // TODO(crbug.com/431218724): Store the session_id, for use (stats tracking) in
            // importItem() / importItemsDone().
            responseObserver.onNext(response.build());
            responseObserver.onCompleted();
        }

        @Override
        public void importItem(
                ImportItemRequest request, StreamObserver<ImportItemResponse> responseObserver) {
            switch (request.getItemType()) {
                case SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA:
                    // Supported type - continue below.
                    break;
                case SYSTEM_APP_API_ITEM_TYPE_UNSPECIFIED:
                case UNRECOGNIZED:
                    responseObserver.onError(
                            new StatusRuntimeException(
                                    Status.INVALID_ARGUMENT.withDescription(
                                            "Invalid or unsupported item type")));
                    return;
            }

            ParcelFileDescriptor pfd = PFD_CONTEXT_KEY.get(Context.current());
            if (pfd == null) {
                responseObserver.onError(
                        new StatusRuntimeException(
                                Status.INVALID_ARGUMENT.withDescription(
                                        "Missing ParcelFileDescriptor")));
                return;
            }

            BrowserFileType fileType;
            try {
                BrowserFileMetadata fileMetadata =
                        BrowserFileMetadata.parseFrom(request.getFileMetadata().getValue());
                fileType = fileMetadata.getFileType();
            } catch (com.google.protobuf.InvalidProtocolBufferException e) {
                responseObserver.onError(
                        new StatusRuntimeException(
                                Status.INVALID_ARGUMENT.withDescription(
                                        "Invalid or missing file_metadata")));
                return;
            }
            switch (fileType) {
                case BROWSER_FILE_TYPE_BOOKMARKS:
                case BROWSER_FILE_TYPE_READING_LIST:
                case BROWSER_FILE_TYPE_BROWSING_HISTORY:
                    // TODO(crbug.com/430254294): Hook up to the actual import logic (i.e. to
                    // StablePortabilityDataImporter from components/user_data_importer/) via JNI.
                    break;
                case UNRECOGNIZED:
                case BROWSER_FILE_TYPE_UNSPECIFIED:
                    responseObserver.onError(
                            new StatusRuntimeException(
                                    Status.INVALID_ARGUMENT.withDescription(
                                            "Invalid or unrecognized file type")));
                    return;
            }

            responseObserver.onNext(ImportItemResponse.newBuilder().build());
            responseObserver.onCompleted();
        }

        @Override
        public void importItemsDone(
                ImportItemsDoneRequest request,
                StreamObserver<ImportItemsDoneResponse> responseObserver) {
            switch (request.getItemType()) {
                case SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA:
                    // Supported type - continue below.
                    break;
                case SYSTEM_APP_API_ITEM_TYPE_UNSPECIFIED:
                case UNRECOGNIZED:
                    responseObserver.onError(
                            new StatusRuntimeException(
                                    Status.INVALID_ARGUMENT.withDescription(
                                            "Invalid or unsupported item type")));
                    return;
            }
            // TODO(crbug.com/431218724): Return the actual counts, based on the session_id.
            responseObserver.onNext(
                    ImportItemsDoneResponse.newBuilder().setIgnoredItemCount(1).build());
            responseObserver.onCompleted();
        }
    }
}
