// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactoryJni;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.GlowSpec;

import java.util.Arrays;
import java.util.Collections;

/** Unit tests for {@link GlicBottomSheetContent}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GlicBottomSheetContentUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private ActorKeyedService mActorKeyedService;
    @Mock private BottomSheetContent mNextContent;
    @Mock private ActorKeyedServiceFactory.Natives mActorKeyedServiceFactoryJni;

    private Context mContext;
    private View mContentView;
    private GlicBottomSheetContent mContent;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContentView =
                LayoutInflater.from(mContext)
                        .inflate(
                                org.chromium.chrome.browser.context_sharing.R.layout
                                        .tab_bottom_sheet,
                                null);
        ActorKeyedServiceFactoryJni.setInstanceForTesting(mActorKeyedServiceFactoryJni);
        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);

        mContent = new GlicBottomSheetContent(mContentView, 0.7f, 0xFFFFFFFF, mProfile);
    }

    @Test
    public void testConstructor_InitializesCorrectly() {
        assertEquals(mContentView, mContent.getContentView());
        assertEquals(
                BottomSheetContent.HeightMode.WRAP_CONTENT, mContent.getFullHeightRatio(), 0.001f);
    }

    @Test
    public void testSheetBackgroundGlowSpecOverride() {
        GlowSpec glowSpec = mContent.getSheetBackgroundGlowSpecOverride();
        assertNotNull(glowSpec);
        assertEquals(mContext.getColor(R.color.default_bg_color_blue), glowSpec.color);
        assertEquals(GlowSpec.ShadowSize.LONG, glowSpec.size);
    }

    @Test
    public void testCanBeSuppressed_NoService() {
        ActorKeyedServiceFactory.setForTesting(null);
        when(mActorKeyedServiceFactoryJni.getForProfile(mProfile)).thenReturn(null);
        assertTrue(mContent.canBeSuppressed(mNextContent));
    }

    @Test
    public void testCanBeSuppressed_NoActiveTasks() {
        when(mActorKeyedService.getActiveTasks()).thenReturn(null);
        assertTrue(mContent.canBeSuppressed(mNextContent));

        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.emptyList());
        assertTrue(mContent.canBeSuppressed(mNextContent));
    }

    @Test
    public void testCanBeSuppressed_ActiveTaskUnderActorControl() {
        ActorTask mockTask = mock(ActorTask.class);
        when(mockTask.isUnderActorControl()).thenReturn(true);
        when(mockTask.isCompleted()).thenReturn(false);

        when(mActorKeyedService.getActiveTasks()).thenReturn(Arrays.asList(mockTask));
        assertFalse(mContent.canBeSuppressed(mNextContent));
    }

    @Test
    public void testCanBeSuppressed_ActiveTaskCompleted() {
        ActorTask mockTask = mock(ActorTask.class);
        when(mockTask.isUnderActorControl()).thenReturn(true);
        when(mockTask.isCompleted()).thenReturn(true);

        when(mActorKeyedService.getActiveTasks()).thenReturn(Arrays.asList(mockTask));
        assertTrue(mContent.canBeSuppressed(mNextContent));
    }

    @Test
    public void testCanBeSuppressed_ActiveTaskNotUnderActorControl() {
        ActorTask mockTask = mock(ActorTask.class);
        when(mockTask.isUnderActorControl()).thenReturn(false);
        when(mockTask.isCompleted()).thenReturn(false);

        when(mActorKeyedService.getActiveTasks()).thenReturn(Arrays.asList(mockTask));
        assertTrue(mContent.canBeSuppressed(mNextContent));
    }

    @Test
    public void testCanBeSuppressed_MixedTasks_SuppressedIfAnyUnderActorControl() {
        ActorTask finishedTask = mock(ActorTask.class);
        when(finishedTask.isUnderActorControl()).thenReturn(true);
        when(finishedTask.isCompleted()).thenReturn(true);

        ActorTask passiveTask = mock(ActorTask.class);
        when(passiveTask.isUnderActorControl()).thenReturn(false);
        when(passiveTask.isCompleted()).thenReturn(false);

        ActorTask activeTask = mock(ActorTask.class);
        when(activeTask.isUnderActorControl()).thenReturn(true);
        when(activeTask.isCompleted()).thenReturn(false);

        when(mActorKeyedService.getActiveTasks())
                .thenReturn(Arrays.asList(finishedTask, passiveTask, activeTask));
        assertFalse(mContent.canBeSuppressed(mNextContent));
    }
}
