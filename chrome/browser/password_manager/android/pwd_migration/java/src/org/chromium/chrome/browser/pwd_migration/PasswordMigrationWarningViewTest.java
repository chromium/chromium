// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.is;
import static org.mockito.Mockito.verify;

import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.VISIBLE;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link PasswordMigrationWarningView} */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordMigrationWarningViewTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock
    private Callback<Integer> mDismissCallback;

    private BottomSheetController mBottomSheetController;
    private PasswordMigrationWarningView mView;
    private PropertyModel mModel;

    @Before
    public void setupTest() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        mBottomSheetController = mActivityTestRule.getActivity()
                                         .getRootUiCoordinatorForTesting()
                                         .getBottomSheetController();
        runOnUiThreadBlocking(() -> {
            mModel = PasswordMigrationWarningProperties.createDefaultModel(mDismissCallback);
            mView = new PasswordMigrationWarningView(
                    mActivityTestRule.getActivity(), mBottomSheetController);
            PropertyModelChangeProcessor.create(mModel, mView,
                    PasswordMigrationWarningViewBinder::bindPasswordMigrationWarningView);
        });
    }

    @Test
    @MediumTest
    public void testVisibilityChangedByModel() {
        // After setting the visibility to true, the view should exist and be visible.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        assertThat(mView.getContentView().isShown(), is(true));

        // After hiding the view, the view should still exist but be invisible.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, false));
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);
        assertThat(mView.getContentView().isShown(), is(false));
    }

    @Test
    @MediumTest
    public void testDismissesWhenHidden() {
        // The sheet is shown.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        // The sheet is hidden.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, false));
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);

        // The dismiss callback was called.
        verify(mDismissCallback).onResult(BottomSheetController.StateChangeReason.NONE);
    }

    private @SheetState int getBottomSheetState() {
        return mBottomSheetController.getSheetState();
    }
}
