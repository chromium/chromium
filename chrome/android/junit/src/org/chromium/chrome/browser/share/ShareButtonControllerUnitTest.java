// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.CallbackUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.components.ukm.UkmRecorderJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

/** Unit tests for {@link ShareButtonController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@SuppressWarnings("DoNotMock") // Mocks GURL
public final class ShareButtonControllerUnitTest {
    private static final int WIDTH_DELTA = 50;

    @Rule public JniMocker mJniMocker = new JniMocker();

    private Context mContext;

    @Mock private UkmRecorder.Natives mUkmRecorderJniMock;
    @Mock private Resources mResources;
    @Mock private Tab mTab;
    @Mock private Drawable mDrawable;
    @Mock private ActivityTabProvider mTabProvider;
    @Mock private ObservableSupplier<ShareDelegate> mShareDelegateSupplier;
    @Mock private ShareDelegate mShareDelegate;
    @Mock private GURL mMockGurl;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Tracker mTracker;

    private Configuration mConfiguration = new Configuration();
    private ShareButtonController mShareButtonController;
    private ShareUtils mShareUtils = new ShareUtils();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;
        mJniMocker.mock(UkmRecorderJni.TEST_HOOKS, mUkmRecorderJniMock);

        doReturn(mTab).when(mTabProvider).get();
        doReturn(mContext).when(mTab).getContext();
        mConfiguration.screenWidthDp = AdaptiveToolbarFeatures.DEFAULT_MIN_WIDTH_DP + WIDTH_DELTA;
        doReturn(mConfiguration).when(mResources).getConfiguration();

        doReturn(mock(WebContents.class)).when(mTab).getWebContents();
        doReturn("https").when(mMockGurl).getScheme();
        doReturn(mMockGurl).when(mTab).getUrl();

        doReturn(mShareDelegate).when(mShareDelegateSupplier).get();

        AdaptiveToolbarFeatures.clearParsedParamsForTesting();

        mShareButtonController =
                new ShareButtonController(
                        mContext,
                        mDrawable,
                        mTabProvider,
                        mShareDelegateSupplier,
                        () -> mTracker,
                        mShareUtils,
                        mModalDialogManager,
                        CallbackUtils.emptyRunnable());

        TrackerFactory.setTrackerForTests(mTracker);
    }

    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
    @Test
    public void testIPHCommandHelper() {
        assertNull(
                mShareButtonController.get(/* tab= */ null).getButtonSpec().getIPHCommandBuilder());

        // Verify that IPHCommandBuilder is set just once;
        IPHCommandBuilder builder =
                mShareButtonController.get(mTab).getButtonSpec().getIPHCommandBuilder();

        assertNotNull(mShareButtonController.get(mTab).getButtonSpec().getIPHCommandBuilder());

        // Verify that IPHCommandBuilder is same as before, get(Tab) did not create a new one.
        assertEquals(
                builder, mShareButtonController.get(mTab).getButtonSpec().getIPHCommandBuilder());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
    public void testIPHEvent() {
        doReturn(true)
                .when(mTracker)
                .shouldTriggerHelpUI(
                        FeatureConstants
                                .ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_SHARE_FEATURE);

        View view = mock(View.class);
        mShareButtonController.get(mTab).getButtonSpec().getOnClickListener().onClick(view);

        verify(mTracker, times(1))
                .notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_SHARE_OPENED);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
    public void testDoNotShowOnDataUrl() {
        doReturn("data").when(mMockGurl).getScheme();
        doReturn(mMockGurl).when(mTab).getUrl();
        ButtonData buttonData = mShareButtonController.get(mTab);

        assertFalse(buttonData.canShow());
    }
}
