// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.PRICE_CHANGE;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.SAFETY_HUB;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.SINGLE_TAB;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;

import java.util.List;

/** Unit tests for {@link MaterialSwitchWithTextListContainerView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MaterialSwitchWithTextListContainerViewUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock ListContainerViewDelegate mDelegate;

    private static final String SINGLE_TAB_TITLE = "Single Tab Title";
    private static final String SAFTY_HUB_TITLE = "Safety Hub Title";
    private static final String PRICE_CHANGE_TITLE = "Price Change Title";
    private MaterialSwitchWithTextListContainerView mContainerView;
    private List<Integer> mListContent;

    @Before
    public void setUp() {
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        View view =
                LayoutInflater.from(context)
                        .inflate(R.layout.ntp_customization_ntp_cards_bottom_sheet, null, false);

        mContainerView = view.findViewById(R.id.ntp_cards_container);
        mListContent = List.of(SINGLE_TAB, SAFETY_HUB, PRICE_CHANGE);

        when(mDelegate.getListItems()).thenReturn(mListContent);
    }

    @Test
    public void testDelegateInRenderAllListItems() {
        mContainerView.renderAllListItems(mDelegate);

        verify(mDelegate).getListItems();
        for (int type : mListContent) {
            verify(mDelegate).getListItemTitle(eq(type), any(Context.class));
            verify(mDelegate).isListItemChecked(eq(type));
            verify(mDelegate).getOnCheckedChangeListener(eq(type));
        }

        verify(mDelegate, never()).getListener(anyInt());
        verify(mDelegate, never()).getTrailingIcon(anyInt());
        verify(mDelegate, never()).getListItemSubtitle(anyInt(), any(Context.class));
    }

    @Test
    public void testRenderAllListItems() {
        // Gives the title a specific name to verify later.
        // Item 1: SINGLE_TAB
        when(mDelegate.getListItemTitle(eq(SINGLE_TAB), any())).thenReturn(SINGLE_TAB_TITLE);
        when(mDelegate.isListItemChecked(SINGLE_TAB)).thenReturn(true);

        // Item 2: SAFETY_HUB
        when(mDelegate.getListItemTitle(eq(SAFETY_HUB), any())).thenReturn(SAFTY_HUB_TITLE);
        when(mDelegate.isListItemChecked(SAFETY_HUB)).thenReturn(false);

        // Item 3: PRICE_CHANGE
        when(mDelegate.getListItemTitle(eq(PRICE_CHANGE), any())).thenReturn(PRICE_CHANGE_TITLE);
        when(mDelegate.isListItemChecked(PRICE_CHANGE)).thenReturn(true);

        // Calls the method that creates the views.
        mContainerView.renderAllListItems(mDelegate);

        assertEquals(
                "Container should have 3 child views.",
                mListContent.size(),
                mContainerView.getChildCount());

        // Verifies the first child (SINGLE_TAB)
        MaterialSwitchWithText firstChild = (MaterialSwitchWithText) mContainerView.getChildAt(0);
        assertEquals(SINGLE_TAB_TITLE, firstChild.getText());
        assertTrue("Single Tab switch should be checked", firstChild.isChecked());

        // Verifies the second child (SAFETY_HUB)
        MaterialSwitchWithText secondChild = (MaterialSwitchWithText) mContainerView.getChildAt(1);
        assertEquals(SAFTY_HUB_TITLE, secondChild.getText());
        assertFalse("Safety Hub switch should NOT be checked", secondChild.isChecked());

        // Verifies the third child (PRICE_CHANGE)
        MaterialSwitchWithText thirdChild = (MaterialSwitchWithText) mContainerView.getChildAt(2);
        assertEquals(PRICE_CHANGE_TITLE, thirdChild.getText());
        assertTrue("Price Change switch should be checked", thirdChild.isChecked());
    }

    @Test
    public void testSetAllModuleSwitchesEnabled() {
        // First, render the items to populate the view with real children.
        mContainerView.renderAllListItems(mDelegate);

        assertEquals(mListContent.size(), mContainerView.getChildCount());

        // Calls the method to test
        mContainerView.setAllModuleSwitchesEnabled(false);

        // Verifies the state of all real child views
        for (int i = 0; i < mContainerView.getChildCount(); i++) {
            assertFalse(
                    "Switch at index " + i + " should be disabled",
                    mContainerView.getChildAt(i).isEnabled());
        }

        // Calls the method again with the other value
        mContainerView.setAllModuleSwitchesEnabled(true);

        // Verifies the state again
        for (int i = 0; i < mContainerView.getChildCount(); i++) {
            assertTrue(
                    "Switch at index " + i + " should be enabled",
                    mContainerView.getChildAt(i).isEnabled());
        }
    }
}
