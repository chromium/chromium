// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import static org.mockito.Mockito.doReturn;

import android.content.Intent;
import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.function.Function;

/** Instrumentation tests for {@link ExternalNavigationHandler}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ExternalNavigationDelegateImplTest {

    static class ExternalNavigationDelegateImplForTesting extends ExternalNavigationDelegateImpl {
        private Function<Intent, Boolean> mCanExternalAppHandleIntent;

        public ExternalNavigationDelegateImplForTesting(Tab activityTab) {
            super(activityTab);
        }

        public void setCanExternalAppHandleIntent(Function<Intent, Boolean> value) {
            mCanExternalAppHandleIntent = value;
        }
    }

    public void maybeSetAndGetRequestMetadata(
            ExternalNavigationDelegateImpl delegate,
            Intent intent,
            boolean hasUserGesture,
            boolean isRendererInitiated) {
        delegate.maybeSetRequestMetadata(intent, hasUserGesture, isRendererInitiated);
        IntentWithRequestMetadataHandler.RequestMetadata metadata =
                IntentWithRequestMetadataHandler.getInstance().getRequestMetadataAndClear(intent);
        Assert.assertEquals(hasUserGesture, metadata.hasUserGesture());
        Assert.assertEquals(isRendererInitiated, metadata.isRendererInitiated());
    }

    private ExternalNavigationDelegateImpl mExternalNavigationDelegateImpl;
    private ExternalNavigationDelegateImplForTesting mExternalNavigationDelegateImplForTesting;

    @Mock Tab mMockTab;
    @Mock WindowAndroid mMockWindowAndroid;

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        doReturn(mMockWindowAndroid).when(mMockTab).getWindowAndroid();
        mExternalNavigationDelegateImpl =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new ExternalNavigationDelegateImpl(mMockTab));
        mExternalNavigationDelegateImplForTesting =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new ExternalNavigationDelegateImplForTesting(mMockTab));
    }

    @Test
    @SmallTest
    public void testMaybeSetPendingIncognitoUrl() {
        String url = "http://www.example.com";
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));

        mExternalNavigationDelegateImpl.maybeSetPendingIncognitoUrl(intent);

        Assert.assertTrue(
                intent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false));
        Assert.assertEquals(url, IntentHandler.getPendingIncognitoUrl());
    }

    @Test
    @SmallTest
    public void maybeSetRequestMetadata() {
        String url = "http://www.example.com";
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));

        mExternalNavigationDelegateImpl.maybeSetRequestMetadata(intent, false, false);
        Assert.assertNull(
                IntentWithRequestMetadataHandler.getInstance().getRequestMetadataAndClear(intent));

        maybeSetAndGetRequestMetadata(mExternalNavigationDelegateImpl, intent, true, true);
        maybeSetAndGetRequestMetadata(mExternalNavigationDelegateImpl, intent, true, false);
        maybeSetAndGetRequestMetadata(mExternalNavigationDelegateImpl, intent, false, true);
    }

    @Test
    @SmallTest
    public void testMaybeSetPendingReferrer() {
        String url = "http://www.example.com/";
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));

        String referrerUrl = "http://www.example-referrer.com/";
        mExternalNavigationDelegateImpl.maybeSetPendingReferrer(intent, new GURL(referrerUrl));

        Assert.assertEquals(
                Uri.parse(referrerUrl), intent.getParcelableExtra(Intent.EXTRA_REFERRER));
        Assert.assertEquals(1, intent.getIntExtra(IntentHandler.EXTRA_REFERRER_ID, 0));
        Assert.assertEquals(referrerUrl, IntentHandler.getPendingReferrerUrl(1));
    }
}
