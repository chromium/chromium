// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.text.TextUtils;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsOpenTimeRecorder.CloseCause;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;

import java.util.function.BooleanSupplier;

/** Tests for some parts of {@link CustomTabsOpenTimeRecorder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class CustomTabsOpenTimeRecorderTest {
    private static final String CHROME_PACKAGE_NAME = "chrome.package.name";

    @Mock private Context mAppContext;
    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private CustomTabActivityNavigationController mNavigationController;
    @Mock private BooleanSupplier mIsCctFinishing;
    @Mock private BrowserServicesIntentDataProvider mIntent;

    private CustomTabsOpenTimeRecorder mRecorder;

    @Rule public JniMocker jniMocker = new JniMocker();
    @Mock private CustomTabsOpenTimeRecorder.Natives mNativeMock;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        jniMocker.mock(CustomTabsOpenTimeRecorderJni.TEST_HOOKS, mNativeMock);
        ContextUtils.initApplicationContextForTests(mAppContext);
    }

    private void createRecorder() {
        mRecorder =
                new CustomTabsOpenTimeRecorder(
                        mLifecycleDispatcher, mNavigationController, mIsCctFinishing, mIntent);
    }

    @Test
    public void testGetPackageName_EmptyPcct_3rdParty() {
        createRecorder();
        assertTrue(
                "Should return empty package name",
                TextUtils.isEmpty(mRecorder.getPackageName(true)));
    }

    @Test
    public void testGetPackageName_EmptyPcct_Chrome() {
        when(mAppContext.getPackageName()).thenReturn(CHROME_PACKAGE_NAME);
        when(mIntent.isOpenedByChrome()).thenReturn(true);
        when(mIntent.isTrustedIntent()).thenReturn(true);
        createRecorder();
        assertEquals(
                "Should return Chrome's package name",
                CHROME_PACKAGE_NAME,
                mRecorder.getPackageName(true));
    }

    @Test
    public void testGetPackageName_EmptyPcct_1p() {
        when(mIntent.isTrustedIntent()).thenReturn(true);
        createRecorder();
        assertEquals(
                "Should return 1p package name",
                CustomTabsOpenTimeRecorder.PACKAGE_NAME_EMPTY_1P,
                mRecorder.getPackageName(true));
    }

    @Test
    public void testAuthTabClose() {
        when(mIntent.isAuthTab()).thenReturn(true);
        when(mIsCctFinishing.getAsBoolean()).thenReturn(true);
        createRecorder();
        HistogramWatcher histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.CloseCause", CloseCause.AUTH_TAB);
        mRecorder.onStartWithNative();
        mRecorder.onStopWithNative();
        histogram.assertExpected();
        verify(mNativeMock)
                .recordCustomTabSession(anyLong(), anyString(), anyLong(), eq(false), anyBoolean());
    }
}
