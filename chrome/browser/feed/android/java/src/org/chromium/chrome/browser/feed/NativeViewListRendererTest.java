// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.instanceOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.appcompat.widget.AppCompatTextView;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.xsurface.ListLayoutHelper;

import java.util.Arrays;

/** Unit tests for {@link NativeViewListRenderer}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NativeViewListRendererTest {
    static class DummyViewGroup extends ViewGroup {
        DummyViewGroup(Context context) {
            super(context);
        }

        @Override
        protected void onLayout(boolean changed, int left, int top, int right, int bottom) {}
    }

    private FeedListContentManager mManager;
    private Context mContext;
    private NativeViewListRenderer mRenderer;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mContext = Robolectric.buildActivity(Activity.class).get();

        // Note: this behaves both like a mock and a real object.
        // Methods calls can be mocked or tracked to validate class behavior.
        mManager =
                Mockito.mock(
                        FeedListContentManager.class,
                        Mockito.withSettings()
                                .useConstructor()
                                .defaultAnswer(Mockito.CALLS_REAL_METHODS));
        mRenderer =
                Mockito.mock(
                        NativeViewListRenderer.class,
                        Mockito.withSettings()
                                .useConstructor(mContext)
                                .defaultAnswer(Mockito.CALLS_REAL_METHODS));
    }

    @Test
    @SmallTest
    public void testBind_ReturninhgRecyclerView() {
        assertThat(mRenderer.bind(mManager), instanceOf(RecyclerView.class));
    }

    @Test
    @SmallTest
    public void testOnCreateViewHolder() {
        mManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            createContent("1"), createContent("2"), createContent("3")
                        }));
        mRenderer.bind(mManager);
        NativeViewListRenderer.ViewHolder viewHolder =
                mRenderer.onCreateViewHolder(
                        new DummyViewGroup(mContext), mRenderer.getItemViewType(1));
        assertThat(viewHolder.itemView, instanceOf(FrameLayout.class));
        FrameLayout frameLayout = (FrameLayout) viewHolder.itemView;
        assertThat(frameLayout.getChildAt(0), instanceOf(TextView.class));
        assertEquals("2", ((TextView) frameLayout.getChildAt(0)).getText());
    }

    @Test
    @SmallTest
    public void testOnBindViewHolder() {
        mManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            createContent("1"), createContent("2"), createContent("3")
                        }));
        mRenderer.bind(mManager);
        NativeViewListRenderer.ViewHolder viewHolder =
                mRenderer.onCreateViewHolder(
                        new DummyViewGroup(mContext), mRenderer.getItemViewType(2));
        mRenderer.onBindViewHolder(viewHolder, 2);
        verify(mManager, times(1)).bindNativeView(2, viewHolder.itemView);
    }

    @Test
    @SmallTest
    public void testUnbind() {
        mManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            createContent("1"), createContent("2"), createContent("3")
                        }));
        RecyclerView view = (RecyclerView) mRenderer.bind(mManager);
        assertNotNull(view.getAdapter());
        assertNotNull(view.getLayoutManager());

        mRenderer.unbind();
        assertNull(view.getAdapter());
        assertNull(view.getLayoutManager());
        verify(mRenderer, times(1)).onItemRangeRemoved(0, 3);
    }

    @Test
    public void testObserver_itemsAddedOnBind() {
        mManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            createContent("1"), createContent("2"), createContent("3")
                        }));
        mRenderer.bind(mManager);
        verify(mRenderer, times(1)).onItemRangeInserted(0, 3);
    }

    @Test
    public void testObserver_itemsAddedLater() {
        mRenderer.bind(mManager);
        mManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            createContent("1"), createContent("2"), createContent("3")
                        }));
        verify(mRenderer, times(1)).onItemRangeInserted(0, 3);
    }

    @Test
    public void testObserver_itemsRemoved() {
        mManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            createContent("1"), createContent("2"), createContent("3")
                        }));
        mRenderer.bind(mManager);

        mManager.removeContents(1, 2);
        verify(mRenderer, times(1)).onItemRangeRemoved(1, 2);
    }

    @Test
    public void testObserver_itemsRemovedOnUnbind() {
        mManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            createContent("1"), createContent("2"), createContent("3")
                        }));
        mRenderer.bind(mManager);

        mRenderer.unbind();
        verify(mRenderer, times(1)).onItemRangeRemoved(0, 3);
    }

    @Test
    public void testObserver_itemsUpdated() {
        mManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            createContent("1"), createContent("2"), createContent("3")
                        }));
        mRenderer.bind(mManager);

        mManager.updateContents(
                1,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            createContent("a"), createContent("b")
                        }));
        verify(mRenderer, times(1)).onItemRangeChanged(1, 2);
    }

    @Test
    public void testObserver_itemMoved() {
        mManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            createContent("1"), createContent("2"), createContent("3")
                        }));
        mRenderer.bind(mManager);

        mManager.moveContent(2, 1);
        verify(mRenderer, times(1)).onItemMoved(2, 1);
    }

    private FeedListContentManager.FeedContent createContent(String text) {
        TextView v = new AppCompatTextView(mContext);
        v.setText(text);
        return new FeedListContentManager.NativeViewContent(0, v.toString(), v);
    }

    @Test
    public void testGetListLayoutHelper() {
        mManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            createContent("1"), createContent("2"), createContent("3")
                        }));
        mRenderer.bind(mManager);

        ListLayoutHelper helper = mRenderer.getListLayoutHelper();
        LinearLayoutManager expectedLayoutManager =
                (LinearLayoutManager) mRenderer.getListViewForTest().getLayoutManager();
        assertEquals(
                expectedLayoutManager.findFirstVisibleItemPosition(),
                helper.findFirstVisibleItemPosition());
        assertEquals(
                expectedLayoutManager.findLastVisibleItemPosition(),
                helper.findLastVisibleItemPosition());
    }

    @Test
    public void testLayoutHelperSetColumnCount() {
        mManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            createContent("1"), createContent("2"), createContent("3")
                        }));
        mRenderer.bind(mManager);

        boolean res = mRenderer.getListLayoutHelper().setColumnCount(3);
        assertTrue("Failed to set column count.", res);
    }
}
