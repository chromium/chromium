// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import static org.chromium.chrome.browser.hub.HubPaneHostProperties.ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.PANE_ROOT_VIEW;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link HubPaneHostView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubPaneHostViewUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock Runnable mOnActionButton;

    private Activity mActivity;
    private HubPaneHostView mPaneHost;
    private Button mActionButton;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() throws Exception {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(TestActivity activity) {
        mActivity = activity;
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        LayoutInflater inflater = LayoutInflater.from(mActivity);
        mPaneHost = (HubPaneHostView) inflater.inflate(R.layout.hub_pane_host_layout, null, false);
        mActionButton = mPaneHost.findViewById(R.id.host_action_button);
        mActivity.setContentView(mPaneHost);

        mPropertyModel = new PropertyModel(HubPaneHostProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mPropertyModel, mPaneHost, HubPaneHostViewBinder::bind);
    }

    @Test
    @MediumTest
    public void testActionButtonVisibility() {
        DisplayButtonData displayButtonData =
                new ResourceButtonData(
                        R.string.button_new_tab, R.string.button_new_tab, R.drawable.ic_add);
        FullButtonData fullButtonData = new DelegateButtonData(displayButtonData, mOnActionButton);
        assertEquals(View.GONE, mActionButton.getVisibility());

        mPropertyModel.set(ACTION_BUTTON_DATA, fullButtonData);
        assertEquals(View.VISIBLE, mActionButton.getVisibility());

        mPropertyModel.set(ACTION_BUTTON_DATA, null);
        assertEquals(View.GONE, mActionButton.getVisibility());
    }

    @Test
    @MediumTest
    public void testActionButtonCallback() {
        DisplayButtonData displayButtonData =
                new ResourceButtonData(
                        R.string.button_new_tab, R.string.button_new_tab, R.drawable.ic_add);
        FullButtonData fullButtonData = new DelegateButtonData(displayButtonData, mOnActionButton);
        mPropertyModel.set(ACTION_BUTTON_DATA, fullButtonData);
        assertTrue(mActionButton.isEnabled());

        mActionButton.callOnClick();
        verify(mOnActionButton).run();

        Mockito.reset(mOnActionButton);
        mPropertyModel.set(ACTION_BUTTON_DATA, null);

        mActionButton.callOnClick();
        verifyNoInteractions(mOnActionButton);
    }

    @Test
    @MediumTest
    public void testEmptyActionButtonCallbackDisablesButton() {
        DisplayButtonData displayButtonData =
                new ResourceButtonData(
                        R.string.button_new_tab, R.string.button_new_tab, R.drawable.ic_add);
        FullButtonData fullButtonData = new DelegateButtonData(displayButtonData, null);
        mPropertyModel.set(ACTION_BUTTON_DATA, fullButtonData);
        assertFalse(mActionButton.isEnabled());

        // Verify this doesn't crash if no button data Runnable exists.
        mActionButton.callOnClick();
    }

    @Test
    @MediumTest
    public void testSetRootView() {
        View root1 = new View(mActivity);
        View root2 = new View(mActivity);
        View root3 = new View(mActivity);

        ViewGroup paneFrame = mPaneHost.findViewById(R.id.pane_frame);
        assertEquals(0, paneFrame.getChildCount());

        mPropertyModel.set(PANE_ROOT_VIEW, root1);
        verifyChildren(paneFrame, root1);

        mPropertyModel.set(PANE_ROOT_VIEW, root2);
        verifyChildren(paneFrame, root1, root2);

        ShadowLooper.runUiThreadTasks();
        verifyChildren(paneFrame, root2);

        mPropertyModel.set(PANE_ROOT_VIEW, root1);
        mPropertyModel.set(PANE_ROOT_VIEW, root2);
        mPropertyModel.set(PANE_ROOT_VIEW, root3);
        mPropertyModel.set(PANE_ROOT_VIEW, root2);
        verifyChildren(paneFrame, root2, root3);

        ShadowLooper.runUiThreadTasks();
        verifyChildren(paneFrame, root2);

        mPropertyModel.set(PANE_ROOT_VIEW, null);
        assertEquals(0, paneFrame.getChildCount());
    }

    @Test
    @MediumTest
    public void testSetRootView_alphaRestored() {
        View root1 = new View(mActivity);
        View root2 = new View(mActivity);

        mPropertyModel.set(PANE_ROOT_VIEW, root1);
        mPropertyModel.set(PANE_ROOT_VIEW, root2);
        ShadowLooper.runUiThreadTasks();
        assertEquals(1, root2.getAlpha(), /* delta= */ 0);

        // Inspired by b/325372945 where the alpha needed to be reset, even when no animations ran.
        mPropertyModel.set(PANE_ROOT_VIEW, null);
        mPropertyModel.set(PANE_ROOT_VIEW, root1);
        assertEquals(1, root1.getAlpha(), /* delta= */ 0);
    }

    /** Order of children does not matter. */
    private void verifyChildren(ViewGroup parent, View... children) {
        assertEquals(children.length, parent.getChildCount());
        List<View> expectedChildList = Arrays.asList(children);
        for (int i = 0; i < parent.getChildCount(); i++) {
            View child = parent.getChildAt(i);
            assertTrue(child.toString(), expectedChildList.contains(child));
        }
    }
}
