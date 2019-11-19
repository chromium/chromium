// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests for {@link TabGridIphItemMediator}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGridIphItemMediatorUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    Tracker mTracker;
    @Mock
    Profile mProfile;
    @Mock
    View mView;

    private PropertyModel mModel;
    private TabGridIphItemMediator mMediator;

    @Before
    public void setUp() {
        RecordUserAction.setDisabledForTests(true);
        RecordHistogram.setDisabledForTests(true);

        MockitoAnnotations.initMocks(this);

        mModel = new PropertyModel(TabGridIphItemProperties.ALL_KEYS);
        mMediator = new TabGridIphItemMediator(mModel, mProfile);
        TrackerFactory.setTrackerForTests(mTracker);
    }

    @After
    public void tearDown() {
        RecordUserAction.setDisabledForTests(false);
        RecordHistogram.setDisabledForTests(false);
    }

    @Test
    public void testShowIphDialogButtonListener() {
        View.OnClickListener listener =
                mModel.get(TabGridIphItemProperties.IPH_ENTRANCE_SHOW_BUTTON_LISTENER);
        assertNotNull(listener);
        assertFalse(mModel.get(TabGridIphItemProperties.IS_IPH_DIALOG_VISIBLE));
        listener.onClick(mView);
        assertTrue(mModel.get(TabGridIphItemProperties.IS_IPH_DIALOG_VISIBLE));
    }

    @Test
    public void testCloseIphEntranceButtonListener() {
        View.OnClickListener listener =
                mModel.get(TabGridIphItemProperties.IPH_ENTRANCE_CLOSE_BUTTON_LISTENER);
        assertNotNull(listener);
        mModel.set(TabGridIphItemProperties.IS_IPH_ENTRANCE_VISIBLE, true);
        listener.onClick(mView);
        assertFalse(mModel.get(TabGridIphItemProperties.IS_IPH_ENTRANCE_VISIBLE));
        verify(mTracker).shouldTriggerHelpUI(FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE);
        verify(mTracker).dismissed(FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE);
    }

    @Test
    public void testCloseIphDialogButtonListener() {
        View.OnClickListener listener =
                mModel.get(TabGridIphItemProperties.IPH_DIALOG_CLOSE_BUTTON_LISTENER);
        assertNotNull(listener);
        mModel.set(TabGridIphItemProperties.IS_IPH_DIALOG_VISIBLE, true);
        listener.onClick(mView);
        assertFalse(mModel.get(TabGridIphItemProperties.IS_IPH_DIALOG_VISIBLE));
    }

    @Test
    public void testScrimViewObserver() {
        ScrimView.ScrimObserver observer =
                mModel.get(TabGridIphItemProperties.IPH_SCRIM_VIEW_OBSERVER);
        assertNotNull(observer);
        mModel.set(TabGridIphItemProperties.IS_IPH_DIALOG_VISIBLE, true);
        observer.onScrimClick();
        assertFalse(mModel.get(TabGridIphItemProperties.IS_IPH_DIALOG_VISIBLE));
    }

    @Test
    public void testRequestButNotShowIph() {
        doReturn(false).when(mTracker).wouldTriggerHelpUI(
                FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE);
        assertFalse(mModel.get(TabGridIphItemProperties.IS_IPH_ENTRANCE_VISIBLE));
        assertFalse(mModel.get(TabGridIphItemProperties.IS_INCOGNITO));

        mMediator.maybeShowIPH(true);

        // Iph entrance should not show.
        assertFalse(mModel.get(TabGridIphItemProperties.IS_IPH_ENTRANCE_VISIBLE));
        assertFalse(mModel.get(TabGridIphItemProperties.IS_INCOGNITO));
    }

    @Test
    public void testRequestAndShowIph() {
        doReturn(true).when(mTracker).wouldTriggerHelpUI(
                FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE);
        assertFalse(mModel.get(TabGridIphItemProperties.IS_IPH_ENTRANCE_VISIBLE));
        assertFalse(mModel.get(TabGridIphItemProperties.IS_INCOGNITO));

        mMediator.maybeShowIPH(true);

        // Iph entrance should show and should reflect incognito state.
        assertTrue(mModel.get(TabGridIphItemProperties.IS_IPH_ENTRANCE_VISIBLE));
        assertTrue(mModel.get(TabGridIphItemProperties.IS_INCOGNITO));
    }
}
