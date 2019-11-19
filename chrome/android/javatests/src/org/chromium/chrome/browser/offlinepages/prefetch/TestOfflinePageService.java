// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.prefetch;

import android.content.Context;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;

import com.google.protobuf.InvalidProtocolBufferException;

import org.junit.Assert;

import org.chromium.base.Log;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.components.gcm_driver.GCMDriver;
import org.chromium.components.gcm_driver.GCMMessage;
import org.chromium.components.gcm_driver.instance_id.FakeInstanceIDWithSubtype;
import org.chromium.components.offline_pages.core.prefetch.proto.AnyOuterClass.Any;
import org.chromium.components.offline_pages.core.prefetch.proto.OfflinePages.Archive;
import org.chromium.components.offline_pages.core.prefetch.proto.OfflinePages.GeneratePageBundleRequest;
import org.chromium.components.offline_pages.core.prefetch.proto.OfflinePages.PageBundle;
import org.chromium.components.offline_pages.core.prefetch.proto.OfflinePages.PageInfo;
import org.chromium.components.offline_pages.core.prefetch.proto.OfflinePages.PageParameters;
import org.chromium.components.offline_pages.core.prefetch.proto.OperationOuterClass.Operation;
import org.chromium.components.offline_pages.core.prefetch.proto.StatusOuterClass;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.util.WebServer;
import org.chromium.net.test.util.WebServer.HTTPHeader;

import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.HashMap;

/**
 * A fake OfflinePageService.
 */
public class TestOfflinePageService {
    private static final String TAG = "TestOPS";
    private static final String BODY_PREFIX = "body-name-";
    private HashMap<String, GeneratePageBundleRequest> mOperations =
            new HashMap<String, GeneratePageBundleRequest>();
    private ArrayList<String> mIncompleteOperations = new ArrayList<String>();
    // Determines how this fake service responds to requests for pages.
    private HashMap<String, PageBehavior> mPageBehaviors = new HashMap<String, PageBehavior>();
    private StatusOuterClass.Code mDefaultGenerateStatus = StatusOuterClass.Code.OK;

    private boolean mForbidGeneratePageBundle;
    public void setForbidGeneratePageBundle(boolean shouldForbid) {
        mForbidGeneratePageBundle = shouldForbid;
    }

    private static int sNextOperationIndex = 1;
    private static String newOperationName() {
        String name = "operation-" + String.valueOf(sNextOperationIndex);
        sNextOperationIndex++;
        return name;
    }

    private static int sNextBodyIndex = 1;
    private static String newBodyName() {
        String name = "body-name-" + String.valueOf(sNextBodyIndex);
        sNextBodyIndex++;
        return name;
    }
    /** Defines the behavior of TestOfflinePageService for a single page. */
    public static class PageBehavior {
        public StatusOuterClass.Code generateStatus = StatusOuterClass.Code.OK;
        public StatusOuterClass.Code getStatus = StatusOuterClass.Code.OK;
        public String bodyName = newBodyName();
        public byte[] body = bodyContent(bodyName);
    }

    public void setPageBehavior(String url, PageBehavior behavior) {
        mPageBehaviors.put(url, behavior);
    }

    public void setDefaultGenerateStatus(StatusOuterClass.Code status) {
        mDefaultGenerateStatus = status;
    }

    public PageBehavior getPageBehavior(String url) {
        if (!mPageBehaviors.containsKey(url)) {
            PageBehavior behavior = new PageBehavior();
            behavior.generateStatus = mDefaultGenerateStatus;
            mPageBehaviors.put(url, behavior);
        }
        return mPageBehaviors.get(url);
    }

    public CallbackHelper GeneratePageBundleCalled = new CallbackHelper();
    public CallbackHelper GetOperationCalled = new CallbackHelper();
    public CallbackHelper ReadCalled = new CallbackHelper();

    /**
     * Handle a request (GeneratePageBundle, GetOperation, or Read). By default, responds with
     * successfully 'rendered' URLs. Use setPageBehavior to change this behavior.
     */
    public boolean handleRequest(WebServer.HTTPRequest request, OutputStream stream)
            throws IOException {
        // Figure out what kind of request this is, and dispatch the appropriate method.
        if (request.getMethod().equals("POST")
                && request.getURI().startsWith("/v1:GeneratePageBundle")) {
            try {
                if (mForbidGeneratePageBundle) {
                    respondForbidden(stream);
                } else {
                    GeneratePageBundleRequest bundleRequest =
                            GeneratePageBundleRequest.parseFrom(request.getBody());
                    handleGeneratePageBundle(bundleRequest, stream);
                }
                GeneratePageBundleCalled.notifyCalled();
                return true;
            } catch (InvalidProtocolBufferException e) {
                Log.e(TAG, "InvalidProtocolBufferException " + e.getMessage());
            }
        } else if (request.getMethod().equals("GET") && request.getURI().startsWith("/v1/media/")) {
            String suffix = request.getURI().substring(10);
            String[] nameAndQuery = suffix.split("[?]", 2);
            if (nameAndQuery.length == 2) {
                for (HTTPHeader header : request.getHeaders()) {
                    if (header.key.equalsIgnoreCase("range")) {
                        // The server is not equipped to correctly handle range requests for the
                        // initial request, as the response won't be cached.
                        Assert.fail("received range request for page data. Range=" + header.value);
                    }
                }
                if (handleRead(nameAndQuery[0], stream)) {
                    ReadCalled.notifyCalled();
                    return true;
                }
            }
        } else if (request.getMethod().equals("GET") && request.getURI().startsWith("/v1/")) {
            String operationNameAndQueryParams = request.getURI().substring(4);
            String[] parts = operationNameAndQueryParams.split("[?]", 2);
            if (parts.length == 2) {
                if (handleGetOperation(parts[0], stream)) {
                    GetOperationCalled.notifyCalled();
                    return true;
                }
            }
        }
        return false;
    }

