// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;

/** Unit tests for {@link PdfPageIphController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PdfPageIphControllerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ActivityTabProvider mActivityTabProvider;
    @Mock private View mToolbarMenuButton;
    @Mock private AppMenuHandler mAppMenuHandler;
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private WeakReference<Context> mWeakReferenceContext;
    @Mock private Tab mTab;
    @Mock private NativePage mNativePage;
    @Mock private Tracker mTracker;
    @Mock private Profile mProfile;

    @Captor private ArgumentCaptor<IphCommand> mIphCommandCaptor;

    private PdfPageIphController mController;

    @Before
    public void setUp() {
        PdfUtils.setShouldOpenPdfInlineForTesting(true);
        Context context = ApplicationProvider.getApplicationContext();
        doReturn(mWeakReferenceContext).when(mWindowAndroid).getContext();
        doReturn(context).when(mWeakReferenceContext).get();
        doReturn(context).when(mToolbarMenuButton).getContext();

        TrackerFactory.setTrackerForTests(mTracker);
        when(mTracker.wouldTriggerHelpUi(FeatureConstants.IPH_PDF_PAGE_DOWNLOAD)).thenReturn(true);

        when(mTab.isNativePage()).thenReturn(true);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.isPdf()).thenReturn(true);
    }

    @After
    public void tearDown() {
        TrackerFactory.setTrackerForTests(null);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testDownloadIph() {
        initializeController(true);
        ActivityTabTabObserver activityTabTabObserver =
                mController.getActiveTabObserverForTesting();
        activityTabTabObserver.onPageLoadFinished(mTab, JUnitTestGURLs.EXAMPLE_URL);
        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());
        // The download button in CTA appears in the icon row for phone.
        verifyIphCommand(true);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testDownloadIph_CCT() {
        initializeController(false);
        ActivityTabTabObserver activityTabTabObserver =
                mController.getActiveTabObserverForTesting();
        activityTabTabObserver.onPageLoadFinished(mTab, JUnitTestGURLs.EXAMPLE_URL);
        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());
        // The download button in CCT appears in the icon row regardless of whether it is on a phone
        // or tablet.
        verifyIphCommand(true);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testDownloadIph_NullTab() {
        initializeController(true);
        ActivityTabTabObserver activityTabTabObserver =
                mController.getActiveTabObserverForTesting();
        activityTabTabObserver.onPageLoadFinished(null, JUnitTestGURLs.EXAMPLE_URL);
        verify(mUserEducationHelper, never()).requestShowIph(mIphCommandCaptor.capture());
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testDownloadIph_NotPdf() {
        initializeController(true);
        when(mNativePage.isPdf()).thenReturn(false);
        ActivityTabTabObserver activityTabTabObserver =
                mController.getActiveTabObserverForTesting();
        activityTabTabObserver.onPageLoadFinished(mTab, JUnitTestGURLs.EXAMPLE_URL);
        verify(mUserEducationHelper, never()).requestShowIph(mIphCommandCaptor.capture());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testDownloadIph_Tablet() {
        initializeController(true);
        ActivityTabTabObserver activityTabTabObserver =
                mController.getActiveTabObserverForTesting();
        activityTabTabObserver.onPageLoadFinished(mTab, JUnitTestGURLs.EXAMPLE_URL);
        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());
        // The download button in CTA appears in the menu row for tablet.
        verifyIphCommand(false);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testDownloadIph_Tablet_CCT() {
        initializeController(false);
        ActivityTabTabObserver activityTabTabObserver =
                mController.getActiveTabObserverForTesting();
        activityTabTabObserver.onPageLoadFinished(mTab, JUnitTestGURLs.EXAMPLE_URL);
        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());
        // The download button in CCT appears in the icon row regardless of whether it is on a phone
        // or tablet.
        verifyIphCommand(true);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.HIDE_TABLET_TOOLBAR_DOWNLOAD_BUTTON)
    @Config(qualifiers = "sw600dp")
    public void testDownloadIph_Tablet_DownloadButtonInToolbar() {
        initializeController(true);
        ActivityTabTabObserver activityTabTabObserver =
                mController.getActiveTabObserverForTesting();
        activityTabTabObserver.onPageLoadFinished(mTab, JUnitTestGURLs.EXAMPLE_URL);
        verify(mUserEducationHelper, never()).requestShowIph(mIphCommandCaptor.capture());
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testDownloadIph_TrackerWouldNotTrigger() {
        initializeController(true);
        when(mTracker.wouldTriggerHelpUi(FeatureConstants.IPH_PDF_PAGE_DOWNLOAD)).thenReturn(false);
        ActivityTabTabObserver activityTabTabObserver =
                mController.getActiveTabObserverForTesting();
        activityTabTabObserver.onPageLoadFinished(mTab, JUnitTestGURLs.EXAMPLE_URL);
        verify(mUserEducationHelper, never()).requestShowIph(mIphCommandCaptor.capture());
    }

    private void initializeController(boolean isBrowserApp) {
        mController =
                new PdfPageIphController(
                        mWindowAndroid,
                        mActivityTabProvider,
                        mProfile,
                        mToolbarMenuButton,
                        mAppMenuHandler,
                        mUserEducationHelper,
                        isBrowserApp);
    }

    private void verifyIphCommand(boolean isIconRow) {
        IphCommand command = mIphCommandCaptor.getValue();
        Assert.assertEquals(
                "IphCommand feature should match.",
                FeatureConstants.IPH_PDF_PAGE_DOWNLOAD,
                command.featureName);
        Assert.assertEquals(
                "IphCommand stringId should match.",
                R.string.pdf_page_download_iph_text,
                command.stringId);

        command.onShowCallback.run();
        verify(mAppMenuHandler)
                .setMenuHighlight(isIconRow ? R.id.offline_page_id : R.id.download_page_id);

        command.onDismissCallback.run();
        verify(mAppMenuHandler).clearMenuHighlight();
    }
}
