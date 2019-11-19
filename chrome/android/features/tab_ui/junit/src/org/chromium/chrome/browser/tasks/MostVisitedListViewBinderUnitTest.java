// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.tasks.MostVisitedListProperties.IS_VISIBLE;

import android.view.View;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests for {@link MostVisitedListViewBinder}.
 */
@RunWith(LocalRobolectricTestRunner.class)
public final class MostVisitedListViewBinderUnitTest {
    @Mock
    private ViewGroup mViewGroup;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void bind_setsVisibilityCorrectly() {
        PropertyModel model = new PropertyModel(MostVisitedListProperties.ALL_KEYS);

        Mockito.reset(mViewGroup);
        model.set(IS_VISIBLE, true);
        MostVisitedListViewBinder.bind(model, mViewGroup, IS_VISIBLE);
        verify(mViewGroup).setVisibility(View.VISIBLE);

        Mockito.reset(mViewGroup);
        model.set(IS_VISIBLE, false);
        MostVisitedListViewBinder.bind(model, mViewGroup, IS_VISIBLE);
        verify(mViewGroup).setVisibility(View.GONE);
    }
}
