// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Tests for {@link IntentHeadersRecorder}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowRecordHistogram.class})
public class IntentHeadersRecorderTest {
    private static final String SAFE_HEADER = "Safe-Header";
    private static final String UNSAFE_HEADER = "Unsafe-Header";

    @Mock public IntentHeadersRecorder.HeaderClassifier mClassifier;
    private IntentHeadersRecorder mRecorder;

    @Before
    public void setUp() {
        ShadowRecordHistogram.reset();
        MockitoAnnotations.initMocks(this);

        doReturn(true).when(mClassifier).isCorsSafelistedHeader(eq(SAFE_HEADER), anyString());
        doReturn(false).when(mClassifier).isCorsSafelistedHeader(eq(UNSAFE_HEADER), anyString());

        mRecorder = new IntentHeadersRecorder(mClassifier);
    }

    @Test
    public void noHeaders_firstParty() {
        mRecorder.report(true);
        assertUma(1, 0, 0, 0, 0, 0);
    }

    @Test
    public void noHeaders_thirdParty() {
        mRecorder.report(false);
        assertUma(0, 0, 0, 1, 0, 0);
    }

    @Test
    public void safeHeaders_firstParty() {
        mRecorder.recordHeader(SAFE_HEADER, "");
        mRecorder.report(true);
        assertUma(0, 1, 0, 0, 0, 0);
    }

    @Test
    public void safeHeaders_thirdParty() {
        mRecorder.recordHeader(SAFE_HEADER, "");
        mRecorder.report(false);
        assertUma(0, 0, 0, 0, 1, 0);
    }

    @Test
    public void unsafeHeaders_firstParty() {
        mRecorder.recordHeader(UNSAFE_HEADER, "");
        mRecorder.report(true);
        assertUma(0, 0, 1, 0, 0, 0);
    }

    @Test
    public void unsafeHeaders_thirdParty() {
        mRecorder.recordHeader(UNSAFE_HEADER, "");
        mRecorder.report(false);
        assertUma(0, 0, 0, 0, 0, 1);
    }

    @Test
    public void mixedHeaders_firstParty() {
        mRecorder.recordHeader(SAFE_HEADER, "");
        mRecorder.recordHeader(UNSAFE_HEADER, "");
        mRecorder.report(true);
        assertUma(0, 0, 1, 0, 0, 0);
    }

    @Test
    public void mixedHeaders_thirdParty() {
        mRecorder.recordHeader(SAFE_HEADER, "");
        mRecorder.recordHeader(UNSAFE_HEADER, "");
        mRecorder.report(false);
        assertUma(0, 0, 0, 0, 0, 1);
    }

    private void assertUma(int fpNoHeaders, int fpSafeHeaders, int fpUnsafeHeaders,
                           int tpNoHeaders, int tpSafeHeaders, int tpUnsafeHeaders) {
        Assert.assertEquals("first party no headers", fpNoHeaders,
                RecordHistogram.getHistogramValueCountForTesting("Android.IntentHeaders",
                        IntentHeadersRecorder.IntentHeadersResult.FIRST_PARTY_NO_HEADERS));

        Assert.assertEquals("first party safe headers", fpSafeHeaders,
                RecordHistogram.getHistogramValueCountForTesting("Android.IntentHeaders",
                        IntentHeadersRecorder.IntentHeadersResult.FIRST_PARTY_ONLY_SAFE_HEADERS));

        Assert.assertEquals("first party unsafe headers", fpUnsafeHeaders,
                RecordHistogram.getHistogramValueCountForTesting("Android.IntentHeaders",
                        IntentHeadersRecorder.IntentHeadersResult.FIRST_PARTY_UNSAFE_HEADERS));

        Assert.assertEquals("third party no headers", tpNoHeaders,
                RecordHistogram.getHistogramValueCountForTesting("Android.IntentHeaders",
                        IntentHeadersRecorder.IntentHeadersResult.THIRD_PARTY_NO_HEADERS));

        Assert.assertEquals("third party safe headers", tpSafeHeaders,
                RecordHistogram.getHistogramValueCountForTesting("Android.IntentHeaders",
                        IntentHeadersRecorder.IntentHeadersResult.THIRD_PARTY_ONLY_SAFE_HEADERS));

        Assert.assertEquals("third party unsafe headers", tpUnsafeHeaders,
                RecordHistogram.getHistogramValueCountForTesting("Android.IntentHeaders",
                        IntentHeadersRecorder.IntentHeadersResult.THIRD_PARTY_UNSAFE_HEADERS));
    }
}
