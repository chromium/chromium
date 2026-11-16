// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;

import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link CriticalActionsViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CriticalActionsViewBinderTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private TestActivity mActivity;
    private ImageView mExpandIcon;
    private FrameLayout mPanelContainer;
    private @Nullable Runnable mRowClickHandler;
    private @Nullable CharSequence mRowClickAccessibilityLabel;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor<
                    PropertyModel, CriticalActionsViewBinder.ViewHolder, PropertyKey>
            mProcessor;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mExpandIcon = new ImageView(mActivity);
        mPanelContainer = new FrameLayout(mActivity);
        mModel = new PropertyModel(CriticalActionsProperties.ALL_KEYS);
        mProcessor =
                PropertyModelChangeProcessor.create(
                        mModel,
                        new CriticalActionsViewBinder.ViewHolder(
                                mExpandIcon,
                                mPanelContainer,
                                handler -> mRowClickHandler = handler,
                                label -> mRowClickAccessibilityLabel = label),
                        CriticalActionsViewBinder::bindRow);
    }

    @After
    public void tearDown() {
        mProcessor.destroy();
    }

    @Test
    public void testExpandIcon() {
        mModel.set(CriticalActionsProperties.EXPAND_ICON_VISIBLE, true);
        assertEquals(View.VISIBLE, mExpandIcon.getVisibility());
        mModel.set(CriticalActionsProperties.EXPAND_ICON_VISIBLE, false);
        assertEquals(View.GONE, mExpandIcon.getVisibility());

        mModel.set(
                CriticalActionsProperties.EXPAND_ICON_CONTENT_DESCRIPTION,
                "2 key browsing actions");
        assertEquals("2 key browsing actions", mExpandIcon.getContentDescription());

        mModel.set(CriticalActionsProperties.EXPAND_ICON_ROTATION, 180f);
        assertEquals(180f, mExpandIcon.getRotation(), 0f);
        mModel.set(CriticalActionsProperties.EXPAND_ICON_ROTATION, 0f);
        assertEquals(0f, mExpandIcon.getRotation(), 0f);
    }

    @Test
    public void testPanelVisible() {
        mModel.set(CriticalActionsProperties.PANEL_VISIBLE, true);
        assertEquals(View.VISIBLE, mPanelContainer.getVisibility());
        mModel.set(CriticalActionsProperties.PANEL_VISIBLE, false);
        assertEquals(View.GONE, mPanelContainer.getVisibility());
    }

    @Test
    public void testRowClick() {
        Runnable handler = () -> {};
        mModel.set(CriticalActionsProperties.ROW_CLICK_HANDLER, handler);
        assertSame(handler, mRowClickHandler);
        mModel.set(CriticalActionsProperties.ROW_CLICK_HANDLER, null);
        assertNull(mRowClickHandler);

        mModel.set(CriticalActionsProperties.ROW_CLICK_ACCESSIBILITY_LABEL, "Expand section");
        assertEquals("Expand section", mRowClickAccessibilityLabel);
        mModel.set(CriticalActionsProperties.ROW_CLICK_ACCESSIBILITY_LABEL, null);
        assertNull(mRowClickAccessibilityLabel);
    }

    @Test
    public void testBindActionItem() {
        TextView view = new TextView(mActivity);
        PropertyModel itemModel =
                new PropertyModel.Builder(CriticalActionsProperties.ACTION_ITEM_KEYS)
                        .with(CriticalActionsProperties.LABEL, "Filled a form")
                        .build();
        PropertyModelChangeProcessor.create(
                itemModel, view, CriticalActionsViewBinder::bindActionItem);

        assertEquals("Filled a form", view.getText().toString());
    }
}
