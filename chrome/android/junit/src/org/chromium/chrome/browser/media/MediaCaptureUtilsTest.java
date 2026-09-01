// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.app.Activity;
import android.content.Context;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Tests for {@link MediaCaptureUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MediaCaptureUtilsTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Callback<Tab> mTestingCallback;
    @Mock private Context mContext;
    @Mock private TabWindowManager mTabWindowManager;

    @After
    public void tearDown() {
        MediaCaptureUtils.setBringTabToFrontCallbackForTesting(null);
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
    }

    @Test
    @SmallTest
    public void testBringTabToFront_WithTestingCallback() {
        MediaCaptureUtils.setBringTabToFrontCallbackForTesting(mTestingCallback);
        MediaCaptureUtils.bringTabToFront(mContext, mTab);

        verify(mTestingCallback).onResult(mTab);
    }

    @Test
    @SmallTest
    public void testBringTabToFront_NullActivity() {
        doReturn(mWindowAndroid).when(mTab).getWindowAndroidChecked();
        doReturn(new WeakReference<Activity>(null)).when(mWindowAndroid).getActivity();
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
        MediaCaptureUtils.bringTabToFront(mContext, mTab);

        verifyNoInteractions(mTabWindowManager);
    }

    @Test
    @SmallTest
    public void testBringTabToFront_ValidActivity() {
        Activity activity = Robolectric.buildActivity(Activity.class).get();
        doReturn(mWindowAndroid).when(mTab).getWindowAndroidChecked();
        doReturn(new WeakReference<Activity>(activity)).when(mWindowAndroid).getActivity();
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);

        MediaCaptureUtils.bringTabToFront(activity, mTab);

        verify(mTabWindowManager).getIdForWindow(activity);
    }

    @Test
    @SmallTest
    public void testBringTabToFront_MultiWindowInstance() {
        Activity activity = Robolectric.buildActivity(Activity.class).get();
        doReturn(mWindowAndroid).when(mTab).getWindowAndroidChecked();
        doReturn(new WeakReference<Activity>(activity)).when(mWindowAndroid).getActivity();
        doReturn(100).when(mTab).getId();

        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
        doReturn(1).when(mTabWindowManager).getIdForWindow(activity);

        MediaCaptureUtils.bringTabToFront(activity, mTab);

        verify(mTabWindowManager).getIdForWindow(activity);
    }
}
