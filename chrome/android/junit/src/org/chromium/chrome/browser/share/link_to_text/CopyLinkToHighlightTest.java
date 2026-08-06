// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.link_to_text;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.SelectionActionMenuClientWrapper.MenuType;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.selection.TextSelectionActionMenuDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content.browser.selection.SelectionPopupControllerImpl;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.List;

/** Unit tests for Copy Link to Highlight menu item in {@link TextSelectionActionMenuDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CopyLinkToHighlightTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private RenderFrameHost mRenderFrameHost;
    @Mock private SelectionPopupControllerImpl mSelectionPopupController;
    @Mock private LinkToTextBridge.Natives mLinkToTextBridgeJniMock;
    @Mock private View mContainerView;

    private TextSelectionActionMenuDelegate mDelegate;
    private static final GURL TEST_URL = new GURL("https://example.com");

    @Before
    public void setUp() {
        LinkToTextBridgeJni.setInstanceForTesting(mLinkToTextBridgeJniMock);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getUrl()).thenReturn(TEST_URL);
        when(mWebContents.getFocusedFrame()).thenReturn(mRenderFrameHost);
        when(mWebContents.getOrSetUserData(SelectionPopupControllerImpl.class, null))
                .thenReturn(mSelectionPopupController);
        when(mSelectionPopupController.getSelectedText()).thenReturn("selected text");

        mDelegate = new TextSelectionActionMenuDelegate(mTab);
    }

    @Test
    public void testGetAdditionalMenuItems_FeatureEnabled() {
        FeatureOverrides.overrideFlag(ChromeFeatureList.COPY_LINK_TO_HIGHLIGHT, true);
        List<SelectionMenuItem> items =
                mDelegate.getAdditionalMenuItems(
                        MenuType.DROPDOWN,
                        /* isSelectionPassword= */ false,
                        /* isSelectionReadOnly= */ true,
                        "selected text");
        assertTrue(hasCopyLinkToHighlight(items));
    }

    @Test
    public void testGetAdditionalMenuItems_EmptySelectionText() {
        FeatureOverrides.overrideFlag(ChromeFeatureList.COPY_LINK_TO_HIGHLIGHT, true);
        List<SelectionMenuItem> items =
                mDelegate.getAdditionalMenuItems(
                        MenuType.DROPDOWN,
                        /* isSelectionPassword= */ false,
                        /* isSelectionReadOnly= */ true,
                        "");
        assertFalse(hasCopyLinkToHighlight(items));
    }

    @Test
    public void testGetAdditionalMenuItems_SelectionTextIsPassword() {
        FeatureOverrides.overrideFlag(ChromeFeatureList.COPY_LINK_TO_HIGHLIGHT, true);
        List<SelectionMenuItem> items =
                mDelegate.getAdditionalMenuItems(
                        MenuType.DROPDOWN,
                        /* isSelectionPassword= */ true,
                        /* isSelectionReadOnly= */ true,
                        "*password*");
        assertFalse(hasCopyLinkToHighlight(items));
    }

    @Test
    public void testGetAdditionalMenuItems_EditableText() {
        FeatureOverrides.overrideFlag(ChromeFeatureList.COPY_LINK_TO_HIGHLIGHT, true);
        List<SelectionMenuItem> items =
                mDelegate.getAdditionalMenuItems(
                        MenuType.DROPDOWN,
                        /* isSelectionPassword= */ false,
                        /* isSelectionReadOnly= */ false,
                        "selected text");
        assertFalse(hasCopyLinkToHighlight(items));
    }

    @Test
    public void testHandleMenuItemClick() {
        FeatureOverrides.overrideFlag(ChromeFeatureList.COPY_LINK_TO_HIGHLIGHT, true);
        SelectionMenuItem item =
                new SelectionMenuItem.Builder("Copy link to highlight")
                        .setId(R.id.contextmenu_copy_link_to_highlight)
                        .setGroupId(org.chromium.content.R.id.select_action_menu_delegate_items)
                        .build();

        boolean handled = mDelegate.handleMenuItemClick(item, mWebContents, mContainerView);
        assertTrue(handled);

        // Verify that LinkToTextBridge was called
        verify(mLinkToTextBridgeJniMock).shouldOfferLinkToText(TEST_URL);
    }

    private boolean hasCopyLinkToHighlight(List<SelectionMenuItem> items) {
        return items.stream().anyMatch(item -> item.id == R.id.contextmenu_copy_link_to_highlight);
    }
}
