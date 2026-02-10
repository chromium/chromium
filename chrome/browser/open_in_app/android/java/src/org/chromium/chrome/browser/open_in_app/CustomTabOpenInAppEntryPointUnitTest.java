// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.open_in_app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.Drawable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;

/** Unit tests for {@link CustomTabOpenInAppEntryPoint}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CustomTabOpenInAppEntryPointUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;

    private Context mContext;
    private SettableNullableObservableSupplier<Tab> mTabSupplier;
    private CustomTabOpenInAppEntryPoint mEntryPoint;
    private UserDataHost mUserDataHost;

    @Before
    public void setUp() {
        mContext = Robolectric.buildActivity(Activity.class).setup().get();
        mTabSupplier = ObservableSuppliers.createNullable();
        mUserDataHost = new UserDataHost();
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);

        mEntryPoint = new CustomTabOpenInAppEntryPoint(mTabSupplier, mContext);
        mTabSupplier.set(mTab);
    }

    @Test
    public void getOpenInAppInfoForMenuItem() {
        assertNull(mEntryPoint.getOpenInAppInfoForMenuItem());

        OpenInAppDelegate delegate = OpenInAppDelegate.from(mTab);
        OpenInAppDelegate.OpenInAppInfo info =
                new OpenInAppDelegate.OpenInAppInfo(() -> {}, "app", mock(Drawable.class));
        delegate.updateOpenInAppInfo(info);

        assertEquals(info, mEntryPoint.getOpenInAppInfoForMenuItem());

        delegate.updateOpenInAppInfo(null);
        assertNull(mEntryPoint.getOpenInAppInfoForMenuItem());
    }

    @Test
    public void destroy() {
        OpenInAppDelegate delegate = OpenInAppDelegate.from(mTab);
        OpenInAppDelegate.OpenInAppInfo info =
                new OpenInAppDelegate.OpenInAppInfo(() -> {}, "app", mock(Drawable.class));
        delegate.updateOpenInAppInfo(info);
        assertEquals(info, mEntryPoint.getOpenInAppInfoForMenuItem());

        mEntryPoint.destroy();
        assertNull(mEntryPoint.getOpenInAppInfoForMenuItem());

        // Updating info after destroy shouldn't affect the entry point.
        delegate.updateOpenInAppInfo(info);
        assertNull(mEntryPoint.getOpenInAppInfoForMenuItem());
    }
}
