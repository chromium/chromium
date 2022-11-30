// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertNotNull;
import static junit.framework.Assert.assertTrue;

import static org.junit.Assert.assertNull;

import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.UiThreadTest;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

import java.util.Arrays;
import java.util.HashSet;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for {@link TabSelectionEditorLayoutBinder}.
 */
@SuppressWarnings("ArraysAsListWithZeroOrOneArgument")
@RunWith(ChromeJUnit4ClassRunner.class)
public class TabSelectionEditorLayoutBinderTest extends BlankUiTestActivityTestCase {
    private TabSelectionEditorLayout mEditorLayoutView;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mMCP;
    private SelectionDelegate<Integer> mSelectionDelegate;
    private ViewGroup mParentView;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        mParentView = new LinearLayout(getActivity());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel = new PropertyModel(TabSelectionEditorProperties.ALL_KEYS);
            mSelectionDelegate = new SelectionDelegate<>();
            getActivity().setContentView(mParentView);
            mEditorLayoutView =
                    (TabSelectionEditorLayout) getActivity().getLayoutInflater().inflate(
                            R.layout.tab_selection_editor_layout, null);
            mEditorLayoutView.initialize(mParentView, null, new RecyclerView.Adapter() {
                @SuppressWarnings("ConstantConditions")
                @NonNull
                @Override
                public RecyclerView.ViewHolder onCreateViewHolder(
                        @NonNull ViewGroup viewGroup, int i) {
                    return null;
                }

                @Override
                public void onBindViewHolder(@NonNull RecyclerView.ViewHolder viewHolder, int i) {}

                @Override
                public int getItemCount() {
                    return 0;
                }
            }, mSelectionDelegate);

            mMCP = PropertyModelChangeProcessor.create(
                    mModel, mEditorLayoutView, TabSelectionEditorLayoutBinder::bind);
        });
    }

    @Override
    public void tearDownTest() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(mMCP::destroy);
        super.tearDownTest();
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testBindViews() {
        // TODO(1005929): test other properties as well.
        mModel.set(TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_TEXT, "Test");
        Assert.assertEquals("Test",
                ((TextView) mEditorLayoutView.findViewById(R.id.action_button))
                        .getText()
                        .toString());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testBindActionButtonClickListener() {
        AtomicBoolean actionButtonClicked = new AtomicBoolean(false);
        mModel.set(TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_LISTENER,
                v -> actionButtonClicked.set(true));
        mEditorLayoutView.findViewById(R.id.action_button).performClick();
        assertTrue(actionButtonClicked.get());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testActionButtonEnabling() {
        Button button = mEditorLayoutView.findViewById(R.id.action_button);
        mModel.set(TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_ENABLING_THRESHOLD, 1);
        assertFalse(button.isEnabled());

        HashSet<Integer> selectedItem = new HashSet<>(Arrays.asList(1));
        mSelectionDelegate.setSelectedItems(selectedItem);
        assertTrue(button.isEnabled());

        mModel.set(TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_ENABLING_THRESHOLD, 2);
        mSelectionDelegate.setSelectedItems(selectedItem);
        assertFalse(button.isEnabled());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testActionButtonContentDescriptionBinding() {
        // Set up action button.
        Button button = mEditorLayoutView.findViewById(R.id.action_button);
        mModel.set(TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_ENABLING_THRESHOLD, 1);

        int expectedResourceId = R.plurals.accessibility_tab_selection_editor_group_button;
        mModel.set(TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_DESCRIPTION_RESOURCE_ID,
                expectedResourceId);
        assertNull(button.getContentDescription());

        // Simulate selection.
        HashSet<Integer> selectedItem = new HashSet<>(Arrays.asList(1));
        mSelectionDelegate.setSelectedItems(selectedItem);

        assertNotNull(button.getContentDescription());
    }
}
