// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import static org.chromium.components.feature_engagement.FeatureConstants.TAB_GROUP_CREATION_DIALOG_SYNC_TEXT_FEATURE;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabGroupFeatureUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;

/** Tests for TabGroupCreationDialogManager. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupCreationDialogManagerUnitTest {
    private static final int TAB1_ID = 456;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tracker mTracker;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Profile mProfile;
    @Mock private TabModel mTabModel;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabGroupVisualDataDialogManager mTabGroupVisualDataDialogManager;
    @Mock private Runnable mOnTabGroupCreation;

    private TabGroupCreationDialogManager mTabGroupCreationDialogManager;

    @Before
    public void setUp() {
        TrackerFactory.setTrackerForTests(mTracker);

        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mTabGroupCreationDialogManager =
                new TabGroupCreationDialogManager(
                        activity, mModalDialogManager, mOnTabGroupCreation);

        doReturn(mTabModel).when(mTabGroupModelFilter).getTabModel();
        doReturn(mProfile).when(mTabModel).getProfile();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID)
    public void testCreationDialogNotSkippedByDialogFlag() {
        assertFalse(TabGroupFeatureUtils.shouldSkipGroupCreationDialog());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID)
    public void testCreationDialogSkippedByDialogFlag() {
        assertTrue(TabGroupFeatureUtils.shouldSkipGroupCreationDialog());
    }

    @Test
    public void testShowOnWillMergingCreateNewGroup() {
        mTabGroupCreationDialogManager.setDialogManagerForTesting(mTabGroupVisualDataDialogManager);

        mTabGroupCreationDialogManager.showDialog(TAB1_ID, mTabGroupModelFilter);
        ModalDialogProperties.Controller controller =
                mTabGroupCreationDialogManager.getDialogControllerForTesting();
        verify(mTabGroupVisualDataDialogManager)
                .showDialog(TAB1_ID, mTabGroupModelFilter, controller);
    }

    @Test
    public void testRunnableOnDismiss() {
        mTabGroupCreationDialogManager.setDialogManagerForTesting(mTabGroupVisualDataDialogManager);

        mTabGroupCreationDialogManager.showDialog(TAB1_ID, mTabGroupModelFilter);
        ModalDialogProperties.Controller controller =
                mTabGroupCreationDialogManager.getDialogControllerForTesting();
        controller.onDismiss(null, DialogDismissalCause.UNKNOWN);
        verify(mTracker).dismissed(eq(TAB_GROUP_CREATION_DIALOG_SYNC_TEXT_FEATURE));
        verify(mOnTabGroupCreation).run();
    }
}
