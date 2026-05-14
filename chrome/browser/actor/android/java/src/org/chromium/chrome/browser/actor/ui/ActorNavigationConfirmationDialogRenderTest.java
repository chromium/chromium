// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.app.Activity;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Render tests for {@link ActorNavigationConfirmationDialog}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ActorNavigationConfirmationDialogRenderTest {
    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_GLIC)
                    .build();

    private Activity mActivity;
    private ModalDialogManager mModalDialogManager;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        mModalDialogManager = ((BlankUiTestActivity) mActivity).getModalDialogManager();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testDialog() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ActorNavigationConfirmationDialog.show(mActivity, mModalDialogManager, null);
                });

        CriteriaHelper.pollUiThread(() -> mModalDialogManager.isShowing());

        var view =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                ((AppModalPresenter)
                                                mModalDialogManager.getCurrentPresenterForTest())
                                        .getDialogViewForTesting());

        mRenderTestRule.render(view, "actor_navigation_confirmation_dialog");
    }
}
