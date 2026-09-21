// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.open_in_app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
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
import android.net.Uri;

import org.junit.After;
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
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.omnibox.OmniboxChipManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.external_intents.ExternalNavigationHelper;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link TabbedOpenInAppEntryPoint}. */
@RunWith(BaseRobolectricTestRunner.class)
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
        when(mTab.getUrl()).thenReturn(mUrl);
        when(mPackageManager.getApplicationInfo(any(), anyInt())).thenReturn(new ApplicationInfo());
        when(mPackageManager.getApplicationLogo(any(ApplicationInfo.class))).thenReturn(mIcon);
        when(mPackageManager.getApplicationLabel(any(ApplicationInfo.class))).thenReturn(LABEL);
        when(mContext.getPackageManager()).thenReturn(mPackageManager);

        mResolveInfo.filter = mIntentFilter;
        mActivityInfo.packageName = PACKAGE;
        mResolveInfo.activityInfo = mActivityInfo;

        mNavigationHandle = NavigationHandle.createForTesting(mUrl, false, 0, true);
        mNavigationHandle.callDidFinishForTesting(mUrl);

        mEntryPoint = new TabbedOpenInAppEntryPoint(mTabSupplier, mOmniboxChipManager, mContext);
        mTabSupplier.set(mTab);
    }

    @After
    public void tearDown() {
        mEntryPoint.destroy();
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
        var shownWatcher =
                HistogramWatcher.newSingleRecordWatcher("Android.OpenInApp.Impression", true);
        mEntryPoint.onResolveInfosFetched(delegate, infos, mIntent, mUrl, /* navigationId= */ 123L);
        shownWatcher.assertExpected();

        // Resolve infos received; the app info should be non-null.
        assertNonNull(delegate.getCurrentOpenInAppInfo());

        // Calling it again on same package should not emit.
        var shownWatcher2 =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.OpenInApp.Impression")
                        .build();
        mEntryPoint.onResolveInfosFetched(delegate, infos, mIntent, mUrl, /* navigationId= */ 123L);
        shownWatcher2.assertExpected();

        ArgumentCaptor<Runnable> actionCaptor = ArgumentCaptor.forClass(Runnable.class);
        String chipTitle = mContext.getString(R.string.open_in_app);
        String chipDescription = mContext.getString(R.string.open_in_app_desc, LABEL);
        verify(mOmniboxChipManager, times(2))
                .placeChip(eq(chipTitle), eq(mIcon), eq(chipDescription), actionCaptor.capture());

        // Simulate chip click.
        var clickWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.OpenInApp.Clicked.OmniboxChip", true);
        actionCaptor.getValue().run();
        clickWatcher.assertExpected();

        verify(mExternalNavigationHelper).launchExternalApp(eq(mIntent), eq(mContext));
        ShadowLooper.idleMainLooper();
        ArgumentCaptor<TabClosureParams> closureParamsCaptor =
                ArgumentCaptor.forClass(TabClosureParams.class);
        verify(mTabModelSelector).tryCloseTab(closureParamsCaptor.capture(), eq(false));
        assertEquals(TabClosingSource.OPEN_IN_APP, closureParamsCaptor.getValue().tabClosingSource);

        when(mOmniboxChipManager.isChipPlaced()).thenReturn(true);

        var appInfo = mEntryPoint.getOpenInAppInfoForMenuItem();
        assertNonNull(appInfo);
        assertEquals(LABEL, appInfo.appName);
        assertEquals(mIcon, appInfo.appIcon);
        assertEquals(PACKAGE, appInfo.packageName);

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

        // Now if we get info again, it should emit again (even on same domain because it was
        // cleared).
        var shownWatcher3 =
                HistogramWatcher.newSingleRecordWatcher("Android.OpenInApp.Impression", true);
        mEntryPoint.onResolveInfosFetched(delegate, infos, mIntent, mUrl, /* navigationId= */ 123L);
        shownWatcher3.assertExpected();
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
        verify(mOmniboxChipManager).placeChip(any(), any(), any(), actionCaptor.capture());

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
        ArgumentCaptor<TabClosureParams> closureParamsCaptor =
                ArgumentCaptor.forClass(TabClosureParams.class);
        verify(mTabModelSelector).tryCloseTab(closureParamsCaptor.capture(), eq(false));
        assertEquals(TabClosingSource.OPEN_IN_APP, closureParamsCaptor.getValue().tabClosingSource);
    }

    @Test
    public void testOnAppInstallationStateChanged() {
        OpenInAppDelegate delegate = OpenInAppDelegate.from(mTab);
        delegate.setExternalNavigationHelper(mExternalNavigationHelper);

        // Set up ShadowPackageManager to resolve the intent.
        var shadowPackageManager =
                Shadows.shadowOf(ContextUtils.getApplicationContext().getPackageManager());
        Intent targetIntent = new Intent(Intent.ACTION_VIEW);
        targetIntent.setData(android.net.Uri.parse(mUrl.getSpec()));
        targetIntent.addCategory(Intent.CATEGORY_BROWSABLE);

        // Local real ActivityInfo with enabled state, and not the mock ones so that
        // the Robolectric test runner doesn't drop it.
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = PACKAGE;
        activityInfo.name = PACKAGE + ".MainActivity";
        activityInfo.enabled = true;
        activityInfo.exported = true;
        ApplicationInfo appInfo = new ApplicationInfo();
        appInfo.packageName = PACKAGE;
        appInfo.enabled = true;
        activityInfo.applicationInfo = appInfo;

        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = activityInfo;

        // Use a real IntentFilter instead of the mock mIntentFilter so Robolectric's
        // matching logic works correctly.
        IntentFilter realFilter = new IntentFilter(Intent.ACTION_VIEW);
        realFilter.addCategory(Intent.CATEGORY_DEFAULT);
        realFilter.addCategory(Intent.CATEGORY_BROWSABLE);
        realFilter.addDataScheme("http");
        realFilter.addDataScheme("https");
        resolveInfo.filter = realFilter;

        // Add the resolve info to fake package manager.
        shadowPackageManager.addResolveInfoForIntent(targetIntent, resolveInfo);

        // Mock current committed URL of WebContents.
        when(mWebContents.getLastCommittedUrl()).thenReturn(mUrl);

        // 1. Trigger app installation state changed via OS package broadcast.
        Intent packageAddedIntent = new Intent(Intent.ACTION_PACKAGE_ADDED);
        packageAddedIntent.setData(Uri.parse("package:" + PACKAGE));
        mContext.sendBroadcast(packageAddedIntent);

        // 2. AsyncTask executes intent query. Pump background tasks and UI thread.
        RobolectricUtil.runAllBackgroundAndUi();

        // 3. Verify it resolved successfully and app info is not null.
        assertNotNull(delegate.getCurrentOpenInAppInfo());

        // 4. Remove package and resolve info from PackageManager, trigger package removal
        // broadcast, and verify it cleans up.
        shadowPackageManager.deletePackage(PACKAGE);
        shadowPackageManager.removeResolveInfosForIntent(targetIntent, PACKAGE);
        Intent packageRemovedIntent = new Intent(Intent.ACTION_PACKAGE_REMOVED);
        packageRemovedIntent.setData(Uri.parse("package:" + PACKAGE));
        mContext.sendBroadcast(packageRemovedIntent);
        RobolectricUtil.runAllBackgroundAndUi();
        assertNull(delegate.getCurrentOpenInAppInfo());
    }

    @Test
    public void testOnResolveInfosFetched_dropsStaleOutOfOrderTask() {
        OpenInAppDelegate delegate = OpenInAppDelegate.from(mTab);
        delegate.setLastNavigatedUrl(mUrl);

        var infos = new OpenInAppEntryPoint.ResolveResult.Info(mResolveInfo);
        mEntryPoint.setResolveTaskIdForTesting(2);

        // Simulate resolution of a newer task (taskId = 2).
        mEntryPoint.onResolveInfosFetched(
                delegate, infos, mIntent, mUrl, /* navigationId= */ 1L, 2);
        assertNotNull(delegate.getCurrentOpenInAppInfo());

        // Simulate delayed completion of an older task (taskId = 1) resolving to None.
        mEntryPoint.onResolveInfosFetched(
                delegate,
                new OpenInAppEntryPoint.ResolveResult.None(),
                mIntent,
                mUrl,
                /* navigationId= */ 1L,
                /* taskId= */ 1);

        // The older task (None) must be ignored; the newer app info should be preserved.
        assertNotNull(delegate.getCurrentOpenInAppInfo());
    }
}
