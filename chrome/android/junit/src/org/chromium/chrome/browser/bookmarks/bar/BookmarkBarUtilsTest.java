// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowDrawable;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.bookmarks.BookmarkItem;

/** Unit tests for {@link BookmarkBarUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkBarUtilsTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BookmarkItem mItem;

    @Test
    @SmallTest
    @Config(shadows = {ShadowDrawable.class})
    public void testCreateListItem() {
        final var title = "Title";
        when(mItem.getTitle()).thenReturn(title);

        final var context = ApplicationProvider.getApplicationContext();
        final var listItem = BookmarkBarUtils.createListItemFor(context, mItem);
        assertEquals(BookmarkBarUtils.ViewType.ITEM, listItem.type);
        assertEquals(title, listItem.model.get(BookmarkBarButtonProperties.TITLE));

        // TODO(crbug.com/347632437): Replace star filled icon w/ favicon.
        final var icon = Shadows.shadowOf(listItem.model.get(BookmarkBarButtonProperties.ICON));
        assertEquals(R.drawable.btn_star_filled, icon.getCreatedFromResId());
    }
}
