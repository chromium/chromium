// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.drawable.LayerDrawable;

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

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonDataProvider;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link GlicToolbarButtonController}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.GLIC,
    ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2
})
@DisableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
public class GlicToolbarButtonControllerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private Profile mProfile;
    @Mock private ActorKeyedService mActorService;
    @Mock private GlicToolbarButtonController.GlicButtonDelegate mToggleGlicCallback;
    @Mock private Tracker mTracker;
    @Mock private ButtonDataProvider.ButtonDataObserver mObserver;
    @Mock private GlicKeyedServiceFactory.Natives mGlicKeyedServiceFactoryJniMock;
    @Mock private GlicKeyedService mGlicKeyedService;
    @Captor private ArgumentCaptor<ActorKeyedService.Observer> mActorObserverCaptor;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        ActorKeyedServiceFactory.setForTesting(mActorService);
        GlicKeyedServiceFactoryJni.setInstanceForTesting(mGlicKeyedServiceFactoryJniMock);
        when(mGlicKeyedServiceFactoryJniMock.getForProfile(mProfile)).thenReturn(mGlicKeyedService);
    }

    @Test
    public void testButtonData() {
        GlicToolbarButtonController controller =
                new GlicToolbarButtonController(
                        mContext, () -> mTab, mToggleGlicCallback, () -> mTracker, () -> null);
        ButtonData buttonData = controller.get(mTab);

        Assert.assertTrue(buttonData.canShow());
        Assert.assertTrue(buttonData.isEnabled());
        Assert.assertNotNull(buttonData.getButtonSpec());
        Assert.assertEquals(
                mContext.getString(R.string.glic_button_entrypoint_ask_gemini_label),
                buttonData.getButtonSpec().getContentDescription());
    }

    @Test
    public void testButtonData_Ntp() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        GlicToolbarButtonController controller =
                new GlicToolbarButtonController(
                        mContext, () -> mTab, mToggleGlicCallback, () -> mTracker, () -> null);
        ButtonData buttonData = controller.get(mTab);

        Assert.assertFalse(buttonData.canShow());
    }

    @Test
    public void testButtonData_OffTheRecord() {
        when(mTab.isOffTheRecord()).thenReturn(true);
        when(mTab.isIncognito()).thenReturn(true);
        GlicToolbarButtonController controller =
                new GlicToolbarButtonController(
                        mContext, () -> mTab, mToggleGlicCallback, () -> mTracker, () -> null);
        ButtonData buttonData = controller.get(mTab);

        Assert.assertFalse(buttonData.canShow());
    }

    @Test
    public void testOnClick() {
        GlicToolbarButtonController controller =
                new GlicToolbarButtonController(
                        mContext, () -> mTab, mToggleGlicCallback, () -> mTracker, () -> null);

        controller.onClick(null);

        verify(mToggleGlicCallback).onClick(false);
    }

    @Test
    public void testTaskState_Review() {
        GlicToolbarButtonController controller =
                new GlicToolbarButtonController(
                        mContext, () -> mTab, mToggleGlicCallback, () -> mTracker, () -> null);
        controller.addObserver(mObserver);

        // Initial call to set up observation.
        controller.get(mTab);
        verify(mActorService).addObserver(mActorObserverCaptor.capture());
        ActorKeyedService.Observer actorObserver = mActorObserverCaptor.getValue();

        // Mock an active task that needs review.
        ActorTask task = mock(ActorTask.class);
        when(task.getState()).thenReturn(ActorTaskState.WAITING_ON_USER);
        when(mActorService.getCurrentActiveTask()).thenReturn(task);

        // Trigger state change.
        actorObserver.onTaskStateChanged(1, ActorTaskState.WAITING_ON_USER);

        ButtonData buttonData = controller.get(mTab);
        Assert.assertEquals(
                mContext.getString(R.string.glic_button_status_review),
                mContext.getString(buttonData.getButtonSpec().getActionChipLabelResId()));
        verify(mObserver).buttonDataChanged(true);
    }

    @Test
    public void testTaskState_Working() {
        GlicToolbarButtonController controller =
                new GlicToolbarButtonController(
                        mContext, () -> mTab, mToggleGlicCallback, () -> mTracker, () -> null);

        controller.get(mTab);
        verify(mActorService).addObserver(mActorObserverCaptor.capture());
        ActorKeyedService.Observer actorObserver = mActorObserverCaptor.getValue();

        // Mock an active task that is working.
        ActorTask task = mock(ActorTask.class);
        when(task.getState()).thenReturn(ActorTaskState.ACTING);
        when(mActorService.getCurrentActiveTask()).thenReturn(task);

        // Trigger state change.
        actorObserver.onTaskStateChanged(1, ActorTaskState.ACTING);

        ButtonData buttonData = controller.get(mTab);
        Assert.assertTrue(buttonData.getButtonSpec().getDrawable() instanceof LayerDrawable);
        Assert.assertEquals(0, buttonData.getButtonSpec().getActionChipLabelResId());
    }

    @Test
    public void testTaskState_Done_Persists() {
        GlicToolbarButtonController controller =
                new GlicToolbarButtonController(
                        mContext, () -> mTab, mToggleGlicCallback, () -> mTracker, () -> null);
        controller.addObserver(mObserver);

        controller.get(mTab);
        verify(mActorService).addObserver(mActorObserverCaptor.capture());
        ActorKeyedService.Observer actorObserver = mActorObserverCaptor.getValue();

        ActorTask task = mock(ActorTask.class);
        when(task.getState()).thenReturn(ActorTaskState.FINISHED);
        when(mActorService.getCurrentActiveTask()).thenReturn(task);

        actorObserver.onTaskStateChanged(1, ActorTaskState.FINISHED);

        ButtonData buttonData = controller.get(mTab);
        Assert.assertEquals(
                mContext.getString(R.string.glic_button_status_done),
                mContext.getString(buttonData.getButtonSpec().getActionChipLabelResId()));

        when(mActorService.getCurrentActiveTask()).thenReturn(null);

        buttonData = controller.get(mTab);
        Assert.assertEquals(
                mContext.getString(R.string.glic_button_status_done),
                mContext.getString(buttonData.getButtonSpec().getActionChipLabelResId()));
    }

    @Test
    public void testTaskState_Done_FromNotificationOnly() {
        GlicToolbarButtonController controller =
                new GlicToolbarButtonController(
                        mContext, () -> mTab, mToggleGlicCallback, () -> mTracker, () -> null);
        controller.addObserver(mObserver);

        controller.get(mTab);
        verify(mActorService).addObserver(mActorObserverCaptor.capture());
        ActorKeyedService.Observer actorObserver = mActorObserverCaptor.getValue();

        // Mock that the service returns null (task is gone).
        when(mActorService.getCurrentActiveTask()).thenReturn(null);

        // Trigger state change to FINISHED.
        actorObserver.onTaskStateChanged(1, ActorTaskState.FINISHED);

        // Verify that the state becomes DONE.
        ButtonData buttonData = controller.get(mTab);
        Assert.assertEquals(
                mContext.getString(R.string.glic_button_status_done),
                mContext.getString(buttonData.getButtonSpec().getActionChipLabelResId()));
        verify(mObserver).buttonDataChanged(true);
    }

    @Test
    public void testTaskState_Done_ClearedOnClick() {
        GlicToolbarButtonController controller =
                new GlicToolbarButtonController(
                        mContext, () -> mTab, mToggleGlicCallback, () -> mTracker, () -> null);

        controller.get(mTab);
        verify(mActorService).addObserver(mActorObserverCaptor.capture());
        ActorKeyedService.Observer actorObserver = mActorObserverCaptor.getValue();

        ActorTask task = mock(ActorTask.class);
        when(task.getState()).thenReturn(ActorTaskState.FINISHED);
        when(mActorService.getCurrentActiveTask()).thenReturn(task);

        actorObserver.onTaskStateChanged(1, ActorTaskState.FINISHED);

        when(mActorService.getCurrentActiveTask()).thenReturn(null);

        ButtonData buttonData = controller.get(mTab);
        Assert.assertEquals(
                mContext.getString(R.string.glic_button_status_done),
                mContext.getString(buttonData.getButtonSpec().getActionChipLabelResId()));

        controller.onClick(null);

        buttonData = controller.get(mTab);
        Assert.assertEquals(0, buttonData.getButtonSpec().getActionChipLabelResId());
    }

    @Test
    public void testTaskState_Done_ClearedOnNewTask() {
        GlicToolbarButtonController controller =
                new GlicToolbarButtonController(
                        mContext, () -> mTab, mToggleGlicCallback, () -> mTracker, () -> null);

        controller.get(mTab);
        verify(mActorService).addObserver(mActorObserverCaptor.capture());
        ActorKeyedService.Observer actorObserver = mActorObserverCaptor.getValue();

        ActorTask task = mock(ActorTask.class);
        when(task.getState()).thenReturn(ActorTaskState.FINISHED);
        when(mActorService.getCurrentActiveTask()).thenReturn(task);

        actorObserver.onTaskStateChanged(1, ActorTaskState.FINISHED);

        ActorTask newTask = mock(ActorTask.class);
        when(newTask.getState()).thenReturn(ActorTaskState.ACTING);
        when(mActorService.getCurrentActiveTask()).thenReturn(newTask);

        actorObserver.onTaskStateChanged(2, ActorTaskState.ACTING);

        ButtonData buttonData = controller.get(mTab);
        Assert.assertTrue(buttonData.getButtonSpec().getDrawable() instanceof LayerDrawable);
        Assert.assertEquals(0, buttonData.getButtonSpec().getActionChipLabelResId());
    }

    @Test
    public void testProfileSwitching() {
        GlicToolbarButtonController controller =
                new GlicToolbarButtonController(
                        mContext, () -> mTab, mToggleGlicCallback, () -> mTracker, () -> null);

        // Initial profile.
        controller.get(mTab);
        verify(mActorService).addObserver(any());

        // Switch profile.
        Profile newProfile = mock(Profile.class);
        Tab newTab = mock(Tab.class);
        when(newTab.getProfile()).thenReturn(newProfile);
        when(newTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        // Let's just verify it removes the old observer.
        controller.get(newTab);
        verify(mActorService).removeObserver(any());
        // Since sServiceForTesting is still the same, it will add it back (or to the same service).
        verify(mActorService, atLeastOnce()).addObserver(any());
    }

    @Test
    public void testDestroy() {
        GlicToolbarButtonController controller =
                new GlicToolbarButtonController(
                        mContext, () -> mTab, mToggleGlicCallback, () -> mTracker, () -> null);

        controller.get(mTab);
        controller.destroy();

        verify(mActorService).removeObserver(any());
    }

    @Test
    public void testIsPanelOpen_Initial() {
        ChromeAndroidTask task = mock(ChromeAndroidTask.class);
        when(task.getOrCreateNativeBrowserWindowPtr(mProfile)).thenReturn(123L);
        when(mGlicKeyedService.isPanelShowingForBrowser(123L)).thenReturn(true);

        GlicToolbarButtonController controller =
                new GlicToolbarButtonController(
                        mContext, () -> mTab, mToggleGlicCallback, () -> mTracker, () -> task);

        ButtonData buttonData = controller.get(mTab);

        Assert.assertTrue(buttonData.getButtonSpec().isChecked());
    }

    @Test
    public void testIsPanelOpen_GlobalShowHide() {
        ChromeAndroidTask task = mock(ChromeAndroidTask.class);
        when(task.getOrCreateNativeBrowserWindowPtr(mProfile)).thenReturn(123L);
        when(mGlicKeyedService.isPanelShowingForBrowser(123L)).thenReturn(true);

        GlicToolbarButtonController controller =
                new GlicToolbarButtonController(
                        mContext, () -> mTab, mToggleGlicCallback, () -> mTracker, () -> task);

        controller.get(mTab); // Initialize observations

        controller.onGlobalShowHide(true);

        ButtonData buttonData = controller.get(mTab);
        Assert.assertTrue(buttonData.getButtonSpec().isChecked());

        controller.onGlobalShowHide(false);
        buttonData = controller.get(mTab);
        Assert.assertFalse(buttonData.getButtonSpec().isChecked());
    }
}
