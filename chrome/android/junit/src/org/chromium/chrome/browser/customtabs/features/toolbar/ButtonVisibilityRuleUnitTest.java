// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewTreeObserver;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams.ButtonType;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.CustomTabsButtonState;
import org.chromium.chrome.browser.customtabs.features.toolbar.ButtonVisibilityRule.ButtonId;

/** Tests for button visibility rule in CustomTab Toolbar. */
@RunWith(BaseRobolectricTestRunner.class)
public class ButtonVisibilityRuleUnitTest {
    @Rule public MockitoRule mRule = MockitoJUnit.rule();

    @Mock View mCloseButton;
    @Mock View mMenuButton;
    @Mock View mSecurityButton;
    @Mock View mCustom1Button;
    @Mock View mCustom2Button;
    @Mock View mMinimizeButton;
    @Mock View mExpandButton;
    @Mock View mOptionalButton;

    @Mock ViewTreeObserver mViewTreeObserver;

    LayoutParams mLayoutParams = new LayoutParams(WRAP_CONTENT, WRAP_CONTENT);

    @Before
    public void setup() {
        View[] buttons =
                new View[] {
                    mCloseButton,
                    mMenuButton,
                    mSecurityButton,
                    mCustom1Button,
                    mCustom2Button,
                    mMinimizeButton,
                    mExpandButton,
                    mOptionalButton
                };
        for (View button : buttons) {
            when(button.getLayoutParams()).thenReturn(mLayoutParams);
            when(button.getMeasuredWidth()).thenReturn(48);
        }
    }

    @Test
    public void showAllButtons() {
        int toolbarWidth = 48 * 5 + 68; // close, menu, custom1, minimize, optional + url
        ButtonVisibilityRule buttonVisibilityRule =
                new ButtonVisibilityRule(68, /* activated= */ true);
        buttonVisibilityRule.setToolbarWidth(toolbarWidth);
        buttonVisibilityRule.addButton(ButtonId.CLOSE, mCloseButton, true);
        buttonVisibilityRule.addButton(ButtonId.MENU, mMenuButton, true);
        buttonVisibilityRule.addButton(ButtonId.SECURITY, mSecurityButton, false);
        buttonVisibilityRule.addButton(ButtonId.CUSTOM_1, mCustom1Button, true);
        buttonVisibilityRule.addButton(ButtonId.MINIMIZE, mMinimizeButton, true);
        buttonVisibilityRule.addButton(ButtonId.MTB, mOptionalButton, true);

        verify(mCloseButton, never()).setVisibility(eq(View.GONE));
        verify(mMenuButton, never()).setVisibility(eq(View.GONE));
        verify(mSecurityButton, never()).setVisibility(anyInt());
        verify(mCustom1Button, never()).setVisibility(eq(View.GONE));
        verify(mMinimizeButton, never()).setVisibility(eq(View.GONE));
        verify(mOptionalButton, never()).setVisibility(eq(View.GONE));
    }

    @Test
    public void hideLowPriorityButtons() {
        int toolbarWidth =
                48 * 3 + 68; // close, menu, custom1 + url. No space for minimize, optional
        ButtonVisibilityRule buttonVisibilityRule =
                new ButtonVisibilityRule(68, /* activated= */ true);
        buttonVisibilityRule.setToolbarWidth(toolbarWidth);
        buttonVisibilityRule.addButton(ButtonId.CLOSE, mCloseButton, true);
        buttonVisibilityRule.addButton(ButtonId.MENU, mMenuButton, true);
        buttonVisibilityRule.addButton(ButtonId.CUSTOM_1, mCustom1Button, true);
        buttonVisibilityRule.addButton(ButtonId.MINIMIZE, mMinimizeButton, true);
        buttonVisibilityRule.addButton(ButtonId.MTB, mOptionalButton, true);

        verify(mCloseButton, never()).setVisibility(eq(View.GONE));
        verify(mMenuButton, never()).setVisibility(eq(View.GONE));
        verify(mCustom1Button, never()).setVisibility(eq(View.GONE));
        verify(mMinimizeButton).setVisibility(eq(View.GONE));
        verify(mOptionalButton).setVisibility(eq(View.GONE));

        clearInvocations(mCloseButton);
        clearInvocations(mMenuButton);
        clearInvocations(mCustom1Button);
        clearInvocations(mMinimizeButton);
        clearInvocations(mOptionalButton);

        // Toolbar widens after device rotates to landscape. Verify all the suppressed buttons
        // become visible.
        toolbarWidth += 200;
        buttonVisibilityRule.setToolbarWidth(toolbarWidth);
        verify(mCloseButton, never()).setVisibility(anyInt());
        verify(mMenuButton, never()).setVisibility(anyInt());
        verify(mCustom1Button, never()).setVisibility(anyInt());
        verify(mMinimizeButton).setVisibility(eq(View.VISIBLE));
        verify(mOptionalButton).setVisibility(eq(View.VISIBLE));
    }

