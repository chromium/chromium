// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.ActionMode;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.task.test.ShadowPostTask.TestImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar.CustomTabLocationBar;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.LocationBarModel;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.JUnitTestGURLs;

import java.util.concurrent.TimeUnit;

/**
 * Tests AMP url handling in the CustomTab Toolbar.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowLooper.class, ShadowPostTask.class})
@LooperMode(Mode.PAUSED)
public class CustomTabToolbarUnitTest {
    @Rule
    public MockitoRule mRule = MockitoJUnit.rule();

    @Mock
    LocationBarModel mLocationBarModel;
    @Mock
    ActionMode.Callback mActionModeCallback;
    @Mock
    CustomTabToolbarAnimationDelegate mAnimationDelegate;
    @Mock
    BrowserStateBrowserControlsVisibilityDelegate mControlsVisibleDelegate;

    private Activity mActivity;
    private CustomTabLocationBar mLocationBar;
    private TextView mTitleBar;
    private TextView mUrlBar;

    @Before
    public void setup() {
        ShadowPostTask.setTestImpl(new TestImpl() {
            @Override
            public void postDelayedTask(TaskTraits taskTraits, Runnable task, long delay) {
                new Handler(Looper.getMainLooper()).postDelayed(task, delay);
            }
        });
        Mockito.doReturn(R.string.accessibility_security_btn_secure)
                .when(mLocationBarModel)
                .getSecurityIconContentDescriptionResourceId();
        Mockito.doReturn(R.color.default_icon_color_tint_list)
                .when(mLocationBarModel)
                .getSecurityIconColorStateList();

        mActivity = Robolectric.buildActivity(TestActivity.class).get();
        CustomTabToolbar toolbar = (CustomTabToolbar) LayoutInflater.from(mActivity).inflate(
                R.layout.custom_tabs_toolbar, null, false);
        mLocationBar = (CustomTabLocationBar) toolbar.createLocationBar(mLocationBarModel,
                mActionModeCallback, () -> null, () -> null, mControlsVisibleDelegate);
        mUrlBar = toolbar.findViewById(R.id.url_bar);
        mTitleBar = toolbar.findViewById(R.id.title_bar);
        mLocationBar.setAnimDelegateForTesting(mAnimationDelegate);
    }

    @After
    public void tearDown() {
        mActivity.finish();
        ShadowPostTask.reset();
        CachedFeatureFlags.setForTesting(ChromeFeatureList.CCT_BRAND_TRANSPARENCY, null);
    }

    @Test
    public void testParsesPublisherFromAmp() {
        assertEquals("www.nyt.com",
                CustomTabToolbar.parsePublisherNameFromUrl(
                        JUnitTestGURLs.getGURL(JUnitTestGURLs.AMP_URL)));
        assertEquals("www.nyt.com",
                CustomTabToolbar.parsePublisherNameFromUrl(
                        JUnitTestGURLs.getGURL(JUnitTestGURLs.AMP_CACHE_URL)));
        assertEquals(JUnitTestGURLs.EXAMPLE_URL,
                CustomTabToolbar.parsePublisherNameFromUrl(
                        JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL)));
    }

    @Test
    public void testShowBranding_DomainOnly() {
        assertUrlAndTitleVisible(/*titleVisible=*/false, /*urlVisible=*/true);
        mLocationBar.showBranding();
        ShadowLooper.idleMainLooper();
        verify(mLocationBarModel, never()).notifyTitleChanged();
        verify(mAnimationDelegate, never()).prepareTitleAnim(mUrlBar, mTitleBar);
        verify(mAnimationDelegate, never()).startTitleAnimation(any());
        assertUrlAndTitleVisible(/*titleVisible=*/false, /*urlVisible=*/true);

        // Run all UI tasks, until the branding is finished.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mLocationBarModel, never()).notifyTitleChanged();
        verify(mLocationBarModel).notifyUrlChanged();
        verify(mLocationBarModel).notifySecurityStateChanged();

        assertEquals("URL bar is not visible.", mUrlBar.getVisibility(), View.VISIBLE);
    }

    @Test
    public void testShowBranding_DomainAndTitle() {
        // Set title before the branding starts, so the state is domain and title.
        mLocationBar.setShowTitle(true);
        ShadowLooper.idleMainLooper();
        verify(mLocationBarModel).notifyTitleChanged();
        verify(mAnimationDelegate).prepareTitleAnim(mUrlBar, mTitleBar);
        // Animation not started since branding is not completed.
        verify(mAnimationDelegate, never()).startTitleAnimation(any());

        // Title should be hidden, title animation is not necessary yet.
        mLocationBar.showBranding();
        ShadowLooper.idleMainLooper();
        verify(mLocationBarModel, times(2)).notifyTitleChanged();
        assertUrlAndTitleVisible(/*titleVisible=*/false, /*urlVisible=*/true);

        // Run all UI tasks, until the branding is finished.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mLocationBarModel, times(3)).notifyTitleChanged();
        verify(mLocationBarModel).notifyUrlChanged();
        verify(mLocationBarModel).notifySecurityStateChanged();
        verify(mAnimationDelegate, times(2)).prepareTitleAnim(mUrlBar, mTitleBar);

        assertEquals("URL bar is not visible.", mUrlBar.getVisibility(), View.VISIBLE);
    }

    @Test
    public void testToolbarBrandingDelegateImpl_EmptyToRegular() {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.CCT_BRAND_TRANSPARENCY, true);

        assertUrlAndTitleVisible(/*titleVisible=*/false, /*urlVisible=*/true);
        mLocationBar.showEmptyLocationBar();
        assertUrlAndTitleVisible(/*titleVisible=*/false, /*urlVisible=*/false);

        // Attempt to update title and URL, should noop since location bar is still in empty state.
        mLocationBar.onTitleChanged();
        mLocationBar.onUrlChanged();
        verify(mLocationBarModel, never()).notifyUrlChanged();
        verify(mLocationBarModel, never()).notifySecurityStateChanged();

        mLocationBar.showRegularToolbar();
        assertUrlAndTitleVisible(/*titleVisible=*/false, /*urlVisible=*/true);
        verify(mLocationBarModel).notifyUrlChanged();
        verify(mLocationBarModel).notifyTitleChanged();
        verify(mLocationBarModel).notifySecurityStateChanged();
        verifyBrowserControlVisibleForRequiredDuration();
    }

    @Test
    public void testToolbarBrandingDelegateImpl_EmptyToBranding() {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.CCT_BRAND_TRANSPARENCY, true);

        assertUrlAndTitleVisible(/*titleVisible=*/false, /*urlVisible=*/true);
        mLocationBar.showEmptyLocationBar();
        assertUrlAndTitleVisible(/*titleVisible=*/false, /*urlVisible=*/false);

        // Attempt to update title and URL, should noop since location bar is still in empty state.
        mLocationBar.setShowTitle(true);
        mLocationBar.setUrlBarHidden(false);
        verify(mLocationBarModel, never()).notifyUrlChanged();
        verify(mLocationBarModel, never()).notifySecurityStateChanged();

        mLocationBar.showBrandingLocationBar();
        assertUrlAndTitleVisible(/*titleVisible=*/false, /*urlVisible=*/true);

        // Attempt to update title and URL to show Title only - should be ignored during branding.
        reset(mLocationBarModel);
        mLocationBar.setShowTitle(true);
        mLocationBar.setUrlBarHidden(true);
        verifyNoMoreInteractions(mLocationBarModel);

        // After getting back to regular toolbar, title should become visible now.
        mLocationBar.showRegularToolbar();
        assertUrlAndTitleVisible(/*titleVisible=*/true, /*urlVisible=*/false);
        verify(mLocationBarModel, atLeastOnce()).notifyUrlChanged();
        verify(mLocationBarModel, atLeastOnce()).notifyTitleChanged();
        verify(mLocationBarModel, atLeastOnce()).notifySecurityStateChanged();
        verifyBrowserControlVisibleForRequiredDuration();
    }

    private void assertUrlAndTitleVisible(boolean titleVisible, boolean urlVisible) {
        int expectedTitleVisibility = titleVisible ? View.VISIBLE : View.GONE;
        int expectedUrlVisibility = urlVisible ? View.VISIBLE : View.GONE;

        assertEquals(
                "Title visibility is off.", expectedTitleVisibility, mTitleBar.getVisibility());
        assertEquals("URL bar visibility is off.", expectedUrlVisibility, mUrlBar.getVisibility());
    }

    private void verifyBrowserControlVisibleForRequiredDuration() {
        // Verify browser control is visible for required duration (3000ms).
        ShadowLooper looper = Shadows.shadowOf(Looper.getMainLooper());
        verify(mControlsVisibleDelegate).showControlsPersistent();
        looper.idleFor(2999, TimeUnit.MILLISECONDS);
        verify(mControlsVisibleDelegate, never()).releasePersistentShowingToken(anyInt());
        looper.idleFor(1, TimeUnit.MILLISECONDS);
        verify(mControlsVisibleDelegate).releasePersistentShowingToken(anyInt());
    }
}
