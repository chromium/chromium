// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric.nonembedded.crash;

import static org.chromium.android_webview.nonembedded.crash.CrashInfo.createCrashInfoForTesting;
import static org.chromium.android_webview.nonembedded.crash.CrashInfoEqualityMatcher.equalsTo;

import androidx.test.filters.SmallTest;

import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.nonembedded.crash.CrashInfo;
import org.chromium.android_webview.nonembedded.crash.CrashInfo.UploadState;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for CrashInfo. */
@RunWith(BaseRobolectricTestRunner.class)
public class CrashInfoTest {
    /** Test that merging two {@code CrashInfo} objects works correctly. */
    @Test
    @SmallTest
    public void testMergeCrashInfo() {
        CrashInfo a =
                createCrashInfoForTesting(
                        "123456", 10987654321L, null, -1, "org.test.package", null);
        CrashInfo b =
                createCrashInfoForTesting(
                        "123456", -1, "abcdefg", 12345678910L, null, UploadState.UPLOADED);

        CrashInfo merged = new CrashInfo(a, b);
        CrashInfo expected =
                createCrashInfoForTesting(
                        "123456",
                        10987654321L,
                        "abcdefg",
                        12345678910L,
                        "org.test.package",
                        UploadState.UPLOADED);
        Assert.assertThat(merged, equalsTo(expected));
    }

    /** Test that merging two {@code CrashInfo} objects works correctly. */
    @Test
    @SmallTest
    public void testMergeCrashInfo_differentUploadStates() {
        CrashInfo a = createCrashInfoForTesting("123456", -1, null, -1, null, UploadState.PENDING);
        CrashInfo b =
                createCrashInfoForTesting("123456", -1, "abcdefg", -1, null, UploadState.UPLOADED);

        CrashInfo merged = new CrashInfo(a, b);
        CrashInfo expected =
                createCrashInfoForTesting("123456", -1, "abcdefg", -1, null, UploadState.UPLOADED);
        // UPLOADED state is the merge result, regardless of order.
        Assert.assertThat(merged, equalsTo(expected));
    }

    /** Test that merging two {@code CrashInfo} objects works correctly. */
    @Test
    @SmallTest
    public void testMergeCrashInfo_differentCaptureTime() {
        CrashInfo a = createCrashInfoForTesting("123456", 1234567, null, -1, null, null);
        CrashInfo b = createCrashInfoForTesting("123456", 1234555, null, -1, null, null);

        CrashInfo merged = new CrashInfo(a, b);
        CrashInfo expected = createCrashInfoForTesting("123456", 1234555, null, -1, null, null);
        // Older capture time is the merging result regardless of the order.
        Assert.assertThat(merged, equalsTo(expected));
    }

    /** Test compatibility with old JSON format. */
    @Test
    @SmallTest
    public void testSerializeToJson() throws Throwable {
        final String jsonObjectString =
                "{'crash-local-id':'123456abc','crash-capture-time':1234567890,"
                        + "'crash-is-hidden':false,"
                        + "'crash-keys':{'app-package-name':'org.test.package'}}";
        JSONObject expectedJsonObject = new JSONObject(jsonObjectString);

        CrashInfo c =
                createCrashInfoForTesting(
                        "123456abc", 1234567890, null, -1, "org.test.package", null);
        Assert.assertEquals(c.serializeToJson(), expectedJsonObject.toString());
    }

    /** Test compatibility with old JSON format. */
    @Test
    @SmallTest
    public void testReadFromJsonString() throws Throwable {
        final String jsonObjectString =
                "{'crash-local-id':'123456abc','crash-capture-time':1234567890,"
                        + "'crash-keys':{'app-package-name':'org.test.package'}}";

        CrashInfo parsed = CrashInfo.readFromJsonString(jsonObjectString);
        CrashInfo expected =
                createCrashInfoForTesting(
                        "123456abc", 1234567890, null, -1, "org.test.package", null);
        Assert.assertThat(parsed, equalsTo(expected));
    }
}
