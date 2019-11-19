// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.webapk.lib.common.WebApkCommonUtils;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.InputStream;

/**
 * Tests HostBrowserClassLoader.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HostBrowserClassLoaderTest {

    /**
     * Mock package name for the remote host browser.
     */
    private static final String REMOTE_PACKAGE_NAME = "remote_package";

    private static final int REMOTE_DEX_VERSION = 1;
    private static final int REMOTE_VERSION_CODE = 5;

    private Context mContext;
    private Context mRemoteContext;
    private AssetManager mRemoteAssetManager;
    private PackageManager mPackageManager;

    /**
     * Stub DexLoader. Used to verify the version of the runtime library dex which is used to build
     * the ClassLoader.
     */
    private DexLoader mMockDexLoader;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        mPackageManager = mContext.getPackageManager();
        setRemoteVersionCode(REMOTE_VERSION_CODE);

        mRemoteAssetManager = Mockito.mock(AssetManager.class);
        setRemoteDexVersion(REMOTE_DEX_VERSION);

        mRemoteContext = Mockito.mock(Context.class);
        Mockito.when(mRemoteContext.getAssets()).thenReturn(mRemoteAssetManager);
        Mockito.when(mRemoteContext.getPackageName()).thenReturn(REMOTE_PACKAGE_NAME);
        Mockito.when(mRemoteContext.getPackageManager()).thenReturn(mPackageManager);

        mMockDexLoader = Mockito.mock(DexLoader.class);
    }

    /**
     * Test upgrading to a new runtime dex version.
     */
    @Test
    public void testNewRuntimeDexVersion() {
        HostBrowserClassLoader.createClassLoader(mContext, mRemoteContext, mMockDexLoader, null);

        String expectedDexName = WebApkCommonUtils.getRuntimeDexName(REMOTE_DEX_VERSION);
        verifyDexLoaderLoadCall(expectedDexName);
        Mockito.reset(mMockDexLoader);

        setRemoteVersionCode(REMOTE_VERSION_CODE + 1);
        setRemoteDexVersion(REMOTE_DEX_VERSION + 1);
        HostBrowserClassLoader.createClassLoader(mContext, mRemoteContext, mMockDexLoader, null);

        expectedDexName = WebApkCommonUtils.getRuntimeDexName(REMOTE_DEX_VERSION + 1);
        verifyDexLoaderLoadCall(expectedDexName);
    }

    /**
     * Test that HostBrowserClassLoader#deleteCachedDexes() is not called if nothing has changed
     * between calls to HostBrowserClassLoader#createClassLoader().
     */
    @Test
    public void testCreateClassLoaderTwiceSameCachedData() {
        String expectedDexName = WebApkCommonUtils.getRuntimeDexName(REMOTE_DEX_VERSION);
        HostBrowserClassLoader.createClassLoader(mContext, mRemoteContext, mMockDexLoader, null);
        verifyDexLoaderLoadCall(expectedDexName);
        Mockito.reset(mMockDexLoader);

        HostBrowserClassLoader.createClassLoader(mContext, mRemoteContext, mMockDexLoader, null);
        verifyDexLoaderLoadCall(expectedDexName);
        Mockito.verify(mMockDexLoader, Mockito.never()).deleteCachedDexes(Mockito.any(File.class));
    }

    /**
     * Test that the ClassLoader is reused if nothing changed since the last call to
     * HostBrowserClassLoader#createClassLoader() and HostBrowserClassLoader is still running.
     * Reusing the ClassLoader whenever possible is important because building the ClassLoader is
     * expensive.
     */
    @Test
    public void testCanReuseClassLoader() {
        HostBrowserClassLoader.createClassLoader(mContext, mRemoteContext, mMockDexLoader, null);
        Assert.assertTrue(
                HostBrowserClassLoader.canReuseClassLoaderInstance(mContext, mRemoteContext));
    }

    /**
     * Test that the ClassLoader is not reused (even if there is no new runtime dex version) if
     * Chrome is updated but WebAPK is still running.
     */
    @Test
    public void testDontReuseClassLoaderRemoteVersionCodeChange() {
        HostBrowserClassLoader.createClassLoader(mContext, mRemoteContext, mMockDexLoader, null);
        setRemoteVersionCode(REMOTE_VERSION_CODE + 1);
        Assert.assertFalse(
                HostBrowserClassLoader.canReuseClassLoaderInstance(mContext, mRemoteContext));
    }

    /**
     * Creates an InputStream with {@link value} as its data.
     */
    public InputStream createIntInputStream(int value) {
        String stringValue = "" + value;
        return new ByteArrayInputStream(stringValue.getBytes());
    }

    /**
     * Sets the remote host browser's version code.
     */
    public void setRemoteVersionCode(int versionCode) {
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = REMOTE_PACKAGE_NAME;
        packageInfo.versionCode = versionCode;
        Shadows.shadowOf(mPackageManager).addPackage(packageInfo);
    }

    /**
     * Sets the version of the current runtime library dex stored in the remote host browser's
     * assets.
     */
    public void setRemoteDexVersion(int dexVersion) {
        try {
            Mockito.when(mRemoteAssetManager.open("webapk_dex_version.txt"))
                    .thenReturn(createIntInputStream(dexVersion));
        } catch (Exception e) {
            Assert.fail();
        }
    }

    /**
     * Verifies {@link DexLoader#load()} call.
     * @param expectedDexName The name of the dex in the remote host browser's assets that
     *                        {@link DexLoader#load()} should have been called with.
     */
    public void verifyDexLoaderLoadCall(String expectedDexName) {
        Mockito.verify(mMockDexLoader)
                .load(Mockito.any(Context.class), Mockito.eq(expectedDexName),
                        (String) Mockito.isNull(), Mockito.any(File.class),
                        Mockito.any(File.class));
    }
}
