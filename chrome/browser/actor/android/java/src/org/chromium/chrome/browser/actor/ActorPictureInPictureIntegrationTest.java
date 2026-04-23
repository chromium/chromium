// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.util.Size;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.util.Collections;

/**
 * Integration tests for {@link ActorPictureInPictureController} and {@link
 * OffscreenRenderingManager}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Tests PiP transitions which involve activity level state.")
public class ActorPictureInPictureIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private ActorPictureInPictureController mController;
    private Tab mTab;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTab = mActivityTestRule.getActivityTab();
        ChromeTabUtils.waitForTabPageLoaded(mTab, (String) null);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = mActivityTestRule.getProfile(false);
                    mController =
                            new ActorPictureInPictureController(
                                    mActivityTestRule.getActivity(),
                                    () -> profile,
                                    () ->
                                            mActivityTestRule
                                                    .getActivity()
                                                    .findViewById(android.R.id.content),
                                    () -> mActivityTestRule.getActivity().getTabModelSelector(),
                                    () -> {},
                                    (glic) -> {},
                                    new Size(1080, 1920),
                                    (inPip) -> {});
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (mController != null) {
                        mController.destroy();
                    }
                    OffscreenRenderingManager.getInstance().destroy();
                });
    }

    @Test
    @MediumTest
    public void testOffscreenRenderingTransition() throws Exception {
        WebContents webContents = mTab.getWebContents();
        WindowAndroid originalWindow = webContents.getTopLevelNativeWindow();
        assertNotNull(originalWindow);
        assertFalse(mTab.getIsOffscreenRenderingSupplier().get());

        // Simulate entering PiP.
        // In a real scenario, this is triggered by onPictureInPictureModeChanged,
        // but we can invoke the internal logic directly for the integration test.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Setup mock actor task so that getCurrentActingTab returns our tab.
                    ActorTask mockTask = org.mockito.Mockito.mock(ActorTask.class);
                    org.mockito.Mockito.when(mockTask.getLastActedTabs())
                            .thenReturn(Collections.singleton(mTab.getId()));
                    ActorKeyedService actorService =
                            org.mockito.Mockito.mock(ActorKeyedService.class);
                    org.mockito.Mockito.when(actorService.getCurrentActiveTask())
                            .thenReturn(mockTask);
                    org.mockito.Mockito.when(actorService.getActiveTasksCount()).thenReturn(1);
                    ActorKeyedServiceFactory.setForTesting(actorService);

                    mController.onPictureInPictureEvent(
                            androidx.core.pip.PictureInPictureDelegate.Event.ENTERED, null);
                });

        // Verify offscreen rendering is active.
        assertTrue(mTab.getIsOffscreenRenderingSupplier().get());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WindowAndroid offscreenWindow = webContents.getTopLevelNativeWindow();
                    assertNotNull(offscreenWindow);
                    assertTrue(offscreenWindow != originalWindow);
                });

        // Simulate exiting PiP.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mController.onPictureInPictureEvent(
                            androidx.core.pip.PictureInPictureDelegate.Event.EXITED, null);
                });

        // Verify restoration.
        assertFalse(mTab.getIsOffscreenRenderingSupplier().get());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(originalWindow, webContents.getTopLevelNativeWindow());
                });
    }
}
