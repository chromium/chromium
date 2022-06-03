// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.not;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.os.Bundle;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.AppHooksImpl;

import java.util.ArrayList;
import java.util.List;

/**
 * Test class for {@link CctOfflinePageModelObserver}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CctOfflinePageModelObserverTest {
    private static final String APP_NAME = "abc.xyz";
    private static final String ORIGIN_STRING = "[abc.xyz, []]";
    private static final String URL = "http://www.google.com";

    @Mock
    private Context mContext;
    @Mock
    private PackageManager mPackageManager;
    @Mock
    private AppHooksImpl mAppHooks;

    private PackageInfo mInfo;

    @Before
    public void setUp() throws PackageManager.NameNotFoundException {
        MockitoAnnotations.initMocks(this);

        mInfo = new PackageInfo();
        mInfo.signatures = new Signature[0];

        when(mPackageManager.getPackageInfo(eq(APP_NAME), anyInt())).thenReturn(mInfo);
        when(mPackageManager.getPackageInfo(not(eq(APP_NAME)), anyInt()))
                .thenThrow(new PackageManager.NameNotFoundException());

        when(mContext.getPackageManager()).thenReturn(mPackageManager);

        List<String> allowlist = new ArrayList<>();
        allowlist.add(APP_NAME);
        when(mAppHooks.getOfflinePagesCctAllowlist()).thenReturn(allowlist);

        ContextUtils.initApplicationContextForTests(mContext);
        AppHooks.setInstanceForTesting(mAppHooks);
    }

    @After
    public void tearDown() {
        AppHooks.setInstanceForTesting(null);
    }

    @Test
    public void testSendBroadcastForAppNameInAllowlist_addedPage() {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        CctOfflinePageModelObserver.onPageChanged(ORIGIN_STRING, true, URL);

        verify(mContext).sendBroadcast(intentCaptor.capture());

        Intent intent = intentCaptor.getValue();
        assertEquals(APP_NAME, intent.getPackage());
        assertEquals(CctOfflinePageModelObserver.ACTION_OFFLINE_PAGES_UPDATED, intent.getAction());
        Bundle pageInfo = intent.getParcelableExtra(CctOfflinePageModelObserver.PAGE_INFO_KEY);
        assertNotNull(pageInfo);

        assertTrue(pageInfo.getBoolean(CctOfflinePageModelObserver.IS_NEW_KEY));
        assertEquals(URL, pageInfo.getString(CctOfflinePageModelObserver.URL_KEY));
    }

    @Test
    public void testSendBroadcastForAppNameInAllowlist_deletedPage() {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        CctOfflinePageModelObserver.onPageChanged(ORIGIN_STRING, false, URL);

        verify(mContext).sendBroadcast(intentCaptor.capture());

        Intent intent = intentCaptor.getValue();
        assertEquals(APP_NAME, intent.getPackage());
        assertEquals(CctOfflinePageModelObserver.ACTION_OFFLINE_PAGES_UPDATED, intent.getAction());
        Bundle pageInfo = intent.getParcelableExtra(CctOfflinePageModelObserver.PAGE_INFO_KEY);
        assertNotNull(pageInfo);

        assertFalse(pageInfo.getBoolean(CctOfflinePageModelObserver.IS_NEW_KEY));
        assertEquals(URL, pageInfo.getString(CctOfflinePageModelObserver.URL_KEY));
    }

    @Test
    public void testDoesNotSendBroadcastForAppNameNotInAllowlist() {
        String originString = "[xyz.abc,[]]";
        CctOfflinePageModelObserver.onPageChanged(originString, true, URL);

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mContext, never()).sendBroadcast(intentCaptor.capture());
    }

    @Test
    public void testDoesNotSendBroadcastForChrome() {
        String originString = "";
        CctOfflinePageModelObserver.onPageChanged(originString, true, URL);

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mContext, never()).sendBroadcast(intentCaptor.capture());
    }
}
