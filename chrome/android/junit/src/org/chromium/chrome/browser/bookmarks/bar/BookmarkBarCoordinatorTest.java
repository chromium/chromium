// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static android.util.TypedValue.COMPLEX_UNIT_DIP;
import static android.util.TypedValue.applyDimension;
import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import static org.hamcrest.Matchers.equalTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewStub;
import android.widget.ImageButton;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ActivityScenario.ActivityAction;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpener;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkOpener;
import org.chromium.chrome.browser.bookmarks.FakeBookmarkModel;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.widget.CoordinatorLayoutForPointer;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for {@link BookmarkBarCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkBarCoordinatorTest {

    @Rule
    public final ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private BrowserControlsManager mBrowserControlsManager;
    @Mock private FaviconHelperJni mFaviconHelperJni;
    @Mock private Callback<Integer> mHeightChangeCallback;
    @Mock private Callback<Integer> mHeightSupplierObserver;
    @Mock private ImageServiceBridgeJni mImageServiceBridgeJni;
    @Mock private Profile mProfile;
    @Mock private BookmarkOpener mBookmarkOpener;
    @Mock private BookmarkManagerOpener mBookmarkManagerOpener;

    private BookmarkBarCoordinator mCoordinator;
    private BookmarkId mDesktopFolderId;
    private RecyclerView mItemsContainer;
    private FakeBookmarkModel mModel;
    private ImageButton mOverflowButton;
    private ObservableSupplierImpl<Profile> mProfileSupplier;
    private BookmarkBar mView;

    @Before
    public void setUp() {
        mModel = FakeBookmarkModel.createModel();
        mDesktopFolderId = mModel.getDesktopFolderId();
        mProfileSupplier = new ObservableSupplierImpl<>(mProfile);

        when(mFaviconHelperJni.init()).thenReturn(1L);

        BookmarkModel.setInstanceForTesting(mModel);
        FaviconHelperJni.setInstanceForTesting(mFaviconHelperJni);
        ImageServiceBridgeJni.setInstanceForTesting(mImageServiceBridgeJni);

        onActivity(this::createCoordinator);
    }

    private @NonNull BookmarkId addItemToDesktopFolder(@NonNull String title) {
        final int index = mModel.getChildCount(mDesktopFolderId);
        return mModel.addBookmark(mDesktopFolderId, index, title, GURL.emptyGURL());
    }

    private void assertItemRenderedAtIndex(@NonNull BookmarkId itemId, int index) {
        final var item = mModel.getBookmarkById(itemId);
        assertNotNull(item);

        final var renderedItem = (BookmarkBarButton) mItemsContainer.getChildAt(index);
        assertNotNull(renderedItem);
        assertEquals(item.getTitle(), renderedItem.getTitleForTesting());
    }

    private void assertItemWidthAtIndex(int index, int width) {
        final var renderedItem = (BookmarkBarButton) mItemsContainer.getChildAt(index);
        assertNotNull(renderedItem);
        assertEquals(width, renderedItem.getWidth());
    }

    private void assertItemsRenderedCount(int count) {
        // NOTE: Use `Criteria` rather than `Assert` to allow polling via `CriteriaHelper`.
        Criteria.checkThat(mItemsContainer.getChildCount(), equalTo(count));
    }

    private void createCoordinator(@NonNull Activity activity) {
        final var contentView = new CoordinatorLayoutForPointer(activity, /* attrs= */ null);
        activity.setContentView(contentView);

        final var viewStub = new ViewStub(activity, R.layout.bookmark_bar);
        viewStub.setOnInflateListener((stub, view) -> mView = (BookmarkBar) view);
        contentView.addView(viewStub, new LayoutParams(MATCH_PARENT, WRAP_CONTENT));

        // NOTE: `viewStub` inflation occurs during coordinator construction.
        mView = null;
        mCoordinator =
                new BookmarkBarCoordinator(
                        activity,
                        mActivityLifecycleDispatcher,
                        mBrowserControlsManager,
                        mHeightChangeCallback,
                        mProfileSupplier,
                        viewStub,
                        mBookmarkOpener,
                        new ObservableSupplierImpl<>(mBookmarkManagerOpener));
        assertNotNull("Verify view stub inflation during construction.", mView);

        mItemsContainer = mView.findViewById(R.id.bookmark_bar_items_container);
        assertNotNull("Verify items container existence.", mItemsContainer);

        mOverflowButton = mView.findViewById(R.id.bookmark_bar_overflow_button);
        assertNotNull("Verify overflow button existence.", mOverflowButton);
    }

    private void moveItemToDesktopFolderAtIndex(@NonNull BookmarkId itemId, int index) {
        mModel.moveBookmark(itemId, mDesktopFolderId, index);
    }

    private void removeItem(@NonNull BookmarkId itemId) {
        mModel.deleteBookmark(itemId);
    }

    private void setItemTitle(@NonNull BookmarkId itemId, @Nullable String title) {
        mModel.setBookmarkTitle(itemId, title);
    }

    private @NonNull List<BookmarkId> setItemsWithinDesktopFolder(@NonNull List<String> titles) {
        final List<BookmarkId> itemIds = new ArrayList<>();
        mModel.performExtensiveBookmarkChanges(
                () -> {
                    mModel.getChildIds(mDesktopFolderId).stream().forEach(this::removeItem);
                    titles.stream().map(this::addItemToDesktopFolder).forEach(itemIds::add);
                });
        return itemIds;
    }

    private void onActivity(@NonNull ActivityAction<TestActivity> callback) {
        mActivityScenarioRule.getScenario().onActivity(callback);
    }

    @Test
    @SmallTest
    public void testConstructorWhenTopControlOffsetIsNonZero() {
        testConstructor(/* topControlOffset= */ -1);
    }

    @Test
    @SmallTest
    public void testConstructorWhenTopControlOffsetIsZero() {
        testConstructor(/* topControlOffset= */ 0);
    }

    private void testConstructor(int topControlOffset) {
        // Initialize browser controls manager.
        final int topControlsHeight = 1;
        when(mBrowserControlsManager.getTopControlsHeight()).thenReturn(topControlsHeight);
        when(mBrowserControlsManager.getTopControlOffset()).thenReturn(topControlOffset);

        // Invoke constructor.
        onActivity(this::createCoordinator);

        assertEquals(
                "Verify view top margin.",
                topControlsHeight - mView.getHeight(),
                ((MarginLayoutParams) mView.getLayoutParams()).topMargin);

        assertEquals(
                "Verify view visibility.",
                topControlOffset == 0 ? View.VISIBLE : View.GONE,
                mView.getVisibility());
    }

    @Test
    @SmallTest
    public void testOnBookmarkBarHeightChanged() {
        // Verify initial state.
        ObservableSupplier<Integer> heightSupplier = mCoordinator.getHeightSupplier();
        assertEquals("Verify initial state.", 0, heightSupplier.get().intValue());

        // NOTE: the `mHeightChangeCallback` is expected to have been registered for observation
        // during `mCoordinator` construction and notified of initial height via posted task.
        onActivity(
                activity -> {
                    verify(mHeightChangeCallback).onResult(mView.getHeight());
                    verifyNoMoreInteractions(mHeightSupplierObserver);
                });

        // Register another observer explicitly.
        heightSupplier.addObserver(mHeightSupplierObserver);

        // Verify state after height-changing layout.
        final var rect = new Rect(1, 2, 3, 4);
        mView.layout(rect.left, rect.top, rect.right, rect.bottom);
        assertEquals(
                "Verify state after height-changing layout.",
                rect.height(),
                heightSupplier.get().intValue());
        verify(mHeightChangeCallback).onResult(rect.height());
        verify(mHeightSupplierObserver).onResult(rect.height());

        // Verify state after height-consistent layout.
        rect.top += 1;
        rect.bottom += 1;
        mView.layout(rect.left, rect.top, rect.right, rect.bottom);
        assertEquals(
                "Verify state after height-consistent layout.",
                rect.height(),
                heightSupplier.get().intValue());
        verifyNoMoreInteractions(mHeightChangeCallback);
        verifyNoMoreInteractions(mHeightSupplierObserver);

        // Clean up.
        heightSupplier.removeObserver(mHeightSupplierObserver);
    }

    @Test
    @SmallTest
    public void testOnBookmarkBarItemAdded() {
        onActivity(
                activity -> {
                    assertItemsRenderedCount(0);
                    final var itemId = addItemToDesktopFolder("Item 1");
                    Robolectric.flushForegroundThreadScheduler();
                    assertItemsRenderedCount(1);
                    assertItemRenderedAtIndex(itemId, 0);
                });
    }

    @Test
    @SmallTest
    public void testOnBookmarkBarItemMoved() {
        onActivity(
                activity -> {
                    // Set up.
                    final var itemIds = setItemsWithinDesktopFolder(List.of("Item 1", "Item 2"));
                    Robolectric.flushForegroundThreadScheduler();
                    assertItemsRenderedCount(2);
                    assertItemRenderedAtIndex(itemIds.get(0), 0);
                    assertItemRenderedAtIndex(itemIds.get(1), 1);

                    // Test case.
                    moveItemToDesktopFolderAtIndex(itemIds.get(1), 0);
                    Robolectric.flushForegroundThreadScheduler();
                    assertItemsRenderedCount(2);
                    assertItemRenderedAtIndex(itemIds.get(1), 0);
                    assertItemRenderedAtIndex(itemIds.get(0), 1);
                });
    }

    @Test
    @SmallTest
    public void testOnBookmarkBarItemRemoved() {
        onActivity(
                activity -> {
                    // Set up.
                    final var itemIds = setItemsWithinDesktopFolder(List.of("Item 1", "Item 2"));
                    Robolectric.flushForegroundThreadScheduler();
                    assertItemsRenderedCount(2);
                    assertItemRenderedAtIndex(itemIds.get(0), 0);
                    assertItemRenderedAtIndex(itemIds.get(1), 1);

                    // Test case.
                    removeItem(itemIds.get(0));
                    Robolectric.flushForegroundThreadScheduler();
                    assertItemsRenderedCount(1);
                    assertItemRenderedAtIndex(itemIds.get(1), 0);
                });
    }

    @Test
    @SmallTest
    public void testOnBookmarkBarItemUpdated() {
        onActivity(
                activity -> {
                    // Set up.
                    assertItemsRenderedCount(0);
                    final var itemId = addItemToDesktopFolder("Item 1");
                    Robolectric.flushForegroundThreadScheduler();
                    assertItemsRenderedCount(1);
                    assertItemRenderedAtIndex(itemId, 0);

                    // Test case.
                    setItemTitle(itemId, "Item 1 (Updated)");
                    Robolectric.flushForegroundThreadScheduler();
                    assertItemsRenderedCount(1);
                    assertItemRenderedAtIndex(itemId, 0);
                });
    }

    @Test
    @SmallTest
    public void testOnBookmarkBarItemsChanged() {
        onActivity(
                activity -> {
                    // Set up.
                    var itemIds = setItemsWithinDesktopFolder(List.of("Item 1", "Item 2"));
                    Robolectric.flushForegroundThreadScheduler();
                    assertItemsRenderedCount(2);
                    assertItemRenderedAtIndex(itemIds.get(0), 0);
                    assertItemRenderedAtIndex(itemIds.get(1), 1);

                    // Test case.
                    itemIds = setItemsWithinDesktopFolder(List.of("Item 3", "Item 4"));
                    Robolectric.flushForegroundThreadScheduler();
                    assertItemsRenderedCount(2);
                    assertItemRenderedAtIndex(itemIds.get(0), 0);
                    assertItemRenderedAtIndex(itemIds.get(1), 1);
                });
    }

    @Test
    @SmallTest
    @Config(qualifiers = "w600dp")
    public void testOnConfigurationChanged() {
        onActivity(
                activity -> {
                    // Verify observer registration.
                    var observer = ArgumentCaptor.forClass(ConfigurationChangedObserver.class);
                    verify(mActivityLifecycleDispatcher).register(observer.capture());

                    // Set up item with a long title.
                    setItemsWithinDesktopFolder(List.of("Title".repeat(100)));
                    Robolectric.flushForegroundThreadScheduler();

                    // Verify item max width constraint at "w600dp".
                    var metrics = activity.getResources().getDisplayMetrics();
                    assertItemsRenderedCount(1);
                    assertItemWidthAtIndex(
                            /* index= */ 0,
                            /* width= */ Math.round(
                                    applyDimension(COMPLEX_UNIT_DIP, 187, metrics)));

                    // Change configuration to below "w600dp".
                    RuntimeEnvironment.setQualifiers("w599dp");
                    var newConfig = Resources.getSystem().getConfiguration();
                    activity.onConfigurationChanged(newConfig);
                    observer.getValue().onConfigurationChanged(newConfig);

                    // NOTE: Robolectric does not automatically re-measure/-layout the view.
                    mItemsContainer.measure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);
                    mItemsContainer.layout(
                            mItemsContainer.getLeft(),
                            mItemsContainer.getTop(),
                            mItemsContainer.getRight(),
                            mItemsContainer.getBottom());

                    // Verify item max width constraint below "w600dp".
                    assertItemsRenderedCount(1);
                    assertItemWidthAtIndex(
                            /* index= */ 0,
                            /* width= */ Math.round(
                                    applyDimension(COMPLEX_UNIT_DIP, 124, metrics)));

                    // Verify observer unregistration.
                    mCoordinator.destroy();
                    verify(mActivityLifecycleDispatcher).unregister(observer.getValue());
                });
    }

    @Test
    @SmallTest
    public void testOnItemsOverflowChanged() {
        onActivity(
                activity -> {
                    // Test case: empty state.
                    final var count = new AtomicInteger();
                    assertItemsRenderedCount(count.get());
                    assertEquals(View.INVISIBLE, mOverflowButton.getVisibility());

                    // Test case: minimally-populated state.
                    addItemToDesktopFolder("" + count.incrementAndGet());
                    Robolectric.flushForegroundThreadScheduler();
                    assertItemsRenderedCount(count.get());
                    assertEquals(View.INVISIBLE, mOverflowButton.getVisibility());

                    // Test case: populated-to-overflow state.
                    final var id = new AtomicReference<BookmarkId>();
                    CriteriaHelper.pollUiThreadForJUnit(
                            () -> {
                                id.set(addItemToDesktopFolder("" + count.incrementAndGet()));
                                Robolectric.flushForegroundThreadScheduler();
                                assertItemsRenderedCount(count.get() - 1);
                                assertEquals(View.VISIBLE, mOverflowButton.getVisibility());
                            });

                    // Test case: maximally-populated state.
                    removeItem(id.get());
                    Robolectric.flushForegroundThreadScheduler();
                    assertItemsRenderedCount(count.decrementAndGet());
                    assertEquals(View.INVISIBLE, mOverflowButton.getVisibility());
                });
    }

    @Test
    @SmallTest
    public void testOnProfileChanged() {
        onActivity(
                activity -> {
                    // Set up.
                    var itemIds = setItemsWithinDesktopFolder(List.of("Item 1", "Item 2"));
                    Robolectric.flushForegroundThreadScheduler();
                    assertItemsRenderedCount(2);
                    assertItemRenderedAtIndex(itemIds.get(0), 0);
                    assertItemRenderedAtIndex(itemIds.get(1), 1);

                    // Test case: `null` profile.
                    BookmarkModel.setInstanceForTesting(null);
                    mProfileSupplier.set(null);
                    Robolectric.flushForegroundThreadScheduler();
                    assertItemsRenderedCount(0);

                    // Test case: profile w/ populated model.
                    itemIds = setItemsWithinDesktopFolder(List.of("Item 3", "Item 4"));
                    BookmarkModel.setInstanceForTesting(mModel);
                    mProfileSupplier.set(mProfile);
                    Robolectric.flushForegroundThreadScheduler();
                    assertItemsRenderedCount(2);
                    assertItemRenderedAtIndex(itemIds.get(0), 0);
                    assertItemRenderedAtIndex(itemIds.get(1), 1);
                });
    }

    @Test
    @SmallTest
    public void testOnTopControlsHeightChanged() {
        // Initialize browser controls manager.
        final int topControlsHeight = 1;
        when(mBrowserControlsManager.getTopControlsHeight()).thenReturn(topControlsHeight);

        // Simulate top controls height changed.
        final var obs = ArgumentCaptor.forClass(BrowserControlsStateProvider.Observer.class);
        verify(mBrowserControlsManager).addObserver(obs.capture());
        obs.getValue()
                .onTopControlsHeightChanged(
                        mBrowserControlsManager.getTopControlsHeight(),
                        mBrowserControlsManager.getTopControlsMinHeight());

        assertEquals(
                "Verify view top margin.",
                topControlsHeight - mView.getHeight(),
                ((MarginLayoutParams) mView.getLayoutParams()).topMargin);
    }

    @Test
    @SmallTest
    public void testOnTopControlsOffsetChanged() {
        // Initialize browser controls manager.
        final var topControlOffset = new AtomicInteger(-1);
        when(mBrowserControlsManager.getTopControlOffset())
                .thenAnswer(invocation -> topControlOffset.get());

        // Simulate top controls offset changed to non-zero value.
        final var obs = ArgumentCaptor.forClass(BrowserControlsStateProvider.Observer.class);
        verify(mBrowserControlsManager).addObserver(obs.capture());
        obs.getValue()
                .onControlsOffsetChanged(
                        mBrowserControlsManager.getTopControlOffset(),
                        mBrowserControlsManager.getTopControlsMinHeightOffset(),
                        /* topControlsMinHeightChanged= */ false,
                        mBrowserControlsManager.getBottomControlOffset(),
                        mBrowserControlsManager.getBottomControlsMinHeightOffset(),
                        /* bottomControlsMinHeightChanged= */ false,
                        /* requestNewFrame= */ false,
                        /* isVisibilityForced= */ false);

        assertEquals(
                "Verify view visibility after top controls offset changed to non-zero value.",
                View.GONE,
                mView.getVisibility());

        // Simulate top controls offset changed to zero value.
        topControlOffset.set(0);
        obs.getValue()
                .onControlsOffsetChanged(
                        mBrowserControlsManager.getTopControlOffset(),
                        mBrowserControlsManager.getTopControlsMinHeightOffset(),
                        /* topControlsMinHeightChanged= */ false,
                        mBrowserControlsManager.getBottomControlOffset(),
                        mBrowserControlsManager.getBottomControlsMinHeightOffset(),
                        /* bottomControlsMinHeightChanged= */ false,
                        /* requestNewFrame= */ false,
                        /* isVisibilityForced= */ false);

        assertEquals(
                "Verify view visibility after top controls offset changed to zero value.",
                View.VISIBLE,
                mView.getVisibility());
    }
}
