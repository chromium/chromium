// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.autofill_ai;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.editors.common.EditorDialogToolbar;
import org.chromium.ui.base.TestActivity;

/** Unit tests for autofill entity editor. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EntityEditorViewTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private Activity mActivity;
    private EntityEditorView mEditorView;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
    }

    private void createEditorView() {
        mEditorView = new EntityEditorView(mActivity);
    }

    @Test
    @SmallTest
    public void editorTitle() {
        createEditorView();

        mEditorView.setEditorTitle("Title");

        EditorDialogToolbar titleBar = mEditorView.getContainerView().findViewById(R.id.action_bar);
        assertEquals("Title", titleBar.getTitle().toString());
    }

    @Test
    @SmallTest
    public void clickDoneButton() {
        createEditorView();

        Runnable doneRunnable = mock(Runnable.class);
        mEditorView.setDoneRunnable(doneRunnable);

        mEditorView.getContainerView().findViewById(R.id.editor_dialog_done_button).performClick();

        verify(doneRunnable).run();
    }

    @Test
    @SmallTest
    public void clickCancelButton() {
        createEditorView();

        Runnable cancelRunnable = mock(Runnable.class);
        mEditorView.setCancelRunnable(cancelRunnable);

        mEditorView
                .getContainerView()
                .findViewById(R.id.payments_edit_cancel_button)
                .performClick();

        verify(cancelRunnable).run();
    }
}
