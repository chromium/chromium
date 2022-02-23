// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.clipboard;

import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.drawable.BitmapDrawable;
import android.view.ContextThemeWrapper;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionDrawableState;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewViewBinder;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.io.ByteArrayOutputStream;

/**
 * Tests for {@link ClipboardSuggestionProcessor}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ClipboardSuggestionProcessorTest {
    private static final GURL TEST_URL = new GURL("http://url");
    private static final GURL ARABIC_URL = new GURL("http://site.com/مرحبا/123");

    @Mock
    SuggestionHost mSuggestionHost;
    @Mock
    LargeIconBridge mIconBridge;
    @Mock
    Resources mResources;

    private Context mContext;
    private ClipboardSuggestionProcessor mProcessor;
    private AutocompleteMatch mSuggestion;
    private PropertyModel mModel;
    private Bitmap mBitmap;
    private ViewGroup mRootView;
    private TextView mTitleTextView;
    private TextView mContentTextView;
    private int mLastSetTextDirection = -1;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        mContext = new ContextThemeWrapper(
                ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mBitmap = Bitmap.createBitmap(10, 5, Config.ARGB_8888);
        mProcessor = new ClipboardSuggestionProcessor(mContext, mSuggestionHost, () -> mIconBridge);
        mRootView = new LinearLayout(mContext);
        mTitleTextView = new TextView(mContext);
        mTitleTextView.setId(R.id.line_1);
        mContentTextView = new TextView(mContext) {
            @Override
            public void setTextDirection(int textDirection) {
                super.setTextDirection(textDirection);
                mLastSetTextDirection = textDirection;
            }
        };
        mContentTextView.setId(R.id.line_2);
        mRootView.addView(mTitleTextView);
        mRootView.addView(mContentTextView);
    }

    /** Create clipboard suggestion for test, and click the reveal button. */
    private void createClipboardSuggestionAndClickReveal(int type, GURL url) {
        createClipboardSuggestion(type, url, null);

        // Click reveal button
        mProcessor.revealButtonClickHandler(mSuggestion, mModel);
    }

    /** Create clipboard suggestion for test, and click the reveal button. */
    private void createClipboardSuggestionAndClickReveal(
            int type, GURL url, byte[] clipboardImageData) {
        createClipboardSuggestion(type, url, clipboardImageData);

        // Click reveal button
        mProcessor.revealButtonClickHandler(mSuggestion, mModel);
    }

    /** Create clipboard suggestion for test. */
    private void createClipboardSuggestion(int type, GURL url) {
        createClipboardSuggestion(type, url, null);
    }

    /** Create clipboard suggestion for test. */
    private void createClipboardSuggestion(int type, GURL url, byte[] clipboardImageData) {
        mSuggestion = AutocompleteMatchBuilder.searchWithType(type)
                              .setIsSearch(type != OmniboxSuggestionType.CLIPBOARD_URL)
                              .setUrl(url)
                              .setClipboardImageData(clipboardImageData)
                              .build();
        mModel = mProcessor.createModel();
        mProcessor.populateModel(mSuggestion, mModel, 0);
        SuggestionViewViewBinder.bind(mModel, mRootView, SuggestionViewProperties.TEXT_LINE_1_TEXT);
        SuggestionViewViewBinder.bind(mModel, mRootView, SuggestionCommonProperties.COLOR_SCHEME);
        SuggestionViewViewBinder.bind(
                mModel, mRootView, SuggestionViewProperties.IS_SEARCH_SUGGESTION);
        SuggestionViewViewBinder.bind(mModel, mRootView, SuggestionViewProperties.TEXT_LINE_2_TEXT);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void clipboardSuggestion_identifyUrlSuggestion() {
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_URL, GURL.emptyGURL());
        Assert.assertFalse(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_TEXT, GURL.emptyGURL());
        Assert.assertTrue(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_IMAGE, GURL.emptyGURL());
        Assert.assertTrue(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void clipboardSuggestion_showsFaviconWhenAvailable() {
        final ArgumentCaptor<LargeIconCallback> callback =
                ArgumentCaptor.forClass(LargeIconCallback.class);
        createClipboardSuggestionAndClickReveal(OmniboxSuggestionType.CLIPBOARD_URL, TEST_URL);
        SuggestionDrawableState icon1 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon1);

        verify(mIconBridge).getLargeIconForUrl(eq(TEST_URL), anyInt(), callback.capture());
        callback.getValue().onLargeIconAvailable(mBitmap, 0, false, 0);
        SuggestionDrawableState icon2 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon2);

        Assert.assertNotEquals(icon1, icon2);
        Assert.assertEquals(mBitmap, ((BitmapDrawable) icon2.drawable).getBitmap());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void clipboardSuggestion_showsFallbackIconWhenNoFaviconIsAvailable() {
        final ArgumentCaptor<LargeIconCallback> callback =
                ArgumentCaptor.forClass(LargeIconCallback.class);
        createClipboardSuggestionAndClickReveal(OmniboxSuggestionType.CLIPBOARD_URL, TEST_URL);
        SuggestionDrawableState icon1 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon1);

        verify(mIconBridge).getLargeIconForUrl(eq(TEST_URL), anyInt(), callback.capture());
        callback.getValue().onLargeIconAvailable(null, 0, false, 0);
        SuggestionDrawableState icon2 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon2);

        Assert.assertEquals(icon1, icon2);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void clipobardSuggestion_urlAndTextDirection() {
        final ArgumentCaptor<LargeIconCallback> callback =
                ArgumentCaptor.forClass(LargeIconCallback.class);
        // URL
        createClipboardSuggestionAndClickReveal(OmniboxSuggestionType.CLIPBOARD_URL, ARABIC_URL);
        Assert.assertFalse(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
        verify(mIconBridge).getLargeIconForUrl(eq(ARABIC_URL), anyInt(), callback.capture());
        callback.getValue().onLargeIconAvailable(null, 0, false, 0);
        Assert.assertEquals(TextView.TEXT_DIRECTION_LTR, mLastSetTextDirection);

        // Text
        createClipboardSuggestionAndClickReveal(
                OmniboxSuggestionType.CLIPBOARD_TEXT, GURL.emptyGURL());
        Assert.assertEquals(TextView.TEXT_DIRECTION_INHERIT, mLastSetTextDirection);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void clipboardSuggestion_showsThumbnailWhenAvailable() {
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        Assert.assertTrue(mBitmap.compress(Bitmap.CompressFormat.PNG, 100, baos));
        byte[] bitmapData = baos.toByteArray();
        createClipboardSuggestionAndClickReveal(
                OmniboxSuggestionType.CLIPBOARD_IMAGE, GURL.emptyGURL(), bitmapData);
        SuggestionDrawableState icon = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon);

        // Since |icon| is Bitmap -> PNG -> Bitmap, the image changed, we just check the size to
        // make sure they are same.
        Assert.assertEquals(
                mBitmap.getWidth(), ((BitmapDrawable) icon.drawable).getBitmap().getWidth());
        Assert.assertEquals(
                mBitmap.getHeight(), ((BitmapDrawable) icon.drawable).getBitmap().getHeight());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void clipboardSuggestion_thumbnailShouldResizeIfTooLarge() {
        int size = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_decoration_image_size);

        Bitmap largeBitmap = Bitmap.createBitmap(size * 2, size * 2, Config.ARGB_8888);
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        Assert.assertTrue(largeBitmap.compress(Bitmap.CompressFormat.PNG, 100, baos));
        byte[] bitmapData = baos.toByteArray();
        createClipboardSuggestionAndClickReveal(
                OmniboxSuggestionType.CLIPBOARD_IMAGE, GURL.emptyGURL(), bitmapData);
        SuggestionDrawableState icon = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon);

        Assert.assertEquals(size, ((BitmapDrawable) icon.drawable).getBitmap().getWidth());
        Assert.assertEquals(size, ((BitmapDrawable) icon.drawable).getBitmap().getHeight());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void clipboardSuggestion_revealButton() {
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_URL, GURL.emptyGURL());
        Assert.assertNotNull(mModel.get(BaseSuggestionViewProperties.ACTIONS));
        mProcessor.revealButtonClickHandler(mSuggestion, mModel);
        Assert.assertNotNull(mModel.get(BaseSuggestionViewProperties.ACTIONS));

        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_TEXT, GURL.emptyGURL());
        Assert.assertNotNull(mModel.get(BaseSuggestionViewProperties.ACTIONS));
        mProcessor.revealButtonClickHandler(mSuggestion, mModel);
        Assert.assertNotNull(mModel.get(BaseSuggestionViewProperties.ACTIONS));

        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_IMAGE, GURL.emptyGURL());
        Assert.assertNotNull(mModel.get(BaseSuggestionViewProperties.ACTIONS));
        mProcessor.revealButtonClickHandler(mSuggestion, mModel);
        Assert.assertNotNull(mModel.get(BaseSuggestionViewProperties.ACTIONS));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void clipboardSuggestion_noContentByDefault() {
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_URL, GURL.emptyGURL());
        SuggestionSpannable textLine2 = mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
        Assert.assertEquals(0, textLine2.length());

        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_TEXT, GURL.emptyGURL());
        textLine2 = mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
        Assert.assertEquals(0, textLine2.length());

        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_IMAGE, GURL.emptyGURL());
        textLine2 = mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
        Assert.assertEquals(0, textLine2.length());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void clipboardSuggestion_revealAndConcealButton() {
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_URL, GURL.emptyGURL());
        SuggestionSpannable textLine2 = mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
        Assert.assertEquals(0, textLine2.length());

        // Click reveal button
        mProcessor.revealButtonClickHandler(mSuggestion, mModel);
        textLine2 = mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
        Assert.assertNotEquals(0, textLine2.length());

        // Click conceal button
        mProcessor.concealButtonClickHandler(mSuggestion, mModel);
        textLine2 = mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
        Assert.assertEquals(0, textLine2.length());

        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_TEXT, GURL.emptyGURL());
        textLine2 = mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
        Assert.assertEquals(0, textLine2.length());

        // Click reveal button
        mProcessor.revealButtonClickHandler(mSuggestion, mModel);
        textLine2 = mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
        Assert.assertNotEquals(0, textLine2.length());

        // Click conceal button
        mProcessor.concealButtonClickHandler(mSuggestion, mModel);
        textLine2 = mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
        Assert.assertEquals(0, textLine2.length());

        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_IMAGE, GURL.emptyGURL());
        textLine2 = mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
        Assert.assertEquals(0, textLine2.length());
        // Image suggestions never have content in the text line 2.
    }
}
