// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui.util;

import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.containsInAnyOrder;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import android.support.test.filters.SmallTest;

import org.hamcrest.BaseMatcher;
import org.hamcrest.Description;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.crash.CrashInfo;
import org.chromium.android_webview.common.crash.CrashInfo.UploadState;
import org.chromium.android_webview.devui.util.WebViewCrashInfoCollector;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;

import java.util.Arrays;
import java.util.List;
import java.util.Objects;

/**
 * Unit tests for WebViewCrashInfoCollector.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(SINGLE_PROCESS)
public class WebViewCrashInfoCollectorTest {
    private CrashInfo createCrashInfo(String localId, long captureTime, String uploadId,
            long uploadTime, String packageName, String variations, UploadState state) {
        CrashInfo crashInfo = new CrashInfo(localId);
        crashInfo.captureTime = captureTime;
        crashInfo.uploadId = uploadId;
        crashInfo.uploadTime = uploadTime;
        crashInfo.packageName = packageName;
        if (variations != null) crashInfo.variations = Arrays.asList(variations.split(","));
        crashInfo.uploadState = state;

        return crashInfo;
    }

    private class CrashInfoEqualityMatcher extends BaseMatcher<CrashInfo> {
        private final CrashInfo mCrashInfo;

        CrashInfoEqualityMatcher(CrashInfo crashInfo) {
            mCrashInfo = crashInfo;
        }

        @Override
        public boolean matches(Object o) {
            if (o == mCrashInfo) return true;
            if (o == null || o.getClass() != mCrashInfo.getClass()) return false;

            CrashInfo c = (CrashInfo) o;
            return mCrashInfo.uploadState == c.uploadState
                    && Objects.equals(mCrashInfo.localId, c.localId)
                    && mCrashInfo.captureTime == c.captureTime
                    && Objects.equals(mCrashInfo.packageName, c.packageName)
                    && Objects.equals(mCrashInfo.variations, c.variations)
                    && Objects.equals(mCrashInfo.uploadId, c.uploadId)
                    && mCrashInfo.uploadTime == c.uploadTime;
        }

        @Override
        public void describeTo(Description description) {
            description.appendText("equals CrashInfo");
        }
    }

    /**
     * Test that merging two {@code CrashInfo} objects works correctly.
     */
    @Test
    @SmallTest
    public void testMergeCrashInfo() {
        CrashInfo a = createCrashInfo("123456", 10987654321L, null, -1, null, "123456,78910", null);
        CrashInfo b = createCrashInfo(
                "123456", -1, "abcdefg", 12345678910L, null, null, UploadState.UPLOADED);
        WebViewCrashInfoCollector.mergeCrashInfo(a, b);
        Assert.assertEquals("123456", a.localId);
        Assert.assertEquals(10987654321L, a.captureTime);
        Assert.assertEquals("abcdefg", a.uploadId);
        Assert.assertEquals(12345678910L, a.uploadTime);
        Assert.assertNull(a.packageName);
        Assert.assertEquals(Arrays.asList("123456", "78910"), a.variations);
        Assert.assertEquals(UploadState.UPLOADED, a.uploadState);
    }

    /**
     * Test that merging two {@code CrashInfo} objects works correctly.
     */
    @Test
    @SmallTest
    public void testMergeCrashInfo_differentUploadStates() {
        CrashInfo a = createCrashInfo("123456", -1, null, -1, null, null, UploadState.PENDING);
        CrashInfo b = createCrashInfo("123456", -1, null, -1, null, null, UploadState.UPLOADED);
        WebViewCrashInfoCollector.mergeCrashInfo(a, b);
        Assert.assertEquals("123456", a.localId);
        // UPLOADED state is the merge result, regardless of order.
        Assert.assertEquals(UploadState.UPLOADED, a.uploadState);
    }

    /**
     * Test that merging two {@code CrashInfo} objects works correctly.
     */
    @Test
    @SmallTest
    public void testMergeCrashInfo_differentCaptureTime() {
        CrashInfo a = createCrashInfo("123456", 1234567, null, -1, null, null, null);
        CrashInfo b = createCrashInfo("123456", 1234555, null, -1, null, null, null);
        WebViewCrashInfoCollector.mergeCrashInfo(a, b);
        Assert.assertEquals("123456", a.localId);
        // Older capture time is the merging result regardless of the order.
        Assert.assertEquals(1234555, a.captureTime);
    }

    /**
     * Test that merging {@code CrashInfo} that has the same {@code localID} works correctly.
     */
    @Test
    @SmallTest
    public void testMergeDuplicates() {
        List<CrashInfo> testList = Arrays.asList(
                createCrashInfo("xyz123", 112233445566L, null, -1, null, null, UploadState.PENDING),
                createCrashInfo(
                        "def789", -1, "55667788", 123344556677L, null, null, UploadState.UPLOADED),
                createCrashInfo("abc456", -1, null, -1, null, null, UploadState.PENDING),
                createCrashInfo(
                        "xyz123", 112233445566L, null, -1, "com.test.package", "222222", null),
                createCrashInfo("abc456", 445566778899L, null, -1, "org.test.package", null, null),
                createCrashInfo("abc456", -1, null, -1, null, null, null),
                createCrashInfo(
                        "xyz123", -1, "11223344", 223344556677L, null, null, UploadState.UPLOADED));
        List<CrashInfo> uniqueList = WebViewCrashInfoCollector.mergeDuplicates(testList);
        Assert.assertThat(uniqueList,
                containsInAnyOrder(
                        new CrashInfoEqualityMatcher(createCrashInfo("abc456", 445566778899L, null,
                                -1, "org.test.package", null, UploadState.PENDING)),
                        new CrashInfoEqualityMatcher(
                                createCrashInfo("xyz123", 112233445566L, "11223344", 223344556677L,
                                        "com.test.package", "222222", UploadState.UPLOADED)),
                        new CrashInfoEqualityMatcher(createCrashInfo("def789", -1, "55667788",
                                123344556677L, null, null, UploadState.UPLOADED))));
    }

    /**
     * Test that sort method works correctly; it sorts by recent crashes first (capture time then
     * upload time).
     */
    @Test
    @SmallTest
    public void testSortByRecentCaptureTime() {
        List<CrashInfo> testList = Arrays.asList(
                createCrashInfo("xyz123", -1, "11223344", 123L, null, null, UploadState.UPLOADED),
                createCrashInfo("def789", 111L, "55667788", 100L, null, null, UploadState.UPLOADED),
                createCrashInfo("abc456", -1, null, -1, null, null, UploadState.PENDING),
                createCrashInfo("ghijkl", 112L, null, -1, "com.test.package", "222222", null),
                createCrashInfo("abc456", 112L, null, 112L, "org.test.package", null, null),
                createCrashInfo(null, 100, "11223344", -1, "com.test.package", null, null),
                createCrashInfo("abc123", 100, null, -1, null, null, null));
        WebViewCrashInfoCollector.sortByMostRecent(testList);
        Assert.assertThat(testList,
                contains(new CrashInfoEqualityMatcher(createCrashInfo(
                                 "abc456", 112L, null, 112L, "org.test.package", null, null)),
                        new CrashInfoEqualityMatcher(createCrashInfo(
                                "ghijkl", 112L, null, -1, "com.test.package", "222222", null)),
                        new CrashInfoEqualityMatcher(createCrashInfo("def789", 111L, "55667788",
                                100L, null, null, UploadState.UPLOADED)),
                        new CrashInfoEqualityMatcher(createCrashInfo(
                                null, 100, "11223344", -1, "com.test.package", null, null)),
                        new CrashInfoEqualityMatcher(
                                createCrashInfo("abc123", 100, null, -1, null, null, null)),
                        new CrashInfoEqualityMatcher(createCrashInfo(
                                "xyz123", -1, "11223344", 123L, null, null, UploadState.UPLOADED)),
                        new CrashInfoEqualityMatcher(createCrashInfo(
                                "abc456", -1, null, -1, null, null, UploadState.PENDING))));
    }
}
