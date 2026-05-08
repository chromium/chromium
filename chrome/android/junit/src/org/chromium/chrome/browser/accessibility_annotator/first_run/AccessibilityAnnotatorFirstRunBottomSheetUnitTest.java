// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility_annotator.first_run;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;

/** Unit tests for {@link AccessibilityAnnotatorFirstRunBottomSheet}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AccessibilityAnnotatorFirstRunBottomSheetUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private AccessibilityAnnotatorFirstRunBottomSheetContent mContent;
    @Mock private AccessibilityAnnotatorFirstRunBottomSheetCoordinator.Delegate mDelegate;
    @Mock private SettingsCustomTabLauncher mCustomTabLauncher;

    private AccessibilityAnnotatorFirstRunBottomSheetMediator mMediator;

    private static final String MANAGE_SETTINGS_URL = "https://example.com/manage";
    private static final String LEARN_MORE_URL = "https://example.com/learn_more";

    @Before
    public void setUp() {
        mMediator =
                new AccessibilityAnnotatorFirstRunBottomSheetMediator(
                        mContext, mBottomSheetController, mContent, mDelegate, mCustomTabLauncher);
    }

    @Test
    public void testOnManageSettingsClicked() {
        mMediator.requestShowContent(MANAGE_SETTINGS_URL, LEARN_MORE_URL);
        mMediator.onManageSettingsClicked();

        verify(mCustomTabLauncher).openUrlInCct(mContext, MANAGE_SETTINGS_URL);
        verify(mDelegate).onManageSettingsClicked();
    }

    @Test
    public void testOnLearnMoreClicked() {
        mMediator.requestShowContent(MANAGE_SETTINGS_URL, LEARN_MORE_URL);
        mMediator.onLearnMoreClicked();

        verify(mCustomTabLauncher).openUrlInCct(mContext, LEARN_MORE_URL);
        verify(mDelegate).onLearnMoreClicked();
    }

    @Test
    public void testOnAcknowledgeClicked() {
        mMediator.onAcknowledgeClicked();

        verify(mDelegate).onInfoAcknowledged();
        verify(mBottomSheetController)
                .hideContent(mContent, true, StateChangeReason.INTERACTION_COMPLETE);
    }

    @Test
    public void testOnSheetClosed_Dismissed() {
        ArgumentCaptor<BottomSheetObserver> captor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        when(mBottomSheetController.requestShowContent(mContent, true)).thenReturn(true);
        mMediator.requestShowContent(MANAGE_SETTINGS_URL, LEARN_MORE_URL);
        verify(mBottomSheetController).addObserver(captor.capture());

        captor.getValue().onSheetClosed(StateChangeReason.SWIPE);

        verify(mDelegate).onInfoDismissed();
        verify(mBottomSheetController).removeObserver(captor.getValue());
    }

    @Test
    public void testOnSheetClosed_Acknowledge() {
        ArgumentCaptor<BottomSheetObserver> captor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        when(mBottomSheetController.requestShowContent(mContent, true)).thenReturn(true);
        mMediator.requestShowContent(MANAGE_SETTINGS_URL, LEARN_MORE_URL);
        verify(mBottomSheetController).addObserver(captor.capture());

        captor.getValue().onSheetClosed(StateChangeReason.INTERACTION_COMPLETE);

        verify(mDelegate, never()).onInfoDismissed();
        verify(mBottomSheetController).removeObserver(captor.getValue());
    }
}
