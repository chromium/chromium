// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static android.view.View.GONE;
import static android.view.View.VISIBLE;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.hub.HubBottomToolbarProperties.BOTTOM_TOOLBAR_VISIBLE;
import static org.chromium.chrome.browser.hub.HubColorMixer.COLOR_MIXER;

import android.app.Activity;
import android.view.LayoutInflater;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link HubBottomToolbarView} and {@link HubBottomToolbarViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubBottomToolbarViewUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private HubColorMixer mHubColorMixer;

    private HubBottomToolbarView mBottomToolbarView;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        mBottomToolbarView =
                (HubBottomToolbarView)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.hub_bottom_toolbar_layout, null, false);

        mPropertyModel =
                new PropertyModel.Builder(HubBottomToolbarProperties.ALL_BOTTOM_KEYS)
                        .with(COLOR_MIXER, mHubColorMixer)
                        .with(BOTTOM_TOOLBAR_VISIBLE, false)
                        .build();

        PropertyModelChangeProcessor.create(
                mPropertyModel, mBottomToolbarView, HubBottomToolbarViewBinder::bind);

        activity.setContentView(mBottomToolbarView);
    }

    @Test
    @SmallTest
    public void testVisibilityToggle() {
        // Initial state should be GONE
        assertEquals(GONE, mBottomToolbarView.getVisibility());

        // Test setting visibility to true
        mPropertyModel.set(BOTTOM_TOOLBAR_VISIBLE, true);
        assertEquals(VISIBLE, mBottomToolbarView.getVisibility());

        // Test setting visibility to false
        mPropertyModel.set(BOTTOM_TOOLBAR_VISIBLE, false);
        assertEquals(GONE, mBottomToolbarView.getVisibility());
    }
}
