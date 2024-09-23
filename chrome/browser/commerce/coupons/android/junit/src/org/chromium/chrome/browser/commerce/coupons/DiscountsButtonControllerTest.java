// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce.coupons;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.commerce.CommerceBottomSheetContentController;
import org.chromium.chrome.browser.tab.Tab;

/** Unit tests for {@link DiscountsButtonController} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DiscountsButtonControllerTest {
    @Mock private CommerceBottomSheetContentController mCommerceBottomSheetContentController;
    @Mock private Tab mActiveTab;
    private Context mContext;
    private DiscountsButtonController mDiscountsButtonController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        mDiscountsButtonController =
                new DiscountsButtonController(
                        mContext,
                        () -> mActiveTab,
                        null,
                        () -> mCommerceBottomSheetContentController);
    }

    @Test
    public void testOnClick() {
        mDiscountsButtonController.onClick(null);
        verify(mCommerceBottomSheetContentController, times(1)).requestShowContent();
    }
}
