// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.ActionMode;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.task.test.ShadowPostTask.TestImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar.CustomTabLocationBar;
import org.chromium.chrome.browser.toolbar.LocationBarModel;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.JUnitTestGURLs;

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

        Activity activity = Robolectric.buildActivity(TestActivity.class).get();
        CustomTabToolbar toolbar = (CustomTabToolbar) LayoutInflater.from(activity).inflate(
                R.layout.custom_tabs_toolbar, null, false);
        mLocationBar = (CustomTabLocationBar) toolbar.createLocationBar(
                mLocationBarModel, mActionModeCallback, () -> null, () -> null);
        mUrlBar = toolbar.findViewById(R.id.url_bar);
        mTitleBar = toolbar.findViewById(R.id.title_bar);
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
        mLocationBar.setAnimDelegateForTesting(mAnimationDelegate);

        mLocationBar.showBranding();
        ShadowLooper.idleMainLooper();
        verify(mLocationBarModel).notifyTitleChanged();
        verify(mAnimationDelegate).prepareTitleAnim(mUrlBar, mTitleBar);
        verify(mAnimationDelegate).startTitleAnimation(any());
        assertEquals("URL bar should not be visible during branding.", mUrlBar.getVisibility(),
                View.GONE);

        // Run all UI tasks, until the branding is finished.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mLocationBarModel, times(2)).notifyTitleChanged();
        verify(mLocationBarModel).notifyUrlChanged();
        verify(mLocationBarModel).notifySecurityStateChanged();

        assertEquals("URL bar is not visible.", mUrlBar.getVisibility(), View.VISIBLE);
    }

    @Test
    public void testShowBranding_DomainAndTitle() {
        mLocationBar.setAnimDelegateForTesting(mAnimationDelegate);

        // Set title before the branding starts, so the state is domain and title.
        mLocationBar.setShowTitle(true);
        ShadowLooper.idleMainLooper();
        verify(mLocationBarModel).notifyTitleChanged();
        verify(mAnimationDelegate).prepareTitleAnim(mUrlBar, mTitleBar);
        // Animation not started since branding is not completed.
        verify(mAnimationDelegate, never()).startTitleAnimation(any());

        mLocationBar.showBranding();
        ShadowLooper.idleMainLooper();
        verify(mLocationBarModel, times(2)).notifyTitleChanged();
        verify(mAnimationDelegate, times(2)).prepareTitleAnim(mUrlBar, mTitleBar);
        verify(mAnimationDelegate).startTitleAnimation(any());
        assertEquals("URL bar should not be visible during branding.", mUrlBar.getVisibility(),
                View.GONE);

        // Run all UI tasks, until the branding is finished.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mLocationBarModel, times(3)).notifyTitleChanged();
        verify(mLocationBarModel).notifyUrlChanged();
        verify(mLocationBarModel).notifySecurityStateChanged();

        assertEquals("URL bar is not visible.", mUrlBar.getVisibility(), View.VISIBLE);
    }
}
