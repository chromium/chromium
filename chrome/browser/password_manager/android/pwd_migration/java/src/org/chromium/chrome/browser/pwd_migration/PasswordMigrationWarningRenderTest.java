// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.VISIBLE;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.util.Arrays;
import java.util.List;

/**
 * These tests render screenshots of password migration warning sheet and compare them to a gold
 * standard.
 */
@RunWith(ParameterizedRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordMigrationWarningRenderTest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(new ParameterSet().value(false, false).name("Default"),
                    new ParameterSet().value(false, true).name("RTL"),
                    new ParameterSet().value(true, false).name("NightMode"));

    @Mock
    private Callback<Integer> mDismissCallback;

    private BottomSheetController mBottomSheetController;
    private PasswordMigrationWarningView mView;
    private PropertyModel mModel;

    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(Component.UI_BROWSER_AUTOFILL)
                    .build();

    public PasswordMigrationWarningRenderTest(boolean nightModeEnabled, boolean useRtlLayout) {
        setRtlForTesting(useRtlLayout);
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mRenderTestRule.setVariantPrefix(useRtlLayout ? "RTL" : "LTR");
    }

    @Before
    public void setUp() throws InterruptedException {
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

    @After
    public void tearDown() {
        setRtlForTesting(false);
        try {
            ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());
        } catch (Exception e) {
            // Activity was already closed (e.g. due to last test tearing down the suite).
        }
        runOnUiThreadBlocking(() -> {
            ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
        });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsPasswordMigrationWwarningFirstPage() throws Exception {
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView =
                mActivityTestRule.getActivity().findViewById(R.id.pwd_migration_warning_sheet);
        mRenderTestRule.render(bottomSheetView, "pwd_migration_warning_first_page");
    }
}
