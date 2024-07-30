// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.SelectAroundCaretResult;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.touch_selection.SelectionEventType;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link TapToSeekSelectionManager} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({
    ChromeFeatureList.READALOUD,
    ChromeFeatureList.READALOUD_PLAYBACK,
    ChromeFeatureList.READALOUD_TAP_TO_SEEK
})
public class TapToSeekSelectionManagerUnitTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock private ReadAloudController mReadAloudController;

    @Mock private Profile mProfile;
    private static final GURL sTestGURL = JUnitTestGURLs.EXAMPLE_URL;
    @Mock private ObservableSupplier<Tab> mMockTabProvider;
    @Mock private Tab mTab;
    @Mock private Tab mTab2;
    @Mock private WebContents mWebContents;
    @Mock private WebContents mWebContents2;
    @Mock private SelectionClient mSelectionClient;
    @Mock private SelectionPopupController mSelectionPopupController;
    @Mock private SelectionClient mSmartSelectionClient;
    @Mock private SelectionClient mSmartSelectionClient2;

    private TapToSeekSelectionManager mTapToSeekSelectionManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(false).when(mProfile).isOffTheRecord();
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mWebContents2).when(mTab2).getWebContents();
        doReturn(mTab).when(mMockTabProvider).get();
        TapToSeekSelectionManager.setSmartSelectionClient(mSmartSelectionClient);
        TapToSeekSelectionManager.setSelectionPopupController(mSelectionPopupController);

        mTapToSeekSelectionManager =
                new TapToSeekSelectionManager(mReadAloudController, mMockTabProvider);
    }

    @Test
    public void testOnSurroundingTextReceived() {
        mTapToSeekSelectionManager.onSurroundingTextReceived("test", 12, 12);
        verify(mReadAloudController).tapToSeek("test", 12, 12);
    }

    @Test
    public void testOnSurroundingTextReceived_NotTap() {
        mTapToSeekSelectionManager.onSurroundingTextReceived("test", 12, 16);
        verify(mReadAloudController, never()).tapToSeek("test", 12, 16);
    }

    @Test
    public void testOnActivePlaybackTabUpdated() {
        mTapToSeekSelectionManager.onActivePlaybackTabUpdated(mTab);
        verify(mSelectionPopupController, times(1))
                .setSelectionClient(mTapToSeekSelectionManager.getSelectionClient());
        verify(mSmartSelectionClient, times(1))
                .addSurroundingTextReceivedListeners(mTapToSeekSelectionManager);
    }

    @Test
    public void testOnActivePlaybackTabUpdated_PreviousHooksRemoved() {
        mTapToSeekSelectionManager.onActivePlaybackTabUpdated(mTab);
        verify(mSelectionPopupController, times(1))
                .setSelectionClient(mTapToSeekSelectionManager.getSelectionClient());
        verify(mSmartSelectionClient, times(1))
                .addSurroundingTextReceivedListeners(mTapToSeekSelectionManager);

        // update playback tab to second Tab and check that hooks are released
        doReturn(mTapToSeekSelectionManager.getSelectionClient())
                .when(mSelectionPopupController)
                .getSelectionClient();
        TapToSeekSelectionManager.setSmartSelectionClient(mSmartSelectionClient2);
        mTapToSeekSelectionManager.onActivePlaybackTabUpdated(mTab2);
        verify(mSmartSelectionClient, times(1))
                .removeSurroundingTextReceivedListeners(mTapToSeekSelectionManager);
        verify(mSelectionPopupController, times(1)).setSelectionClient(null);
    }

    @Test
    public void testOnActivePlaybackTabUpdated_SameTab() {
        mTapToSeekSelectionManager.onActivePlaybackTabUpdated(mTab);
        verify(mSelectionPopupController, times(1))
                .setSelectionClient(mTapToSeekSelectionManager.getSelectionClient());
        verify(mSmartSelectionClient, times(1))
                .addSurroundingTextReceivedListeners(mTapToSeekSelectionManager);

        // update playback tab to same Tab and check that nothing happens
        mTapToSeekSelectionManager.onActivePlaybackTabUpdated(mTab);
        verify(mSmartSelectionClient, never())
                .removeSurroundingTextReceivedListeners(mTapToSeekSelectionManager);
        verify(mSelectionPopupController, never()).setSelectionClient(null);
    }

    @Test
    public void testTapToSeekSelectionClient() {
        // add hooks
        mTapToSeekSelectionManager.onActivePlaybackTabUpdated(mTab);

        TapToSeekSelectionManager.TapToSeekSelectionClient client =
                mTapToSeekSelectionManager.getSelectionClient();

        client.onSelectionChanged("selection");
        verify(mSmartSelectionClient, times(1)).onSelectionChanged("selection");
        verify(mSmartSelectionClient, times(1)).requestSelectionPopupUpdates(false);

        client.onSelectionEvent(SelectionEventType.SELECTION_HANDLES_SHOWN, 1, 2);
        verify(mSmartSelectionClient, times(1))
                .onSelectionEvent(SelectionEventType.SELECTION_HANDLES_SHOWN, 1, 2);

        SelectAroundCaretResult result = new SelectAroundCaretResult(0, 1, 2, 3);
        client.selectAroundCaretAck(result);
        verify(mSmartSelectionClient, times(1)).selectAroundCaretAck(result);

        client.requestSelectionPopupUpdates(true);
        verify(mSmartSelectionClient, times(1)).requestSelectionPopupUpdates(true);

        client.cancelAllRequests();
        verify(mSmartSelectionClient, times(1)).cancelAllRequests();

        client.setTextClassifier(null);
        verify(mSmartSelectionClient, times(1)).setTextClassifier(null);

        client.getTextClassifier();
        verify(mSmartSelectionClient, times(1)).getTextClassifier();

        client.getCustomTextClassifier();
        verify(mSmartSelectionClient, times(1)).getCustomTextClassifier();

        client.getSelectionEventProcessor();
        verify(mSmartSelectionClient, times(1)).getSelectionEventProcessor();
    }
}
