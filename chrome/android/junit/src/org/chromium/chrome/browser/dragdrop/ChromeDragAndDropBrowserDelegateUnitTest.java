// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.ClipData.Item;
import android.content.ClipDescription;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager.NameNotFoundException;
import android.net.Uri;
import android.view.DragAndDropPermissions;
import android.view.DragEvent;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.IntentUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.dragdrop.DragDropMetricUtils.UrlIntentSource;
import org.chromium.url.JUnitTestGURLs;

/** Unit test for {@link ChromeDragAndDropBrowserDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeDragAndDropBrowserDelegateUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Activity mActivity;
    @Mock private DragEvent mDragEvent;
    @Mock private DragAndDropPermissions mDragAndDropPermissions;
    @Mock private Profile mProfile;

    private Context mApplicationContext;
    private ChromeDragAndDropBrowserDelegate mDelegate;
    private FeatureList.TestValues mTestValues;

    @Before
    public void setup() throws NameNotFoundException {
        mTestValues = new TestValues();
        mTestValues.addFeatureFlagOverride(ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU, true);
        mTestValues.addFeatureFlagOverride(ChromeFeatureList.ANIMATED_IMAGE_DRAG_SHADOW, false);
        FeatureList.setTestValues(mTestValues);

        mApplicationContext = ContextUtils.getApplicationContext();
        ContextUtils.initApplicationContextForTests(mApplicationContext);
        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(false);

        when(mActivity.requestDragAndDropPermissions(mDragEvent))
                .thenReturn(mDragAndDropPermissions);
        when(mActivity.getApplicationContext())
                .thenReturn(ApplicationProvider.getApplicationContext());

        mDelegate = new ChromeDragAndDropBrowserDelegate(() -> mActivity);
    }

    @After
    public void teardown() {
        ChromeDragAndDropBrowserDelegate.setClipDataItemWithPendingIntentForTesting(null);
    }

    @Test
    public void testDragAndDropBrowserDelegate_getDragAndDropPermissions() {
        mTestValues.addFieldTrialParamOverride(
                ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU,
                ChromeDragAndDropBrowserDelegate.PARAM_DROP_IN_CHROME,
                "true");
        mDelegate = new ChromeDragAndDropBrowserDelegate(() -> mActivity);
        assertTrue("SupportDropInChrome should be true.", mDelegate.getSupportDropInChrome());
        assertFalse(
                "SupportAnimatedImageDragShadow should be false.",
                mDelegate.getSupportAnimatedImageDragShadow());

        DragAndDropPermissions permissions = mDelegate.getDragAndDropPermissions(mDragEvent);
        assertNotNull("DragAndDropPermissions should not be null.", permissions);
    }

    @Test
    public void testDragAndDropBrowserDelegate_NotSupportDropInChrome() {
        mTestValues.addFieldTrialParamOverride(
                ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU,
                ChromeDragAndDropBrowserDelegate.PARAM_DROP_IN_CHROME,
                "false");
        mDelegate = new ChromeDragAndDropBrowserDelegate(() -> mActivity);
        assertFalse("SupportDropInChrome should be false.", mDelegate.getSupportDropInChrome());

        AssertionError error = null;
        try {
            mDelegate.getDragAndDropPermissions(mDragEvent);
        } catch (AssertionError e) {
            error = e;
        }

        assertNotNull(
                "getDragAndDropPermissions should raise assert exception "
                        + "when accessed with drop in Chrome disabled.",
                error);
    }

    @Test
    @Config(sdk = 30)
    public void testDragAndDropBrowserDelegate_createLinkIntent_PostR() {
        MultiWindowTestUtils.enableMultiInstance();
        Intent intent =
                mDelegate.createUrlIntent(
                        JUnitTestGURLs.EXAMPLE_URL.getSpec(), UrlIntentSource.TAB_IN_STRIP);
        assertEquals(
                "The intent flags should match.",
                Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_MULTIPLE_TASK,
                intent.getFlags());
        assertEquals(
                "The intent class should be DragAndDropLauncherActivity.",
                DragAndDropLauncherActivity.class.getName(),
                intent.getComponent().getClassName());
        assertTrue(
                "The intent should contain the CATEGORY_BROWSABLE category.",
                intent.getCategories().contains(Intent.CATEGORY_BROWSABLE));
        assertTrue(
                "preferNew extra should be true.",
                intent.getBooleanExtra(IntentHandler.EXTRA_PREFER_NEW, false));
        assertEquals(
                "The intent should contain Uri data.",
                Uri.parse(JUnitTestGURLs.EXAMPLE_URL.getSpec()),
                intent.getData());
        assertFalse(
                "The intent should not contain the trusted application extra.",
                intent.hasExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA));
        assertEquals(
                "The UrlIntentSource extra should match.",
                UrlIntentSource.TAB_IN_STRIP,
                intent.getIntExtra(IntentHandler.EXTRA_URL_DRAG_SOURCE, UrlIntentSource.UNKNOWN));
    }

    @Test
    @Config(sdk = 29)
    public void testDragAndDropBrowserDelegate_createLinkIntent_PreR() {
        Intent intent =
                mDelegate.createUrlIntent(
                        JUnitTestGURLs.EXAMPLE_URL.getSpec(), UrlIntentSource.LINK);
        assertNull("The intent should be null on R- versions.", intent);
    }

    @Test
    @Config(sdk = 30)
    public void testBuildClipData() {
        MultiWindowTestUtils.enableMultiInstance();
        var dropData = createTabDropData(1, false);
        var data = mDelegate.buildClipData(dropData);
        assertEquals(
                "The browser clip data is not as expected",
                dropData.buildTabClipDataText(),
                data.getItemAt(0).getText());
        assertNull("The clip data should not have intent set.", data.getItemAt(0).getIntent());
        assertTrue(
                "The clip data should contain chrome/tab mimetype.",
                data.getDescription().hasMimeType(MimeTypeUtils.CHROME_MIMETYPE_TAB));
        assertTrue(
                "The clip data should contain chrome/link mimetype.",
                data.getDescription().hasMimeType(MimeTypeUtils.CHROME_MIMETYPE_LINK));
        assertTrue(
                "The clip data should contain text/plain mimetype.",
                data.getDescription().hasMimeType(ClipDescription.MIMETYPE_TEXT_PLAIN));
        assertTrue(
                "The clip data should contain text/vnd.android.intent mimetype.",
                data.getDescription().hasMimeType(ClipDescription.MIMETYPE_TEXT_INTENT));
    }

    @Test
    public void testBuildClipDataForTabDragWithItemFromBuilder() {
        testBuildClipDataForTabDragToCreateNewInstance(true);
    }

    @Test
    public void testBuildClipDataForTabDragWithNullItemFromBuilder() {
        testBuildClipDataForTabDragToCreateNewInstance(false);
    }

    @Test
    public void testBuildFlags_dropDataHasNoTab() {
        MultiWindowTestUtils.enableMultiInstance();
        int originalFlag = 0;
        var dropData = new ChromeDropDataAndroid.Builder().build();
        var flags = mDelegate.buildFlags(originalFlag, dropData);
        assertEquals("Original flag should not be modified.", originalFlag, flags);
    }

    @Test
    public void testBuildFlags_dropDataHasTabAndTabDragToCreateInstanceNotAllowed() {
        MultiWindowTestUtils.enableMultiInstance();
        int originalFlag = 0;
        var dropData = createTabDropData(1, false);
        var flags = mDelegate.buildFlags(originalFlag, dropData);
        assertEquals("Original flag should not be modified.", originalFlag, flags);
    }

    @Test
    public void testBuildFlags_dropDataHasTabAndTabDragToCreateInstanceAllowed() {
        MultiWindowTestUtils.enableMultiInstance();
        int originalFlag = 0;
        var dropData = createTabDropData(1, true);
        var flags = mDelegate.buildFlags(originalFlag, dropData);
        assertTrue(
                "Drag flags should contain DRAG_FLAG_GLOBAL_SAME_APPLICATION.",
                (flags & (1 << 12)) != 0);
        assertTrue(
                "Drag flags should contain DRAG_FLAG_START_INTENT_SENDER_ON_UNHANDLED_DRAG.",
                (flags & (1 << 13)) != 0);
    }

    private ChromeDropDataAndroid createTabDropData(
            int tabId, boolean allowDragToCreateNewInstance) {
        Tab tab = MockTab.createAndInitialize(tabId, mProfile);
        return new ChromeDropDataAndroid.Builder()
                .withTab(tab)
                .withAllowDragToCreateInstance(allowDragToCreateNewInstance)
                .build();
    }

    private void testBuildClipDataForTabDragToCreateNewInstance(boolean withItem) {
        MultiWindowTestUtils.enableMultiInstance();
        var tab = MockTab.createAndInitialize(1, mProfile);
        var dropData = createTabDropData(1, true);
        var item =
                withItem
                        ? new Item(
                                DragAndDropLauncherActivity.getTabIntent(
                                        mApplicationContext,
                                        tab,
                                        MultiWindowUtils.INVALID_INSTANCE_ID))
                        : null;
        ChromeDragAndDropBrowserDelegate.setClipDataItemWithPendingIntentForTesting(item);

        var data = mDelegate.buildClipData(dropData);
        assertNotNull("The clip data should have an intent set.", data.getItemAt(0).getIntent());
        assertEquals(
                "Tab id extra is incorrect.",
                tab.getId(),
                data.getItemAt(0)
                        .getIntent()
                        .getIntExtra(IntentHandler.EXTRA_DRAGGED_TAB_ID, Tab.INVALID_TAB_ID));
    }
}
