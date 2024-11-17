// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;

/** Unit tests for {@link PageZoomIphController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PageZoomIphControllerTest {

    @Mock private Context mContext;
    @Mock private AppMenuHandler mAppMenuHandler;
    @Mock private View mToolbarMenuButton;
    @Mock private UserEducationHelper mUserEducationHelper;

    @Captor private ArgumentCaptor<IphCommand> mIphCommandCaptor;

    private final TestValues mTestValues = new TestValues();

    private PageZoomIphController mPageZoomIphController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        FeatureList.setTestValues(mTestValues);

        Resources resources = ApplicationProvider.getApplicationContext().getResources();
        doReturn(resources).when(mContext).getResources();
        doReturn(mContext).when(mToolbarMenuButton).getContext();

        mPageZoomIphController =
                new PageZoomIphController(
                        mAppMenuHandler, mToolbarMenuButton, mUserEducationHelper);
    }

    @Test
    public void testPageZoomIphShown() {
        mPageZoomIphController.showColdStartIph();
        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());

        IphCommand command = mIphCommandCaptor.getValue();
        Assert.assertEquals(
                "IphCommand feature should match.",
                command.featureName,
                FeatureConstants.PAGE_ZOOM_FEATURE);
        Assert.assertEquals(
                "IphCommand stringId should match.",
                R.string.page_zoom_iph_message,
                command.stringId);

        command.onShowCallback.run();
        verify(mAppMenuHandler).setMenuHighlight(R.id.page_zoom_id);

        command.onDismissCallback.run();
        verify(mAppMenuHandler).clearMenuHighlight();
    }
}