    @Test
    public void minimizeButtonGetsHigherPriorityThanDefaultDevButton_minimizeOverCustom1() {
        // Close, menu, minimize.
        // custom1 = share/default deprioritized over minimize.
        int toolbarWidth = 48 * 3 + 68;
        ButtonVisibilityRule buttonVisibilityRule =
                new ButtonVisibilityRule(68, /* activated= */ true);
        buttonVisibilityRule.setToolbarWidth(toolbarWidth);
        buttonVisibilityRule.setCustomButtonState(
                CustomTabsButtonState.BUTTON_STATE_DEFAULT,
                CustomTabsButtonState.BUTTON_STATE_DEFAULT);
        buttonVisibilityRule.addButton(ButtonId.CLOSE, mCloseButton, true);
        buttonVisibilityRule.addButton(ButtonId.MENU, mMenuButton, true);
        buttonVisibilityRule.addButtonForCustomAction(
                ButtonId.CUSTOM_1, mCustom1Button, true, ButtonType.CCT_SHARE_BUTTON);
        buttonVisibilityRule.addButton(ButtonId.MINIMIZE, mMinimizeButton, true);
        buttonVisibilityRule.addButton(ButtonId.MTB, mOptionalButton, true);

        ArgumentCaptor<Integer> captor = ArgumentCaptor.forClass(Integer.class);
        verify(mCustom1Button, atLeastOnce()).setVisibility(captor.capture());
        assertEquals(View.GONE, (long) captor.getValue());

        // Verify the last visibility set to the minimize button is VISIBLE.
        captor = ArgumentCaptor.forClass(Integer.class);
        verify(mMinimizeButton, atLeastOnce()).setVisibility(captor.capture());
        assertEquals(View.VISIBLE, (long) captor.getValue());
    }

    @Test
    public void minimizeButtonGetsHigherPriorityThanDefaultDevButton_minimizeAndCustom2() {
        // Close, menu, minimize, custom2
        // custom1 = open-in-browser/default deprioritized over minimize and custom2(share).
        int toolbarWidth = 48 * 4 + 68;
        ButtonVisibilityRule buttonVisibilityRule =
                new ButtonVisibilityRule(68, /* activated= */ true);
        buttonVisibilityRule.setToolbarWidth(toolbarWidth);
        buttonVisibilityRule.setCustomButtonState(
                CustomTabsButtonState.BUTTON_STATE_DEFAULT,
                CustomTabsButtonState.BUTTON_STATE_DEFAULT);
        buttonVisibilityRule.addButton(ButtonId.CLOSE, mCloseButton, true);
        buttonVisibilityRule.addButton(ButtonId.MENU, mMenuButton, true);
        buttonVisibilityRule.addButtonForCustomAction(
                ButtonId.CUSTOM_1, mCustom1Button, true, ButtonType.CCT_OPEN_IN_BROWSER_BUTTON);
        buttonVisibilityRule.addButtonForCustomAction(
                ButtonId.CUSTOM_2, mCustom2Button, true, ButtonType.CCT_SHARE_BUTTON);
        buttonVisibilityRule.addButton(ButtonId.MINIMIZE, mMinimizeButton, true);
        buttonVisibilityRule.addButton(ButtonId.MTB, mOptionalButton, true);

        ArgumentCaptor<Integer> captor = ArgumentCaptor.forClass(Integer.class);
        verify(mCustom1Button, atLeastOnce()).setVisibility(captor.capture());
        assertEquals(View.GONE, (long) captor.getValue());

        // Verify the last visibility set to the minimize button is VISIBLE.
        captor = ArgumentCaptor.forClass(Integer.class);
        verify(mMinimizeButton, atLeastOnce()).setVisibility(captor.capture());
        assertEquals(View.VISIBLE, (long) captor.getValue());
    }

    @Test
    public void disabledOperation() {
        int toolbarWidth =
                48 * 3 + 68; // close, menu, custom1 + url. No space for minimize, optional
        ButtonVisibilityRule buttonVisibilityRule =
                new ButtonVisibilityRule(68, /* activated= */ false);
        buttonVisibilityRule.setToolbarWidth(toolbarWidth);
        buttonVisibilityRule.addButton(ButtonId.CLOSE, mCloseButton, true);
        buttonVisibilityRule.addButton(ButtonId.MENU, mMenuButton, true);
        buttonVisibilityRule.addButton(ButtonId.CUSTOM_1, mCustom1Button, true);
        buttonVisibilityRule.addButton(ButtonId.MINIMIZE, mMinimizeButton, true);
        buttonVisibilityRule.addButton(ButtonId.MTB, mOptionalButton, true);

        // Verify that buttons' visibility get never updated.
        verify(mCloseButton, never()).setVisibility(anyInt());
        verify(mMenuButton, never()).setVisibility(anyInt());
        verify(mCustom1Button, never()).setVisibility(anyInt());
        verify(mMinimizeButton, never()).setVisibility(anyInt());
        verify(mOptionalButton, never()).setVisibility(anyInt());
    }

