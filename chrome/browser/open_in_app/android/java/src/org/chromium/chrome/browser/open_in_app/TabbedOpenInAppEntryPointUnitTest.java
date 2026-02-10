// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.open_in_app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.Drawable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.OmniboxChipManager;
import org.chromium.chrome.browser.tab.Tab;

/** Unit tests for {@link TabbedOpenInAppEntryPoint}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabbedOpenInAppEntryPointUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private OmniboxChipManager mOmniboxChipManager;

    private Context mContext;
    private SettableNullableObservableSupplier<Tab> mTabSupplier;
    private TabbedOpenInAppEntryPoint mEntryPoint;
    private UserDataHost mUserDataHost;

    @Before
    public void setUp() {
        mContext = Robolectric.buildActivity(Activity.class).setup().get();
        mTabSupplier = ObservableSuppliers.createNullable();
        mUserDataHost = new UserDataHost();
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);

        mEntryPoint = new TabbedOpenInAppEntryPoint(mTabSupplier, mOmniboxChipManager, mContext);
        mTabSupplier.set(mTab);
    }

    @Test
    public void showAndDismissChip() {
        OpenInAppDelegate delegate = OpenInAppDelegate.from(mTab);
        Drawable icon = mock(Drawable.class);
        Runnable action = () -> {};
        OpenInAppDelegate.OpenInAppInfo info =
                new OpenInAppDelegate.OpenInAppInfo(action, "app", icon);

        delegate.updateOpenInAppInfo(info);

        ArgumentCaptor<OmniboxChipManager.ChipCallback> callbackCaptor =
                ArgumentCaptor.forClass(OmniboxChipManager.ChipCallback.class);
        verify(mOmniboxChipManager)
                .showChip(
                        eq(mContext.getString(R.string.open_in_app)),
                        eq(icon),
                        eq(mContext.getString(R.string.open_in_app)),
                        eq(action),
                        callbackCaptor.capture());
        when(mOmniboxChipManager.isChipShown()).thenReturn(true);

        // When chip is shown, it shouldn't be in the menu.
        callbackCaptor.getValue().onChipShown();
        assertNull(mEntryPoint.getOpenInAppInfoForMenuItem());

        // When chip is hidden, it should be in the menu.
        callbackCaptor.getValue().onChipHidden();
        assertEquals(info, mEntryPoint.getOpenInAppInfoForMenuItem());

        // When info is null, chip should be dismissed.
        delegate.updateOpenInAppInfo(null);
        verify(mOmniboxChipManager).dismissChip();
    }
}
