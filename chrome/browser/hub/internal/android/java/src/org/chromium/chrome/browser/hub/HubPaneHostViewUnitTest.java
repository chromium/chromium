// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import static org.chromium.chrome.browser.hub.HubPaneHostProperties.ACTION_BUTTON_DATA;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

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

        mActionButton.callOnClick();
        verify(mOnActionButton).run();

        Mockito.reset(mOnActionButton);
        mPropertyModel.set(ACTION_BUTTON_DATA, null);

        mActionButton.callOnClick();
        verifyNoInteractions(mOnActionButton);
    }
}
