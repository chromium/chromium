// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import static org.chromium.chrome.browser.hub.HubPaneHostProperties.ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.EDGE_TO_EDGE_BOTTOM_INSETS;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.FLOATING_ACTION_BUTTON_SUPPLIER_CALLBACK;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.HAIRLINE_VISIBILITY;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.PANE_ROOT_VIEW;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.SNACKBAR_CONTAINER_CALLBACK;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
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
    @Mock Callback<ViewGroup> mSnackbarContainerCallback;

    private Activity mActivity;
    private HubPaneHostView mPaneHost;
    private Button mActionButton;
    private ImageView mHairline;
    private ViewGroup mSnackbarContainer;
    private PropertyModel mPropertyModel;
    private Supplier<View> mFloatingActionButtonSupplier;

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
        mHairline = mPaneHost.findViewById(R.id.pane_top_hairline);
        mSnackbarContainer = mPaneHost.findViewById(R.id.pane_host_view_snackbar_container);
        mActivity.setContentView(mPaneHost);

        mPropertyModel = new PropertyModel(HubPaneHostProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mPropertyModel, mPaneHost, HubPaneHostViewBinder::bind);
    }

    @Test
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

    @Test
    public void testHairlineVisibility() {
        assertEquals(View.GONE, mHairline.getVisibility());

        mPropertyModel.set(HAIRLINE_VISIBILITY, true);
        assertEquals(View.VISIBLE, mHairline.getVisibility());

        mPropertyModel.set(HAIRLINE_VISIBILITY, false);
        assertEquals(View.GONE, mHairline.getVisibility());
    }

    @Test
    public void testFloatingActionButtonSupplier() {
        Callback<Supplier<View>> callback =
                (floatingActionButtonSupplier) -> {
                    mFloatingActionButtonSupplier = floatingActionButtonSupplier;
                };

        assertNull(mFloatingActionButtonSupplier);
        mPropertyModel.set(FLOATING_ACTION_BUTTON_SUPPLIER_CALLBACK, callback);
        assertNotNull(mFloatingActionButtonSupplier);

        assertEquals(View.GONE, mActionButton.getVisibility());
        assertNull(mFloatingActionButtonSupplier.get());

        mActionButton.setVisibility(View.VISIBLE);
        assertEquals(mFloatingActionButtonSupplier.get(), mActionButton);
    }

    @Test
    public void testSnackbarContainerSupplier() {
        mPropertyModel.set(SNACKBAR_CONTAINER_CALLBACK, mSnackbarContainerCallback);
        verify(mSnackbarContainerCallback).onResult(mSnackbarContainer);
    }

    @Test
    public void testSnackbarContainerFabAnimation() {
        mActionButton.layout(0, 0, 100, 100);
        mSnackbarContainer.layout(0, 100, 100, 100);

        int oldMargin = getBottomMargin(mActionButton);
        View.OnLayoutChangeListener listener =
                mPaneHost.getSnackbarLayoutChangeListenerForTesting();

        listener.onLayoutChange(
                mSnackbarContainer,
                /* left= */ 0,
                /* top= */ 50,
                /* right= */ 100,
                /* bottom= */ 100,
                /* oldLeft= */ 0,
                /* oldTop= */ 100,
                /* oldRight= */ 100,
                /* oldBottom= */ 100);
        ShadowLooper.runUiThreadTasks();

        assertEquals(oldMargin + 50, getBottomMargin(mActionButton));

        listener.onLayoutChange(
                mSnackbarContainer,
                /* left= */ 0,
                /* top= */ 100,
                /* right= */ 100,
                /* bottom= */ 100,
                /* oldLeft= */ 0,
                /* oldTop= */ 50,
                /* oldRight= */ 100,
                /* oldBottom= */ 100);
        ShadowLooper.runUiThreadTasks();

        assertEquals(oldMargin, getBottomMargin(mActionButton));
    }

    @Test
    public void testBottomMarginForFloatingActionButton() {
        int fixedMargin =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(
                                org.chromium.chrome.browser.hub.R.dimen
                                        .floating_action_button_margin);
        assertEquals(fixedMargin, getBottomMargin(mActionButton));

        // Bottom margin use the larger insets since larger than the original.
        int edgeToEdgeMargin = 10;
        mPropertyModel.set(EDGE_TO_EDGE_BOTTOM_INSETS, edgeToEdgeMargin);
        assertEquals(edgeToEdgeMargin + fixedMargin, getBottomMargin(mActionButton));
    }

    @Test
    public void testSnackbarContainerFabAnimation_EdgeToEdge() {
        mActionButton.layout(0, 0, 100, 100);
        mSnackbarContainer.layout(0, 100, 100, 100);

        int oldMargin = getBottomMargin(mActionButton);
        int edgeToEdgeMargin = 24;
        mPropertyModel.set(EDGE_TO_EDGE_BOTTOM_INSETS, edgeToEdgeMargin);
        assertEquals(oldMargin + edgeToEdgeMargin, getBottomMargin(mActionButton));

        View.OnLayoutChangeListener listener =
                mPaneHost.getSnackbarLayoutChangeListenerForTesting();

        listener.onLayoutChange(
                mSnackbarContainer,
                /* left= */ 0,
                /* top= */ 50,
                /* right= */ 100,
                /* bottom= */ 100,
                /* oldLeft= */ 0,
                /* oldTop= */ 100,
                /* oldRight= */ 100,
                /* oldBottom= */ 100);
        ShadowLooper.runUiThreadTasks();

        assertEquals(
                "Snackbar height should be used to calculate bottom margin.",
                oldMargin + 50,
                getBottomMargin(mActionButton));

        listener.onLayoutChange(
                mSnackbarContainer,
                /* left= */ 0,
                /* top= */ 100,
                /* right= */ 100,
                /* bottom= */ 100,
                /* oldLeft= */ 0,
                /* oldTop= */ 50,
                /* oldRight= */ 100,
                /* oldBottom= */ 100);
        ShadowLooper.runUiThreadTasks();

        assertEquals(oldMargin + edgeToEdgeMargin, getBottomMargin(mActionButton));
    }

    private int getBottomMargin(View view) {
        FrameLayout.LayoutParams params = (FrameLayout.LayoutParams) view.getLayoutParams();
        return params.bottomMargin;
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