    /** @return test content for the body with the given name. */
    private static byte[] bodyContent(String bodyName) {
        final int bodyLength = 1000;
        // The content is just the body name repeated until BODY_LENGTH is reached.
        StringBuilder body = new StringBuilder();
        while (body.length() < bodyLength) {
            body.append(bodyName);
        }
        body.setLength(bodyLength);
        return body.toString().getBytes();
    }

    /** Handle a Read request by returning fake MHTML content. */
    private boolean handleRead(String bodyName, OutputStream output) throws IOException {
        if (!bodyName.startsWith(BODY_PREFIX)) {
            return false;
        }
        WebServer.writeResponse(output, WebServer.STATUS_OK, bodyContent(bodyName));
        return true;
    }

    /** Handle a GetOperation request. */
    private boolean handleGetOperation(String operation, OutputStream output) throws IOException {
        GeneratePageBundleRequest request = mOperations.get(operation);
        if (request == null) {
            Log.e(TAG, "No operation " + operation);
            return false;
        }
        writeOperationResponse(operation, request, false, output);
        return true;
    }

    private boolean writeOperationResponse(String operationName, GeneratePageBundleRequest request,
            boolean generateRequest, OutputStream output) throws IOException {
        PageBundle.Builder bundle = PageBundle.newBuilder();

        boolean somePagesIncomplete = false;
        for (int i = 0; i < request.getPagesCount(); i++) {
            PageParameters params = request.getPages(i);
            PageBehavior behavior = getPageBehavior(params.getUrl());
            StatusOuterClass.Code status =
                    generateRequest ? behavior.generateStatus : behavior.getStatus;
            if (status == StatusOuterClass.Code.NOT_FOUND) {
                somePagesIncomplete = true;
            }
            Archive.Builder archive = Archive.newBuilder().addPageInfos(
                    PageInfo.newBuilder()
                            .setUrl(params.getUrl())
                            .setStatus(StatusOuterClass.Status.newBuilder().setCode(
                                    status.getNumber())));
            if (!behavior.bodyName.isEmpty() && status == StatusOuterClass.Code.OK) {
                archive.setBodyName(behavior.bodyName);
                archive.setBodyLength(behavior.body.length);
            }
            bundle.addArchives(archive.build());
        }
        Any anyBundle = Any.newBuilder()
                                .setTypeUrl("type.googleapis.com/"
                                        + "google.internal.chrome.offlinepages.v1.PageBundle")
                                .setValue(bundle.build().toByteString())
                                .build();
        boolean done = !generateRequest || !somePagesIncomplete;
        Operation.Builder operation = Operation.newBuilder().setName(operationName).setDone(done);
        if (done) {
            operation.setResponse(anyBundle);
        } else {
            operation.setMetadata(anyBundle);
        }
        WebServer.writeResponse(output, WebServer.STATUS_OK, operation.build().toByteArray());
        return done;
    }

    /** Handle a GeneratePageBundle request. */
    private void handleGeneratePageBundle(GeneratePageBundleRequest request, OutputStream output)
            throws IOException {
        String operationName = "operations/empty";
        if (request.getPagesCount() > 0) {
            operationName = newOperationName();
        }
        mOperations.put(operationName, request);
        if (!writeOperationResponse(operationName, request, true, output)) {
            synchronized (mIncompleteOperations) {
                mIncompleteOperations.add(operationName);
            }
        }
    }

    /** Send a "Forbidden by OPS" response. */
    private void respondForbidden(OutputStream output) throws IOException {
        WebServer.writeResponse(
                output, "403 Forbidden", "... request forbidden by OPS ...".getBytes());
    }

    /**
     * Sends a GCM message indicating the completion of a page bundle, and returns the operation
     * name that was completed. Returns null if no bundle needs to be sent. If more than one bundle
     * needs to be sent, the first completed bundle is sent. This can be called repeatedly until no
     * more bundles are ready.
     *
     * This method is typically not called on the server thread, so access to members should be
     * synchronized.
     */
    public String sendPushMessage() {
        CriteriaHelper.pollInstrumentationThread(() -> {
            Boolean result;
            synchronized (mIncompleteOperations) {
                result = !mIncompleteOperations.isEmpty();
            }
            return result;
        });

        String operationName;
        synchronized (mIncompleteOperations) {
            operationName = mIncompleteOperations.remove(0);
        }
        final String prefetchSubtype = "com.google.chrome.OfflinePagePrefetch";
        // We have to wait until Chrome gets the GCM token.
        CriteriaHelper.pollInstrumentationThread(() -> {
            try {
                FakeInstanceIDWithSubtype.getAuthorizedEntityForSubtype(prefetchSubtype);
                return true;
            } catch (IllegalStateException e) {
                return false;
            }
        }, "GetGCMToken not complete", 15000, 500);
        final String senderId =
                FakeInstanceIDWithSubtype.getAuthorizedEntityForSubtype(prefetchSubtype);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Context context = InstrumentationRegistry.getInstrumentation()
                                      .getTargetContext()
                                      .getApplicationContext();

            Bundle extras = new Bundle();
            extras.putString("pageBundle", operationName);
            extras.putString("subtype", prefetchSubtype); // is this necessary?

            GCMMessage message = new GCMMessage(senderId, extras);
            ChromeBrowserInitializer.getInstance(context).handleSynchronousStartup();
            GCMDriver.dispatchMessage(message);
        });
        return operationName;
    }
}
