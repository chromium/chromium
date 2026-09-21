// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.autofill_ai;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.editors.common.EditorDialogToolbar;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.TestActivity;

/** Unit tests for autofill entity editor. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
public class EntityEditorViewTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Runnable mRunnable;

    private TestActivity mActivity;
    private EntityEditorView mEditorView;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
    }

    private void createEditorView() {
        mEditorView = new EntityEditorView(mActivity);
    }

    @Test
    public void editorTitle() {
        createEditorView();

        mEditorView.setEditorTitle("Title");

        EditorDialogToolbar titleBar = mEditorView.getContainerView().findViewById(R.id.action_bar);
        assertEquals("Title", titleBar.getTitle().toString());
    }

    @Test
    public void clickDoneButton() {
        createEditorView();

        mEditorView.setDoneRunnable(mRunnable);

        mEditorView.getContainerView().findViewById(R.id.editor_dialog_done_button).performClick();

        verify(mRunnable).run();
    }

    @Test
    public void clickCancelButton() {
        createEditorView();

        mEditorView.setCancelRunnable(mRunnable);

        mEditorView
                .getContainerView()
                .findViewById(R.id.payments_edit_cancel_button)
                .performClick();

        verify(mRunnable).run();
    }
}
