// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.net.Uri;
import android.os.Build;
import android.view.DragAndDropPermissions;
import android.view.DragEvent;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.TabViewAndroidDelegate.DragAndDropBrowserDelegateImpl;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragAndDropBrowserDelegate;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for the TabViewAndroidDelegate. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class TabViewAndroidDelegateTest {
    private final ArgumentCaptor<TabObserver> mTabObserverCaptor =
            ArgumentCaptor.forClass(TabObserver.class);

    @Mock
    private TabImpl mTab;

    @Mock
    private WebContents mWebContents;

    @Mock
    private WindowAndroid mWindowAndroid;

    @Mock
    private ContentView mContentView;

    @Mock
    private DragEvent mDragEvent;

    @Mock
    private DragAndDropPermissions mDragAndDropPermissions;

    @Mock
    private Activity mActivity;

    @Mock
    private ActivityInfo mActivityInfo;

    @Mock
    private PackageManager mPackageManager;

    private ApplicationViewportInsetSupplier mApplicationInsetSupplier;
    private ObservableSupplierImpl<Integer> mFeatureInsetSupplier;
    private TabViewAndroidDelegate mViewAndroidDelegate;

    @Before
    public void setUp() throws NameNotFoundException {
        MockitoAnnotations.initMocks(this);

        mFeatureInsetSupplier = new ObservableSupplierImpl<>();

        mApplicationInsetSupplier = ApplicationViewportInsetSupplier.createForTests();
        mApplicationInsetSupplier.addSupplier(mFeatureInsetSupplier);

        when(mWindowAndroid.getApplicationBottomInsetProvider())
                .thenReturn(mApplicationInsetSupplier);
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getContext()).thenReturn(mActivity);
        when(mActivity.getApplicationContext())
                .thenReturn(ApplicationProvider.getApplicationContext());
        when(mActivity.requestDragAndDropPermissions(mDragEvent))
                .thenReturn(mDragAndDropPermissions);

        Context mApplicationContext = Mockito.spy(ContextUtils.getApplicationContext());
        when(mApplicationContext.getPackageManager()).thenReturn(mPackageManager);
        when(mPackageManager.getActivityInfo(any(), anyInt())).thenReturn(mActivityInfo);
        ContextUtils.initApplicationContextForTests(mApplicationContext);

        FeatureList.TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU, false);
        testValues.addFeatureFlagOverride(ChromeFeatureList.NEW_INSTANCE_FROM_DRAGGED_LINK, true);
        FeatureList.setTestValues(testValues);
        mViewAndroidDelegate = new TabViewAndroidDelegate(mTab, mContentView);
        verify(mTab).addObserver(mTabObserverCaptor.capture());
    }

    @Test
    public void testInset() {
        mFeatureInsetSupplier.set(10);
        assertEquals("The bottom inset for the tab should be non-zero.", 10,
                mViewAndroidDelegate.getViewportInsetBottom());
    }

    @Test
    public void testInset_afterHidden() {
        mFeatureInsetSupplier.set(10);

        when(mTab.isHidden()).thenReturn(true);

        mTabObserverCaptor.getValue().onHidden(mTab, 0);
        assertEquals("The bottom inset for the tab should be zero.", 0,
                mViewAndroidDelegate.getViewportInsetBottom());
    }

    @Test
    public void testInset_afterDetachAndAttach() {
        mFeatureInsetSupplier.set(10);

        assertEquals("The bottom inset for the tab should be non-zero.", 10,
                mViewAndroidDelegate.getViewportInsetBottom());

        mTabObserverCaptor.getValue().onActivityAttachmentChanged(mTab, null);

        assertEquals("The bottom inset for the tab should be zero.", 0,
                mViewAndroidDelegate.getViewportInsetBottom());

        WindowAndroid window = Mockito.mock(WindowAndroid.class);
        mTabObserverCaptor.getValue().onActivityAttachmentChanged(mTab, window);
        assertEquals("The bottom inset for the tab should be non-zero.", 10,
                mViewAndroidDelegate.getViewportInsetBottom());
    }

    @Test
    public void testDragAndDropBrowserDelegate_getDragAndDropPermissions() {
        DragAndDropBrowserDelegate delegate = new DragAndDropBrowserDelegateImpl(mTab, true);
        assertTrue("SupportDropInChrome should be true.", delegate.getSupportDropInChrome());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            DragAndDropPermissions permissions = delegate.getDragAndDropPermissions(mDragEvent);
            assertNotNull("DragAndDropPermissions should not be null.", permissions);
        }
    }

    @Test
    @Config(sdk = 30)
    public void testDragAndDropBrowserDelegate_createLinkIntent_PostR() {
        DragAndDropBrowserDelegate delegate = new DragAndDropBrowserDelegateImpl(mTab, true);
        mActivityInfo.launchMode = ActivityInfo.LAUNCH_SINGLE_INSTANCE_PER_TASK;
        Intent intent = delegate.createLinkIntent(JUnitTestGURLs.EXAMPLE_URL);
        assertEquals("The intent flags should match.",
                Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_MULTIPLE_TASK,
                intent.getFlags());
        assertEquals("The intent class should be ChromeTabbedActivity.",
                ChromeTabbedActivity.class.getName(), intent.getComponent().getClassName());
        assertTrue("preferNew extra should be true.",
                intent.getBooleanExtra(IntentHandler.EXTRA_PREFER_NEW, false));
        assertEquals("The intent should contain Uri data.", Uri.parse(JUnitTestGURLs.EXAMPLE_URL),
                intent.getData());
    }

    @Test
    @Config(sdk = 29)
    public void testDragAndDropBrowserDelegate_createLinkIntent_PreR() {
        DragAndDropBrowserDelegate delegate = new DragAndDropBrowserDelegateImpl(mTab, true);
        Intent intent = delegate.createLinkIntent(JUnitTestGURLs.EXAMPLE_URL);
        assertNull("The intent should be null on R- versions.", intent);
    }
}
