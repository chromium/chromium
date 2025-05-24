// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestUtils;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.ui.dragdrop.DragDropMetricUtils.UrlIntentSource;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

/** Unit tests for {@link DragAndDropLauncherActivity}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 31)
public class DragAndDropLauncherActivityUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public ExpectedException exception = ExpectedException.none();
    @Mock private Profile mProfile;

    private Context mContext;
    private String mLinkUrl;

    @Before
    public void setup() {
        MultiWindowTestUtils.enableMultiInstance();
        mContext = ContextUtils.getApplicationContext();
        mLinkUrl = JUnitTestGURLs.HTTP_URL.getSpec();
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);
    }

    @Test
    public void testGetLinkLauncherIntent_defaultWindowId() {
        Intent intent =
                DragAndDropLauncherActivity.getLinkLauncherIntent(
                        mContext,
                        mLinkUrl,
                        TabWindowManager.INVALID_WINDOW_ID,
                        UrlIntentSource.LINK);
        assertEquals(
                "The intent action should be DragAndDropLauncherActivity.ACTION_DRAG_DROP_VIEW.",
                DragAndDropLauncherActivity.ACTION_DRAG_DROP_VIEW,
                intent.getAction());
        assertNotNull(
                "The intent creation timestamp should be saved.",
                DragAndDropLauncherActivity.getIntentCreationTimestampMs());
        assertEquals(
                "The intent class should be DragAndDropLauncherActivity.",
                DragAndDropLauncherActivity.class.getName(),
                intent.getComponent().getClassName());
        assertTrue(
                "The intent should contain the CATEGORY_BROWSABLE category.",
                intent.getCategories().contains(Intent.CATEGORY_BROWSABLE));
        assertFalse(
                "Intent should not contain the EXTRA_WINDOW_ID.",
                intent.hasExtra(IntentHandler.EXTRA_WINDOW_ID));
    }

    @Test
    public void testGetLinkLauncherIntent_specificWindowId() {
        Intent intent =
                DragAndDropLauncherActivity.getLinkLauncherIntent(
                        mContext, mLinkUrl, 3, UrlIntentSource.LINK);
        assertTrue(
                "Intent should contain the EXTRA_WINDOW_ID.",
                intent.hasExtra(IntentHandler.EXTRA_WINDOW_ID));
        assertEquals(
                "The EXTRA_WINDOW_ID intent extra value should match.",
                3,
                intent.getIntExtra(IntentHandler.EXTRA_WINDOW_ID, -1));
        assertEquals(
                "The EXTRA_URL_SOURCE intent extra value should match.",
                UrlIntentSource.LINK,
                intent.getIntExtra(IntentHandler.EXTRA_URL_DRAG_SOURCE, UrlIntentSource.UNKNOWN));
    }

    @Test
    public void testGetTabIntent_specificWindowId() {
        testGetTabOrGroupIntent(/* isGroupDrag= */ false, /* destWindowId= */ 2);
    }

    @Test
    public void testGetTabIntent_defaultWindowId() {
        testGetTabOrGroupIntent(
                /* isGroupDrag= */ false, /* destWindowId= */ TabWindowManager.INVALID_WINDOW_ID);
    }

    @Test
    public void testGetTabGroupIntent_specificWindowId() {
        testGetTabOrGroupIntent(/* isGroupDrag= */ true, /* destWindowId= */ 2);
    }

    @Test
    public void testGetTabGroupIntent_defaultWindowId() {
        testGetTabOrGroupIntent(
                /* isGroupDrag= */ true, /* destWindowId= */ TabWindowManager.INVALID_WINDOW_ID);
    }

    @Test
    public void testIsIntentValid_invalidIntentAction() {
        Intent intent =
                DragAndDropLauncherActivity.getLinkLauncherIntent(
                        mContext,
                        mLinkUrl,
                        TabWindowManager.INVALID_WINDOW_ID,
                        UrlIntentSource.LINK);
        intent.setAction(Intent.ACTION_VIEW);
        exception.expect(AssertionError.class);
        exception.expectMessage("The intent action is invalid.");
        assertFalse(
                "The intent action is invalid.", DragAndDropLauncherActivity.isIntentValid(intent));
    }

    @Test
    public void testIsIntentValid_missingIntentCreationTimestamp() {
        Intent intent =
                DragAndDropLauncherActivity.getLinkLauncherIntent(
                        mContext,
                        mLinkUrl,
                        TabWindowManager.INVALID_WINDOW_ID,
                        UrlIntentSource.LINK);
        DragAndDropLauncherActivity.setIntentCreationTimestampMs(null);
        assertFalse(
                "The intent creation timestamp is missing.",
                DragAndDropLauncherActivity.isIntentValid(intent));
    }

    private void testGetTabOrGroupIntent(boolean isGroupDrag, int destWindowId) {
        Tab tab = MockTab.createAndInitialize(1, mProfile);
        ChromeDropDataAndroid dropData =
                isGroupDrag
                        ? createTabGroupDropData(/* allowDragToCreateNewInstance= */ true)
                        : createTabDropData(tab, /* allowDragToCreateNewInstance= */ true);
        int sourceWindowId = 1;
        Intent intent =
                DragAndDropLauncherActivity.buildTabOrGroupIntent(
                        dropData, mContext, sourceWindowId, destWindowId);
        assertEquals(
                "The intent action should be DragAndDropLauncherActivity.ACTION_DRAG_DROP_VIEW.",
                DragAndDropLauncherActivity.ACTION_DRAG_DROP_VIEW,
                intent.getAction());
        assertNotNull(
                "The intent creation timestamp should be saved.",
                DragAndDropLauncherActivity.getIntentCreationTimestampMs());
        assertEquals(
                "The intent class should be DragAndDropLauncherActivity.",
                DragAndDropLauncherActivity.class.getName(),
                intent.getComponent().getClassName());
        assertTrue(
                "The intent should contain the CATEGORY_BROWSABLE category.",
                intent.getCategories().contains(Intent.CATEGORY_BROWSABLE));
        assertEquals(
                "The EXTRA_DRAGDROP_TAB_WINDOW_ID intent extra value should match.",
                sourceWindowId,
                intent.getIntExtra(IntentHandler.EXTRA_DRAGDROP_TAB_WINDOW_ID, -1));
        if (destWindowId == TabWindowManager.INVALID_WINDOW_ID) {
            assertFalse(
                    "Intent should not contain the EXTRA_WINDOW_ID.",
                    intent.hasExtra(IntentHandler.EXTRA_WINDOW_ID));
        } else {
            assertEquals(
                    "The EXTRA_WINDOW_ID intent extra value should match.",
                    destWindowId,
                    intent.getIntExtra(IntentHandler.EXTRA_WINDOW_ID, -1));
        }
        if (isGroupDrag) {
            assertEquals(
                    "The EXTRA_URL_SOURCE intent extra value should match.",
                    UrlIntentSource.TAB_GROUP_IN_STRIP,
                    intent.getIntExtra(
                            IntentHandler.EXTRA_URL_DRAG_SOURCE, UrlIntentSource.UNKNOWN));
            assertEquals(
                    "The TabGroupMetadata intent extra value should match.",
                    buildTabGroupMetadata(),
                    IntentHandler.getTabGroupMetadata(intent));
        } else {
            assertEquals(
                    "The EXTRA_URL_SOURCE intent extra value should match.",
                    UrlIntentSource.TAB_IN_STRIP,
                    intent.getIntExtra(
                            IntentHandler.EXTRA_URL_DRAG_SOURCE, UrlIntentSource.UNKNOWN));
            assertEquals(
                    "The EXTRA_DRAGGED_TAB_ID intent extra value should match.",
                    tab.getId(),
                    intent.getIntExtra(IntentHandler.EXTRA_DRAGGED_TAB_ID, Tab.INVALID_TAB_ID));
            assertEquals(
                    "The intent data value should match.",
                    Uri.parse(tab.getUrl().getSpec()),
                    intent.getData());
        }
    }

    private ChromeDropDataAndroid createTabDropData(Tab tab, boolean allowDragToCreateNewInstance) {
        return new ChromeTabDropDataAndroid.Builder()
                .withTab(tab)
                .withAllowDragToCreateInstance(allowDragToCreateNewInstance)
                .build();
    }

    private ChromeDropDataAndroid createTabGroupDropData(boolean allowDragToCreateNewInstance) {
        return new ChromeTabGroupDropDataAndroid.Builder()
                .withTabGroupMetadata(buildTabGroupMetadata())
                .withAllowDragToCreateInstance(allowDragToCreateNewInstance)
                .build();
    }

    private TabGroupMetadata buildTabGroupMetadata() {
        Token tabGroupId = new Token(2L, 2L);
        String tabGroupTitle = "Regrouped tabs";
        int rootId = 1;
        ArrayList<Entry<Integer, String>> tabIdsToUrls =
                new ArrayList<>(
                        List.of(
                                Map.entry(1, "https://www.amazon.com/"),
                                Map.entry(2, "https://www.youtube.com/"),
                                Map.entry(3, "https://www.facebook.com/")));

        TabGroupMetadata tabGroupMetadata =
                new TabGroupMetadata(
                        rootId,
                        /* selectedTabId= */ rootId,
                        /* sourceWindowId= */ TabWindowManager.INVALID_WINDOW_ID,
                        tabGroupId,
                        tabIdsToUrls,
                        /* tabGroupColor= */ 0,
                        tabGroupTitle,
                        /* mhtmlTabTitle= */ null,
                        /* tabGroupCollapsed= */ false,
                        /* isGroupShared= */ false,
                        /* isIncognito= */ false);
        return tabGroupMetadata;
    }
}
