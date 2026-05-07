// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.LayerDrawable;
import android.view.View;

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
import org.robolectric.Robolectric;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonDataProvider;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.url.JUnitTestGURLs;

import java.util.Collections;

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
    @Mock private BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    private BrowserStateBrowserControlsVisibilityDelegate mBrowserControlsVisibilityDelegate;
    @Mock private GlicToolbarButtonController.GlicButtonDelegate mToggleGlicCallback;
    @Mock private Tracker mTracker;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private ButtonDataProvider.ButtonDataObserver mObserver;
    @Mock private GlicKeyedServiceFactory.Natives mGlicKeyedServiceFactoryJniMock;
    @Mock private GlicKeyedService mGlicKeyedService;
    @Mock private GlicEnabling.Natives mGlicEnablingJniMock;
    @Mock private ChromeAndroidTask mTask;
    @Captor private ArgumentCaptor<ActorKeyedService.Observer> mActorObserverCaptor;

    private Activity mActivity;
    private Context mContext;
    private GlicToolbarButtonController mController;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mActivity = Robolectric.setupActivity(Activity.class);

        when(mTab.getProfile()).thenReturn(mProfile);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        ActorKeyedServiceFactory.setForTesting(mActorService);
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);
        GlicKeyedServiceFactoryJni.setInstanceForTesting(mGlicKeyedServiceFactoryJniMock);
        when(mGlicKeyedServiceFactoryJniMock.getForProfile(any())).thenReturn(mGlicKeyedService);
        mBrowserControlsVisibilityDelegate =
                new BrowserStateBrowserControlsVisibilityDelegate(
                        ObservableSuppliers.alwaysFalse());
        when(mBrowserControlsVisibilityManager.getBrowserVisibilityDelegate())
                .thenReturn(mBrowserControlsVisibilityDelegate);
        GlicEnablingJni.setInstanceForTesting(mGlicEnablingJniMock);
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        mController =
                new GlicToolbarButtonController(
                        mActivity,
                        () -> mTab,
                        mToggleGlicCallback,
                        () -> mTracker,
                        () -> mTask,
                        mBrowserControlsVisibilityManager,
                        () -> mTabModelSelector);
    }


    @Test
    public void testButtonData() {
        ButtonData buttonData = mController.get(mTab);

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
        ButtonData buttonData = mController.get(mTab);

        Assert.assertFalse(buttonData.canShow());
    }

    @Test
    public void testButtonData_OffTheRecord() {
        when(mTab.isOffTheRecord()).thenReturn(true);
        when(mTab.isIncognito()).thenReturn(true);
        ButtonData buttonData = mController.get(mTab);

        Assert.assertFalse(buttonData.canShow());
    }

    @Test
    public void testOnClick() {
        mController.onClick(null);

        verify(mToggleGlicCallback).onClick(false);
    }

    @Test
    public void testTaskState_Review() {
        mController.addObserver(mObserver);

        // Initial call to set up observation.
        mController.get(mTab);
        verify(mActorService).addObserver(mActorObserverCaptor.capture());
        ActorKeyedService.Observer actorObserver = mActorObserverCaptor.getValue();

        // Mock an active task that needs review.
        ActorTask task = mock(ActorTask.class);
        when(task.getState()).thenReturn(ActorTaskState.WAITING_ON_USER);
        when(mActorService.getCurrentActiveTask()).thenReturn(task);

        // Trigger state change.
        actorObserver.onTaskStateChanged(1, ActorTaskState.WAITING_ON_USER);

        ButtonData buttonData = mController.get(mTab);
        Assert.assertEquals(
                mContext.getString(R.string.glic_button_status_review),
                mContext.getString(buttonData.getButtonSpec().getActionChipLabelResId()));
        verify(mObserver).buttonDataChanged(true);
    }

    @Test
    public void testTaskState_Working() {
        mController.get(mTab);
        verify(mActorService).addObserver(mActorObserverCaptor.capture());
        ActorKeyedService.Observer actorObserver = mActorObserverCaptor.getValue();

        // Mock an active task that is working.
        ActorTask task = mock(ActorTask.class);
        when(task.getState()).thenReturn(ActorTaskState.ACTING);
        when(mActorService.getCurrentActiveTask()).thenReturn(task);

        // Trigger state change.
        actorObserver.onTaskStateChanged(1, ActorTaskState.ACTING);

        ButtonData buttonData = mController.get(mTab);
        Assert.assertTrue(buttonData.getButtonSpec().getDrawable() instanceof LayerDrawable);
        Assert.assertEquals(0, buttonData.getButtonSpec().getActionChipLabelResId());
    }

    @Test
    public void testTaskState_PausedByUser() {
        mController.get(mTab);
        verify(mActorService).addObserver(mActorObserverCaptor.capture());
        ActorKeyedService.Observer actorObserver = mActorObserverCaptor.getValue();

        ActorTask task = mock(ActorTask.class);
        when(task.getState()).thenReturn(ActorTaskState.PAUSED_BY_USER);
        when(mActorService.getCurrentActiveTask()).thenReturn(task);

        actorObserver.onTaskStateChanged(1, ActorTaskState.PAUSED_BY_USER);

        ButtonData buttonData = mController.get(mTab);
        Assert.assertFalse(buttonData.getButtonSpec().getDrawable() instanceof LayerDrawable);
        Assert.assertEquals(0, buttonData.getButtonSpec().getActionChipLabelResId());
    }

    @Test
    public void testTaskState_Done_Persists() {
        mController.addObserver(mObserver);

        mController.get(mTab);
        verify(mActorService).addObserver(mActorObserverCaptor.capture());
        ActorKeyedService.Observer actorObserver = mActorObserverCaptor.getValue();

        ActorTask task = mock(ActorTask.class);
        when(task.getState()).thenReturn(ActorTaskState.FINISHED);
        when(mActorService.getCurrentActiveTask()).thenReturn(task);

        actorObserver.onTaskStateChanged(1, ActorTaskState.FINISHED);

        ButtonData buttonData = mController.get(mTab);
        Assert.assertEquals(
                mContext.getString(R.string.glic_button_status_done),
                mContext.getString(buttonData.getButtonSpec().getActionChipLabelResId()));

        when(mActorService.getCurrentActiveTask()).thenReturn(null);

        buttonData = mController.get(mTab);
        Assert.assertEquals(
                mContext.getString(R.string.glic_button_status_done),
                mContext.getString(buttonData.getButtonSpec().getActionChipLabelResId()));
    }

    @Test
    public void testTaskState_Done_FromNotificationOnly() {
        mController.addObserver(mObserver);
        mController.get(mTab);
        verify(mActorService).addObserver(mActorObserverCaptor.capture());
        ActorKeyedService.Observer actorObserver = mActorObserverCaptor.getValue();

        // Mock that the service returns null (task is gone).
        when(mActorService.getCurrentActiveTask()).thenReturn(null);

        // Trigger state change to FINISHED.
        actorObserver.onTaskStateChanged(1, ActorTaskState.FINISHED);

        // Verify that the state becomes DONE.
        ButtonData buttonData = mController.get(mTab);
        Assert.assertEquals(
                mContext.getString(R.string.glic_button_status_done),
                mContext.getString(buttonData.getButtonSpec().getActionChipLabelResId()));
        verify(mObserver).buttonDataChanged(true);
    }

    @Test
    public void testTaskState_Done_ClearedOnClick() {
        mController.get(mTab);
        verify(mActorService).addObserver(mActorObserverCaptor.capture());
        ActorKeyedService.Observer actorObserver = mActorObserverCaptor.getValue();

        ActorTask task = mock(ActorTask.class);
        when(task.getState()).thenReturn(ActorTaskState.FINISHED);
        when(mActorService.getCurrentActiveTask()).thenReturn(task);

        actorObserver.onTaskStateChanged(1, ActorTaskState.FINISHED);

        when(mActorService.getCurrentActiveTask()).thenReturn(null);

        ButtonData buttonData = mController.get(mTab);
        Assert.assertEquals(
                mContext.getString(R.string.glic_button_status_done),
                mContext.getString(buttonData.getButtonSpec().getActionChipLabelResId()));

        mController.onClick(null);

        buttonData = mController.get(mTab);
        Assert.assertEquals(0, buttonData.getButtonSpec().getActionChipLabelResId());
    }

    @Test
    public void testTaskState_Done_ClearedOnNewTask() {
        mController.get(mTab);
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

        ButtonData buttonData = mController.get(mTab);
        Assert.assertTrue(buttonData.getButtonSpec().getDrawable() instanceof LayerDrawable);
        Assert.assertEquals(0, buttonData.getButtonSpec().getActionChipLabelResId());
    }

    @Test
    public void testProfileSwitching() {
        // Initial profile.
        mController.get(mTab);
        verify(mActorService).addObserver(any());

        // Switch profile.
        Profile newProfile = mock(Profile.class);
        Tab newTab = mock(Tab.class);
        when(newTab.getProfile()).thenReturn(newProfile);
        when(newTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        // Let's just verify it removes the old observer.
        mController.get(newTab);
        verify(mActorService).removeObserver(any());
        // Since sServiceForTesting is still the same, it will add it back (or to the same service).
        verify(mActorService, atLeastOnce()).addObserver(any());
    }

    @Test
    public void testDestroy() {
        mController.get(mTab);
        mController.destroy();

        verify(mActorService).removeObserver(any());
    }

    @Test
    public void testIsPanelOpen_Initial() {
        ChromeAndroidTask task = mock(ChromeAndroidTask.class);
        when(task.getNativeBrowserWindowPtr(mProfile, mActivity)).thenReturn(123L);
        when(mGlicKeyedService.isPanelShowingForBrowser(123L)).thenReturn(true);

        GlicToolbarButtonController controller =
                new GlicToolbarButtonController(
                        mActivity,
                        () -> mTab,
                        mToggleGlicCallback,
                        () -> mTracker,
                        () -> task,
                        mBrowserControlsVisibilityManager,
                        () -> mTabModelSelector);

        ButtonData buttonData = controller.get(mTab);

        Assert.assertTrue(buttonData.getButtonSpec().isChecked());
    }

    @Test
    public void testIsPanelOpen_GlobalShowHide() {
        ChromeAndroidTask task = mock(ChromeAndroidTask.class);
        when(task.getNativeBrowserWindowPtr(mProfile, mActivity)).thenReturn(123L);
        when(mGlicKeyedService.isPanelShowingForBrowser(123L)).thenReturn(true);

        GlicToolbarButtonController controller =
                new GlicToolbarButtonController(
                        mActivity,
                        () -> mTab,
                        mToggleGlicCallback,
                        () -> mTracker,
                        () -> task,
                        mBrowserControlsVisibilityManager,
                        () -> mTabModelSelector);

        controller.get(mTab); // Initialize observations

        controller.onGlobalShowHideForTesting();

        ButtonData buttonData = controller.get(mTab);
        Assert.assertTrue(buttonData.getButtonSpec().isChecked());

        when(mGlicKeyedService.isPanelShowingForBrowser(123L)).thenReturn(false);
        controller.onGlobalShowHideForTesting();
        buttonData = controller.get(mTab);
        Assert.assertFalse(buttonData.getButtonSpec().isChecked());
    }

    @Test
    public void testTaskState_Working_LocksToolbar() {
        mController.get(mTab);
        verify(mActorService).addObserver(mActorObserverCaptor.capture());
        ActorKeyedService.Observer actorObserver = mActorObserverCaptor.getValue();

        ActorTask task = mock(ActorTask.class);
        when(task.getState()).thenReturn(ActorTaskState.ACTING);
        when(mActorService.getCurrentActiveTask()).thenReturn(task);

        actorObserver.onTaskStateChanged(1, ActorTaskState.ACTING);

        Assert.assertEquals(
                BrowserControlsState.SHOWN, mBrowserControlsVisibilityDelegate.get().intValue());
    }

    @Test
    public void testTaskState_Done_UnlocksToolbar() {
        mController.get(mTab);
        verify(mActorService).addObserver(mActorObserverCaptor.capture());
        ActorKeyedService.Observer actorObserver = mActorObserverCaptor.getValue();

        ActorTask task = mock(ActorTask.class);
        when(task.getState()).thenReturn(ActorTaskState.ACTING);
        when(mActorService.getCurrentActiveTask()).thenReturn(task);

        actorObserver.onTaskStateChanged(1, ActorTaskState.ACTING);
        Assert.assertEquals(
                BrowserControlsState.SHOWN, mBrowserControlsVisibilityDelegate.get().intValue());

        // Now change to FINISHED.
        when(task.getState()).thenReturn(ActorTaskState.FINISHED);
        actorObserver.onTaskStateChanged(1, ActorTaskState.FINISHED);

        Assert.assertEquals(
                BrowserControlsState.BOTH, mBrowserControlsVisibilityDelegate.get().intValue());
    }

    @Test
    public void testOnClick_WithActiveTask_ShowsMenu() {
        mController.get(mTab);

        // Mock an active task.
        ActorTask task = mock(ActorTask.class);
        when(task.getTitle()).thenReturn("Test Task");
        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(task));
        when(mTab.getId()).thenReturn(1);
        when(mActorService.getActiveTaskIdOnTab(1)).thenReturn(null);

        // Set up show hook.
        Runnable showHook = mock(Runnable.class);
        AnchoredPopupWindow.setShowHookForTesting(showHook);

        // Click should show menu.
        View view = new View(mContext);
        mController.onClick(view);

        // Verify popup was shown.
        verify(showHook).run();
    }

    @Test
    public void testOnClick_WithActiveTaskOnActingTab_BypassesMenu() {
        mController.get(mTab);

        // Mock an active task.
        ActorTask task = mock(ActorTask.class);
        when(task.getTitle()).thenReturn("Test Task");
        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(task));

        // Set active tab as acting tab.
        when(mTab.getId()).thenReturn(1);
        when(mActorService.getActiveTaskIdOnTab(1)).thenReturn(123); // Some task ID.

        // Set up show hook.
        Runnable showHook = mock(Runnable.class);
        AnchoredPopupWindow.setShowHookForTesting(showHook);

        // Click should bypass menu.
        View view = new View(mContext);
        mController.onClick(view);

        // Verify popup was NOT shown.
        verify(showHook, never()).run();
        verify(mToggleGlicCallback).onClick(false);
    }
}
