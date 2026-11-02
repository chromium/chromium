// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ClipData;
import android.content.ClipData.Item;
import android.content.ClipDescription;
import android.content.Intent;
import android.net.Uri;
import android.view.DragEvent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.dragdrop.DragDropMetricUtils;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropType;
import org.chromium.url.GURL;

/** Tests for {@link TabStripLinkDragDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.TAB_STRIP_LINK_DRAG_DROP)
public class TabStripLinkDragDelegateTest {
    private static final String LINK_URL = "https://example.com/";
    private static final String SEARCH_URL = "https://www.google.com/search?q=hello";
    private static final float X_DP = 50f;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private StripLayoutHelper mStripLayoutHelper;
    @Mock private Profile mProfile;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private AutocompleteController mAutocompleteController;

    private TabStripLinkDragDelegate mDelegate;

    @Before
    public void setUp() {
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        AutocompleteController.setInstanceForTesting(mAutocompleteController);
        when(mTemplateUrlService.getUrlForSearchQuery(anyString())).thenReturn(SEARCH_URL);

        mDelegate = new TabStripLinkDragDelegate(() -> mStripLayoutHelper, () -> mProfile);
    }

    private static ClipDescription createClipDescription(String... mimeTypes) {
        return new ClipDescription(/* label= */ null, mimeTypes);
    }

    private static DragEvent mockDropEvent(Item item) {
        DragEvent event = mock(DragEvent.class);
        when(event.getAction()).thenReturn(DragEvent.ACTION_DROP);
        when(event.getClipData())
                .thenReturn(
                        new ClipData(
                                /* label= */ null,
                                new String[] {ClipDescription.MIMETYPE_TEXT_PLAIN},
                                item));
        return event;
    }

    private static Intent createLinkIntent(String url) {
        Intent intent = new Intent();
        intent.setData(Uri.parse(url));
        return intent;
    }

    private void setClassifiedUrl(String text, String url) {
        AutocompleteMatch match = mock(AutocompleteMatch.class);
        when(match.getUrl()).thenReturn(new GURL(url));
        when(mAutocompleteController.classify(text)).thenReturn(match);
    }

    private static GURL extractUrl(Item item) {
        return TabStripLinkDragDelegate.extractUrlFromItem(item);
    }

    @Test
    public void testCanHandle_linkMimeTypes() {
        for (String mimeType :
                new String[] {
                    ClipDescription.MIMETYPE_TEXT_PLAIN,
                    ClipDescription.MIMETYPE_TEXT_URILIST,
                    ClipDescription.MIMETYPE_TEXT_INTENT
                }) {
            assertTrue(
                    "Link drag should be handled for " + mimeType,
                    mDelegate.canHandle(createClipDescription(mimeType)));
        }
    }

    @Test
    public void testCanHandle_rejectsOtherData() {
        assertFalse(mDelegate.canHandle(null));
        assertFalse(mDelegate.canHandle(createClipDescription("image/png")));
        assertFalse(
                "Browser content should be left to the tab drag logic.",
                mDelegate.canHandle(
                        createClipDescription(
                                MimeTypeUtils.CHROME_MIMETYPE_TAB,
                                ClipDescription.MIMETYPE_TEXT_PLAIN)));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_LINK_DRAG_DROP)
    public void testCanHandle_flagDisabled() {
        assertFalse(
                mDelegate.canHandle(createClipDescription(ClipDescription.MIMETYPE_TEXT_PLAIN)));
    }

    @Test
    public void testDragEvents_forwardedToStrip() {
        assertTrue(mDelegate.onDragEnter(X_DP));
        verify(mStripLayoutHelper).handleLinkDragEnter(X_DP);

        assertTrue(mDelegate.onDragWithin(X_DP + 1));
        verify(mStripLayoutHelper).handleLinkDragWithin(X_DP + 1);

        assertTrue(mDelegate.onDragExit());
        verify(mStripLayoutHelper).handleLinkDragExit();
    }

    @Test
    public void testOnDrop_opensLink() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        DragDropMetricUtils.HISTOGRAM_DRAG_DROP_TAB_TYPE,
                        DragDropType.LINK_TO_TAB_STRIP);

        assertTrue(
                "Link drop should be handled.",
                mDelegate.onDrop(
                        mockDropEvent(new Item("title", createLinkIntent(LINK_URL), null)), X_DP));

        ArgumentCaptor<GURL> urlCaptor = ArgumentCaptor.forClass(GURL.class);
        verify(mStripLayoutHelper).handleLinkDrop(eq(X_DP), urlCaptor.capture());
        assertEquals(LINK_URL, urlCaptor.getValue().getSpec());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnDrop_noProfile_ignored() {
        mDelegate = new TabStripLinkDragDelegate(() -> mStripLayoutHelper, () -> null);

        assertFalse(
                "Text drop without a profile should not be handled.",
                mDelegate.onDrop(mockDropEvent(new Item("hello world")), X_DP));
        verify(mStripLayoutHelper, never()).handleLinkDrop(anyFloat(), any());
    }

    @Test
    public void testExtractUrl_fromIntentData() {
        GURL url = extractUrl(new Item("title", createLinkIntent(LINK_URL), null));
        assertNotNull(url);
        assertEquals(LINK_URL, url.getSpec());
    }

    @Test
    public void testExtractUrl_fromUri() {
        GURL url = extractUrl(new Item(null, null, Uri.parse(LINK_URL)));
        assertNotNull(url);
        assertEquals(LINK_URL, url.getSpec());
    }

    @Test
    public void testExtractUrl_ignoresText() {
        // Text isn't parsed for URLs, even if it contains one.
        assertNull(extractUrl(new Item("Link title\n" + LINK_URL)));
        assertNull(extractUrl(new Item(LINK_URL)));
        assertNull(extractUrl(new Item("hello world")));
    }

    @Test
    public void testExtractUrl_rejectsNonHttpSchemes() {
        String javascriptUrl = "javascript:alert(1)";
        assertNull(extractUrl(new Item(javascriptUrl, createLinkIntent(javascriptUrl), null)));
        assertNull(extractUrl(new Item(null, null, Uri.parse("content://example/1"))));
    }

    @Test
    public void testGetUrlForText_opensClassifiedUrl() {
        setClassifiedUrl("example.com/foo", "http://example.com/foo");

        GURL url = TabStripLinkDragDelegate.getUrlForText(mProfile, " example.com/foo\n");
        assertNotNull(url);
        assertEquals("http://example.com/foo", url.getSpec());
        verify(mTemplateUrlService, never()).getUrlForSearchQuery(any());
    }

    @Test
    public void testGetUrlForText_nonHttpUrl_searches() {
        String javascriptUrl = "javascript:alert(1)";
        setClassifiedUrl(javascriptUrl, javascriptUrl);

        assertEquals(
                new GURL(SEARCH_URL),
                TabStripLinkDragDelegate.getUrlForText(mProfile, javascriptUrl));
        verify(mTemplateUrlService).getUrlForSearchQuery(javascriptUrl);
    }

    @Test
    public void testGetUrlForText_noClassification_searches() {
        assertEquals(
                new GURL(SEARCH_URL), TabStripLinkDragDelegate.getUrlForText(mProfile, "hello"));
        verify(mTemplateUrlService).getUrlForSearchQuery("hello");
    }

    @Test
    public void testGetUrlForText_longText_searchesWithoutClassifying() {
        String longText = "a".repeat(TabStripLinkDragDelegate.MAX_TEXT_LENGTH_TO_CLASSIFY + 1);

        assertEquals(
                new GURL(SEARCH_URL), TabStripLinkDragDelegate.getUrlForText(mProfile, longText));
        verify(mAutocompleteController, never()).classify(any());
        verify(mTemplateUrlService).getUrlForSearchQuery(longText);
    }

    @Test
    public void testGetUrlForText_invalidSearchUrl_returnsNull() {
        when(mTemplateUrlService.getUrlForSearchQuery("hello")).thenReturn("");

        assertNull(TabStripLinkDragDelegate.getUrlForText(mProfile, "hello"));
    }

    @Test
    public void testGetUrlForText_emptyText_returnsNull() {
        assertNull(TabStripLinkDragDelegate.getUrlForText(mProfile, " \n\t "));
        verify(mAutocompleteController, never()).classify(any());
        verify(mTemplateUrlService, never()).getUrlForSearchQuery(any());
    }
}
