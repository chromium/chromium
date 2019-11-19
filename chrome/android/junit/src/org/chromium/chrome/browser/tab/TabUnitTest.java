// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.Map;

/**
 * Tests for {@link Tab}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabUnitTest {
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;

    @Mock
    private WindowAndroid mWindowAndroid;
    @Mock
    private LoadUrlParams mLoadUrlParams;
    @Mock
    private EmptyTabObserver mObserver;
    @Mock
    private Context mContext;
    @Mock
    private WeakReference<Context> mWeakReferenceContext;
    @Mock
    private WeakReference<Activity> mWeakReferenceActivity;
    @Mock
    private Activity mActivity;

    private Tab mTab;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doReturn(mWeakReferenceActivity).when(mWindowAndroid).getActivity();
        doReturn(mWeakReferenceContext).when(mWindowAndroid).getContext();
        doReturn(mActivity).when(mWeakReferenceActivity).get();
        doReturn(mContext).when(mWeakReferenceContext).get();
        doReturn(mContext).when(mContext).getApplicationContext();

        // Bypass feature checks.
        Map<String, Boolean> features = new HashMap<>();
        features.put("ShoppingAssist", true);
        ChromeFeatureList.setTestFeatures(features);

        mTab = new Tab(TAB1_ID, null, false, null);
        mTab.addObserver(mObserver);
    }

    @Test
    @SmallTest
    public void testSetRootIdWithChange() {
        assertThat(mTab.getRootId(), equalTo(TAB1_ID));

        mTab.setRootId(TAB2_ID);

        verify(mObserver).onRootIdChanged(mTab, TAB2_ID);
        assertThat(mTab.getRootId(), equalTo(TAB2_ID));
        assertThat(mTab.isTabStateDirty(), equalTo(true));
    }

    @Test
    @SmallTest
    public void testSetRootIdWithoutChange() {
        assertThat(mTab.getRootId(), equalTo(TAB1_ID));
        mTab.setIsTabStateDirty(false);

        mTab.setRootId(TAB1_ID);

        verify(mObserver, never()).onRootIdChanged(any(Tab.class), anyInt());
        assertThat(mTab.getRootId(), equalTo(TAB1_ID));
        assertThat(mTab.isTabStateDirty(), equalTo(false));
    }
}
