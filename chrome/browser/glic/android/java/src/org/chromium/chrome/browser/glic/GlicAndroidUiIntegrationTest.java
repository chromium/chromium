// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.not;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;

import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.ActorOverlayState;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.HandoffButtonState;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.UiTabState;
import org.chromium.chrome.browser.actor.ui.ControlOwnership;
import org.chromium.chrome.browser.actor.ui.TabIndicatorStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarPrefs;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.Collections;
import java.util.Set;

/** UI Integration tests for Glic Android native components. */
@DoNotBatch(reason = "Overrides global mock factories and starts activity.")
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction({DeviceFormFactor.PHONE})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({
    ChromeFeatureList.GLIC,
    ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2,
    ChromeFeatureList.TAB_BOTTOM_SHEET
})
@DisableFeatures({
    ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL,
    ChromeFeatureList.ANDROID_BOTTOM_BAR
})
public class GlicAndroidUiIntegrationTest {
    private static final int TEST_TASK_ID = 1;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActorKeyedService mActorService;
    @Mock private ActorTask mActorTask;
    @Mock private GlicKeyedService mGlicService;
    @Captor private ArgumentCaptor<ActorKeyedService.Observer> mActorObserverCaptor;

    @Captor
    private ArgumentCaptor<GlicKeyedService.AllowedChangedObserver> mAllowedChangedObserverCaptor;

    private Tab mTab;

    @Before
    public void setUp() throws Exception {
        GlicEnabling.setEnabledForTesting(true, /* forwardToNative= */ false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AdaptiveToolbarPrefs.saveToolbarButtonManualOverride(
                            AdaptiveToolbarButtonVariant.GLIC);
                });
        ActorKeyedServiceFactory.setForTesting(mActorService);
        GlicKeyedServiceFactory.setForTesting(mGlicService);
        when(mActorService.getActiveTasks()).thenReturn(Collections.emptyList());

        mActivityTestRule.startMainActivityOnBlankPage();
        mTab = mActivityTestRule.getActivityTab();
        ChromeTabUtils.waitForTabPageLoaded(mTab, (String) null);

