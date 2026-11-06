// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.selection;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.view.Menu;
import android.view.MenuItem;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.glic.GlicMetrics;
import org.chromium.chrome.browser.glic.GlicMetricsJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for {@link GlicSelectionMenuTracker}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlicSelectionMenuTrackerUnitTest {
    private static final String HISTOGRAM_NAME = "Glic.WebContentsContextMenu.TextSelection";
    private static final String PROVIDER_HISTOGRAM =
            "Glic.Consumer.WebContentsContextMenu.TextSelection";
    private static final int SHOWN_AND_IGNORED = 0;
    private static final int EXECUTED = 1;

    private final ChromeSelectionDropdownMenuDelegate mDropdownDelegate =
            new ChromeSelectionDropdownMenuDelegate();

    private GlicSelectionMenuTracker mTracker;

    @Before
    public void setUp() {
        mTracker = new GlicSelectionMenuTracker();
    }

    private Menu createMenuWithAskGemini(boolean visible, boolean enabled) {
        Menu menu = mock(Menu.class);
        MenuItem item = mock(MenuItem.class);
        when(item.isVisible()).thenReturn(visible);
        when(item.isEnabled()).thenReturn(enabled);
        when(menu.findItem(R.id.contextmenu_ask_gemini)).thenReturn(item);
        return menu;
    }

    private void closeFloatingMenu() {
        mTracker.onFloatingMenuDestroyed(() -> false);
        ShadowLooper.runUiThreadTasks();
    }

    private ListItem createDropdownAskGemini(boolean enabled) {
        return mDropdownDelegate.getMenuItem(
                "Ask Gemini",
                /* contentDescription= */ null,
                R.id.select_action_menu_delegate_items,
                R.id.contextmenu_ask_gemini,
                /* startIcon= */ null,
                /* isIconTintable= */ false,
                /* groupContainsIcon= */ false,
                enabled,
                /* intent= */ null,
                /* order= */ 0);
    }

    private ModelList createDropdownItems(ListItem askGemini) {
        ModelList items = new ModelList();
        items.add(mDropdownDelegate.getDivider());
        items.add(askGemini);
        return items;
    }

    @Test
    public void testFloatingShownAndDestroyed_recordsShownAndIgnored() {
        HistogramWatcher watcher = HistogramWatcher.newBuilder()
                                           .expectIntRecord(HISTOGRAM_NAME, SHOWN_AND_IGNORED)
                                           .expectIntRecord(PROVIDER_HISTOGRAM, SHOWN_AND_IGNORED)
                                           .build();
        mTracker.onFloatingMenuPrepared(
                createMenuWithAskGemini(/* visible= */ true, /* enabled= */ true));
        closeFloatingMenu();
        watcher.assertExpected();
    }

    @Test
    public void testFloatingShownAndClickedAndDestroyed_recordsExecuted() {
        HistogramWatcher watcher = HistogramWatcher.newBuilder()
                                           .expectIntRecord(HISTOGRAM_NAME, EXECUTED)
                                           .expectIntRecord(PROVIDER_HISTOGRAM, EXECUTED)
                                           .build();
        mTracker.onFloatingMenuPrepared(
                createMenuWithAskGemini(/* visible= */ true, /* enabled= */ true));
        mTracker.onFloatingItemClicked(R.id.contextmenu_ask_gemini);
        closeFloatingMenu();
        watcher.assertExpected();
    }

    @Test
    public void testFloatingShownAndOtherItemClickedAndDestroyed_recordsShownAndIgnored() {
        HistogramWatcher watcher = HistogramWatcher.newBuilder()
                                           .expectIntRecord(HISTOGRAM_NAME, SHOWN_AND_IGNORED)
                                           .expectIntRecord(PROVIDER_HISTOGRAM, SHOWN_AND_IGNORED)
                                           .build();
        mTracker.onFloatingMenuPrepared(
                createMenuWithAskGemini(/* visible= */ true, /* enabled= */ true));
        mTracker.onFloatingItemClicked(android.R.id.copy);
        closeFloatingMenu();
        watcher.assertExpected();
    }

    @Test
    public void testFloatingMenuWithoutItem_recordsNothing() {
        HistogramWatcher watcher = HistogramWatcher.newBuilder()
                                           .expectNoRecords(HISTOGRAM_NAME)
                                           .expectNoRecords(PROVIDER_HISTOGRAM)
                                           .build();
        mTracker.onFloatingMenuPrepared(mock(Menu.class));
        closeFloatingMenu();
        watcher.assertExpected();
    }

    @Test
    public void testFloatingDisabledItem_recordsNothing() {
        HistogramWatcher watcher = HistogramWatcher.newBuilder()
                                           .expectNoRecords(HISTOGRAM_NAME)
                                           .expectNoRecords(PROVIDER_HISTOGRAM)
                                           .build();
        mTracker.onFloatingMenuPrepared(
                createMenuWithAskGemini(/* visible= */ true, /* enabled= */ false));
        closeFloatingMenu();
        watcher.assertExpected();
    }

    @Test
    public void testFloatingHiddenItem_recordsNothing() {
        HistogramWatcher watcher = HistogramWatcher.newBuilder()
                                           .expectNoRecords(HISTOGRAM_NAME)
                                           .expectNoRecords(PROVIDER_HISTOGRAM)
                                           .build();
        mTracker.onFloatingMenuPrepared(
                createMenuWithAskGemini(/* visible= */ false, /* enabled= */ true));
        closeFloatingMenu();
        watcher.assertExpected();
    }

    @Test
    public void testFloatingRestartedInSameTask_keepsSession() {
        AtomicBoolean running = new AtomicBoolean();
        mTracker.onFloatingMenuPrepared(
                createMenuWithAskGemini(/* visible= */ true, /* enabled= */ true));

        HistogramWatcher noRecordsWatcher = HistogramWatcher.newBuilder()
                                                    .expectNoRecords(HISTOGRAM_NAME)
                                                    .expectNoRecords(PROVIDER_HISTOGRAM)
                                                    .build();
        mTracker.onFloatingMenuDestroyed(running::get);
        running.set(true); // Re-shown in the same task.
        ShadowLooper.runUiThreadTasks();
        noRecordsWatcher.assertExpected();

        HistogramWatcher watcher = HistogramWatcher.newBuilder()
                                           .expectIntRecord(HISTOGRAM_NAME, SHOWN_AND_IGNORED)
                                           .expectIntRecord(PROVIDER_HISTOGRAM, SHOWN_AND_IGNORED)
                                           .build();
        closeFloatingMenu();
        watcher.assertExpected();
    }

    @Test
    public void testFloatingPreparedAgainWithoutItem_recordsShownAndIgnored() {
        HistogramWatcher watcher = HistogramWatcher.newBuilder()
                                           .expectIntRecord(HISTOGRAM_NAME, SHOWN_AND_IGNORED)
                                           .expectIntRecord(PROVIDER_HISTOGRAM, SHOWN_AND_IGNORED)
                                           .build();
        mTracker.onFloatingMenuPrepared(
                createMenuWithAskGemini(/* visible= */ true, /* enabled= */ true));
        mTracker.onFloatingMenuPrepared(mock(Menu.class));
        closeFloatingMenu();
        watcher.assertExpected();
    }

    @Test
    public void testFloatingDestroyedTwice_recordsOnce() {
        HistogramWatcher watcher = HistogramWatcher.newBuilder()
                                           .expectIntRecord(HISTOGRAM_NAME, SHOWN_AND_IGNORED)
                                           .expectIntRecord(PROVIDER_HISTOGRAM, SHOWN_AND_IGNORED)
                                           .build();
        mTracker.onFloatingMenuPrepared(
                createMenuWithAskGemini(/* visible= */ true, /* enabled= */ true));
        closeFloatingMenu();
        closeFloatingMenu();
        watcher.assertExpected();
    }

    @Test
    public void testDropdownShownAndDismissed_recordsShownAndIgnored() {
        HistogramWatcher watcher = HistogramWatcher.newBuilder()
                                           .expectIntRecord(HISTOGRAM_NAME, SHOWN_AND_IGNORED)
                                           .expectIntRecord(PROVIDER_HISTOGRAM, SHOWN_AND_IGNORED)
                                           .build();
        ListItem askGemini = createDropdownAskGemini(/* enabled= */ true);
        mTracker.onDropdownMenuShown(createDropdownItems(askGemini));
        mTracker.onDropdownMenuDismissed();
        watcher.assertExpected();
    }

    @Test
    public void testDropdownDisabledItem_recordsNothing() {
        HistogramWatcher watcher = HistogramWatcher.newBuilder()
                                           .expectNoRecords(HISTOGRAM_NAME)
                                           .expectNoRecords(PROVIDER_HISTOGRAM)
                                           .build();
        ListItem askGemini = createDropdownAskGemini(/* enabled= */ false);
        mTracker.onDropdownMenuShown(createDropdownItems(askGemini));
        mTracker.onDropdownMenuDismissed();
        watcher.assertExpected();
    }

    @Test
    public void testDropdownSynchronousDismissOnClick_recordsExecuted() {
        HistogramWatcher watcher = HistogramWatcher.newBuilder()
                                           .expectIntRecord(HISTOGRAM_NAME, EXECUTED)
                                           .expectIntRecord(PROVIDER_HISTOGRAM, EXECUTED)
                                           .build();
        ListItem askGemini = createDropdownAskGemini(/* enabled= */ true);
        mTracker.onDropdownMenuShown(createDropdownItems(askGemini));
        mTracker.wrapDropdownClickListener(model -> mTracker.onDropdownMenuDismissed())
                .onItemClick(askGemini.model);
        watcher.assertExpected();
    }

    @Test
    public void testDropdownClickResetsBetweenSessions_recordsExecutedThenShownAndIgnored() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(HISTOGRAM_NAME, EXECUTED, SHOWN_AND_IGNORED)
                        .expectIntRecords(PROVIDER_HISTOGRAM, EXECUTED, SHOWN_AND_IGNORED)
                        .build();
        ListItem askGemini = createDropdownAskGemini(/* enabled= */ true);
        mTracker.onDropdownMenuShown(createDropdownItems(askGemini));
        mTracker.wrapDropdownClickListener(model -> {}).onItemClick(askGemini.model);
        mTracker.onDropdownMenuDismissed();

        ListItem secondAskGemini = createDropdownAskGemini(/* enabled= */ true);
        mTracker.onDropdownMenuShown(createDropdownItems(secondAskGemini));
        mTracker.onDropdownMenuDismissed();
        watcher.assertExpected();
    }

    @Test
    public void testEnterpriseProviderMetric_recordsEnterprise() {
        Profile profile = mock(Profile.class);
        GlicMetrics.Natives glicMetricsJni = mock(GlicMetrics.Natives.class);
        GlicMetricsJni.setInstanceForTesting(glicMetricsJni);
        when(glicMetricsJni.getMetricsInfix(profile)).thenReturn("Enterprise");

        GlicSelectionMenuTracker tracker = new GlicSelectionMenuTracker(() -> profile);
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(HISTOGRAM_NAME, SHOWN_AND_IGNORED)
                        .expectIntRecord(
                                "Glic.Enterprise.WebContentsContextMenu.TextSelection",
                                SHOWN_AND_IGNORED)
                        .expectNoRecords("Glic.Consumer.WebContentsContextMenu.TextSelection")
                        .build();
        tracker.onFloatingMenuPrepared(
                createMenuWithAskGemini(/* visible= */ true, /* enabled= */ true));
        tracker.onFloatingMenuDestroyed(() -> false);
        ShadowLooper.runUiThreadTasks();
        watcher.assertExpected();
    }
}
