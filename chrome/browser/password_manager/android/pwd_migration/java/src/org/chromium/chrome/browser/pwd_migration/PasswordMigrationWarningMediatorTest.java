// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

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

    @Mock
    private BottomSheetController mBottomSheetController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMediator.initialize(
                PasswordMigrationWarningProperties.createDefaultModel(mMediator::onDismissed));
    }

    @Test
    public void testShowWarningChangesVisibility() {
        PropertyModel model = mMediator.getModel();
        assertFalse(model.get(VISIBLE));
        mMediator.showWarning();
        assertTrue(model.get(VISIBLE));
    }

    @Test
    public void testOnDismissedChangesVisibility() {
        PropertyModel model = mMediator.getModel();
        mMediator.showWarning();
        assertTrue(model.get(VISIBLE));
        mMediator.onDismissed(StateChangeReason.NONE);
        assertFalse(model.get(VISIBLE));
    }

    @Test
    public void testDismissHandlerChangesVisibility() {
        PropertyModel model = mMediator.getModel();
        assertNotNull(model.get(DISMISS_HANDLER));
        assertFalse(model.get(VISIBLE));
        mMediator.showWarning();
        assertTrue(model.get(VISIBLE));
        model.get(DISMISS_HANDLER).onResult(StateChangeReason.NONE);
        assertFalse(model.get(VISIBLE));
    }
}
