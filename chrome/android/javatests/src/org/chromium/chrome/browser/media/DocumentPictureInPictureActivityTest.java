// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Intent;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;
import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;

/** Tests for DocumentPictureInPictureActivity. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@MinAndroidSdkLevel(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
public class DocumentPictureInPictureActivityTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Mock private AconfigFlaggedApiDelegate mAconfigMock;

    private Tab mTab;
    private WebContents mParentWebContents;
    private WebContents mWebContents;

    @Before
    public void setUp() {
        mActivityTestRule.startOnBlankPage();
        mTab = mActivityTestRule.getActivityTab();
        mParentWebContents = mTab.getWebContents();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWebContents =
                            spy(
                                    WebContentsFactory.createWebContents(
                                            mTab.getProfile(),
                                            /* initiallyHidden= */ false,
                                            /* initializeRenderer= */ true));
                    doReturn(mParentWebContents)
                            .when(mWebContents)
                            .getDocumentPictureInPictureOpener();
                });

        DocumentPictureInPictureActivity.setWebContentsForTesting(mWebContents);
        DocumentPictureInPictureActivity.setIgnoreSdkVersionForTesting(true);

        Promise<Void> promise = ThreadUtils.runOnUiThreadBlocking(() -> Promise.fulfilled(null));
        when(mAconfigMock.requestPinnedWindowingLayer(any(), any())).thenReturn(promise);
        AconfigFlaggedApiDelegate.setInstanceForTesting(mAconfigMock);
    }

    @After
    public void tearDown() {
        DocumentPictureInPictureActivity.setWebContentsForTesting(null);
    }

    @Test
    @MediumTest
    public void testStartActivity() throws Exception {
        DocumentPictureInPictureActivity activity = launchActivity();

        CriteriaHelper.pollUiThread(() -> !activity.isFinishing());
        verify(mAconfigMock).requestPinnedWindowingLayer(any(), any());
    }

    @Test
    @MediumTest
    public void testExitOnInitiatorTabClose() throws Exception {
        DocumentPictureInPictureActivity activity = launchActivity();
        CriteriaHelper.pollUiThread(() -> !activity.isFinishing());

        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        CriteriaHelper.pollUiThread(() -> activity.isFinishing() || activity.isDestroyed());
    }

    @Test
    @MediumTest
    public void testExitOnCloseContents() throws Exception {
        DocumentPictureInPictureActivity activity = launchActivity();
        CriteriaHelper.pollUiThread(() -> !activity.isFinishing());

        JavaScriptUtils.executeJavaScript(mWebContents, "window.close()");

        CriteriaHelper.pollUiThread(() -> activity.isFinishing() || activity.isDestroyed());
    }

    @Test
    @MediumTest
    public void testExitOnNavigation() throws Exception {
        DocumentPictureInPictureActivity activity = launchActivity();
        CriteriaHelper.pollUiThread(() -> !activity.isFinishing());

        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mWebContents, "window.location.href='https://www.example.com/';");

        CriteriaHelper.pollUiThread(() -> activity.isFinishing() || activity.isDestroyed());
    }

    @Test
    @MediumTest
    public void testExitOnBackToTab() throws Exception {
        DocumentPictureInPictureActivity activity = launchActivity();
        CriteriaHelper.pollUiThread(() -> !activity.isFinishing());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Click on the back to tab button.
                    View headerView =
                            activity.getWindow()
                                    .getDecorView()
                                    .findViewById(R.id.document_picture_in_picture_header);
                    View backToTabButton =
                            headerView.findViewById(
                                    R.id.document_picture_in_picture_header_back_to_tab);
                    backToTabButton.performClick();
                });

        CriteriaHelper.pollUiThread(() -> activity.isFinishing() || activity.isDestroyed());
    }

    @Test
    @MediumTest
    public void testActivityRecreationOnDensityChange() throws Exception {
        DocumentPictureInPictureActivity activity = launchActivity();
        CriteriaHelper.pollUiThread(() -> !activity.isFinishing());

        // Remove window options and web contents to ensure they are restored from the bundle.
        activity.getIntent().removeExtra(DocumentPictureInPictureActivity.WINDOW_OPTIONS_KEY);
        DocumentPictureInPictureActivity.setWebContentsForTesting(null);

        // Trigger a density change.
        Configuration currentConfig = activity.getResources().getConfiguration();
        Configuration newConfig = new Configuration(currentConfig);
        newConfig.densityDpi += 20;
        DocumentPictureInPictureActivity newActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        DocumentPictureInPictureActivity.class,
                        Stage.RESUMED,
                        () -> activity.performOnConfigurationChanged(newConfig));

        CriteriaHelper.pollUiThread(() -> activity.isFinishing() || activity.isDestroyed());
        CriteriaHelper.pollUiThread(() -> !newActivity.isFinishing());
        CriteriaHelper.pollUiThread(() -> newActivity.getWebContentsForTesting() == mWebContents);
    }

    @Test
    @MediumTest
    public void testActivityRecreationOnUiModeChange() throws Exception {
        DocumentPictureInPictureActivity activity = launchActivity();
        CriteriaHelper.pollUiThread(() -> !activity.isFinishing());

        // Remove window options and web contents to ensure they are restored from the bundle.
        activity.getIntent().removeExtra(DocumentPictureInPictureActivity.WINDOW_OPTIONS_KEY);
        DocumentPictureInPictureActivity.setWebContentsForTesting(null);

        // Trigger a UI mode change
        DocumentPictureInPictureActivity newActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        DocumentPictureInPictureActivity.class,
                        Stage.RESUMED,
                        () -> activity.onNightModeStateChanged());

        CriteriaHelper.pollUiThread(() -> activity.isFinishing() || activity.isDestroyed());
        CriteriaHelper.pollUiThread(() -> !newActivity.isFinishing());
        CriteriaHelper.pollUiThread(() -> newActivity.getWebContentsForTesting() == mWebContents);
    }

    private DocumentPictureInPictureActivity launchActivity() throws Exception {
        Intent intent =
                new Intent(
                        InstrumentationRegistry.getInstrumentation().getTargetContext(),
                        DocumentPictureInPictureActivity.class);
        // We set WebContents via static setter in setUp, so we don't need to put it in intent.
        // But we do need window options.
        Bundle optionsBundle = new Bundle();
        intent.putExtra(DocumentPictureInPictureActivity.WINDOW_OPTIONS_KEY, optionsBundle);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        return ActivityTestUtils.launchActivityWithTimeout(
                InstrumentationRegistry.getInstrumentation(),
                DocumentPictureInPictureActivity.class,
                () -> {
                    InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
                    return null;
                },
                10000);
    }
}
