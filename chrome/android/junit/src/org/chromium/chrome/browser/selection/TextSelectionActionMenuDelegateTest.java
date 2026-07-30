// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.selection;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.SelectionActionMenuClientWrapper.MenuType;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.List;

/** Unit tests for {@link TextSelectionActionMenuDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.COPY_LINK_TO_HIGHLIGHT)
public class TextSelectionActionMenuDelegateTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private ReaderModeManager mReaderModeManager;
    @Mock private WebContents mWebContents;
    @Mock private DomDistillerUrlUtilsJni mDomDistillerUrlUtilsJni;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private TemplateUrl mTemplateUrl;

    private MockTab mTab;
    private TextSelectionActionMenuDelegate mDelegate;

    private boolean containsId(List<SelectionMenuItem> items, int id) {
        for (SelectionMenuItem item : items) {
            if (item.id == id) {
                return true;
            }
        }
        return false;
    }

    @Before
    public void setUp() {
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);

        DomDistillerUrlUtilsJni.setInstanceForTesting(mDomDistillerUrlUtilsJni);
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(false);

        mTab = MockTab.createAndInitialize(1, mProfile);
        mTab.setUrl(new GURL("https://example.com"));
        mTab.getUserDataHost().setUserData(ReaderModeManager.class, mReaderModeManager);

        mDelegate = new TextSelectionActionMenuDelegate(mTab);
    }

    @Test
    public void testGetAdditionalMenuItems_standardWebPage() {
        List<SelectionMenuItem> items =
                mDelegate.getAdditionalMenuItems(
                        MenuType.DROPDOWN,
                        /* isSelectionPassword= */ false,
                        /* isSelectionReadOnly= */ true,
                        /* selectedText= */ "test");

        assertEquals(2, items.size());
        SelectionMenuItem item = items.get(0);
        assertEquals(R.id.contextmenu_open_in_reading_mode, item.id);

        boolean handled =
                mDelegate.handleMenuItemClick(item, mWebContents, /* containerView= */ null);
        assertTrue(handled);
        verify(mReaderModeManager).activateReaderMode(ReaderModeManager.EntryPoint.CONTEXT_MENU);
    }

    @Test
    public void testGetAdditionalMenuItems_chromeUrl() {
        mTab.setUrl(new GURL("chrome://settings"));

        List<SelectionMenuItem> items =
                mDelegate.getAdditionalMenuItems(
                        MenuType.DROPDOWN,
                        /* isSelectionPassword= */ false,
                        /* isSelectionReadOnly= */ true,
                        /* selectedText= */ "test");

        assertFalse(containsId(items, R.id.contextmenu_open_in_reading_mode));
    }

    @Test
    public void testGetAdditionalMenuItems_nativePage() {
        mTab.setIsNativePage(true);

        List<SelectionMenuItem> items =
                mDelegate.getAdditionalMenuItems(
                        MenuType.DROPDOWN,
                        /* isSelectionPassword= */ false,
                        /* isSelectionReadOnly= */ true,
                        /* selectedText= */ "test");

        assertFalse(containsId(items, R.id.contextmenu_open_in_reading_mode));
    }

    @Test
    public void testGetAdditionalMenuItems_distilledPage() {
        GURL url = new GURL(UrlConstants.DISTILLER_SCHEME + "://example.com");
        mTab.setUrl(url);
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(true);

        List<SelectionMenuItem> items =
                mDelegate.getAdditionalMenuItems(
                        MenuType.DROPDOWN,
                        /* isSelectionPassword= */ false,
                        /* isSelectionReadOnly= */ true,
                        /* selectedText= */ "test");

        assertFalse(containsId(items, R.id.contextmenu_open_in_reading_mode));
    }

    @Test
    public void testGetWebSearchMenuItemTitle_valid() {
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        when(mTemplateUrlService.getDefaultSearchEngineTemplateUrl()).thenReturn(mTemplateUrl);
        when(mTemplateUrl.getKeyword()).thenReturn("google");
        when(mTemplateUrlService.getFullNameFromTemplateUrl("google")).thenReturn("Google");

        Context context = ApplicationProvider.getApplicationContext();
        String title = mDelegate.getWebSearchMenuItemTitle(context, "test query");

        assertEquals(
                context.getString(R.string.contextmenu_search_web_for_text, "Google", "test query"),
                title);
    }

    @Test
    public void testGetWebSearchMenuItemTitle_nullOrEmpty() {
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        Context context = ApplicationProvider.getApplicationContext();

        // TemplateUrl null
        when(mTemplateUrlService.getDefaultSearchEngineTemplateUrl()).thenReturn(null);
        assertNull(mDelegate.getWebSearchMenuItemTitle(context, "test"));

        // Full name empty
        when(mTemplateUrlService.getDefaultSearchEngineTemplateUrl()).thenReturn(mTemplateUrl);
        when(mTemplateUrl.getKeyword()).thenReturn("google");
        when(mTemplateUrlService.getFullNameFromTemplateUrl("google")).thenReturn("");
        assertNull(mDelegate.getWebSearchMenuItemTitle(context, "test"));

        // Selected text empty
        when(mTemplateUrlService.getFullNameFromTemplateUrl("google")).thenReturn("Google");
        assertNull(mDelegate.getWebSearchMenuItemTitle(context, ""));
    }
}
