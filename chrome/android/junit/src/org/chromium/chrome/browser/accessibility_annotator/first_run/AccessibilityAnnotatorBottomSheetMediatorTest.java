// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility_annotator.first_run;

import static org.mockito.Mockito.verify;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;

/** Tests for {@link AccessibilityAnnotatorBottomSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AccessibilityAnnotatorBottomSheetMediatorTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private AccessibilityAnnotatorBottomSheetContent mContent;
    @Mock private AccessibilityAnnotatorBottomSheetCoordinator.Delegate mDelegate;
    @Mock private SettingsCustomTabLauncher mCustomTabLauncher;

    private AccessibilityAnnotatorBottomSheetMediator mMediator;

    private static final String MANAGE_SETTINGS_URL = "https://example.com/manage";
    private static final String LEARN_MORE_URL = "https://example.com/learn_more";

    @Before
    public void setUp() {
        mMediator =
                new AccessibilityAnnotatorBottomSheetMediator(
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
}
