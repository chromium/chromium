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
import org.chromium.chrome.browser.user_education.IPHCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;

/** Unit tests for {@link PageZoomIPHController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PageZoomIPHControllerTest {

    @Mock private Context mContext;
    @Mock private AppMenuHandler mAppMenuHandler;
    @Mock private View mToolbarMenuButton;
    @Mock private UserEducationHelper mUserEducationHelper;

    @Captor private ArgumentCaptor<IPHCommand> mIPHCommandCaptor;

    private final TestValues mTestValues = new TestValues();

    private PageZoomIPHController mPageZoomIPHController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        FeatureList.setTestValues(mTestValues);

        Resources resources = ApplicationProvider.getApplicationContext().getResources();
        doReturn(resources).when(mContext).getResources();
        doReturn(mContext).when(mToolbarMenuButton).getContext();

        mPageZoomIPHController =
                new PageZoomIPHController(
                        mAppMenuHandler, mToolbarMenuButton, mUserEducationHelper);
    }

    @Test
    public void testPageZoomIPHShown() {
        mPageZoomIPHController.showColdStartIPH();
        verify(mUserEducationHelper).requestShowIPH(mIPHCommandCaptor.capture());

        IPHCommand command = mIPHCommandCaptor.getValue();
        Assert.assertEquals(
                "IPHCommand feature should match.",
                command.featureName,
                FeatureConstants.PAGE_ZOOM_FEATURE);
        Assert.assertEquals(
                "IPHCommand stringId should match.",
                R.string.page_zoom_iph_message,
                command.stringId);

        command.onShowCallback.run();
        verify(mAppMenuHandler).setMenuHighlight(R.id.page_zoom_id);

        command.onDismissCallback.run();
        verify(mAppMenuHandler).clearMenuHighlight();
    }
}
