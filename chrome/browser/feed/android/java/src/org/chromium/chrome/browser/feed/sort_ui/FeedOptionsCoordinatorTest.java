// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.sort_ui;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.v2.ContentOrder;
import org.chromium.components.browser_ui.widget.chips.ChipProperties;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/** Tests for {@link FeedOptionsCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedOptionsCoordinatorTest {
    @Mock private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock private FeedOptionsView mView;
    @Mock private ChipView mChipView;
    @Mock private TextView mTextView;

    @Rule public JniMocker mMocker = new JniMocker();

    private FeedOptionsCoordinator mCoordinator;
    private Context mContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        mMocker.mock(FeedServiceBridge.getTestHooksForTesting(), mFeedServiceBridgeJniMock);
        when(mFeedServiceBridgeJniMock.getContentOrderForWebFeed())
                .thenReturn(ContentOrder.REVERSE_CHRON);
        when(mView.createNewChip()).thenReturn(mChipView);
        when(mChipView.getPrimaryTextView()).thenReturn(mTextView);

        mCoordinator = new FeedOptionsCoordinator(mContext, mView);
    }

    @Test
    public void testToggleVisibility_turnoff() {
        PropertyModel model = mCoordinator.getModelForTest();
        model.set(FeedOptionsProperties.VISIBILITY_KEY, true);

        mCoordinator.toggleVisibility();

        assertFalse(mCoordinator.getModelForTest().get(FeedOptionsProperties.VISIBILITY_KEY));
    }

    @Test
    public void testToggleVisibility_turnon() {
        PropertyModel model = mCoordinator.getModelForTest();
        model.set(FeedOptionsProperties.VISIBILITY_KEY, false);

        mCoordinator.toggleVisibility();

        assertTrue(mCoordinator.getModelForTest().get(FeedOptionsProperties.VISIBILITY_KEY));
        assertFalse(mCoordinator.getChipModelsForTest().get(0).get(ChipProperties.SELECTED));
        assertTrue(mCoordinator.getChipModelsForTest().get(1).get(ChipProperties.SELECTED));
    }

    @Test
    public void testToggleVisibility_turnon_updateOrder() {
        PropertyModel model = mCoordinator.getModelForTest();
        model.set(FeedOptionsProperties.VISIBILITY_KEY, false);
        when(mFeedServiceBridgeJniMock.getContentOrderForWebFeed())
                .thenReturn(ContentOrder.GROUPED);

        mCoordinator.toggleVisibility();

        assertTrue(mCoordinator.getModelForTest().get(FeedOptionsProperties.VISIBILITY_KEY));
        assertTrue(mCoordinator.getChipModelsForTest().get(0).get(ChipProperties.SELECTED));
        assertFalse(mCoordinator.getChipModelsForTest().get(1).get(ChipProperties.SELECTED));
    }

    @Test
    public void testEnsureGone_startOn() {
        PropertyModel model = mCoordinator.getModelForTest();
        model.set(FeedOptionsProperties.VISIBILITY_KEY, true);

        mCoordinator.ensureGone();

        assertFalse(mCoordinator.getModelForTest().get(FeedOptionsProperties.VISIBILITY_KEY));
    }

    @Test
    public void testEnsureGone_startOff() {
        PropertyModel model = mCoordinator.getModelForTest();
        model.set(FeedOptionsProperties.VISIBILITY_KEY, false);

        mCoordinator.ensureGone();

        assertFalse(mCoordinator.getModelForTest().get(FeedOptionsProperties.VISIBILITY_KEY));
    }

    @Test
    public void testOptionsSelected() {
        AtomicBoolean listenerCalled = new AtomicBoolean(false);
        mCoordinator.setOptionsListener(
                () -> {
                    listenerCalled.set(true);
                });
        List<PropertyModel> chipModels = mCoordinator.getChipModelsForTest();
        chipModels.get(0).set(ChipProperties.SELECTED, false);
        chipModels.get(1).set(ChipProperties.SELECTED, true);

        mCoordinator.onOptionSelected(chipModels.get(0));

        assertFalse(chipModels.get(1).get(ChipProperties.SELECTED));
        assertTrue(chipModels.get(0).get(ChipProperties.SELECTED));
        assertTrue(listenerCalled.get());
    }
}
