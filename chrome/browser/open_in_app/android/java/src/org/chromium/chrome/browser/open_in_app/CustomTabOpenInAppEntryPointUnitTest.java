// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.open_in_app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.Drawable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.Page;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link CustomTabOpenInAppEntryPoint}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CustomTabOpenInAppEntryPointUnitTest {
    private static final String LABEL = "Label";
    private static final String PACKAGE = "com.example.package";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;

    @Mock(extraInterfaces = WebContentsObserver.Observable.class)
    private WebContents mWebContents;

    @Mock private Intent mIntent;
    @Mock private ResolveInfo mResolveInfo;
    @Mock private IntentFilter mIntentFilter;
    @Mock private Drawable mIcon;
    @Mock private ActivityInfo mActivityInfo;
    @Mock private PackageManager mPackageManager;
    @Spy private Context mContext;

    private SettableNullableObservableSupplier<Tab> mTabSupplier;
    private CustomTabOpenInAppEntryPoint mEntryPoint;
    private UserDataHost mUserDataHost;
    private final GURL mUrl = JUnitTestGURLs.EXAMPLE_URL;
    private NavigationHandle mNavigationHandle;

    @Before
    public void setUp() throws PackageManager.NameNotFoundException {
        mContext = spy(Robolectric.buildActivity(Activity.class).setup().get());
        mTabSupplier = ObservableSuppliers.createNullable();
        mUserDataHost = new UserDataHost();
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mPackageManager.getApplicationInfo(any(), anyInt())).thenReturn(new ApplicationInfo());
        when(mPackageManager.getApplicationLogo(any(ApplicationInfo.class))).thenReturn(mIcon);
        when(mPackageManager.getApplicationLabel(any(ApplicationInfo.class))).thenReturn(LABEL);
        when(mContext.getPackageManager()).thenReturn(mPackageManager);

        mResolveInfo.filter = mIntentFilter;
        mActivityInfo.packageName = PACKAGE;
        mResolveInfo.activityInfo = mActivityInfo;

        mNavigationHandle = NavigationHandle.createForTesting(mUrl, false, 0, true);
        mNavigationHandle.didFinish(
                mUrl,
                /* isErrorPage= */ false,
                /* hasCommitted= */ true,
                /* isPrimaryMainFrameFragmentNavigation= */ false,
                /* isDownload= */ false,
                /* isValidSearchFormUrl= */ false,
                /* transition= */ 0,
                /* errorCode= */ 0,
                /* errorDescription= */ "",
                /* httpStatuscode= */ 200,
                /* isExternalProtocol= */ false,
                /* isPdf= */ false,
                /* mimeType= */ "",
                Page.createForTesting());

        mEntryPoint = new CustomTabOpenInAppEntryPoint(mTabSupplier, mContext);
        mTabSupplier.set(mTab);
    }

    @Test
    public void getOpenInAppInfoForMenuItem() {
        OpenInAppDelegate delegate = OpenInAppDelegate.from(mTab);

        var captor = ArgumentCaptor.forClass(WebContentsObserver.class);
        verify(((WebContentsObserver.Observable) mWebContents)).addObserver(captor.capture());
        captor.getValue().didFinishNavigationInPrimaryMainFrame(mNavigationHandle);

        // Initial state after navigation: app info should be null.
        assertNull(mEntryPoint.getOpenInAppInfoForMenuItem());

        // Simulate receiving resolve infos.
        var infos = new OpenInAppEntryPoint.ResolveResult.Info(mResolveInfo);
        mEntryPoint.onResolveInfosFetched(infos, mIntent, mUrl);

        // Resolve infos received: app info should be updated.
        var appInfo = mEntryPoint.getOpenInAppInfoForMenuItem();
        assertNonNull(appInfo);
        assertEquals(LABEL, appInfo.appName);
        assertEquals(mIcon, appInfo.appIcon);

        // Check that delegate is also updated.
        assertEquals(appInfo, delegate.getCurrentOpenInAppInfo());

        // Empty resolve infos: app info should be null.
        mEntryPoint.onResolveInfosFetched(
                new OpenInAppEntryPoint.ResolveResult.None(), mIntent, mUrl);
        assertNull(mEntryPoint.getOpenInAppInfoForMenuItem());
        assertNull(delegate.getCurrentOpenInAppInfo());
    }

    @Test
    public void destroy() {
        var captor = ArgumentCaptor.forClass(WebContentsObserver.class);
        verify(((WebContentsObserver.Observable) mWebContents)).addObserver(captor.capture());
        captor.getValue().didFinishNavigationInPrimaryMainFrame(mNavigationHandle);

        // Simulate receiving resolve infos.
        var infos = new OpenInAppEntryPoint.ResolveResult.Info(mResolveInfo);
        mEntryPoint.onResolveInfosFetched(infos, mIntent, mUrl);

        // Verify it is set.
        assertNonNull(mEntryPoint.getOpenInAppInfoForMenuItem());

        mEntryPoint.destroy();

        // After destroy, it should be null.
        assertNull(mEntryPoint.getOpenInAppInfoForMenuItem());
    }
}
