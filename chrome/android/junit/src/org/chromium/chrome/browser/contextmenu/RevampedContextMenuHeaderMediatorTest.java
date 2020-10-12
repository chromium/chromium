// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.blink_public.common.ContextMenuDataMediaType;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserver;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserver.PerformanceClass;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserverJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Unit tests for the Revamped context menu header mediator.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class RevampedContextMenuHeaderMediatorTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    PerformanceHintsObserver.Natives mNativeMock;
    @Mock
    ContextMenuNativeDelegate mNativeDelegate;

    private Activity mActivity;
    private final Profile mProfile = Mockito.mock(Profile.class);

    @Before
    public void setUpTest() {
        mActivity = Robolectric.setupActivity(Activity.class);
        MockitoAnnotations.initMocks(this);
        mocker.mock(PerformanceHintsObserverJni.TEST_HOOKS, mNativeMock);
    }

    @Test
    public void testPerformanceInfoEnabled() {
        when(mNativeMock.isContextMenuPerformanceInfoEnabled()).thenReturn(true);
        PropertyModel model =
                new PropertyModel.Builder(RevampedContextMenuHeaderProperties.ALL_KEYS)
                        .with(RevampedContextMenuHeaderProperties.URL_PERFORMANCE_CLASS,
                                PerformanceClass.PERFORMANCE_UNKNOWN)
                        .build();
        final ContextMenuParams params =
                new ContextMenuParams(0, ContextMenuDataMediaType.IMAGE, "https://example.org",
                        "https://example.org/sitemap", "", "", "", "", null, false, 0, 0, 0);
        final RevampedContextMenuHeaderMediator mediator =
                new RevampedContextMenuHeaderMediator(mActivity, model,
                        PerformanceClass.PERFORMANCE_FAST, params, mProfile, mNativeDelegate);
        assertThat(model.get(RevampedContextMenuHeaderProperties.URL_PERFORMANCE_CLASS),
                equalTo(PerformanceClass.PERFORMANCE_FAST));
    }

    @Test
    public void testPerformanceInfoDisabled() {
        when(mNativeMock.isContextMenuPerformanceInfoEnabled()).thenReturn(false);
        PropertyModel model =
                new PropertyModel.Builder(RevampedContextMenuHeaderProperties.ALL_KEYS)
                        .with(RevampedContextMenuHeaderProperties.URL_PERFORMANCE_CLASS,
                                PerformanceClass.PERFORMANCE_UNKNOWN)
                        .build();
        final ContextMenuParams params =
                new ContextMenuParams(0, ContextMenuDataMediaType.IMAGE, "https://example.org",
                        "https://example.org/sitemap", "", "", "", "", null, false, 0, 0, 0);
        final RevampedContextMenuHeaderMediator mediator =
                new RevampedContextMenuHeaderMediator(mActivity, model,
                        PerformanceClass.PERFORMANCE_FAST, params, mProfile, mNativeDelegate);
        assertThat(model.get(RevampedContextMenuHeaderProperties.URL_PERFORMANCE_CLASS),
                equalTo(PerformanceClass.PERFORMANCE_UNKNOWN));
    }

    @Test
    public void testNoPerformanceInfoOnNonAnchor() {
        when(mNativeMock.isContextMenuPerformanceInfoEnabled()).thenReturn(true);
        PropertyModel model =
                new PropertyModel.Builder(RevampedContextMenuHeaderProperties.ALL_KEYS)
                        .with(RevampedContextMenuHeaderProperties.URL_PERFORMANCE_CLASS,
                                PerformanceClass.PERFORMANCE_UNKNOWN)
                        .build();
        final ContextMenuParams params = new ContextMenuParams(0, ContextMenuDataMediaType.IMAGE,
                "https://example.org", "", "", "", "", "", null, false, 0, 0, 0);
        final RevampedContextMenuHeaderMediator mediator =
                new RevampedContextMenuHeaderMediator(mActivity, model,
                        PerformanceClass.PERFORMANCE_FAST, params, mProfile, mNativeDelegate);
        assertThat(model.get(RevampedContextMenuHeaderProperties.URL_PERFORMANCE_CLASS),
                equalTo(PerformanceClass.PERFORMANCE_UNKNOWN));
    }
}
