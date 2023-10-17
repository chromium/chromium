// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;

import android.view.View;

import androidx.annotation.Nullable;
import androidx.test.filters.LargeTest;

import com.google.common.collect.ImmutableList;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.search_engines.DefaultSearchEngineDialogHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.search_engines.FakeTemplateUrl;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Render tests for {@link ChoiceScreenCoordinator} */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ChoiceScreenRenderTest {
    public @Rule final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_OMNIBOX)
                    .build();

    public @Rule final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    public @Rule final MockitoRule mMockitoRule =
            MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private @Mock DefaultSearchEngineDialogHelper.Delegate mDelegate;

    private @Nullable ChoiceDialogCoordinator mChoiceDialogCoordinator;

    @Before
    public void setUp() {
        doReturn(
                        ImmutableList.of(
                                new FakeTemplateUrl("Search Engine A", "sea"),
                                new FakeTemplateUrl("Search Engine B", "seb"),
                                new FakeTemplateUrl("Search Engine C", "sec")))
                .when(mDelegate)
                .getSearchEnginesForPromoDialog(anyInt());

        mActivityTestRule.launchActivity(null);
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSignOutDialogForNonSyncingAccount() throws Exception {
        mRenderTestRule.render(showChoiceScreenDialog(), "choice_screen_dialog");
    }

    private View showChoiceScreenDialog() throws Exception {
        assertNull(mChoiceDialogCoordinator);
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mChoiceDialogCoordinator =
                            new ChoiceDialogCoordinator(
                                    mActivityTestRule.getActivity(), mDelegate, (unused) -> {});
                    mChoiceDialogCoordinator.show();

                    PropertyModel dialogModel =
                            mActivityTestRule
                                    .getActivity()
                                    .getModalDialogManager()
                                    .getCurrentDialogForTest();
                    assertNotNull(dialogModel);

                    return dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
                });
    }
}
