// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.VISIBLE;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.ScreenType;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests for {@link PasswordMigrationWarningMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class PasswordMigrationWarningMediatorTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private PasswordMigrationWarningMediator mMediator = new PasswordMigrationWarningMediator();
    private PropertyModel mModel;

    @Mock
    private BottomSheetController mBottomSheetController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mModel = PasswordMigrationWarningProperties.createDefaultModel(
                mMediator::onDismissed, mMediator);
        mMediator.initialize(mModel);
    }

    @Test
    public void testShowWarningChangesVisibility() {
        mModel.set(VISIBLE, false);
        mMediator.showWarning(ScreenType.INTRO_SCREEN);
        assertTrue(mModel.get(VISIBLE));
    }

    @Test
    public void testOnDismissedHidesTheSheet() {
        mMediator.showWarning(ScreenType.INTRO_SCREEN);
        mMediator.onDismissed(StateChangeReason.NONE);
        assertFalse(mModel.get(VISIBLE));
    }

    @Test
    public void testDismissHandlerHidesTheSheet() {
        assertNotNull(mModel.get(DISMISS_HANDLER));
        mMediator.showWarning(ScreenType.INTRO_SCREEN);
        mModel.get(DISMISS_HANDLER).onResult(StateChangeReason.NONE);
        assertFalse(mModel.get(VISIBLE));
    }

    @Test
    public void testOnMoreOptionsChangesTheModel() {
        mMediator.showWarning(ScreenType.INTRO_SCREEN);
        assertEquals(mModel.get(CURRENT_SCREEN), ScreenType.INTRO_SCREEN);
        mMediator.onMoreOptions();
        assertEquals(mModel.get(CURRENT_SCREEN), ScreenType.OPTIONS_SCREEN);
    }

    @Test
    public void testOnAcknowledgeCollapsesTheSheet() {
        mMediator.onAcknowledge(mBottomSheetController);
        verify(mBottomSheetController).collapseSheet(true);
    }

    @Test
    public void testOnCancelCollapsesTheSheet() {
        mMediator.onCancel(mBottomSheetController);
        verify(mBottomSheetController).collapseSheet(true);
    }
}
