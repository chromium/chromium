// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;

import android.app.Activity;
import android.view.LayoutInflater;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ButtonCompat;

@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({ChromeFeatureList.SEARCH_ENGINE_CHOICE})
public class ChoiceScreenViewBinderUnitTest {
    public @Rule TestRule mFeatureProcessor = new Features.JUnitProcessor();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private final PropertyModel mModel = ChoiceScreenProperties.createPropertyModel();

    private ActivityController<Activity> mActivityController;
    private ChoiceScreenView mView;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(Activity.class);

        mView = (ChoiceScreenView) LayoutInflater.from(mActivityController.setup().get())
                        .inflate(R.layout.search_engine_choice_view, /*root=*/null);

        PropertyModelChangeProcessor.create(mModel, mView, ChoiceScreenViewBinder::bindContentView);
    }

    @After
    public void tearDown() {
        mActivityController.destroy();
    }

    @Test
    public void testInitialState() {
        ButtonCompat primaryButton = mView.findViewById(R.id.choice_screen_primary_button);
        assertFalse(primaryButton.isEnabled());
        assertFalse(primaryButton.hasOnClickListeners());

        RecyclerView recyclerView = mView.findViewById(R.id.choice_screen_list);
        assertEquals(0, recyclerView.getChildCount());
    }
}