    @Test
    public void skipControllingDisabledButton() {
        int toolbarWidth =
                48 * 2 + 68; // Allow close, menu buttons. Minimize is off due to other condition.
        ButtonVisibilityRule buttonVisibilityRule =
                new ButtonVisibilityRule(68, /* activated= */ true);

        buttonVisibilityRule.addButton(ButtonId.CLOSE, mCloseButton, true);
        buttonVisibilityRule.addButton(ButtonId.MENU, mMenuButton, true);
        buttonVisibilityRule.addButton(ButtonId.MINIMIZE, mMinimizeButton, true);
        buttonVisibilityRule.update(ButtonId.MINIMIZE, false);
        // Calling order of |addButton| and |setToolbarWidth()| shouldn't matter.
        buttonVisibilityRule.setToolbarWidth(toolbarWidth);
        buttonVisibilityRule.addButton(ButtonId.MTB, mOptionalButton, true);
        buttonVisibilityRule.addButton(ButtonId.CUSTOM_1, mCustom1Button, true);

        verify(mCloseButton, never()).setVisibility(eq(View.GONE));
        verify(mMenuButton, never()).setVisibility(eq(View.GONE));
        verify(mCustom1Button).setVisibility(eq(View.GONE)); // Suppressed
        verify(mMinimizeButton, never()).setVisibility(eq(View.GONE));
        verify(mOptionalButton).setVisibility(eq(View.GONE)); // Suppressed

        clearInvocations(mCloseButton);
        clearInvocations(mMenuButton);
        clearInvocations(mCustom1Button);
        clearInvocations(mMinimizeButton);
        clearInvocations(mOptionalButton);

        // Toolbar widens after device rotates to landscape. Verify the hidden
        // buttons become visible except MINIMIZE which should remain hidden.
        toolbarWidth += 200;
        buttonVisibilityRule.setToolbarWidth(toolbarWidth);
        verify(mCloseButton, never()).setVisibility(anyInt());
        verify(mMenuButton, never()).setVisibility(anyInt());
        verify(mCustom1Button).setVisibility(eq(View.VISIBLE)); // Unsuppressed
        verify(mMinimizeButton, never()).setVisibility(anyInt()); // Remains hidden
        verify(mOptionalButton).setVisibility(eq(View.VISIBLE)); // Unsuppressed

        // Toolbar shortends after rotating back.
        toolbarWidth -= 200;
        buttonVisibilityRule.setToolbarWidth(toolbarWidth);
        verify(mCloseButton, never()).setVisibility(anyInt());
        verify(mMenuButton, never()).setVisibility(anyInt());
        verify(mCustom1Button).setVisibility(eq(View.GONE)); // Suppressed
        verify(mMinimizeButton, never()).setVisibility(anyInt()); // Remains hidden
        verify(mOptionalButton).setVisibility(eq(View.GONE)); // Suppressed
    }

    @Test
    public void allButtonsHidden() {
        int toolbarWidth = 24; // Too narrow to contain even a single button or toolbar
        ButtonVisibilityRule buttonVisibilityRule =
                new ButtonVisibilityRule(68, /* activated= */ true);
        buttonVisibilityRule.setToolbarWidth(toolbarWidth);
        buttonVisibilityRule.addButton(ButtonId.CLOSE, mCloseButton, true);
        buttonVisibilityRule.addButton(ButtonId.MENU, mMenuButton, true);
        buttonVisibilityRule.addButton(ButtonId.CUSTOM_1, mCustom1Button, true);
        buttonVisibilityRule.addButton(ButtonId.MINIMIZE, mMinimizeButton, true);
        buttonVisibilityRule.addButton(ButtonId.MTB, mOptionalButton, true);

        // Verify that all the buttons are gone. The checker should still work without assertion.
        verify(mCloseButton).setVisibility(eq(View.GONE));
        verify(mMenuButton).setVisibility(eq(View.GONE));
        verify(mCustom1Button).setVisibility(eq(View.GONE));
        verify(mMinimizeButton).setVisibility(eq(View.GONE));
        verify(mOptionalButton).setVisibility(eq(View.GONE));
    }

    @Test
    public void delayRefreshTillViewsFinishMeasurementPhase() {
        int toolbarWidth = 24 + 68; // close button should be hidden
        ButtonVisibilityRule buttonVisibilityRule =
                new ButtonVisibilityRule(68, /* activated= */ true);
        when(mCloseButton.getMeasuredWidth()).thenReturn(0);
        when(mCloseButton.getViewTreeObserver()).thenReturn(mViewTreeObserver);
        var onPreDrawCaptor = ArgumentCaptor.forClass(ViewTreeObserver.OnPreDrawListener.class);

        // Adding button doesn't lead to refresh operation hiding the button, since the button
        // hasn't finished the measurement phase.
        buttonVisibilityRule.setToolbarWidth(toolbarWidth);
        buttonVisibilityRule.addButton(ButtonId.CLOSE, mCloseButton, true);

        verify(mViewTreeObserver).addOnPreDrawListener(onPreDrawCaptor.capture());
        verify(mCloseButton, never()).setVisibility(anyInt());

        // Now that the measurement is done, the delayed refresh() should run to hide the button.
        when(mCloseButton.getMeasuredWidth()).thenReturn(48);
        onPreDrawCaptor.getValue().onPreDraw();

        verify(mCloseButton).setVisibility(eq(View.GONE));
    }
}
