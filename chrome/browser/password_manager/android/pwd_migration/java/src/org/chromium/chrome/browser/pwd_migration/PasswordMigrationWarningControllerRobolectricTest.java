// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;

import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.VISIBLE;

import android.app.Activity;
import android.content.Context;

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
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests for {@link PasswordMigrationWarningCoordinator} and {@link
 * PasswordMigrationWarningMediator}
 */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class PasswordMigrationWarningControllerRobolectricTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private final Context mContext = Robolectric.buildActivity(Activity.class).get();
    private PasswordMigrationWarningCoordinator mCoordinator;

    @Mock
    private BottomSheetController mBottomSheetController;

    public PasswordMigrationWarningControllerRobolectricTest() {}

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mCoordinator = new PasswordMigrationWarningCoordinator(mContext, mBottomSheetController);
    }

    @Test
    public void testShowWarningUpdatesModel() {
        PropertyModel model = mCoordinator.getModelForTesting();
        assertNotNull(model.get(DISMISS_HANDLER));
        assertThat(model.get(VISIBLE), is(false));

        mCoordinator.showWarning();

        assertThat(model.get(VISIBLE), is(true));
    }
}
