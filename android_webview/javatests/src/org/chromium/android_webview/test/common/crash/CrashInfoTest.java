// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.common.crash;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;
import static org.chromium.android_webview.test.common.crash.CrashInfoEqualityMatcher.equalsTo;

import androidx.test.filters.SmallTest;

import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.crash.CrashInfo;
import org.chromium.android_webview.common.crash.CrashInfo.UploadState;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.base.test.util.Batch;

import java.util.HashMap;
import java.util.Map;

/**
 * Unit tests for CrashInfo.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(SINGLE_PROCESS) // These are unit tests
@Batch(Batch.UNIT_TESTS)
public class CrashInfoTest {
    /**
     * Create a {@link CrashInfo} object for testing.
     *
     * {@code appPackageName} is used as a representative of crash keys in tests.
     */
    public static CrashInfo createCrashInfo(String localId, long captureTime, String uploadId,
            long uploadTime, String appPackageName, UploadState state) {
        Map<String, String> crashKeys = new HashMap<>();
        if (appPackageName != null) {
            crashKeys.put("app-package-name", appPackageName);
        }
        CrashInfo crashInfo = new CrashInfo(localId, crashKeys);
        crashInfo.captureTime = captureTime;
        crashInfo.uploadId = uploadId;
        crashInfo.uploadTime = uploadTime;
        crashInfo.uploadState = state;

        return crashInfo;
    }

    /**
     * Create a hidden {@link CrashInfo} object for testing.
     *
     * {@code appPackageName} is used as a representative of crash keys in tests.
     */
    public static CrashInfo createHiddenCrashInfo(String localId, long captureTime, String uploadId,
            long uploadTime, String appPackageName, UploadState state) {
        CrashInfo crashInfo =
                createCrashInfo(localId, captureTime, uploadId, uploadTime, appPackageName, state);
        crashInfo.isHidden = true;

        return crashInfo;
    }

    /**
     * Test that merging two {@code CrashInfo} objects works correctly.
     */
    @Test
    @SmallTest
    public void testMergeCrashInfo() {
        CrashInfo a = createCrashInfo("123456", 10987654321L, null, -1, "org.test.package", null);
        CrashInfo b =
                createCrashInfo("123456", -1, "abcdefg", 12345678910L, null, UploadState.UPLOADED);

        CrashInfo merged = new CrashInfo(a, b);
        CrashInfo expected = createCrashInfo("123456", 10987654321L, "abcdefg", 12345678910L,
                "org.test.package", UploadState.UPLOADED);
        Assert.assertThat(merged, equalsTo(expected));
    }

    /**
     * Test that merging two {@code CrashInfo} objects works correctly.
     */
    @Test
    @SmallTest
    public void testMergeCrashInfo_differentUploadStates() {
        CrashInfo a = createCrashInfo("123456", -1, null, -1, null, UploadState.PENDING);
        CrashInfo b = createCrashInfo("123456", -1, "abcdefg", -1, null, UploadState.UPLOADED);

        CrashInfo merged = new CrashInfo(a, b);
        CrashInfo expected =
                createCrashInfo("123456", -1, "abcdefg", -1, null, UploadState.UPLOADED);
        // UPLOADED state is the merge result, regardless of order.
        Assert.assertThat(merged, equalsTo(expected));
    }

    /**
     * Test that merging two {@code CrashInfo} objects works correctly.
     */
    @Test
    @SmallTest
    public void testMergeCrashInfo_differentCaptureTime() {
        CrashInfo a = createCrashInfo("123456", 1234567, null, -1, null, null);
        CrashInfo b = createCrashInfo("123456", 1234555, null, -1, null, null);

        CrashInfo merged = new CrashInfo(a, b);
        CrashInfo expected = createCrashInfo("123456", 1234555, null, -1, null, null);
        // Older capture time is the merging result regardless of the order.
        Assert.assertThat(merged, equalsTo(expected));
    }

    /**
     * Test compitability with old JSON format.
     */
    @Test
    @SmallTest
    public void testSerializeToJson() throws Throwable {
        final String jsonObjectString =
                "{'crash-local-id':'123456abc','crash-capture-time':1234567890,"
                + "'crash-is-hidden':false,"
                + "'crash-keys':{'app-package-name':'org.test.package'}}";
        JSONObject expectedJsonObject = new JSONObject(jsonObjectString);

        CrashInfo c = createCrashInfo("123456abc", 1234567890, null, -1, "org.test.package", null);
        Assert.assertEquals(c.serializeToJson(), expectedJsonObject.toString());
    }

    /**
     * Test compitability with old JSON format.
     */
    @Test
    @SmallTest
    public void testReadFromJsonString() throws Throwable {
        final String jsonObjectString =
                "{'crash-local-id':'123456abc','crash-capture-time':1234567890,"
                + "'crash-keys':{'app-package-name':'org.test.package'}}";

        CrashInfo parsed = CrashInfo.readFromJsonString(jsonObjectString);
        CrashInfo expected =
                createCrashInfo("123456abc", 1234567890, null, -1, "org.test.package", null);
        Assert.assertThat(parsed, equalsTo(expected));
    }
}