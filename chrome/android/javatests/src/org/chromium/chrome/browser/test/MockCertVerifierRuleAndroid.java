// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.test;

import org.jni_zero.NativeMethods;
import org.junit.rules.ExternalResource;

import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

/** JUnit test rule which enables tests to force certificate verification results. */
public class MockCertVerifierRuleAndroid extends ExternalResource {

    private long mNativePtr;

    // Certificate verification result to force.
    private int mResult;

    public MockCertVerifierRuleAndroid(int result) {
        mResult = result;
    }

    @Override
    protected void before() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        mNativePtr = MockCertVerifierRuleAndroidJni.get().init();
        MockCertVerifierRuleAndroidJni.get().setResult(mNativePtr, mResult);
        MockCertVerifierRuleAndroidJni.get().setUp(mNativePtr);
    }

    public void setResult(int result) {
        mResult = result;
        if (mNativePtr != 0) {
            MockCertVerifierRuleAndroidJni.get().setResult(mNativePtr, result);
        }
    }

    @Override
    protected void after() {
        MockCertVerifierRuleAndroidJni.get().tearDown(mNativePtr);
        mNativePtr = 0;
    }

    @NativeMethods
    interface Natives {
        long init();

        void setUp(long nativeMockCertVerifierRuleAndroid);

        void setResult(long nativeMockCertVerifierRuleAndroid, int result);

        void tearDown(long nativeMockCertVerifierRuleAndroid);
    }
}
