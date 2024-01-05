// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.DeclutterMessageCardViewProperties.ALL_KEYS;
import static org.chromium.chrome.browser.tasks.tab_management.DeclutterMessageCardViewProperties.ARCHIVED_TABS_EXPAND_CLICK_HANDLER;
import static org.chromium.chrome.browser.tasks.tab_management.DeclutterMessageCardViewProperties.ARCHIVED_TAB_COUNT;
import static org.chromium.chrome.browser.tasks.tab_management.DeclutterMessageCardViewProperties.DECLUTTER_INFO_TEXT;
import static org.chromium.chrome.browser.tasks.tab_management.DeclutterMessageCardViewProperties.DECLUTTER_SETTINGS_CLICK_HANDLER;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for DeclutterMessageCardViewBinder. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DeclutterMessageCardViewBinderUnitTest {
    private Activity mActivity;
    private View mDeclutterMessageCardView;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mDeclutterMessageCardView =
                LayoutInflater.from(mActivity)
                        .inflate(R.layout.declutter_message_card_layout, /* root= */ null);

        mModel =
                new PropertyModel.Builder(ALL_KEYS)
                        .with(DECLUTTER_INFO_TEXT, R.plurals.tab_declutter_message_card_text_info)
                        .with(ARCHIVED_TAB_COUNT, 10)
                        .with(ARCHIVED_TABS_EXPAND_CLICK_HANDLER, () -> {})
                        .with(DECLUTTER_SETTINGS_CLICK_HANDLER, () -> {})
                        .build();

        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mDeclutterMessageCardView, DeclutterMessageCardViewBinder::bind);
    }

    @Test
    public void testInitialSetup() {
        TextView descriptionView = mDeclutterMessageCardView.findViewById(R.id.declutter_info_text);
        Assert.assertEquals(
                mActivity
                        .getResources()
                        .getQuantityString(
                                mModel.get(DECLUTTER_INFO_TEXT),
                                mModel.get(ARCHIVED_TAB_COUNT),
                                mModel.get(ARCHIVED_TAB_COUNT)),
                descriptionView.getText());

        View settingsButton = mDeclutterMessageCardView.findViewById(R.id.declutter_settings);
        Assert.assertNotNull(settingsButton);

        View expandButton = mDeclutterMessageCardView.findViewById(R.id.declutter_expand_button);
        Assert.assertNotNull(expandButton);
    }

    @Test
    public void testDeclutterTextInfoSingular() {
        mModel.set(ARCHIVED_TAB_COUNT, 1);

        TextView descriptionView = mDeclutterMessageCardView.findViewById(R.id.declutter_info_text);
        Assert.assertEquals(
                mActivity
                        .getResources()
                        .getQuantityString(
                                mModel.get(DECLUTTER_INFO_TEXT),
                                mModel.get(ARCHIVED_TAB_COUNT),
                                mModel.get(ARCHIVED_TAB_COUNT)),
                descriptionView.getText());
    }
}
