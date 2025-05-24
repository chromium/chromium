// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.ALL_KEYS;
import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.ARCHIVE_TIME_DELTA_DAYS;
import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.CLICK_HANDLER;
import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.NUMBER_OF_ARCHIVED_TABS;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.concurrent.TimeoutException;

/** Tests for ArchivedTabsCardViewBinder. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ArchivedTabsCardViewBinderUnitTest {
    private static final int ARCHIVED_TABS = 10;
    private static final int TIME_DELTA = 14;

    private final CallbackHelper mCallbackHelper = new CallbackHelper();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private Activity mActivity;
    private View mArchivedTabsCardView;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    @Before
    public void setUp() throws Exception {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mArchivedTabsCardView =
                LayoutInflater.from(mActivity)
                        .inflate(R.layout.archived_tabs_message_card_view, /* root= */ null);

        mModel =
                new PropertyModel.Builder(ALL_KEYS)
                        .with(NUMBER_OF_ARCHIVED_TABS, ARCHIVED_TABS)
                        .with(ARCHIVE_TIME_DELTA_DAYS, TIME_DELTA)
                        .with(
                                CLICK_HANDLER,
                                () -> {
                                    mCallbackHelper.notifyCalled();
                                })
                        .build();

        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mArchivedTabsCardView, ArchivedTabsCardViewBinder::bind);
    }

    @Test
    public void testSingular() {
        mModel.set(NUMBER_OF_ARCHIVED_TABS, 1);
        mModel.set(ARCHIVE_TIME_DELTA_DAYS, 1);

        TextView titleView = mArchivedTabsCardView.findViewById(R.id.title);
        assertEquals("Inactive tab (1)", titleView.getText());

        TextView subtitleView = mArchivedTabsCardView.findViewById(R.id.subtitle);
        assertEquals("Not used for 1 day or more", subtitleView.getText());
    }

    @Test
    public void testPlural() throws TimeoutException {
        TextView titleView = mArchivedTabsCardView.findViewById(R.id.title);
        assertEquals("Inactive tabs (10)", titleView.getText());

        TextView subtitleView = mArchivedTabsCardView.findViewById(R.id.subtitle);
        assertEquals("Not used for 14 days or more", subtitleView.getText());

        mArchivedTabsCardView.callOnClick();
        mCallbackHelper.waitForOnly();
    }
}
