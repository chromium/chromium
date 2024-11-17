// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import static org.junit.Assert.assertFalse;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit test for {@link ReadAloudIphController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DisableFeatures({ChromeFeatureList.READALOUD_IPH_MENU_BUTTON_HIGHLIGHT_CCT})
public class ReadAloudIphControllerUnitTest {

    @Mock Activity mActivity;
    @Mock View mToolbarMenuButton;
    @Mock AppMenuHandler mAppMenuHandler;
    @Mock UserEducationHelper mUserEducationHelper;
    @Mock Context mContext;
    @Mock Resources mResources;
    @Captor ArgumentCaptor<IphCommand> mIphCommandCaptor;
    @Mock private ObservableSupplier<Tab> mMockTabProvider;
    @Mock ReadAloudController mReadAloudController;
    ObservableSupplierImpl<ReadAloudController> mReadAloudControllerSupplier;
    private MockTab mTab;
    @Mock private Profile mProfile;
    private static final GURL sTestGURL = JUnitTestGURLs.EXAMPLE_URL;

    ReadAloudIphController mController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mResources).when(mContext).getResources();
        doReturn(mContext).when(mToolbarMenuButton).getContext();

        doReturn(false).when(mProfile).isOffTheRecord();
        mTab = new MockTab(1, mProfile);
        mTab.setGurlOverrideForTesting(sTestGURL);
        doReturn(mTab).when(mMockTabProvider).get();

        mReadAloudControllerSupplier = new ObservableSupplierImpl<>();
        mReadAloudControllerSupplier.set(mReadAloudController);
        doReturn(true).when(mReadAloudController).isReadable(mTab);

        mController =
                new ReadAloudIphController(
                        mActivity,
                        mToolbarMenuButton,
                        mAppMenuHandler,
                        mUserEducationHelper,
                        mMockTabProvider,
                        mReadAloudControllerSupplier,
                        /* showAppMenuTextBubble= */ true);
    }

    @Test
    @SmallTest
    public void maybeShowReadAloudAppMenuIph() {
        mController.maybeShowReadAloudAppMenuIph();
        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());

        IphCommand command = mIphCommandCaptor.getValue();
        command.onShowCallback.run();
        verify(mAppMenuHandler).setMenuHighlight(R.id.readaloud_menu_id, true);

        command.onDismissCallback.run();
        verify(mAppMenuHandler).clearMenuHighlight();
    }

    @Test
    @SmallTest
    public void maybeShowReadAloudAppMenuIph_false() {
        doReturn(false).when(mReadAloudController).isReadable(mTab);

        mController.maybeShowReadAloudAppMenuIph();
        verify(mUserEducationHelper, never()).requestShowIph(mIphCommandCaptor.capture());
    }

    @Test
    @SmallTest
    public void maybeShowReadAloudAppMenuIph_invalid() {
        // invalid tab URL
        mTab.setGurlOverrideForTesting(new GURL("http://0x100.0/"));
        mController.maybeShowReadAloudAppMenuIph();
        verify(mUserEducationHelper, never()).requestShowIph(mIphCommandCaptor.capture());

        // null tab
        doReturn(null).when(mMockTabProvider).get();
        mController.maybeShowReadAloudAppMenuIph();
        verify(mUserEducationHelper, never()).requestShowIph(mIphCommandCaptor.capture());
    }

    @Test
    @SmallTest
    public void maybeShowReadAloudAppMenuIph_noTextBubble_disabledHighlight() {
        mController.setShowAppMenuTextBubble(false);
        mController.maybeShowReadAloudAppMenuIph();
        // we shouldn't show the text bubble
        verify(mUserEducationHelper, times(1)).requestShowIph(mIphCommandCaptor.capture());
        IphCommand command = mIphCommandCaptor.getValue();
        assertFalse(command.showTextBubble);
        // but there will still be a highlight WITHOUT the menu highlight
        command.onShowCallback.run();
        verify(mAppMenuHandler).setMenuHighlight(R.id.readaloud_menu_id, false);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.READALOUD_IPH_MENU_BUTTON_HIGHLIGHT_CCT})
    public void maybeShowReadAloudAppMenuIph_noTextBubble_enabledHighlight() {
        mController.setShowAppMenuTextBubble(false);
        mController.maybeShowReadAloudAppMenuIph();
        // we shouldn't show the text bubble
        verify(mUserEducationHelper, times(1)).requestShowIph(mIphCommandCaptor.capture());
        IphCommand command = mIphCommandCaptor.getValue();
        assertFalse(command.showTextBubble);
        // but there will still be a highlight WITH the menu highlight
        command.onShowCallback.run();
        verify(mAppMenuHandler).setMenuHighlight(R.id.readaloud_menu_id, true);
    }
}
