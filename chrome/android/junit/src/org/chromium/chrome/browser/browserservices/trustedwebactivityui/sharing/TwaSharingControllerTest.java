// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.sharing;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.content.Intent;
import android.os.Looper;

import androidx.browser.trusted.sharing.ShareData;
import androidx.browser.trusted.sharing.ShareTarget;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebApkExtras;
import org.chromium.chrome.browser.browserservices.intents.WebApkShareTarget;
import org.chromium.chrome.browser.browserservices.intents.WebappIcon;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.webapps.WebApkPostShareTargetNavigator;
import org.chromium.chrome.browser.webapps.WebApkPostShareTargetNavigatorJni;
import org.chromium.components.webapps.WebApkDistributor;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;

/** Tests for {@link TwaSharingController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TwaSharingControllerTest {
    private static final String SHARE_ACTION = "https://pwa.rocks/share.html";
    private static final String TWA_LAUNCH_URL = "https://pwa.rocks/launch.html";
    private static final String MANIFEST_START_URL = "https://pwa.rocks/manifest_start.html";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private CustomTabActivityTabProvider mTabProvider;
    @Mock private CustomTabActivityNavigationController mNavigationController;
    @Mock private Verifier mVerifier;
    @Mock private BrowserServicesIntentDataProvider mIntentDataProvider;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private WebApkPostShareTargetNavigator.Natives mPostNavigatorNatives;

    private TwaSharingController mController;

    @Before
    public void setUp() {
        WebApkPostShareTargetNavigatorJni.setInstanceForTesting(mPostNavigatorNatives);
        when(mTabProvider.getTab()).thenReturn(mTab);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mVerifier.verify(SHARE_ACTION)).thenReturn(Promise.fulfilled(true));
        when(mIntentDataProvider.getIntent()).thenReturn(new Intent());
        when(mIntentDataProvider.getShareData())
                .thenReturn(new ShareData("test_title", "test_text", Collections.emptyList()));
        mController = new TwaSharingController(mTabProvider, mNavigationController, mVerifier);
    }

    @Test
    public void webApkPostShare_BootstrapsManifestStartUrl() {
        // For a WebAPK share, the URL to load is the share target action (see
        // HostBrowserLauncherParams), so the manifest start URL must be used instead.
        when(mIntentDataProvider.getUrlToLoad()).thenReturn(SHARE_ACTION);
        when(mIntentDataProvider.getWebApkExtras())
                .thenReturn(
                        createWebApkExtras(
                                MANIFEST_START_URL,
                                new WebApkShareTarget(
                                        SHARE_ACTION,
                                        "received_title",
                                        "received_text",
                                        /* isMethodPost= */ true,
                                        /* isEncTypeMultipart= */ false,
                                        /* fileNames= */ null,
                                        /* fileAccepts= */ null)));

        mController.deliverToShareTarget(mIntentDataProvider);
        shadowOf(Looper.getMainLooper()).idle();

        assertEquals(MANIFEST_START_URL, captureBootstrapUrl().getSpec());
    }

    @Test
    public void twaPostShare_BootstrapsUrlToLoad() {
        when(mIntentDataProvider.getUrlToLoad()).thenReturn(TWA_LAUNCH_URL);
        when(mIntentDataProvider.getShareTarget())
                .thenReturn(
                        new ShareTarget(
                                SHARE_ACTION,
                                "POST",
                                null,
                                new ShareTarget.Params("received_title", "received_text", null)));

        mController.deliverToShareTarget(mIntentDataProvider);
        shadowOf(Looper.getMainLooper()).idle();

        assertEquals(TWA_LAUNCH_URL, captureBootstrapUrl().getSpec());
    }

    private GURL captureBootstrapUrl() {
        ArgumentCaptor<GURL> bootstrapUrl = ArgumentCaptor.forClass(GURL.class);
        verify(mPostNavigatorNatives)
                .nativeLoadViewForShareTargetPost(
                        anyBoolean(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        eq(SHARE_ACTION),
                        bootstrapUrl.capture(),
                        eq(mWebContents));
        return bootstrapUrl.getValue();
    }

    private static WebApkExtras createWebApkExtras(
            String manifestStartUrl, WebApkShareTarget shareTarget) {
        return new WebApkExtras(
                "org.chromium.webapk.test",
                new WebappIcon(),
                /* isSplashIconMaskable= */ false,
                /* shellApkVersion= */ 0,
                /* manifestUrl= */ null,
                manifestStartUrl,
                /* manifestId= */ null,
                /* appKey= */ null,
                WebApkDistributor.OTHER,
                new HashMap<>(),
                shareTarget,
                /* isSplashProvidedByWebApk= */ false,
                new ArrayList<>(),
                /* webApkVersionCode= */ 0,
                /* lastUpdateTime= */ 0,
                /* hasCustomName= */ false);
    }
}
