// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import android.os.Bundle;

import androidx.test.filters.SmallTest;

import com.android.webview.chromium.PrefetchOperationResult;
import com.android.webview.chromium.PrefetchOperationStatusCode;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

import org.chromium.android_webview.AwPrefetchCallback;
import org.chromium.android_webview.AwPrefetchCallback.StatusCode;

/** Unit tests for {@link PrefetchOperationResult}. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PrefetchOperationResultTest {

    @Test
    @SmallTest
    public void testFromPrefetchStatusCode_Success() {
        PrefetchOperationResult result =
                PrefetchOperationResult.fromPrefetchStatusCode(
                        StatusCode.PREFETCH_RESPONSE_COMPLETED, null);
        Assert.assertEquals(PrefetchOperationStatusCode.SUCCESS, result.statusCode);
        Assert.assertEquals(0, result.httpResponseStatusCode);
    }

    @Test
    @SmallTest
    public void testFromPrefetchStatusCode_StartFailed() {
        PrefetchOperationResult result =
                PrefetchOperationResult.fromPrefetchStatusCode(
                        StatusCode.PREFETCH_START_FAILED, null);
        Assert.assertEquals(PrefetchOperationStatusCode.FAILURE, result.statusCode);
        Assert.assertEquals(0, result.httpResponseStatusCode);
    }

    @Test
    @SmallTest
    public void testFromPrefetchStatusCode_GenericError() {
        PrefetchOperationResult result =
                PrefetchOperationResult.fromPrefetchStatusCode(
                        StatusCode.PREFETCH_RESPONSE_GENERIC_ERROR, null);
        Assert.assertEquals(PrefetchOperationStatusCode.FAILURE, result.statusCode);
        Assert.assertEquals(0, result.httpResponseStatusCode);
    }

    @Test
    @SmallTest
    public void testFromPrefetchStatusCode_ServerErrorNoExtras() {
        PrefetchOperationResult result =
                PrefetchOperationResult.fromPrefetchStatusCode(
                        StatusCode.PREFETCH_RESPONSE_SERVER_ERROR, null);
        Assert.assertEquals(PrefetchOperationStatusCode.SERVER_FAILURE, result.statusCode);
        Assert.assertEquals(0, result.httpResponseStatusCode);
    }

    @Test
    @SmallTest
    public void testFromPrefetchStatusCode_ServerErrorWithExtras() {
        Bundle extras = new Bundle();
        extras.putInt(AwPrefetchCallback.EXTRA_HTTP_RESPONSE_CODE, 404);
        PrefetchOperationResult result =
                PrefetchOperationResult.fromPrefetchStatusCode(
                        StatusCode.PREFETCH_RESPONSE_SERVER_ERROR, extras);
        Assert.assertEquals(PrefetchOperationStatusCode.SERVER_FAILURE, result.statusCode);
        Assert.assertEquals(404, result.httpResponseStatusCode);
    }

    @Test
    @SmallTest
    public void testFromPrefetchStatusCode_DuplicateRequest() {
        PrefetchOperationResult result =
                PrefetchOperationResult.fromPrefetchStatusCode(
                        StatusCode.DUPLICATE_REQUEST, null);
        Assert.assertEquals(PrefetchOperationStatusCode.DUPLICATE_REQUEST, result.statusCode);
        Assert.assertEquals(0, result.httpResponseStatusCode);
    }

    @Test(expected = IllegalArgumentException.class)
    @SmallTest
    public void testFromPrefetchStatusCode_InvalidStatusCode() {
        PrefetchOperationResult.fromPrefetchStatusCode(-1, null);
    }
}
