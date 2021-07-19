// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.Window;
import android.view.WindowManager;
import android.view.WindowManager.LayoutParams;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;

/**
 * Unit tests for IncognitoTabSnapshotController.java.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DisableFeatures({ChromeFeatureList.INCOGNITO_SCREENSHOT})
public class IncognitoTabSnapshotControllerTest {
    private IncognitoTabSnapshotController mController;
    private WindowManager.LayoutParams mParams;

    @Mock
    Window mWindow;

    @Mock
    TabModelSelector mSelector;

    @Mock
    TabModel mTabModel;

    @Mock
    LayoutManagerChrome mLayoutManager;

    @Mock
    Context mContext;

    @Before
    public void before() {
        MockitoAnnotations.initMocks(this);

        mParams = new LayoutParams();
    }

    @Test
    public void testUpdateIncognitoState_IncognitoAndEarlyReturn() {
        mParams.flags = WindowManager.LayoutParams.FLAG_SECURE;
        doReturn(mParams).when(mWindow).getAttributes();

        mController = spy(
                new IncognitoTabSnapshotController(mContext, mWindow, mLayoutManager, mSelector));
        doReturn(true).when(mController).isShowingIncognito();
        mController.updateIncognitoState();
        verify(mWindow, never()).addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        verify(mWindow, never()).clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
    }

    @Test
    public void testUpdateIncognitoState_NotIncognitoAndEarlyReturn() {
        mParams.flags = 0;
        doReturn(mParams).when(mWindow).getAttributes();

        mController = spy(
                new IncognitoTabSnapshotController(mContext, mWindow, mLayoutManager, mSelector));
        doReturn(false).when(mController).isShowingIncognito();
        mController.updateIncognitoState();
        verify(mWindow, never()).addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        verify(mWindow, never()).clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
    }

    @Test
    public void testUpdateIncognitoState_SwitchingToIncognito() {
        mParams.flags = 0;
        doReturn(mParams).when(mWindow).getAttributes();

        mController = spy(
                new IncognitoTabSnapshotController(mContext, mWindow, mLayoutManager, mSelector));
        doReturn(true).when(mController).isShowingIncognito();
        mController.updateIncognitoState();
        verify(mWindow, atLeastOnce()).addFlags(WindowManager.LayoutParams.FLAG_SECURE);
    }

    @Test
    public void testUpdateIncognitoState_SwitchingToNonIncognito() {
        mParams.flags = WindowManager.LayoutParams.FLAG_SECURE;
        doReturn(mParams).when(mWindow).getAttributes();

        mController = spy(
                new IncognitoTabSnapshotController(mContext, mWindow, mLayoutManager, mSelector));
        doReturn(false).when(mController).isShowingIncognito();
        mController.updateIncognitoState();
        verify(mWindow, atLeastOnce()).clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
    }

    @Test
    public void testIsShowingIncognito_IncognitoModel_NotInOverviewMode() {
        mController =
                new IncognitoTabSnapshotController(mContext, mWindow, mLayoutManager, mSelector);
        mController.setInOverViewMode(false);
        doReturn(mTabModel).when(mSelector).getCurrentModel();
        doReturn(true).when(mTabModel).isIncognito();
        Assert.assertTrue("isShowingIncognito should be true", mController.isShowingIncognito());

        verify(mSelector, never()).getModel(true);
    }

    @Test
    public void testIsShowingIncognito_IncognitoModel_InOverviewMode() {
        mController =
                new IncognitoTabSnapshotController(mContext, mWindow, mLayoutManager, mSelector);
        mController.setInOverViewMode(true);
        doReturn(mTabModel).when(mSelector).getCurrentModel();
        doReturn(true).when(mTabModel).isIncognito();
        Assert.assertTrue("isShowingIncognito should be true", mController.isShowingIncognito());

        verify(mSelector, never()).getModel(true);
    }

    @Test
    public void testIsShowingIncognito_NormalModel_WithIncognitoTab_GridTabSwitcher() {
        mController = spy(
                new IncognitoTabSnapshotController(mContext, mWindow, mLayoutManager, mSelector));
        mController.setInOverViewMode(true);

        doReturn(mTabModel).when(mSelector).getCurrentModel();
        doReturn(false).when(mTabModel).isIncognito();
        doReturn(true).when(mController).isGridTabSwitcherEnabled();
        doReturn(mTabModel).when(mSelector).getModel(true);
        doReturn(1).when(mTabModel).getCount();
        Assert.assertFalse("isShowingIncognito should be false", mController.isShowingIncognito());
    }

    @Test
    public void testIsShowingIncognito_NormalModel_WithIncognitoTab() {
        mController = spy(
                new IncognitoTabSnapshotController(mContext, mWindow, mLayoutManager, mSelector));
        mController.setInOverViewMode(true);

        doReturn(mTabModel).when(mSelector).getCurrentModel();
        doReturn(false).when(mTabModel).isIncognito();
        doReturn(false).when(mController).isGridTabSwitcherEnabled();
        doReturn(mTabModel).when(mSelector).getModel(true);
        doReturn(1).when(mTabModel).getCount();
        Assert.assertTrue("isShowingIncognito should be true", mController.isShowingIncognito());
    }

    @Test
    public void testIsShowingIncognito_NormalModel_NoIncognitoTab() {
        mController = spy(
                new IncognitoTabSnapshotController(mContext, mWindow, mLayoutManager, mSelector));
        mController.setInOverViewMode(true);

        doReturn(mTabModel).when(mSelector).getCurrentModel();
        doReturn(false).when(mTabModel).isIncognito();
        doReturn(false).when(mController).isGridTabSwitcherEnabled();
        doReturn(mTabModel).when(mSelector).getModel(true);
        doReturn(0).when(mTabModel).getCount();
        Assert.assertFalse("isShowingIncognito should be false", mController.isShowingIncognito());
    }
}
