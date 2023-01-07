// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.hamcrest.Matchers.contains;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.view.ViewParent;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.xsurface.ListContentManagerObserver;
import org.chromium.chrome.browser.xsurface.LoggingParameters;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;

/** Unit tests for {@link NtpListContentManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpListContentManagerTest implements ListContentManagerObserver {
    private NtpListContentManager mManager;
    private Context mContext;
    private LinearLayout mParent;

    private boolean mItemRangeInserted;
    private int mItemRangeInsertedStartIndex;
    private int mItemRangeInsertedCount;
    private boolean mItemRangeRemoved;
    private int mItemRangeRemovedStartIndex;
    private int mItemRangeRemovedCount;
    private boolean mItemRangeChanged;
    private int mItemRangeChangedStartIndex;
    private int mItemRangeChangedCount;
    private boolean mItemMoved;
    private int mItemMovedCurIndex;
    private int mItemMovedNewIndex;
    private String mObservedChanges = "";
    private final FeedLoggingParameters mLoggingParametersA = new FeedLoggingParameters(
            "instance-id", "A", /*loggingEnabled=*/true, /*viewActionsEnabled=*/true, null);
    private final FeedLoggingParameters mLoggingParametersB = new FeedLoggingParameters(
            "instance-id", "B", /*loggingEnabled=*/true, /*viewActionsEnabled=*/true, null);

    @Before
    public void setUp() {
        mContext = Robolectric.buildActivity(Activity.class).get();
        mParent = new LinearLayout(mContext);
        mManager = new NtpListContentManager();
        mManager.addObserver(this);
    }

    @Test
    @SmallTest
    public void testFindContentPositionByKey() {
        NtpListContentManager.FeedContent c1 = createExternalViewContent("a");
        NtpListContentManager.FeedContent c2 = createExternalViewContent("b");
        NtpListContentManager.FeedContent c3 = createExternalViewContent("c");

        addContents(0, Arrays.asList(new NtpListContentManager.FeedContent[] {c1, c2, c3}));
        assertEquals(3, mManager.getItemCount());

        assertEquals(0, mManager.findContentPositionByKey("a"));
        assertEquals(1, mManager.findContentPositionByKey("b"));
        assertEquals(2, mManager.findContentPositionByKey("c"));
    }

    @Test
    @SmallTest
    public void testAddContents() {
        NtpListContentManager.FeedContent c1 = createExternalViewContent("a");
        NtpListContentManager.FeedContent c2 = createExternalViewContent("b");
        NtpListContentManager.FeedContent c3 = createExternalViewContent("c");

        addContents(0, Arrays.asList(new NtpListContentManager.FeedContent[] {c1}));
        assertEquals(1, mManager.getItemCount());
        assertEquals(c1, mManager.getContent(0));

        addContents(0, Arrays.asList(new NtpListContentManager.FeedContent[] {c2, c3}));
        assertEquals(3, mManager.getItemCount());
        assertEquals(c2, mManager.getContent(0));
        assertEquals(c3, mManager.getContent(1));
        assertEquals(c1, mManager.getContent(2));

        addContents(3, Arrays.asList(new NtpListContentManager.FeedContent[] {c2, c3}));
        assertEquals(5, mManager.getItemCount());
        assertEquals(c2, mManager.getContent(0));
        assertEquals(c3, mManager.getContent(1));
        assertEquals(c1, mManager.getContent(2));
        assertEquals(c2, mManager.getContent(3));
        assertEquals(c3, mManager.getContent(4));
    }

    @Test
    @SmallTest
    public void testRemoveContents() {
        NtpListContentManager.FeedContent c1 = createExternalViewContent("a");
        NtpListContentManager.FeedContent c2 = createExternalViewContent("b");
        NtpListContentManager.FeedContent c3 = createExternalViewContent("c");
        NtpListContentManager.FeedContent c4 = createExternalViewContent("d");
        NtpListContentManager.FeedContent c5 = createExternalViewContent("e");

        addContents(0, Arrays.asList(new NtpListContentManager.FeedContent[] {c1, c2, c3, c4, c5}));
        assertEquals(5, mManager.getItemCount());

        removeContents(0, 2);
        assertEquals(3, mManager.getItemCount());
        assertEquals(c3, mManager.getContent(0));
        assertEquals(c4, mManager.getContent(1));
        assertEquals(c5, mManager.getContent(2));

        removeContents(1, 2);
        assertEquals(1, mManager.getItemCount());
        assertEquals(c3, mManager.getContent(0));

        removeContents(0, 1);
        assertEquals(0, mManager.getItemCount());
    }

    @Test
    @SmallTest
    public void testUpdateContents() {
        NtpListContentManager.FeedContent c1 = createExternalViewContent("a");
        NtpListContentManager.FeedContent c2 = createExternalViewContent("b");
        NtpListContentManager.FeedContent c3 = createExternalViewContent("c");
        NtpListContentManager.FeedContent c4 = createExternalViewContent("d");
        NtpListContentManager.FeedContent c5 = createExternalViewContent("e");

        addContents(0, Arrays.asList(new NtpListContentManager.FeedContent[] {c1, c2, c3}));
        assertEquals(3, mManager.getItemCount());

        updateContents(1, Arrays.asList(new NtpListContentManager.FeedContent[] {c4, c5}));
        assertEquals(3, mManager.getItemCount());
        assertEquals(c1, mManager.getContent(0));
        assertEquals(c4, mManager.getContent(1));
        assertEquals(c5, mManager.getContent(2));
    }

    @Test
    @SmallTest
    public void testMoveContent() {
        NtpListContentManager.FeedContent c1 = createExternalViewContent("a");
        NtpListContentManager.FeedContent c2 = createExternalViewContent("b");
        NtpListContentManager.FeedContent c3 = createExternalViewContent("c");
        NtpListContentManager.FeedContent c4 = createExternalViewContent("d");
        NtpListContentManager.FeedContent c5 = createExternalViewContent("e");

        addContents(0, Arrays.asList(new NtpListContentManager.FeedContent[] {c1, c2, c3, c4, c5}));
        assertEquals(5, mManager.getItemCount());

        moveContent(0, 3);
        assertEquals(5, mManager.getItemCount());
        assertEquals(c2, mManager.getContent(0));
        assertEquals(c3, mManager.getContent(1));
        assertEquals(c4, mManager.getContent(2));
        assertEquals(c1, mManager.getContent(3));
        assertEquals(c5, mManager.getContent(4));

        moveContent(4, 2);
        assertEquals(5, mManager.getItemCount());
        assertEquals(c2, mManager.getContent(0));
        assertEquals(c3, mManager.getContent(1));
        assertEquals(c5, mManager.getContent(2));
        assertEquals(c4, mManager.getContent(3));
        assertEquals(c1, mManager.getContent(4));
    }

    @Test
    @SmallTest
    public void testGetViewData() {
        NtpListContentManager.FeedContent c1 = createExternalViewContent("foo");
        View v2 = new View(mContext);
        NtpListContentManager.FeedContent c2 = createNativeViewContent(v2);
        View v3 = new View(mContext);
        NtpListContentManager.FeedContent c3 = createNativeViewContent(v3);
        NtpListContentManager.FeedContent c4 = createExternalViewContent("hello");
        View v5 = new View(mContext);
        NtpListContentManager.FeedContent c5 = createNativeViewContent(v5);

        addContents(0, Arrays.asList(new NtpListContentManager.FeedContent[] {c1, c2, c3, c4, c5}));
        assertEquals(5, mManager.getItemCount());

        assertFalse(mManager.isNativeView(0));
        assertTrue(mManager.isNativeView(1));
        assertTrue(mManager.isNativeView(2));
        assertFalse(mManager.isNativeView(3));
        assertTrue(mManager.isNativeView(4));

        assertFalse(mManager.isFullSpan(0));
        assertTrue(mManager.isFullSpan(1));
        assertTrue(mManager.isFullSpan(2));
        assertFalse(mManager.isFullSpan(3));
        assertTrue(mManager.isFullSpan(4));

        assertArrayEquals("foo".getBytes(), mManager.getExternalViewBytes(0));
        assertEquals(v2, getNativeView(mManager.getViewType(1)));
        assertEquals(v3, getNativeView(mManager.getViewType(2)));
        assertArrayEquals("hello".getBytes(), mManager.getExternalViewBytes(3));
        assertEquals(v5, getNativeView(mManager.getViewType(4)));
    }

    @Test
    @SmallTest
    public void testGetViewDataCreatesEnclosingViewOnce() {
        View v = new View(mContext);
        NtpListContentManager.FeedContent c = createNativeViewContent(v);

        addContents(0, Arrays.asList(new NtpListContentManager.FeedContent[] {c}));

        assertEquals(v, getNativeView(mManager.getViewType(0)));
        ViewParent p = v.getParent();

        // Remove and re-add the same view, but with a new NativeViewContent.
        // This time, getNativeView() creates a new enclosing parent view.
        removeContents(0, 1);
        c = createNativeViewContent(v);
        addContents(0, Arrays.asList(new NtpListContentManager.FeedContent[] {c}));
        assertEquals(v, getNativeView(mManager.getViewType(0)));
        assertNotEquals(p, v.getParent());
    }

    @Test
    @SmallTest
    public void testGetContextValuesReturnsLoggingParameters() {
        addContents(0,
                Arrays.asList(new NtpListContentManager.FeedContent[] {
                        createExternalViewContent("A", mLoggingParametersA),
                        createExternalViewContent("B", mLoggingParametersB)}));

        LoggingParameters parameters1 =
                (LoggingParameters) mManager.getContextValues(0).get(LoggingParameters.KEY);
        LoggingParameters parameters2 =
                (LoggingParameters) mManager.getContextValues(1).get(LoggingParameters.KEY);
        assertEquals(parameters1, mLoggingParametersA);
        assertEquals(parameters2, mLoggingParametersB);
    }

    @Test
    @SmallTest
    public void testGetContextValues_SetHandlersAfterAddingContent() {
        addContents(0,
                Arrays.asList(new NtpListContentManager.FeedContent[] {
                        createExternalViewContent("A", mLoggingParametersA)}));
        mManager.setHandlers(Map.of("HKEY1", "someHandler"));

        assertEquals(Map.of("HKEY1", "someHandler", LoggingParameters.KEY, mLoggingParametersA),
                mManager.getContextValues(0));
    }

    @Test
    @SmallTest
    public void testGetContextValues_SetHandlersBeforeAddingContent() {
        mManager.setHandlers(Map.of("HKEY1", "someHandler"));
        addContents(0,
                Arrays.asList(new NtpListContentManager.FeedContent[] {
                        createExternalViewContent("A", mLoggingParametersA)}));

        assertEquals(Map.of("HKEY1", "someHandler", LoggingParameters.KEY, mLoggingParametersA),
                mManager.getContextValues(0));
    }

    @Test
    @SmallTest
    public void testGetNativeViewAfterMove() {
        View v1 = new View(mContext);
        NtpListContentManager.FeedContent c1 = createNativeViewContent(v1);
        View v0 = new View(mContext);
        NtpListContentManager.FeedContent c0 = createNativeViewContent(v0);

        addContents(0, Arrays.asList(new NtpListContentManager.FeedContent[] {c1}));
        int t1 = mManager.getViewType(0);
        addContents(0, Arrays.asList(new NtpListContentManager.FeedContent[] {c0}));
        int t0 = mManager.getViewType(0);

        assertEquals(1, t1);
        assertEquals(2, t0);

        assertEquals(t0, mManager.getViewType(0));
        assertEquals(t1, mManager.getViewType(1));
        assertEquals(v0, getNativeView(t0));
        assertEquals(v1, getNativeView(t1));
    }

    @Test
    @SmallTest
    public void testReplaceRange_Empty() {
        boolean changed = mManager.replaceRange(
                0, 0, Arrays.asList(new NtpListContentManager.FeedContent[] {}));

        assertThat(getContentKeys(), Matchers.empty());
        assertFalse(changed);
        assertEquals("", mObservedChanges);
    }

    @Test
    @SmallTest
    public void testReplaceRange_twoWhileEmpty() {
        NtpListContentManager.FeedContent c1 = createExternalViewContent("a");
        NtpListContentManager.FeedContent c2 = createExternalViewContent("b");
        boolean changed = mManager.replaceRange(
                0, 0, Arrays.asList(new NtpListContentManager.FeedContent[] {c1, c2}));

        assertThat(getContentKeys(), contains("a", "b"));
        assertTrue(changed);
        assertEquals("rangeInserted index=0 count=2", mObservedChanges);
    }

    @Test
    @SmallTest
    public void testReplaceRange_twoInMiddle() {
        NtpListContentManager.FeedContent c1 = createExternalViewContent("a");
        NtpListContentManager.FeedContent c2 = createExternalViewContent("b");
        NtpListContentManager.FeedContent c3 = createExternalViewContent("c");
        NtpListContentManager.FeedContent c4 = createExternalViewContent("d");
        NtpListContentManager.FeedContent c5 = createExternalViewContent("e");
        mManager.replaceRange(
                0, 0, Arrays.asList(new NtpListContentManager.FeedContent[] {c1, c2, c3}));
        mObservedChanges = "";

        boolean changed = mManager.replaceRange(
                1, 1, Arrays.asList(new NtpListContentManager.FeedContent[] {c4, c5}));

        assertThat(getContentKeys(), contains("a", "d", "e", "c"));
        assertTrue(changed);
        assertEquals("rangeRemoved index=1 count=1"
                        + "\nrangeInserted index=1 count=2",
                mObservedChanges);
    }

    @Test
    @SmallTest
    public void testReplaceRange_twoAtEnd() {
        NtpListContentManager.FeedContent c1 = createExternalViewContent("a");
        NtpListContentManager.FeedContent c2 = createExternalViewContent("b");
        NtpListContentManager.FeedContent c3 = createExternalViewContent("c");
        NtpListContentManager.FeedContent c4 = createExternalViewContent("d");
        NtpListContentManager.FeedContent c5 = createExternalViewContent("e");
        mManager.replaceRange(
                0, 0, Arrays.asList(new NtpListContentManager.FeedContent[] {c1, c2, c3}));
        mObservedChanges = "";

        boolean changed = mManager.replaceRange(
                2, 1, Arrays.asList(new NtpListContentManager.FeedContent[] {c4, c5}));

        assertThat(getContentKeys(), contains("a", "b", "d", "e"));
        assertTrue(changed);
        assertEquals("rangeRemoved index=2 count=1"
                        + "\nrangeInserted index=2 count=2",
                mObservedChanges);
    }

    @Test
    @SmallTest
    public void testReplaceRange_twoAtStart() {
        NtpListContentManager.FeedContent c1 = createExternalViewContent("a");
        NtpListContentManager.FeedContent c2 = createExternalViewContent("b");
        NtpListContentManager.FeedContent c3 = createExternalViewContent("c");
        NtpListContentManager.FeedContent c4 = createExternalViewContent("d");
        NtpListContentManager.FeedContent c5 = createExternalViewContent("e");
        mManager.replaceRange(
                0, 0, Arrays.asList(new NtpListContentManager.FeedContent[] {c1, c2, c3}));
        mObservedChanges = "";

        boolean changed = mManager.replaceRange(
                0, 1, Arrays.asList(new NtpListContentManager.FeedContent[] {c4, c5}));

        assertThat(getContentKeys(), contains("d", "e", "b", "c"));
        assertTrue(changed);
        assertEquals("rangeRemoved index=0 count=1"
                        + "\nrangeInserted index=0 count=2",
                mObservedChanges);
    }

    @Test
    @SmallTest
    public void testReplaceRange_moveFirstToLast() {
        NtpListContentManager.FeedContent c1 = createExternalViewContent("a");
        NtpListContentManager.FeedContent c2 = createExternalViewContent("b");
        NtpListContentManager.FeedContent c3 = createExternalViewContent("c");
        mManager.replaceRange(
                0, 0, Arrays.asList(new NtpListContentManager.FeedContent[] {c1, c2, c3}));
        mObservedChanges = "";

        boolean changed = mManager.replaceRange(
                0, 3, Arrays.asList(new NtpListContentManager.FeedContent[] {c2, c3, c1}));

        assertTrue(changed);
        assertThat(getContentKeys(), contains("b", "c", "a"));
        assertEquals("itemMoved from=1 to=0\nitemMoved from=2 to=1", mObservedChanges);
    }

    @Test
    @SmallTest
    public void testReplaceRange_reverseOrder() {
        NtpListContentManager.FeedContent c1 = createExternalViewContent("a");
        NtpListContentManager.FeedContent c2 = createExternalViewContent("b");
        NtpListContentManager.FeedContent c3 = createExternalViewContent("c");
        mManager.replaceRange(
                0, 0, Arrays.asList(new NtpListContentManager.FeedContent[] {c1, c2, c3}));
        mObservedChanges = "";

        boolean changed = mManager.replaceRange(
                0, 3, Arrays.asList(new NtpListContentManager.FeedContent[] {c3, c2, c1}));

        assertTrue(changed);
        assertThat(getContentKeys(), contains("c", "b", "a"));
        assertEquals("itemMoved from=2 to=0\nitemMoved from=2 to=1", mObservedChanges);
    }

    @Test
    @SmallTest
    public void testReplaceRange_removeAll() {
        NtpListContentManager.FeedContent c1 = createExternalViewContent("a");
        NtpListContentManager.FeedContent c2 = createExternalViewContent("b");
        NtpListContentManager.FeedContent c3 = createExternalViewContent("c");
        mManager.replaceRange(
                0, 0, Arrays.asList(new NtpListContentManager.FeedContent[] {c1, c2, c3}));
        mObservedChanges = "";

        boolean changed = mManager.replaceRange(
                0, 3, Arrays.asList(new NtpListContentManager.FeedContent[] {}));

        assertTrue(changed);
        assertThat(getContentKeys(), Matchers.empty());
        assertEquals("rangeRemoved index=0 count=3", mObservedChanges);
    }

    @Test
    @SmallTest
    public void testReplaceRange_complexUpdate() {
        NtpListContentManager.FeedContent a = createExternalViewContent("a");
        NtpListContentManager.FeedContent b = createExternalViewContent("b");
        NtpListContentManager.FeedContent c = createExternalViewContent("c");
        NtpListContentManager.FeedContent d = createExternalViewContent("d");
        NtpListContentManager.FeedContent e = createExternalViewContent("e");
        NtpListContentManager.FeedContent f = createExternalViewContent("f");
        NtpListContentManager.FeedContent g = createExternalViewContent("g");
        NtpListContentManager.FeedContent h = createExternalViewContent("h");
        NtpListContentManager.FeedContent i = createExternalViewContent("i");
        mManager.replaceRange(
                0, 0, Arrays.asList(new NtpListContentManager.FeedContent[] {a, b, c, d, e}));
        mObservedChanges = "";

        boolean changed = mManager.replaceRange(
                0, 5, Arrays.asList(new NtpListContentManager.FeedContent[] {f, g, a, h, c, e, i}));

        assertTrue(changed);
        assertThat(getContentKeys(), contains("f", "g", "a", "h", "c", "e", "i"));
        assertEquals("rangeRemoved index=3 count=1"
                        + "\nrangeRemoved index=1 count=1"
                        + "\nrangeInserted index=0 count=2"
                        + "\nrangeInserted index=3 count=1"
                        + "\nrangeInserted index=6 count=1",
                mObservedChanges);
    }

    @Override
    public void onItemRangeInserted(int startIndex, int count) {
        logChange("rangeInserted index=" + String.valueOf(startIndex)
                + " count=" + String.valueOf(count));
        mItemRangeInserted = true;
        mItemRangeInsertedStartIndex = startIndex;
        mItemRangeInsertedCount = count;
    }

    @Override
    public void onItemRangeRemoved(int startIndex, int count) {
        logChange("rangeRemoved index=" + String.valueOf(startIndex)
                + " count=" + String.valueOf(count));
        mItemRangeRemoved = true;
        mItemRangeRemovedStartIndex = startIndex;
        mItemRangeRemovedCount = count;
    }

    @Override
    public void onItemRangeChanged(int startIndex, int count) {
        logChange("rangeChanged index=" + String.valueOf(startIndex)
                + " count=" + String.valueOf(count));
        mItemRangeChanged = true;
        mItemRangeChangedStartIndex = startIndex;
        mItemRangeChangedCount = count;
    }

    @Override
    public void onItemMoved(int curIndex, int newIndex) {
        logChange("itemMoved from=" + String.valueOf(curIndex) + " to=" + String.valueOf(newIndex));
        mItemMoved = true;
        mItemMovedCurIndex = curIndex;
        mItemMovedNewIndex = newIndex;
    }

    private void addContents(int index, List<NtpListContentManager.FeedContent> contents) {
        mItemRangeInserted = false;
        mItemRangeInsertedStartIndex = -1;
        mItemRangeInsertedCount = -1;
        mManager.addContents(index, contents);
        assertTrue(mItemRangeInserted);
        assertEquals(index, mItemRangeInsertedStartIndex);
        assertEquals(contents.size(), mItemRangeInsertedCount);
    }

    private void removeContents(int index, int count) {
        mItemRangeRemoved = false;
        mItemRangeRemovedStartIndex = -1;
        mItemRangeRemovedCount = -1;
        mManager.removeContents(index, count);
        assertTrue(mItemRangeRemoved);
        assertEquals(index, mItemRangeRemovedStartIndex);
        assertEquals(count, mItemRangeRemovedCount);
    }

    private void updateContents(int index, List<NtpListContentManager.FeedContent> contents) {
        mItemRangeChanged = false;
        mItemRangeChangedStartIndex = -1;
        mItemRangeChangedCount = -1;
        mManager.updateContents(index, contents);
        assertTrue(mItemRangeChanged);
        assertEquals(index, mItemRangeChangedStartIndex);
        assertEquals(contents.size(), mItemRangeChangedCount);
    }

    private void moveContent(int curIndex, int newIndex) {
        mItemMoved = false;
        mItemMovedCurIndex = -1;
        mItemMovedNewIndex = -1;
        mManager.moveContent(curIndex, newIndex);
        assertTrue(mItemMoved);
        assertEquals(curIndex, mItemMovedCurIndex);
        assertEquals(newIndex, mItemMovedNewIndex);
    }

    private void logChange(String change) {
        if (!mObservedChanges.isEmpty()) {
            mObservedChanges += "\n";
        }
        mObservedChanges += change;
    }

    private NtpListContentManager.FeedContent createExternalViewContent(String s) {
        return createExternalViewContent(s, mLoggingParametersA);
    }

    private NtpListContentManager.FeedContent createExternalViewContent(
            String s, FeedLoggingParameters loggingParameters) {
        return new NtpListContentManager.ExternalViewContent(s, s.getBytes(), loggingParameters);
    }

    private NtpListContentManager.FeedContent createNativeViewContent(View v) {
        return new NtpListContentManager.NativeViewContent(123, v.toString(), v);
    }

    private View getNativeView(int viewType) {
        View view = mManager.getNativeView(viewType, mParent);
        assertNotNull(view);
        assertTrue(view instanceof FrameLayout);
        return ((FrameLayout) view).getChildAt(0);
    }
    private List<String> getContentKeys() {
        List<String> result = new ArrayList<>();
        for (NtpListContentManager.FeedContent content : mManager.getContentList()) {
            result.add(content.getKey());
        }
        return result;
    }
}
