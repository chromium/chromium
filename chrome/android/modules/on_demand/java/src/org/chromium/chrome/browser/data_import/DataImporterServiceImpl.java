// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_import;

import android.content.Intent;
import android.os.IBinder;

import io.grpc.Server;
import io.grpc.Status;
import io.grpc.StatusRuntimeException;
import io.grpc.binder.AndroidComponentAddress;
import io.grpc.binder.BinderServerBuilder;
import io.grpc.binder.IBinderReceiver;
import io.grpc.binder.InboundParcelablePolicy;
import io.grpc.binder.ServerSecurityPolicy;
import io.grpc.binder.UntrustedSecurityPolicies;
import io.grpc.stub.StreamObserver;

import org.chromium.base.Log;
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
                        .addService(new TargetService())
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

    private ServerSecurityPolicy getServerSecurityPolicy() {
        // TODO(crbug.com/431218724): Define a proper security policy.
        return ServerSecurityPolicy.newBuilder()
                .servicePolicy(
                        TargetServiceGrpc.SERVICE_NAME, UntrustedSecurityPolicies.untrustedPublic())
                .build();
    }

    private InboundParcelablePolicy getInboundParcelablePolicy() {
        return InboundParcelablePolicy.newBuilder().setAcceptParcelableMetadataValues(true).build();
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
            // TODO(crbug.com/431218724): Parse the data type (bookmarks, reading list, or history)
            // out of file_metadata.
            // TODO(crbug.com/431218724): Parse the file descriptor out of the request metadata.

            // TODO(crbug.com/430254294): Hook up to the actual import logic (i.e. to
            // StablePortabilityDataImporter from components/user_data_importer/) via JNI.
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