        CriteriaHelper.pollUiThread(
                () -> {
                    var rootUiCoordinator =
                            mActivityTestRule.getActivity().getRootUiCoordinatorForTesting();
                    Criteria.checkThat(rootUiCoordinator, Matchers.notNullValue());
                    var adaptiveToolbarUiCoordinator =
                            rootUiCoordinator.getAdaptiveToolbarUiCoordinatorForTesting();
                    Criteria.checkThat(adaptiveToolbarUiCoordinator, Matchers.notNullValue());
                    AdaptiveToolbarButtonController buttonController =
                            adaptiveToolbarUiCoordinator
                                    .getAdaptiveToolbarButtonControllerForTesting();
                    Criteria.checkThat(buttonController, Matchers.notNullValue());
                    buttonController.recomputeUiState();
                    Criteria.checkThat(
                            buttonController.getSingleProviderForTesting(),
                            Matchers.instanceOf(GlicToolbarButtonController.class));
                });
        String defaultDesc =
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.glic_button_entrypoint_ask_gemini_label);
        waitForButtonContentDescription(defaultDesc);
    }

    @After
    public void tearDown() {
        ActorKeyedServiceFactory.setForTesting(null);
        GlicKeyedServiceFactory.setForTesting(null);
        GlicEnabling.setEnabledForTesting(false, false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AdaptiveToolbarPrefs.saveToolbarButtonManualOverride(
                            AdaptiveToolbarButtonVariant.AUTO);
                });
    }

    private void waitForButtonContentDescription(String expectedDesc) {
        CriteriaHelper.pollUiThread(
                () -> {
                    View button =
                            mActivityTestRule
                                    .getActivity()
                                    .findViewById(R.id.optional_toolbar_button);
                    Criteria.checkThat(button, Matchers.notNullValue());
                    Criteria.checkThat(button.isShown(), Matchers.is(true));
                    CharSequence desc = button.getContentDescription();
                    Criteria.checkThat(desc, Matchers.notNullValue());
                    Criteria.checkThat(desc.toString(), Matchers.is(expectedDesc));
                });
    }

    @Test
    @LargeTest
    public void testButtonStateTransitions() {
        // Capture the registered observer on the mock service.
        verify(mActorService, Mockito.atLeastOnce()).addObserver(mActorObserverCaptor.capture());
        ActorKeyedService.Observer observerTemp = null;
        for (ActorKeyedService.Observer obs : mActorObserverCaptor.getAllValues()) {
            if (obs instanceof GlicButtonStateController) {
                observerTemp = obs;
                break;
            }
        }
        Assert.assertNotNull(
                "GlicButtonStateController should be registered as an observer", observerTemp);
        final ActorKeyedService.Observer observer = observerTemp;

        // 1. Verify default state content description.
        String defaultDesc =
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.glic_button_entrypoint_ask_gemini_label);
        waitForButtonContentDescription(defaultDesc);

        // 2. Transition to WORKING.
        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(mActorTask));
        when(mActorService.getCurrentActiveTask()).thenReturn(mActorTask);
        when(mActorTask.getState()).thenReturn(ActorTaskState.ACTING);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    observer.onTaskStateChanged(TEST_TASK_ID, ActorTaskState.ACTING);
                });

        String workingDesc =
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.glic_button_status_working_a11y_label);
        waitForButtonContentDescription(workingDesc);

        // 3. Transition to WAITING_ON_USER (Needs Review).
        when(mActorTask.getState()).thenReturn(ActorTaskState.WAITING_ON_USER);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    observer.onTaskStateChanged(TEST_TASK_ID, ActorTaskState.WAITING_ON_USER);
                });

        String reviewDesc =
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.glic_button_status_review_a11y_label);
        waitForButtonContentDescription(reviewDesc);

        // 4. Transition to FINISHED (Done).
        when(mActorService.getActiveTasks()).thenReturn(Collections.emptyList());
        when(mActorService.getCurrentActiveTask()).thenReturn(null);
        when(mActorTask.getState()).thenReturn(ActorTaskState.FINISHED);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    observer.onTaskStateChanged(TEST_TASK_ID, ActorTaskState.FINISHED);
                });

        String doneDesc =
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.glic_button_status_done_a11y_label);
        waitForButtonContentDescription(doneDesc);
    }

    @Test
    @LargeTest
    public void testButtonClickTogglesUi() {
        // Verify button is displayed.
        onView(withId(R.id.optional_toolbar_button)).check(matches(isDisplayed()));

        // Click the Glic button.
        onView(withId(R.id.optional_toolbar_button)).perform(click());

        // Verify that toggleUI was called on GlicKeyedService.
        verify(mGlicService, Mockito.timeout(5000))
                .toggleUI(Mockito.anyLong(), Mockito.eq(false), Mockito.any(), Mockito.anyInt());
    }

    @Test
    @LargeTest
    public void testButtonClickShowsTaskMenuWhenTaskOnOtherTab() {
        // Set up active task on a different tab.
        int otherTabId = mTab.getId() + 1;
        when(mActorTask.getId()).thenReturn(TEST_TASK_ID);
        when(mActorTask.getTitle()).thenReturn("Background filling task");
        when(mActorTask.getState()).thenReturn(ActorTaskState.ACTING);

        Set<Integer> tabSet = Collections.singleton(otherTabId);
        when(mActorTask.getTabs()).thenReturn(tabSet);
        when(mActorTask.getLastActedTabs()).thenReturn(tabSet);

        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(mActorTask));
        when(mActorService.getActiveTaskIdOnTab(mTab.getId())).thenReturn(null);
        when(mActorService.getActiveTaskIdOnTab(mTab.getId(), false)).thenReturn(null);

        // Click the Glic button.
        onView(withId(R.id.optional_toolbar_button)).perform(click());

        // Verify that the task menu popup is shown and displays the task title.
        String expectedTaskTitle = "Background filling task";
        onView(withText(expectedTaskTitle)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    public void testButtonVisibilityBounds() {
        // Glic should initially be displayed.
        onView(withId(R.id.optional_toolbar_button)).check(matches(isDisplayed()));

        // Capture GlicKeyedService AllowedChangedObserver registration.
        verify(mGlicService, Mockito.atLeastOnce())
                .addAllowedChangedObserver(mAllowedChangedObserverCaptor.capture());

        // Change Glic setting to false.
        GlicEnabling.setEnabledForTesting(false, /* forwardToNative= */ false);

        // Notify that the allowed state changed.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (GlicKeyedService.AllowedChangedObserver obs :
                            mAllowedChangedObserverCaptor.getAllValues()) {
                        obs.onAllowedStateChanged();
                    }
                });

        // Glic button should now be hidden/replaced (the optional button no longer shows Glic).
        String glicDesc =
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.glic_button_entrypoint_ask_gemini_label);
        onView(withId(R.id.optional_toolbar_button))
                .check(matches(not(withContentDescription(glicDesc))));
    }

    @Test
    @LargeTest
    public void testActorOverlayViewAndTakeOver() {
        // Initially, the overlay view should not be visible.
        onView(withId(R.id.actor_overlay)).check(matches(not(isDisplayed())));

        // 1. Simulate Actor UI Tab state update on the tab -> overlay active, border glow visible,
        // handoff button active.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ActorUiTabController tabController = ActorUiTabController.from(mTab);
                    tabController.onUiTabStateChange(
                            new UiTabState(
                                    mTab.getId(),
                                    new ActorOverlayState(
                                            /* isActive= */ true,
                                            /* borderGlowVisible= */ true,
                                            /* mouseDown= */ false),
                                    new HandoffButtonState(
                                            /* isActive= */ true, ControlOwnership.ACTOR),
                                    TabIndicatorStatus.STATIC,
                                    /* borderGlowVisible= */ true));
                });

        // 2. Verify overlay view is displayed.
        onView(withId(R.id.actor_overlay)).check(matches(isDisplayed()));

        // 3. Verify take over task button is displayed.
        onView(withId(R.id.take_over_task_button)).check(matches(isDisplayed()));

        // 4. Mock the task lookup for takeOverTask action.
        when(mActorService.getActiveTaskIdOnTab(mTab.getId())).thenReturn(TEST_TASK_ID);
        when(mActorService.getTask(TEST_TASK_ID)).thenReturn(mActorTask);

        // 5. Click the Take Over button.
        onView(withId(R.id.take_over_task_button)).perform(click());

        // 6. Verify that task.takeOverTask() was called.
        verify(mActorTask).takeOverTask();
    }
}
