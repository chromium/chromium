// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.runtime_library;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.IBinder;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;

/** Instrumentation tests for {@link org.chromium.webapk.WebApkServiceImpl}. */
@RunWith(BaseJUnit4ClassRunner.class)
public class WebApkServiceImplTest {
    private static final String APK_WITH_WEBAPK_SERVICE_PACKAGE =
            "org.chromium.webapk.lib.runtime_library.test.apk_with_webapk_service";
    private static final String WEBAPK_SERVICE_IMPL_WRAPPER_CLASS_NAME =
            "org.chromium.webapk.lib.runtime_library.test.TestWebApkServiceImplWrapper";

    private static final int SMALL_ICON_ID = 1229;

    private Context mContext;
    private Context mTargetContext;

    /** The target app's uid. */
    private int mTargetUid;

    /** CallbackHelper which blocks till the service is connected. */
    private static class ServiceConnectionWaiter extends CallbackHelper
            implements ServiceConnection {
        private IWebApkApi mApi;

        public IWebApkApi api() {
            return mApi;
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            mApi = IWebApkApi.Stub.asInterface(service);
            notifyCalled();
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {}
    }

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mTargetContext = ApplicationProvider.getApplicationContext();
        mTargetUid = getUid(mTargetContext);
    }

    /** Test that an application which is not allowed to use the WebAPK service actually cannot. */
    @Test
    @SmallTest
    public void testApiFailsIfNoPermission() throws Exception {
        IWebApkApi api = bindService(mContext, mTargetUid + 1, SMALL_ICON_ID);
        try {
            // Check that the api either throws an exception or returns a default small icon id.
            int actualSmallIconId = api.getSmallIconId();
            Assert.assertTrue(actualSmallIconId != SMALL_ICON_ID);
        } catch (Exception e) {
        }
    }

    /** Test that an application which is allowed to use the WebAPK service actually can. */
    @Test
    @SmallTest
    public void testApiWorksIfHasPermission() throws Exception {
        IWebApkApi api = bindService(mContext, mTargetUid, SMALL_ICON_ID);
        try {
            // Check that the api returns the real small icon id.
            int actualSmallIconId = api.getSmallIconId();
            Assert.assertEquals(SMALL_ICON_ID, actualSmallIconId);
        } catch (Exception e) {
            e.printStackTrace();
            Assert.fail("Should not have thrown an exception when permission is granted.");
        }
    }

    /** Returns the uid for {@link context}. */
    private static int getUid(Context context) {
        PackageManager packageManager = context.getPackageManager();
        ApplicationInfo appInfo;
        try {
            appInfo =
                    packageManager.getApplicationInfo(
                            context.getPackageName(), PackageManager.GET_META_DATA);
            return appInfo.uid;
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Binds to the WebAPK service and blocks till the service is connected.
     *
     * @param context The context for the application containing the WebAPK service to bind to.
     * @param authorizedUid The uid of the only application allowed to use the WebAPK service's
     *     methods.
     * @param smallIconId The real small icon id.
     * @return IWebApkApi to use to communicate with the service.
     */
    private static IWebApkApi bindService(Context context, int authorizedUid, int smallIconId)
            throws Exception {
        Intent intent = new Intent();
        intent.setComponent(
                new ComponentName(
                        APK_WITH_WEBAPK_SERVICE_PACKAGE, WEBAPK_SERVICE_IMPL_WRAPPER_CLASS_NAME));
        intent.putExtra(WebApkServiceImpl.KEY_SMALL_ICON_ID, smallIconId);
        intent.putExtra(WebApkServiceImpl.KEY_HOST_BROWSER_UID, authorizedUid);

        ServiceConnectionWaiter waiter = new ServiceConnectionWaiter();
        context.bindService(intent, waiter, Context.BIND_AUTO_CREATE);
        waiter.waitForCallback(0);

        IWebApkApi api = waiter.api();
        Assert.assertNotNull(api);
        return api;
    }
}
