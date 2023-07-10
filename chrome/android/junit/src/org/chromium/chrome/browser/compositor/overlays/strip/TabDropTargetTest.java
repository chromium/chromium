// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;

import android.app.Activity;
import android.content.ClipData;
import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.core.view.ContentInfoCompat;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;

/** Tests for {@link TabDropTarget}. */
@RunWith(BaseRobolectricTestRunner.class)
// clang-format off
@Features.EnableFeatures({ChromeFeatureList.TAB_DRAG_DROP_ANDROID})
public class TabDropTargetTest {
    // clang-format on
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Mock
    private MultiInstanceManager mMultiInstanceManager;

    private TabDropTarget mTabDropTarget;
    private Activity mActivity;
    private Context mContext;
    private TabDragSource mTabDragSource;
    private View mTabsToolbarViewDragSource;
    private View mTabsToolbarViewDropTarget;
    private Tab mTabBeingDragged;
    private int mHashCodeDragSource;
    private ContentInfoCompat mContentInfoCompatPayload;

    /** Resets the environment before each test. */
    @Before
    public void beforeTest() {
        MockitoAnnotations.initMocks(this);
        mContext = new ContextThemeWrapper(
                ApplicationProvider.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        mActivity = Robolectric.setupActivity(Activity.class);
        mActivity.setTheme(org.chromium.chrome.R.style.Theme_BrowserUI);

        // Create both the TabDragSource and TabDropTarget.
        mTabDragSource = TabDragSource.getInstance();
        mTabDropTarget = new TabDropTarget();

        // Create two tab views - source and destination.
        mTabsToolbarViewDragSource = new View(mActivity);
        mTabsToolbarViewDragSource.setLayoutParams(new MarginLayoutParams(150, 50));
        mHashCodeDragSource = System.identityHashCode(mTabsToolbarViewDragSource);
        mTabsToolbarViewDropTarget = new View(mActivity);
        mTabsToolbarViewDropTarget.setLayoutParams(new MarginLayoutParams(300, 50));

        // Mock the tab to be dragged.
        mTabBeingDragged = MockTab.createAndInitialize(5, false);

        // Create drop payload from the drag source view.
        mContentInfoCompatPayload =
                new ContentInfoCompat
                        .Builder(createClipData(mTabsToolbarViewDragSource, mTabBeingDragged),
                                ContentInfoCompat.SOURCE_DRAG_AND_DROP)
                        .build();
    }

    @After
    public void tearDown() {
        mContentInfoCompatPayload = null;
        mHashCodeDragSource = 0;
        mTabsToolbarViewDropTarget = null;
        mTabsToolbarViewDragSource = null;
        mTabDropTarget = null;
        mTabDragSource = null;
    }

    private ClipData createClipData(View view, Tab tab) {
        String clipData = mTabDragSource.getClipDataInfo(tab);
        ClipData.Item item = new ClipData.Item((CharSequence) clipData);
        ClipData dragData =
                new ClipData((CharSequence) view.getTag(), TabDragSource.SUPPORTED_MIMETYPES, item);
        return dragData;
    }

    /**
     * Tests the method {@link TabDragSource.DropContentReceiver#onReceiveContent()}.
     *
     * Checks that it successfully accepts a drop of a payload on different tabs layout view.
     */
    @Test
    public void test_TabDropAction_onDifferentTabsLayout_ReturnsSuccess() {
        mTabDragSource.setTabBeingDragged(mTabBeingDragged);
        mTabDragSource.setDragSourceTabsToolbarHashCode(mHashCodeDragSource);
        mTabDragSource.setAcceptNextDrop(true);
        mTabDragSource.setMultiInstanceManager(mMultiInstanceManager);
        Mockito.doNothing()
                .when(mMultiInstanceManager)
                .moveTabToWindow(any(), eq(mTabBeingDragged));

        // Perform action of a simulated drop of ClipData payload on drop target view.
        ContentInfoCompat remainingPayload =
                mTabDropTarget.getDropContentReceiver().onReceiveContent(
                        mTabsToolbarViewDropTarget, mContentInfoCompatPayload);

        // Verify the ClipData dropped was consumed.
        assertTrue("Dropped payload should be consumed.",
                remainingPayload != mContentInfoCompatPayload);
        assertTrue("Remaining payload should be null.", remainingPayload == null);
    }

    /**
     * Tests the method {@link TabDragSource.DropContentReceiver#onReceiveContent()}.
     *
     * Checks that it successfully rejects the drop of payload on same tab layout view.
     */
    @Test
    public void test_TabDropAction_onSameTabsLayout_ReturnsFalse() {
        mTabDragSource.setTabBeingDragged(mTabBeingDragged);
        mTabDragSource.setDragSourceTabsToolbarHashCode(mHashCodeDragSource);
        mTabDragSource.setAcceptNextDrop(true);

        // Perform action of a simulated drop of ClipData payload on drag source view.
        ContentInfoCompat remainingPayload =
                mTabDropTarget.getDropContentReceiver().onReceiveContent(
                        mTabsToolbarViewDragSource, mContentInfoCompatPayload);

        // Verify the drop was not consumed, the original payload was returned.
        assertFalse("Dropped payload should NOT be consumed.",
                remainingPayload != mContentInfoCompatPayload);
        assertFalse("Remaining payload should NOT be null.", remainingPayload == null);
    }

    /**
     * Tests the method {@link TabDragSource.DropContentReceiver#onReceiveContent()}.
     *
     * Checks that it rejects a ClipData drop of a payload with the tabId that does not match with
     * the saved Tab info even if the source and destination windows are different.
     */
    @Test
    public void test_TabDropAction_onDifferentTabsLayout_WithBadClipDataDrop_ReturnsFailure() {
        mTabDragSource.setTabBeingDragged(mTabBeingDragged);
        mTabDragSource.setDragSourceTabsToolbarHashCode(mHashCodeDragSource);
        mTabDragSource.setAcceptNextDrop(true);
        mTabDragSource.setMultiInstanceManager(mMultiInstanceManager);
        Mockito.doNothing()
                .when(mMultiInstanceManager)
                .moveTabToWindow(any(), eq(mTabBeingDragged));

        // Create ClipData for non-Chrome apps with invalid tab id.
        Tab tabForBadClipData = MockTab.createAndInitialize(555, false);
        ContentInfoCompat compactPayloadWithBadClipData =
                new ContentInfoCompat
                        .Builder(createClipData(mTabsToolbarViewDragSource, tabForBadClipData),
                                ContentInfoCompat.SOURCE_DRAG_AND_DROP)
                        .build();

        // Perform action of a simulated drop of ClipData payload on drop target view.
        ContentInfoCompat remainingPayload =
                mTabDropTarget.getDropContentReceiver().onReceiveContent(
                        mTabsToolbarViewDropTarget, compactPayloadWithBadClipData);

        // Verify the ClipData dropped was rejected.
        assertFalse("Dropped bad payload should be returned.",
                remainingPayload != compactPayloadWithBadClipData);
        assertFalse("Remaining payload should NOT be null.", remainingPayload == null);
    }
}
