// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.selection;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
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

import org.chromium.base.FeatureOverrides;
import org.chromium.base.SelectionActionMenuClientWrapper.MenuType;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicKeyedService;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlicInvocationSource;
import org.chromium.chrome.browser.glic.GlicKeyedServiceFactory;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.SelectionMenuItem.ItemGroupOffset;
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
    @Mock private GlicKeyedService mGlicKeyedService;

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

        // These two features gate different menu items (Ask Gemini and copy-link-to-highlight).
        // Disable both by default so existing expectations hold; tests that exercise a specific
        // entry point re-enable the relevant feature explicitly.
        FeatureOverrides.disable(ChromeFeatureList.CLANK_GLIC_CONTEXT_MENU);
        FeatureOverrides.disable(ChromeFeatureList.COPY_LINK_TO_HIGHLIGHT);

        DomDistillerUrlUtilsJni.setInstanceForTesting(mDomDistillerUrlUtilsJni);
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(false);

        mTab = MockTab.createAndInitialize(1, mProfile);
        mTab.setUrl(new GURL("https://example.com"));
        mTab.getUserDataHost().setUserData(ReaderModeManager.class, mReaderModeManager);

        mDelegate = new TextSelectionActionMenuDelegate(mTab);
    }

    /**
     * Enables all conditions required for the "Ask Gemini" selection item to be shown on mobile.
     */
    private void enableAskGeminiForSelection() {
        FeatureOverrides.enable(ChromeFeatureList.CLANK_GLIC_CONTEXT_MENU);
        FeatureOverrides.enable(ChromeFeatureList.TAB_BOTTOM_SHEET);
        FeatureOverrides.disable(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL);
        GlicEnabling.setEnabledForTesting(true);
    }

    private static SelectionMenuItem findItem(List<SelectionMenuItem> items, int id) {
        for (SelectionMenuItem item : items) {
            if (item.id == id) return item;
        }
        return null;
    }

    @Test
    public void testGetAdditionalMenuItems_standardWebPage() {
        // This test exercises the copy-link entry point, which setUp() disables by default.
        FeatureOverrides.enable(ChromeFeatureList.COPY_LINK_TO_HIGHLIGHT);

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
    public void testAskGemini_shownOnFloatingMenu() {
        enableAskGeminiForSelection();

        List<SelectionMenuItem> items =
                mDelegate.getAdditionalMenuItems(
                        MenuType.FLOATING,
                        /* isSelectionPassword= */ false,
                        /* isSelectionReadOnly= */ true,
                        /* selectedText= */ "test");

        assertNotNull(findItem(items, R.id.contextmenu_ask_gemini));
    }

    @Test
    public void testAskGemini_notShownOnDropdownMenu() {
        enableAskGeminiForSelection();

        List<SelectionMenuItem> items =
                mDelegate.getAdditionalMenuItems(
                        MenuType.DROPDOWN,
                        /* isSelectionPassword= */ false,
                        /* isSelectionReadOnly= */ true,
                        /* selectedText= */ "test");

        assertNull(findItem(items, R.id.contextmenu_ask_gemini));
    }

    @Test
    public void testAskGemini_notShownWhenFeatureDisabled() {
        // CLANK_GLIC_CONTEXT_MENU stays disabled (from setUp).
        List<SelectionMenuItem> items =
                mDelegate.getAdditionalMenuItems(
                        MenuType.FLOATING,
                        /* isSelectionPassword= */ false,
                        /* isSelectionReadOnly= */ true,
                        /* selectedText= */ "test");

        assertNull(findItem(items, R.id.contextmenu_ask_gemini));
    }

    @Test
    public void testAskGemini_notShownForPasswordOrEmptySelection() {
        enableAskGeminiForSelection();

        assertNull(
                findItem(
                        mDelegate.getAdditionalMenuItems(
                                MenuType.FLOATING,
                                /* isSelectionPassword= */ true,
                                /* isSelectionReadOnly= */ true,
                                /* selectedText= */ "secret"),
                        R.id.contextmenu_ask_gemini));

        assertNull(
                findItem(
                        mDelegate.getAdditionalMenuItems(
                                MenuType.FLOATING,
                                /* isSelectionPassword= */ false,
                                /* isSelectionReadOnly= */ true,
                                /* selectedText= */ ""),
                        R.id.contextmenu_ask_gemini));
    }

    @Test
    public void testAskGemini_notShownWhenSidePanelEnabled() {
        enableAskGeminiForSelection();
        FeatureOverrides.enable(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL);

        List<SelectionMenuItem> items =
                mDelegate.getAdditionalMenuItems(
                        MenuType.FLOATING,
                        /* isSelectionPassword= */ false,
                        /* isSelectionReadOnly= */ true,
                        /* selectedText= */ "test");

        assertNull(findItem(items, R.id.contextmenu_ask_gemini));
    }

    @Test
    public void testAskGemini_notShownOnIncognito() {
        enableAskGeminiForSelection();
        when(mProfile.isOffTheRecord()).thenReturn(true);

        List<SelectionMenuItem> items =
                mDelegate.getAdditionalMenuItems(
                        MenuType.FLOATING,
                        /* isSelectionPassword= */ false,
                        /* isSelectionReadOnly= */ true,
                        /* selectedText= */ "test");

        assertNull(findItem(items, R.id.contextmenu_ask_gemini));
    }

    @Test
    public void testAskGemini_orderAndCategoryDefaultPosition() {
        enableAskGeminiForSelection();

        List<SelectionMenuItem> items =
                mDelegate.getAdditionalMenuItems(
                        MenuType.FLOATING,
                        /* isSelectionPassword= */ false,
                        /* isSelectionReadOnly= */ true,
                        /* selectedText= */ "test");
        SelectionMenuItem askGemini = findItem(items, R.id.contextmenu_ask_gemini);
        assertNotNull(askGemini);

        assertTrue(askGemini.order < ItemGroupOffset.DEFAULT_ITEMS);
    }

    @Test
    public void testAskGemini_orderAndCategorySecondaryPosition() {
        enableAskGeminiForSelection();
        FeatureOverrides.newBuilder()
                .enable(ChromeFeatureList.CLANK_GLIC_CONTEXT_MENU)
                .param(
                        TextSelectionActionMenuDelegate.PARAM_ASK_GEMINI_SELECTION_MENU_POSITION,
                        TextSelectionActionMenuDelegate.ASK_GEMINI_POSITION_SECONDARY)
                .apply();

        List<SelectionMenuItem> items =
                mDelegate.getAdditionalMenuItems(
                        MenuType.FLOATING,
                        /* isSelectionPassword= */ false,
                        /* isSelectionReadOnly= */ true,
                        /* selectedText= */ "test");
        SelectionMenuItem askGemini = findItem(items, R.id.contextmenu_ask_gemini);
        assertNotNull(askGemini);

        assertTrue(askGemini.order >= ItemGroupOffset.SECONDARY_ASSIST_ITEMS);
        assertTrue(askGemini.order < ItemGroupOffset.TEXT_PROCESSING_ITEMS);
    }

    @Test
    public void testAskGemini_handleClickInvokesGlic() {
        enableAskGeminiForSelection();
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);

        List<SelectionMenuItem> items =
                mDelegate.getAdditionalMenuItems(
                        MenuType.FLOATING,
                        /* isSelectionPassword= */ false,
                        /* isSelectionReadOnly= */ true,
                        /* selectedText= */ "test");
        SelectionMenuItem askGemini = findItem(items, R.id.contextmenu_ask_gemini);
        assertNotNull(askGemini);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Glic.EntryPoint.Click.Other",
                        GlicInvocationSource.WEB_CONTENTS_CONTEXT_MENU);

        boolean handled =
                mDelegate.handleMenuItemClick(askGemini, mWebContents, /* containerView= */ null);

        assertTrue(handled);
        verify(mGlicKeyedService)
                .invokeWithPrompt(mTab, "test", GlicInvocationSource.WEB_CONTENTS_CONTEXT_MENU);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testAskGemini_handleClickInvokesGlicWithoutTextWhenParamDisabled() {
        enableAskGeminiForSelection();
        FeatureOverrides.newBuilder()
                .enable(ChromeFeatureList.CLANK_GLIC_CONTEXT_MENU)
                .param(TextSelectionActionMenuDelegate.PARAM_ASK_GEMINI_SEND_SELECTED_TEXT, "false")
                .apply();
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);

        List<SelectionMenuItem> items =
                mDelegate.getAdditionalMenuItems(
                        MenuType.FLOATING,
                        /* isSelectionPassword= */ false,
                        /* isSelectionReadOnly= */ true,
                        /* selectedText= */ "test");
        SelectionMenuItem askGemini = findItem(items, R.id.contextmenu_ask_gemini);
        assertNotNull(askGemini);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Glic.EntryPoint.Click.Other",
                        GlicInvocationSource.WEB_CONTENTS_CONTEXT_MENU);

        boolean handled =
                mDelegate.handleMenuItemClick(askGemini, mWebContents, /* containerView= */ null);

        assertTrue(handled);
        verify(mGlicKeyedService).invoke(mTab, GlicInvocationSource.WEB_CONTENTS_CONTEXT_MENU);
        histogramWatcher.assertExpected();
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
