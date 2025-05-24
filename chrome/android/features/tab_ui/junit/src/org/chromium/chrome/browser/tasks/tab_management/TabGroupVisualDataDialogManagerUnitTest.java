// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.widget.TextView;

import androidx.appcompat.widget.DialogTitle;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Collections;

/** Tests for TabGroupVisualDataDialogManager. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
// TODO(crbug.com/419289558): Re-enable color surface feature flags
@Features.DisableFeatures({
    ChromeFeatureList.ANDROID_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_UPDATE,
    ChromeFeatureList.ANDROID_THEME_MODULE
})
public class TabGroupVisualDataDialogManagerUnitTest {
    private static final Token TAB_GROUP_ID = new Token(34L, 378L);
    private static final int TAB1_ID = 456;
    private static final String TAB_GROUP_CREATION_DIALOG_SHOWN =
            EventConstants.TAB_GROUP_CREATION_DIALOG_SHOWN;
    private static final String TAB_GROUP_CREATION_DIALOG_SYNC_TEXT_FEATURE =
            FeatureConstants.TAB_GROUP_CREATION_DIALOG_SYNC_TEXT_FEATURE;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;
    @Mock private Tracker mTracker;
    @Mock private SyncService mSyncService;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Profile mProfile;
    @Mock private TabModel mTabModel;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private ModalDialogProperties.Controller mDialogController;
    @Captor private ArgumentCaptor<PropertyModel> mModelCaptor;

    private Activity mActivity;
    private TabGroupVisualDataDialogManager mTabGroupVisualDataDialogManager;

    @Before
    public void setUp() {
        TrackerFactory.setTrackerForTests(mTracker);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mTabGroupVisualDataDialogManager =
                new TabGroupVisualDataDialogManager(
                        mActivity,
                        mModalDialogManager,
                        TabGroupVisualDataDialogManager.DialogType.TAB_GROUP_CREATION,
                        R.string.tab_group_creation_dialog_title);
        TabGroupSyncFeaturesJni.setInstanceForTesting(mTabGroupSyncFeaturesJniMock);
        SyncServiceFactory.setInstanceForTesting(mSyncService);

        doReturn(mTabModel).when(mTabGroupModelFilter).getTabModel();
        doReturn(mProfile).when(mTabModel).getProfile();
        doReturn(true).when(mTabGroupSyncFeaturesJniMock).isTabGroupSyncEnabled(mProfile);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_SYNC_ANDROID})
    public void testVisualDataDialogDelegate_showDialog() {
        mTabGroupVisualDataDialogManager.showDialog(
                TAB_GROUP_ID, mTabGroupModelFilter, mDialogController);
        verify(mModalDialogManager).showDialog(mModelCaptor.capture(), eq(ModalDialogType.APP));

        PropertyModel model = mModelCaptor.getValue();
        Assert.assertEquals(
                mActivity
                        .getResources()
                        .getString(R.string.tab_group_creation_positive_button_text),
                model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));

        DialogTitle title =
                model.get(ModalDialogProperties.CUSTOM_VIEW)
                        .findViewById(R.id.visual_data_dialog_title);
        Assert.assertEquals(
                mActivity.getResources().getString(R.string.tab_group_creation_dialog_title),
                title.getText());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_SYNC_ANDROID})
    public void testVisualDataDialogDelegate_doubleShowDismissed() {
        // Mock a double trigger for the creation dialog observer method for the same group action,
        // but show dialog is only called once.
        mTabGroupVisualDataDialogManager.showDialog(
                TAB_GROUP_ID, mTabGroupModelFilter, mDialogController);
        mTabGroupVisualDataDialogManager.showDialog(
                TAB_GROUP_ID, mTabGroupModelFilter, mDialogController);
        verify(mModalDialogManager, times(1))
                .showDialog(mModelCaptor.capture(), eq(ModalDialogType.APP));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_SYNC_ANDROID})
    public void testVisualDataDialog_descriptionTextNotSet() {
        // Set the opposite values for the conditional statement to be true.
        doReturn(true).when(mTabModel).isIncognitoBranded();
        doReturn(false)
                .when(mTracker)
                .shouldTriggerHelpUi(TAB_GROUP_CREATION_DIALOG_SYNC_TEXT_FEATURE);

        mTabGroupVisualDataDialogManager.showDialog(
                TAB_GROUP_ID, mTabGroupModelFilter, mDialogController);
        verify(mModalDialogManager).showDialog(mModelCaptor.capture(), eq(ModalDialogType.APP));

        PropertyModel model = mModelCaptor.getValue();
        TextView description =
                model.get(ModalDialogProperties.CUSTOM_VIEW)
                        .findViewById(R.id.visual_data_dialog_description);
        Assert.assertEquals(View.GONE, description.getVisibility());
        verify(mTracker, never()).notifyEvent(eq(TAB_GROUP_CREATION_DIALOG_SHOWN));

        mTabGroupVisualDataDialogManager.hideDialog();
        verify(mTracker, never()).dismissed(eq(TAB_GROUP_CREATION_DIALOG_SYNC_TEXT_FEATURE));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_SYNC_ANDROID})
    public void testVisualDataDialog_descriptionTextSetButNotSyncing() {
        doReturn(false).when(mTabModel).isIncognitoBranded();
        doReturn(true)
                .when(mTracker)
                .shouldTriggerHelpUi(TAB_GROUP_CREATION_DIALOG_SYNC_TEXT_FEATURE);
        when(mSyncService.getActiveDataTypes()).thenReturn(Collections.emptySet());

        mTabGroupVisualDataDialogManager.showDialog(
                TAB_GROUP_ID, mTabGroupModelFilter, mDialogController);
        verify(mModalDialogManager).showDialog(mModelCaptor.capture(), eq(ModalDialogType.APP));

        PropertyModel model = mModelCaptor.getValue();
        TextView description =
                model.get(ModalDialogProperties.CUSTOM_VIEW)
                        .findViewById(R.id.visual_data_dialog_description);
        Assert.assertEquals(View.VISIBLE, description.getVisibility());
        Assert.assertEquals(
                mActivity
                        .getResources()
                        .getString(R.string.tab_group_creation_dialog_description_text_sync_off),
                description.getText());
        verify(mTracker).notifyEvent(eq(TAB_GROUP_CREATION_DIALOG_SHOWN));

        mTabGroupVisualDataDialogManager.hideDialog();
        verify(mTracker).dismissed(eq(TAB_GROUP_CREATION_DIALOG_SYNC_TEXT_FEATURE));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_SYNC_ANDROID})
    public void testVisualDataDialog_descriptionTextSetAndSyncing() {
        doReturn(false).when(mTabModel).isIncognitoBranded();
        doReturn(true)
                .when(mTracker)
                .shouldTriggerHelpUi(TAB_GROUP_CREATION_DIALOG_SYNC_TEXT_FEATURE);
        when(mSyncService.getActiveDataTypes())
                .thenReturn(Collections.singleton(DataType.SAVED_TAB_GROUP));

        mTabGroupVisualDataDialogManager.showDialog(
                TAB_GROUP_ID, mTabGroupModelFilter, mDialogController);
        verify(mModalDialogManager).showDialog(mModelCaptor.capture(), eq(ModalDialogType.APP));

        PropertyModel model = mModelCaptor.getValue();
        TextView description =
                model.get(ModalDialogProperties.CUSTOM_VIEW)
                        .findViewById(R.id.visual_data_dialog_description);
        Assert.assertEquals(View.VISIBLE, description.getVisibility());
        Assert.assertEquals(
                mActivity
                        .getResources()
                        .getString(R.string.tab_group_creation_dialog_description_text_sync_on),
                description.getText());
        verify(mTracker).notifyEvent(eq(TAB_GROUP_CREATION_DIALOG_SHOWN));

        mTabGroupVisualDataDialogManager.hideDialog();
        verify(mTracker).dismissed(eq(TAB_GROUP_CREATION_DIALOG_SYNC_TEXT_FEATURE));
    }
}
