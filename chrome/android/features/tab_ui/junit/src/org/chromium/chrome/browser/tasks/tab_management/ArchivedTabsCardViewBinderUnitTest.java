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

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.R;
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

    private Activity mActivity;
    private View mArchivedTabsCardView;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
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
        assertEquals(titleView.getText(), "Inactive tab (1)");

        TextView subtitleView = mArchivedTabsCardView.findViewById(R.id.subtitle);
        assertEquals(subtitleView.getText(), "Not used for 1 day or more");
    }

    @Test
    public void testPlural() throws TimeoutException {
        TextView titleView = mArchivedTabsCardView.findViewById(R.id.title);
        assertEquals(titleView.getText(), "Inactive tabs (10)");

        TextView subtitleView = mArchivedTabsCardView.findViewById(R.id.subtitle);
        assertEquals(subtitleView.getText(), "Not used for 14 days or more");

        mArchivedTabsCardView.callOnClick();
        mCallbackHelper.waitForOnly();
    }
}
