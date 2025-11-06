// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import static org.hamcrest.Matchers.equalTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.core.content.ContextCompat;
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

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpener;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkOpener;
import org.chromium.chrome.browser.bookmarks.FakeBookmarkModel;
import org.chromium.chrome.browser.browser_controls.BrowserControlsOffsetTagsInfo;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.widget.CoordinatorLayoutForPointer;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.resources.ResourceFactory;
import org.chromium.ui.resources.ResourceFactoryJni;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
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

    @Mock private BookmarkBarSceneLayer.Natives mBookmarkBarSceneLayerJniMock;
    @Mock private ResourceFactory.Natives mResourceFactoryJniMock;

    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private LayoutManager mLayoutManager;
    @Mock private Runnable mLayoutManagerRequestUpdate;
    @Mock private FullscreenManager mFullscreenManager;
    @Mock private ResourceManager mResourceManager;
    @Mock private DynamicResourceLoader mDynamicResourceLoader;
    @Mock private BrowserControlsManager mBrowserControlsManager;
    @Mock private FaviconHelperJni mFaviconHelperJni;
    @Mock private Callback<Void> mHeightChangeCallback;
    @Mock private ImageServiceBridgeJni mImageServiceBridgeJni;
    @Mock private Profile mProfile;
    @Mock private Tab mCurrentTab;
    @Mock private BookmarkOpener mBookmarkOpener;
    @Mock private BookmarkManagerOpener mBookmarkManagerOpener;
    @Mock private TopControlsStacker mTopControlsStacker;
    @Mock private ObservableSupplier<@Nullable Tab> mCurrentTabSupplier;
    @Mock private TopUiThemeColorProvider mTopUiThemeColorProvider;

    private BookmarkBarCoordinator mCoordinator;
    private BookmarkId mDesktopFolderId;
    private RecyclerView mItemsContainer;
    private FakeBookmarkModel mModel;
    private FrameLayout mOverflowButton;
    private ObservableSupplierImpl<Profile> mProfileSupplier;
    private BookmarkBar mView;
    private FrameLayout mContentContainer;

    @Before
    public void setUp() {
        mModel = FakeBookmarkModel.createModel();
        mDesktopFolderId = mModel.getDesktopFolderId();
        mProfileSupplier = new ObservableSupplierImpl<>(mProfile);
        BookmarkBarSceneLayerJni.setInstanceForTesting(mBookmarkBarSceneLayerJniMock);
        ResourceFactoryJni.setInstanceForTesting(mResourceFactoryJniMock);

        when(mFaviconHelperJni.init()).thenReturn(1L);
        when(mResourceManager.getBitmapDynamicResourceLoader()).thenReturn(mDynamicResourceLoader);

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

    private void assertItemsRenderedCount(int count) {
        // NOTE: Use `Criteria` rather than `Assert` to allow polling via `CriteriaHelper`.
        Criteria.checkThat(mItemsContainer.getChildCount(), equalTo(count));
    }

    private void createCoordinator(@NonNull Activity activity) {
        final var contentView = new CoordinatorLayoutForPointer(activity, /* attrs= */ null);
        activity.setContentView(contentView);

        final var viewStub = new ViewStub(activity, R.layout.bookmark_bar);
        viewStub.setOnInflateListener(
                (stub, view) -> {
                    mView = (BookmarkBar) view;
                    mContentContainer = mView.findViewById(R.id.bookmark_bar_content_container);
                });
        contentView.addView(viewStub, new LayoutParams(MATCH_PARENT, WRAP_CONTENT));

        // NOTE: `viewStub` inflation occurs during coordinator construction.
        mView = null;
        mCoordinator =
                new BookmarkBarCoordinator(
                        activity,
                        mActivityLifecycleDispatcher,
                        mLayoutManager,
                        mLayoutManagerRequestUpdate,
                        mFullscreenManager,
                        mResourceManager,
                        mBrowserControlsManager,
                        mHeightChangeCallback,
                        mProfileSupplier,
                        viewStub,
                        mCurrentTab,
                        mBookmarkOpener,
                        new ObservableSupplierImpl<>(mBookmarkManagerOpener),
                        mTopControlsStacker,
                        mCurrentTabSupplier,
                        mTopUiThemeColorProvider);

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
    @DisabledTest(message = "https://crbug.com/430058443")
    public void testConstructorWhenTopControlOffsetIsNonZero() {
        testConstructor(/* topControlOffset= */ -1);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/430058443")
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
        // Verify initial state. Height is read from minHeight and hairline's height.
        assertEquals("Verify initial state.", 41, mCoordinator.getTopControlHeight());

        // NOTE: the `mHeightChangeCallback` is expected to have been registered for observation
        // during `mCoordinator` construction and notified of initial height via posted task.
        onActivity(
                activity -> {
                    verify(mHeightChangeCallback).onResult(null);
                });

        // Verify state after height-changing layout.
        final var rect = new Rect(1, 2, 3, 4);
        clearInvocations(mHeightChangeCallback);
        mView.layout(rect.left, rect.top, rect.right, rect.bottom);
        mContentContainer.layout(rect.left, rect.top, rect.right, rect.bottom);
        assertEquals(
                "Verify state after height-changing layout.",
                rect.height() + 1,
                mCoordinator.getTopControlHeight());
        verify(mHeightChangeCallback).onResult(null);

        // Verify state after height-consistent layout.
        rect.top += 1;
        rect.bottom += 1;
        mContentContainer.layout(rect.left, rect.top, rect.right, rect.bottom);
        assertEquals(
                "Verify state after height-consistent layout.",
                rect.height() + 1,
                mCoordinator.getTopControlHeight());
        verifyNoMoreInteractions(mHeightChangeCallback);
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
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnTopControlsHeightChanged() {
        // Initialize browser controls manager. Bookmark bar start height is 40.
        int topControlsHeight = 41;
        when(mBrowserControlsManager.getTopControlsHeight()).thenReturn(topControlsHeight);

        mCoordinator.onTopControlLayerHeightChanged(
                mBrowserControlsManager.getTopControlsHeight(),
                mBrowserControlsManager.getTopControlsMinHeight());

        assertEquals(
                "Verify view top margin.",
                0,
                ((MarginLayoutParams) mView.getLayoutParams()).topMargin);

        topControlsHeight = 51;
        when(mBrowserControlsManager.getTopControlsHeight()).thenReturn(topControlsHeight);
        mCoordinator.onTopControlLayerHeightChanged(
                mBrowserControlsManager.getTopControlsHeight(),
                mBrowserControlsManager.getTopControlsMinHeight());

        assertEquals(
                "Verify view top margin.",
                10,
                ((MarginLayoutParams) mView.getLayoutParams()).topMargin);
    }

    @Test
    @SmallTest
    @SuppressWarnings("DirectInvocationOnMock")
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

    @Test
    @SmallTest
    public void testUpdateBackgroundColor_SetsModelProperties_Incognito() {
        PropertyModel bookmarBarModel = mCoordinator.getModelForTesting();

        when(mCurrentTab.isIncognito()).thenReturn(true);
        when(mTopUiThemeColorProvider.getSceneLayerBackground(mCurrentTab)).thenReturn(Color.BLACK);

        // The expected colors in incognito.
        @ColorInt
        int expectedDarkHairline =
                ContextCompat.getColor(mView.getContext(), R.color.divider_line_bg_color_light);

        ColorStateList expectedLightTint =
                ContextCompat.getColorStateList(
                        mView.getContext(), R.color.default_icon_color_light_tint_list);

        mCoordinator.updateBackgroundColor(mCurrentTab);

        // Verify the incognito colors.
        assertEquals(
                "Hairline color should be set to the dark divider color.",
                expectedDarkHairline,
                bookmarBarModel.get(BookmarkBarProperties.HAIRLINE_COLOR));
        assertEquals(
                "Divider color should be set to the dark divider color.",
                expectedDarkHairline,
                bookmarBarModel.get(BookmarkBarProperties.DIVIDER_COLOR));
        assertEquals(
                "Overflow tint should be set to the light tint.",
                expectedLightTint,
                bookmarBarModel.get(BookmarkBarProperties.OVERFLOW_BUTTON_TINT_LIST));
    }

    @Test
    @SmallTest
    public void testUpdateBackgroundColor_SetsModelProperties_RegularLightTheme() {
        PropertyModel bookmarBarModel = mCoordinator.getModelForTesting();

        // Simulate being on a regular light theme tab (BrandedColorScheme.APP_DEFAULT).
        when(mCurrentTab.isIncognito()).thenReturn(false);
        when(mTopUiThemeColorProvider.getSceneLayerBackground(mCurrentTab)).thenReturn(Color.WHITE);

        // The expected colors for the regular light theme.
        @ColorInt
        int expectedDarkHairline =
                ThemeUtils.getToolbarHairlineColor(
                        mView.getContext(), Color.WHITE, /* isIncognito= */ false);
        ColorStateList expectedDarkTint =
                ContextCompat.getColorStateList(
                        mView.getContext(), R.color.default_icon_color_tint_list);

        mCoordinator.updateBackgroundColor(mCurrentTab);

        // Verify the the colors for the regular light theme mode.
        assertEquals(
                "Hairline color should be set for regular light theme.",
                expectedDarkHairline,
                bookmarBarModel.get(BookmarkBarProperties.HAIRLINE_COLOR));
        assertEquals(
                "Divider color should be set for regular light theme.",
                expectedDarkHairline,
                bookmarBarModel.get(BookmarkBarProperties.DIVIDER_COLOR));
        assertEquals(
                "Overflow tint should be set for regular light theme.",
                expectedDarkTint,
                bookmarBarModel.get(BookmarkBarProperties.OVERFLOW_BUTTON_TINT_LIST));
    }

    @Test
    public void testOffsetTags_ControlsAtTop() {
        doReturn(ControlsPosition.TOP).when(mBrowserControlsManager).getControlsPosition();
        mCoordinator.updateOffsetTag(new BrowserControlsOffsetTagsInfo());
        assertNotNull(
                mCoordinator
                        .getBookmarkBarSceneLayerModelForTesting()
                        .get(BookmarkBarSceneLayerProperties.OFFSET_TAG));

        mCoordinator.updateOffsetTag(null);
        assertNull(
                mCoordinator
                        .getBookmarkBarSceneLayerModelForTesting()
                        .get(BookmarkBarSceneLayerProperties.OFFSET_TAG));
    }

    @Test
    public void testOffsetTags_ControlsAtBottom() {
        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsManager).getControlsPosition();
        mCoordinator.updateOffsetTag(new BrowserControlsOffsetTagsInfo());
        assertNull(
                mCoordinator
                        .getBookmarkBarSceneLayerModelForTesting()
                        .get(BookmarkBarSceneLayerProperties.OFFSET_TAG));
    }
}
