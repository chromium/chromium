// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.open_in_app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.OmniboxChipManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.external_intents.ExternalNavigationHelper;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.Page;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link TabbedOpenInAppEntryPoint}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabbedOpenInAppEntryPointUnitTest {
    private static final String LABEL = "Label";
    private static final String PACKAGE = "com.example.package";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private OmniboxChipManager mOmniboxChipManager;

    @Mock(extraInterfaces = WebContentsObserver.Observable.class)
    private WebContents mWebContents;

    @Mock private Intent mIntent;
    @Mock private ResolveInfo mResolveInfo;
    @Mock private IntentFilter mIntentFilter;
    @Mock private Drawable mIcon;
    @Mock private ActivityInfo mActivityInfo;
    @Mock private PackageManager mPackageManager;
    @Spy private Context mContext;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private ExternalNavigationHelper mExternalNavigationHelper;

    private SettableNullableObservableSupplier<Tab> mTabSupplier;
    private TabbedOpenInAppEntryPoint mEntryPoint;
    private UserDataHost mUserDataHost;
    private final GURL mUrl = JUnitTestGURLs.EXAMPLE_URL;
    private NavigationHandle mNavigationHandle;

    @Before
    public void setUp() throws PackageManager.NameNotFoundException {
        mContext = spy(Robolectric.buildActivity(Activity.class).setup().get());
        mTabSupplier = ObservableSuppliers.createNullable();
        mUserDataHost = new UserDataHost();
        TabModelSelectorSupplier.setInstanceForTesting(mTabModelSelector);
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

        mEntryPoint = new TabbedOpenInAppEntryPoint(mTabSupplier, mOmniboxChipManager, mContext);
        mTabSupplier.set(mTab);
    }

    @Test
    public void placeClickDismissChip() {
        OpenInAppDelegate delegate = OpenInAppDelegate.from(mTab);
        delegate.setExternalNavigationHelper(mExternalNavigationHelper);

        var captor = ArgumentCaptor.forClass(WebContentsObserver.class);
        verify(((WebContentsObserver.Observable) mWebContents)).addObserver(captor.capture());
        captor.getValue().didFinishNavigationInPrimaryMainFrame(mNavigationHandle);

        ShadowLooper.idleMainLooper();

        // New navigation committed; the app info should be null.
        assertNull(delegate.getCurrentOpenInAppInfo());

        // Simulate receiving resolve infos.
        var infos = new OpenInAppEntryPoint.ResolveResult.Info(mResolveInfo);
        mEntryPoint.onResolveInfosFetched(delegate, infos, mIntent, mUrl, /* navigationId= */ 123L);

        // Resolve infos received; the app info should be non-null.
        assertNonNull(delegate.getCurrentOpenInAppInfo());

        ArgumentCaptor<OmniboxChipManager.ChipCallback> callbackCaptor =
                ArgumentCaptor.forClass(OmniboxChipManager.ChipCallback.class);
        ArgumentCaptor<Runnable> actionCaptor = ArgumentCaptor.forClass(Runnable.class);
        String chipTitle = mContext.getString(R.string.open_in_app);
        String chipDescription = mContext.getString(R.string.open_in_app_desc, LABEL);
        verify(mOmniboxChipManager)
                .placeChip(
                        eq(chipTitle),
                        eq(mIcon),
                        eq(chipDescription),
                        actionCaptor.capture(),
                        callbackCaptor.capture());

        // Simulate chip click.
        actionCaptor.getValue().run();
        verify(mExternalNavigationHelper).launchExternalApp(eq(mIntent), eq(mContext));
        ShadowLooper.idleMainLooper();
        verify(mTabModelSelector).tryCloseTab(any(), eq(false));

        when(mOmniboxChipManager.isChipPlaced()).thenReturn(true);

        // When chip is shown, it shouldn't be in the menu.
        callbackCaptor.getValue().onChipShown();
        assertNull(mEntryPoint.getOpenInAppInfoForMenuItem());

        // When chip is hidden, it should be in the menu.
        callbackCaptor.getValue().onChipHidden();
        var appInfo = mEntryPoint.getOpenInAppInfoForMenuItem();
        assertNonNull(appInfo);
        assertEquals(LABEL, appInfo.appName);
        assertEquals(mIcon, appInfo.appIcon);

        captor.getValue().didFinishNavigationInPrimaryMainFrame(mNavigationHandle);
        verify(mOmniboxChipManager).dismissChip();

        // Empty resolve infos; the app info should be null.
        mEntryPoint.onResolveInfosFetched(
                delegate,
                new OpenInAppEntryPoint.ResolveResult.None(),
                mIntent,
                mUrl,
                /* navigationId= */ 0);
        assertNull(delegate.getCurrentOpenInAppInfo());
        verify(mOmniboxChipManager, times(2)).dismissChip();
    }

    @Test
    public void placeClickDismissChip_incognito() {
        when(mTab.isOffTheRecord()).thenReturn(true);
        OpenInAppDelegate delegate = OpenInAppDelegate.from(mTab);
        delegate.setExternalNavigationHelper(mExternalNavigationHelper);

        var captor = ArgumentCaptor.forClass(WebContentsObserver.class);
        verify(((WebContentsObserver.Observable) mWebContents)).addObserver(captor.capture());
        captor.getValue().didFinishNavigationInPrimaryMainFrame(mNavigationHandle);

        ShadowLooper.idleMainLooper();

        var infos = new OpenInAppEntryPoint.ResolveResult.Info(mResolveInfo);
        mEntryPoint.onResolveInfosFetched(delegate, infos, mIntent, mUrl, /* navigationId= */ 123L);

        ArgumentCaptor<Runnable> actionCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mOmniboxChipManager).placeChip(any(), any(), any(), actionCaptor.capture(), any());

        // Simulate chip click.
        actionCaptor.getValue().run();

        ArgumentCaptor<Runnable> confirmationCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mExternalNavigationHelper)
                .launchExternalAppWithIncognitoConfirmation(
                        eq(mIntent), eq(123L), eq(mContext), confirmationCaptor.capture());

        // Tab should not be closed yet.
        verify(mTabModelSelector, never()).tryCloseTab(any(), anyBoolean());

        // Simulate user confirmation in the dialog.
        confirmationCaptor.getValue().run();
        ShadowLooper.idleMainLooper();
        verify(mTabModelSelector).tryCloseTab(any(), eq(false));
    }
}
