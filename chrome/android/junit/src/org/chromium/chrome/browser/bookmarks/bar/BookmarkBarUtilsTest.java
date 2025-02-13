// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.view.KeyEvent;
import android.view.LayoutInflater;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowDrawable;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkImageFetcher;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.Objects;
import java.util.function.BiConsumer;

/** Unit tests for {@link BookmarkBarUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkBarUtilsTest {

    @Rule
    public final ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BiConsumer<BookmarkItem, Integer> mClickCallback;
    @Mock private Drawable mFavicon;
    @Mock private BookmarkImageFetcher mImageFetcher;
    @Mock private BookmarkItem mItem;

    private void assertIcon(@NonNull BookmarkBarButton view, boolean isFolder) {
        if (isFolder) {
            assertEquals(
                    R.drawable.ic_folder_outline_24dp,
                    Shadows.shadowOf(view.getIconForTesting()).getCreatedFromResId());
        } else {
            assertEquals(mFavicon, view.getIconForTesting());
        }
    }

    private void assertIconTintColorList(@NonNull BookmarkBarButton view, boolean isFolder) {
        ColorStateList expectedIconTintList = null;
        if (isFolder) {
            expectedIconTintList =
                    AppCompatResources.getColorStateList(
                            view.getContext(), R.color.default_icon_color_tint_list);
        }
        // NOTE: Reference equivalence may occasionally fail resulting in test flakiness. Instead,
        // compare string representations which should be sufficient to ensure equivalence.
        final ColorStateList actualIconTintList = view.getIconTintListForTesting();
        assertEquals(Objects.toString(expectedIconTintList), Objects.toString(actualIconTintList));
    }

    private @NonNull BookmarkBarButton inflateBookmarkBarButton(@NonNull Context context) {
        return (BookmarkBarButton)
                LayoutInflater.from(context).inflate(R.layout.bookmark_bar_button, null);
    }

    private @NonNull <T> Answer<Void> runCallbackAtIndexWithValue(int index, @Nullable T value) {
        return invocation -> {
            final Callback<T> callback = invocation.getArgument(index);
            callback.onResult(value);
            return null;
        };
    }

    @Test
    @SmallTest
    public void testCreateListItem() {
        testCreateListItem(/* isFolder= */ false);
    }

    @Test
    @SmallTest
    @Config(shadows = {ShadowDrawable.class})
    public void testCreateListItemForFolder() {
        testCreateListItem(/* isFolder= */ true);
    }

    private void testCreateListItem(boolean isFolder) {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            // Set up mocks.
                            final var title = "Title";
                            when(mItem.getTitle()).thenReturn(title);
                            when(mItem.isFolder()).thenReturn(isFolder);
                            doAnswer(runCallbackAtIndexWithValue(1, mFavicon))
                                    .when(mImageFetcher)
                                    .fetchFaviconForBookmark(eq(mItem), any());

                            // Create list item.
                            final var listItem =
                                    BookmarkBarUtils.createListItemFor(
                                            mClickCallback, activity, mImageFetcher, mItem);

                            // Verify expected type.
                            assertEquals(BookmarkBarUtils.ViewType.ITEM, listItem.type);

                            // Bind list item to view.
                            final var view = inflateBookmarkBarButton(activity);
                            activity.setContentView(view);
                            Robolectric.flushForegroundThreadScheduler();
                            PropertyModelChangeProcessor.create(
                                    listItem.model, view, BookmarkBarButtonViewBinder::bind);

                            // Verify expected properties.
                            assertIcon(view, isFolder);
                            assertIconTintColorList(view, isFolder);
                            assertEquals(title, view.getTitleForTesting());

                            // Verify expected event propagation.
                            verify(mClickCallback, never()).accept(any(), any());
                            TouchCommon.singleClickView(view, KeyEvent.META_CTRL_ON);
                            Robolectric.flushForegroundThreadScheduler();
                            verify(mClickCallback).accept(mItem, KeyEvent.META_CTRL_ON);
                        });
    }
}
