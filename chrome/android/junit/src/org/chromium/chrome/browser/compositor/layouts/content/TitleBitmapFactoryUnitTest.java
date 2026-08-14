// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.content;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Paint.FontMetrics;
import android.text.TextPaint;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupsFeatureMap;

/** Tests for {@link TitleBitmapFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({TabGroupsFeatureMap.UPDATE_TAB_GROUP_COLORS})
public class TitleBitmapFactoryUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int TAB_STRIP_HEIGHT_PX = 40;
    private static final float MAX_TEXT_HEIGHT = 100.f;
    private static final float VALID_TEXT_HEIGHT = 10.f;
    private static final float INVALID_TEXT_HEIGHT = 150.f;

    @Mock private TextPaint mTextPaint;
    @Mock private TabModel mTabModel;

    private FontMetrics mFontMetrics;
    private Context mContext;
    private TitleBitmapFactory mFactory;
    private int mFaviconDimension;
    private int mMaxTitleWidth;

    @Before
    public void setup() {
        mFontMetrics = new FontMetrics();
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
        mFaviconDimension =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.compositor_tab_title_favicon_size);
        mMaxTitleWidth =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.compositor_tab_title_max_width);
        mFactory =
                new TitleBitmapFactory(
                        mContext,
                        /* incognito= */ false,
                        /* tabStripHeightPx= */ TAB_STRIP_HEIGHT_PX);
    }

    @Test
    public void testEnforceMaxTextHeight_AlreadyValid() {
        // Fake a valid text height.
        when(mTextPaint.getFontMetrics()).thenReturn(mFontMetrics);
        when(mTextPaint.getTextSize()).thenReturn(VALID_TEXT_HEIGHT);
        mFontMetrics.bottom = VALID_TEXT_HEIGHT;
        mFontMetrics.top = 0.f;

        // Verify we don't adjust the text height if it's already valid.
        TitleBitmapFactory.enforceMaxTextHeight(mTextPaint, MAX_TEXT_HEIGHT);
        verify(mTextPaint, never()).setTextSize(anyFloat());
    }

    @Test
    public void testEnforceMaxTextHeight_Clamped() {
        // Fake an invalid text height.
        when(mTextPaint.getFontMetrics()).thenReturn(mFontMetrics);
        when(mTextPaint.getTextSize()).thenReturn(INVALID_TEXT_HEIGHT);
        mFontMetrics.bottom = INVALID_TEXT_HEIGHT;
        mFontMetrics.top = 0.f;

        // Verify we adjust the text height as it's invalid.
        TitleBitmapFactory.enforceMaxTextHeight(mTextPaint, MAX_TEXT_HEIGHT);
        verify(mTextPaint).setTextSize(eq(MAX_TEXT_HEIGHT));
    }

    @Test
    public void testGetMaxHeightOfFont() {
        mFontMetrics.bottom = 200.f;
        mFontMetrics.top = 150.f;

        // bottom - top = 200.f - 150.f = 50.f
        float expectedMaxHeight = 50.f;
        assertEquals(
                "Unexpected calculated max height.",
                expectedMaxHeight,
                TitleBitmapFactory.getMaxHeightOfFont(mFontMetrics),
                /* delta= */ 0.f);
    }

    @Test
    public void testGetFaviconBitmap_ScaleDown() {
        Bitmap largeFavicon =
                Bitmap.createBitmap(
                        mFaviconDimension * 2, mFaviconDimension * 2, Bitmap.Config.ARGB_8888);
        Bitmap result = mFactory.getFaviconBitmap(largeFavicon);

        assertNotNull("Favicon bitmap should not be null.", result);
        assertEquals(
                "Favicon width should match target dimension.",
                mFaviconDimension,
                result.getWidth());
        assertEquals(
                "Favicon height should match target dimension.",
                mFaviconDimension,
                result.getHeight());
    }

    @Test
    public void testGetFaviconBitmap_CenterSmall() {
        Bitmap smallFavicon =
                Bitmap.createBitmap(
                        mFaviconDimension / 2, mFaviconDimension / 2, Bitmap.Config.ARGB_8888);
        Bitmap result = mFactory.getFaviconBitmap(smallFavicon);

        assertNotNull("Favicon bitmap should not be null.", result);
        assertEquals(
                "Favicon width should match target dimension.",
                mFaviconDimension,
                result.getWidth());
        assertEquals(
                "Favicon height should match target dimension.",
                mFaviconDimension,
                result.getHeight());
    }

    @Test
    public void testGetFaviconBitmap_SequentialNoMatrixBleed() {
        Bitmap largeFavicon =
                Bitmap.createBitmap(
                        mFaviconDimension * 2, mFaviconDimension * 2, Bitmap.Config.ARGB_8888);
        Bitmap smallFavicon =
                Bitmap.createBitmap(
                        mFaviconDimension / 2, mFaviconDimension / 2, Bitmap.Config.ARGB_8888);

        Bitmap b1 = mFactory.getFaviconBitmap(largeFavicon);
        assertNotNull("Large favicon result should not be null.", b1);
        assertEquals(mFaviconDimension, b1.getWidth());
        assertEquals(mFaviconDimension, b1.getHeight());

        Bitmap b2 = mFactory.getFaviconBitmap(smallFavicon);
        assertNotNull("Small favicon result should not be null.", b2);
        assertEquals(mFaviconDimension, b2.getWidth());
        assertEquals(mFaviconDimension, b2.getHeight());

        Bitmap b3 = mFactory.getFaviconBitmap(largeFavicon);
        assertNotNull("Subsequent large favicon result should not be null.", b3);
        assertEquals(mFaviconDimension, b3.getWidth());
        assertEquals(mFaviconDimension, b3.getHeight());
    }

    @Test
    public void testGetTabTitleBitmap_StandardText() {
        Bitmap titleBitmap = mFactory.getTabTitleBitmap("Standard Tab Title");
        assertNotNull("Tab title bitmap should not be null.", titleBitmap);
        assertTrue("Tab title bitmap width should be positive.", titleBitmap.getWidth() > 0);
        assertTrue("Tab title bitmap height should be positive.", titleBitmap.getHeight() > 0);
    }

    @Test
    public void testGetTabTitleBitmap_EmptyText() {
        Bitmap titleBitmap = mFactory.getTabTitleBitmap("");
        assertNotNull("Empty tab title bitmap should not be null.", titleBitmap);
        assertEquals(
                "Empty tab title bitmap should have minimum width of 1.",
                1,
                titleBitmap.getWidth());
        assertTrue(
                "Empty tab title bitmap height should be positive.", titleBitmap.getHeight() > 0);
    }

    @Test
    public void testGetTabTitleBitmap_LongText() {
        String longTitle = "A".repeat(/* count= */ 2000);
        Bitmap titleBitmap = mFactory.getTabTitleBitmap(longTitle);
        assertNotNull("Long tab title bitmap should not be null.", titleBitmap);
        assertEquals(
                "Long tab title bitmap should be clamped to max title width.",
                mMaxTitleWidth,
                titleBitmap.getWidth());
    }

    @Test
    public void testGetGroupTitleBitmap() {
        Token existingGroupId = new Token(/* high= */ 1L, /* low= */ 2L);
        when(mTabModel.tabGroupExists(existingGroupId)).thenReturn(true);
        when(mTabModel.getTabGroupColor(existingGroupId)).thenReturn(TabGroupColorId.GREY);

        Bitmap groupBitmap =
                mFactory.getGroupTitleBitmap(
                        mTabModel, mContext, existingGroupId, "Test Tab Group");
        assertNotNull("Existing group title bitmap should not be null.", groupBitmap);
        assertTrue("Group title bitmap width should be positive.", groupBitmap.getWidth() > 0);

        Token nonExistentGroupId = new Token(/* high= */ 3L, /* low= */ 4L);
        when(mTabModel.tabGroupExists(nonExistentGroupId)).thenReturn(false);

        Bitmap missingGroupBitmap =
                mFactory.getGroupTitleBitmap(
                        mTabModel, mContext, nonExistentGroupId, "Test Tab Group");
        assertNull("Non-existent group title bitmap should be null.", missingGroupBitmap);
    }

    @Test
    public void testGetButtonTextBitmap() {
        Bitmap buttonBitmap = mFactory.getButtonTextBitmap("Button Label");
        assertNotNull("Button text bitmap should not be null.", buttonBitmap);
        assertTrue("Button text bitmap width should be positive.", buttonBitmap.getWidth() > 0);
    }

    @Test
    public void testSequentialInvocations_NoCrashOrRetainedState() {
        Bitmap largeFavicon =
                Bitmap.createBitmap(
                        mFaviconDimension * 2, mFaviconDimension * 2, Bitmap.Config.ARGB_8888);
        Bitmap smallFavicon =
                Bitmap.createBitmap(
                        mFaviconDimension / 2, mFaviconDimension / 2, Bitmap.Config.ARGB_8888);
        Token groupId = new Token(/* high= */ 1L, /* low= */ 2L);
        when(mTabModel.tabGroupExists(groupId)).thenReturn(true);
        when(mTabModel.getTabGroupColor(groupId)).thenReturn(TabGroupColorId.GREY);

        Bitmap tabTitle1 = mFactory.getTabTitleBitmap("First Title");
        Bitmap favicon1 = mFactory.getFaviconBitmap(largeFavicon);
        Bitmap groupTitle = mFactory.getGroupTitleBitmap(mTabModel, mContext, groupId, "Group");
        Bitmap buttonText = mFactory.getButtonTextBitmap("Action");
        Bitmap tabTitle2 = mFactory.getTabTitleBitmap("Second Title");
        Bitmap favicon2 = mFactory.getFaviconBitmap(smallFavicon);

        assertNotNull(tabTitle1);
        assertNotNull(favicon1);
        assertNotNull(groupTitle);
        assertNotNull(buttonText);
        assertNotNull(tabTitle2);
        assertNotNull(favicon2);
    }
}
