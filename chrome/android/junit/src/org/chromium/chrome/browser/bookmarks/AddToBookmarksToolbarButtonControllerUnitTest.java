// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.os.Looper;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ActivityLifecycleDispatcherImpl;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider.ButtonDataObserver;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link AddToBookmarksToolbarButtonController} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
public class AddToBookmarksToolbarButtonControllerUnitTest {

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Tab mTab;
    @Mock private Tracker mTracker;
    @Mock private TabBookmarker mTabBookmarker;
    @Mock private BookmarkModel mBookmarkModel;
    @Mock private ActivityLifecycleDispatcherImpl mActivityLifecycleDispatcher;
    @Captor private ArgumentCaptor<ConfigurationChangedObserver> mConfigurationChangedObserver;

    private ObservableSupplierImpl<Tab> mTabSupplier;
    private ObservableSupplierImpl<BookmarkModel> mBookmarkModelSupplier;

    private UserActionTester mActionTester;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActionTester = new UserActionTester();

        mTabSupplier = new ObservableSupplierImpl<>();
        mTabSupplier.set(mTab);

        when(mBookmarkModel.isBookmarkModelLoaded()).thenReturn(true);
        mBookmarkModelSupplier = new ObservableSupplierImpl<>();
        mBookmarkModelSupplier.set(mBookmarkModel);
    }

    @After
    public void tearDown() throws Exception {
        mActionTester.tearDown();
    }

    /** Sets device qualifiers and notifies the activity about configuration change. */
    private void applyQualifiers(Activity activity, String qualifiers) {
        RuntimeEnvironment.setQualifiers(qualifiers);
        Configuration configuration = Resources.getSystem().getConfiguration();
        // ChromeTabbedActivity takes care of calling onConfigurationChanged on
        // ActivityLifecycleDispatcher, but this is a TestActivity so we do it manually.
        activity.onConfigurationChanged(configuration);
        mConfigurationChangedObserver.getValue().onConfigurationChanged(configuration);
    }

    private AddToBookmarksToolbarButtonController createController(Activity activity) {
        AddToBookmarksToolbarButtonController addToBookmarksToolbarButtonController =
                new AddToBookmarksToolbarButtonController(
                        mTabSupplier,
                        activity,
                        mActivityLifecycleDispatcher,
                        () -> mTabBookmarker,
                        () -> mTracker,
                        mBookmarkModelSupplier);
        verify(mActivityLifecycleDispatcher).register(mConfigurationChangedObserver.capture());
        return addToBookmarksToolbarButtonController;
    }

    @Test
    @Config(qualifiers = "w390dp-h820dp-land")
    public void testButtonData_shownOnPhone() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            when(mTab.getContext()).thenReturn(activity);
                            AddToBookmarksToolbarButtonController
                                    addToBookmarksToolbarButtonController =
                                            createController(activity);
                            ButtonData buttonData = addToBookmarksToolbarButtonController.get(mTab);

                            Assert.assertTrue(buttonData.canShow());
                            Assert.assertTrue(buttonData.isEnabled());
                            Assert.assertNotNull(buttonData.getButtonSpec());

                            applyQualifiers(activity, "+port");

                            Assert.assertTrue(
                                    addToBookmarksToolbarButtonController.get(mTab).canShow());
                        });
    }

    @Test
    @Config(qualifiers = "w600dp-h820dp")
    public void testButtonData_notShownOnTablet() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            when(mTab.getContext()).thenReturn(activity);
                            AddToBookmarksToolbarButtonController
                                    addToBookmarksToolbarButtonController =
                                            createController(activity);
                            ButtonData buttonData = addToBookmarksToolbarButtonController.get(mTab);

                            Assert.assertFalse(buttonData.canShow());
                            applyQualifiers(activity, "+land");

                            Assert.assertFalse(buttonData.canShow());
                        });
    }

    @Test
    @Config(qualifiers = "w600dp-h820dp")
    public void testButtonData_visibilityChangesOnScreenSizeUpdate() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            when(mTab.getContext()).thenReturn(activity);
                            AddToBookmarksToolbarButtonController
                                    addToBookmarksToolbarButtonController =
                                            createController(activity);
                            ButtonData buttonData = addToBookmarksToolbarButtonController.get(mTab);

                            Assert.assertFalse(buttonData.canShow());
                            applyQualifiers(activity, "w390dp-h820dp");

                            Assert.assertTrue(buttonData.canShow());
                        });
    }

    @Test
    public void testOnClick() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            when(mTab.getContext()).thenReturn(activity);
                            AddToBookmarksToolbarButtonController
                                    addToBookmarksToolbarButtonController =
                                            createController(activity);
                            addToBookmarksToolbarButtonController.onClick(null);

                            Assert.assertEquals(
                                    1,
                                    mActionTester.getActionCount(
                                            "MobileTopToolbarAddToBookmarksButton"));
                            verify(mTracker)
                                    .notifyEvent(
                                            EventConstants
                                                    .ADAPTIVE_TOOLBAR_CUSTOMIZATION_ADD_TO_BOOKMARKS_OPENED);
                            verify(mTabBookmarker).addOrEditBookmark(mTab);
                        });
    }

    @Test
    public void testIconUpdate_whenTabChanges() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            Tab anotherTab = mock(Tab.class);
                            when(mTab.getContext()).thenReturn(activity);
                            when(anotherTab.getContext()).thenReturn(activity);
                            // Set the current tab as not bookmarked, and a second tab as
                            // bookmarked.
                            when(mBookmarkModel.hasBookmarkIdForTab(mTab)).thenReturn(false);
                            when(mBookmarkModel.hasBookmarkIdForTab(anotherTab)).thenReturn(true);
                            AddToBookmarksToolbarButtonController
                                    addToBookmarksToolbarButtonController =
                                            createController(activity);

                            ButtonDataObserver mockButtonObserver = mock(ButtonDataObserver.class);
                            addToBookmarksToolbarButtonController.addObserver(mockButtonObserver);

                            ButtonData buttonData = addToBookmarksToolbarButtonController.get(mTab);

                            Drawable originalDrawable = buttonData.getButtonSpec().getDrawable();
                            String originalDescription =
                                    buttonData.getButtonSpec().getContentDescription();

                            // Change the active tab to the one already bookmarked.
                            mTabSupplier.set(anotherTab);

                            buttonData = addToBookmarksToolbarButtonController.get(anotherTab);

                            // The icon and description should be different.
                            Assert.assertNotEquals(
                                    originalDescription,
                                    buttonData.getButtonSpec().getContentDescription());
                            Assert.assertNotEquals(
                                    originalDrawable, buttonData.getButtonSpec().getDrawable());
                            // The provider should have notified of a change.
                            verify(mockButtonObserver).buttonDataChanged(true);
                        });
    }

    @Test
    public void testIconUpdate_whenUrlChanges() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            ArgumentCaptor<TabObserver> tabObserverCaptor =
                                    ArgumentCaptor.forClass(TabObserver.class);
                            when(mTab.getContext()).thenReturn(activity);
                            // Set the current tab as not bookmarked.
                            when(mBookmarkModel.hasBookmarkIdForTab(mTab)).thenReturn(false);
                            AddToBookmarksToolbarButtonController
                                    addToBookmarksToolbarButtonController =
                                            createController(activity);

                            // If an ObservableSupplier already has a value then its change callback
                            // will be called immediately as a separate task, idle the main looper
                            // to ensure all Observable suppliers raise their events.
                            Shadows.shadowOf(Looper.getMainLooper()).idle();

                            verify(mTab).addObserver(tabObserverCaptor.capture());

                            ButtonDataObserver mockButtonObserver = mock(ButtonDataObserver.class);
                            addToBookmarksToolbarButtonController.addObserver(mockButtonObserver);

                            ButtonData buttonData = addToBookmarksToolbarButtonController.get(mTab);

                            Drawable originalDrawable = buttonData.getButtonSpec().getDrawable();
                            String originalDescription =
                                    buttonData.getButtonSpec().getContentDescription();

                            // Set the current tab as bookmarked.
                            when(mBookmarkModel.hasBookmarkIdForTab(mTab)).thenReturn(true);
                            // Send an event notifying that the URL changed on the current tab.
                            tabObserverCaptor.getValue().onUrlUpdated(mTab);

                            buttonData = addToBookmarksToolbarButtonController.get(mTab);

                            // The icon and description should be different.
                            Assert.assertNotEquals(
                                    originalDescription,
                                    buttonData.getButtonSpec().getContentDescription());
                            Assert.assertNotEquals(
                                    originalDrawable, buttonData.getButtonSpec().getDrawable());
                            // The provider should have notified of a change.
                            verify(mockButtonObserver).buttonDataChanged(true);
                        });
    }

    @Test
    public void testIconUpdate_whenBookmarkModelChanges() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            ArgumentCaptor<BookmarkModelObserver> bookmarkModelObserverCaptor =
                                    ArgumentCaptor.forClass(BookmarkModelObserver.class);
                            when(mTab.getContext()).thenReturn(activity);
                            // Set the current tab as not bookmarked.
                            when(mBookmarkModel.hasBookmarkIdForTab(mTab)).thenReturn(false);
                            AddToBookmarksToolbarButtonController
                                    addToBookmarksToolbarButtonController =
                                            createController(activity);

                            // If an ObservableSupplier already has a value then its change callback
                            // will be called immediately as a separate task, idle the main looper
                            // to ensure all Observable suppliers raise their events.
                            Shadows.shadowOf(Looper.getMainLooper()).idle();

                            verify(mBookmarkModel)
                                    .addObserver(bookmarkModelObserverCaptor.capture());

                            ButtonDataObserver mockButtonObserver = mock(ButtonDataObserver.class);
                            addToBookmarksToolbarButtonController.addObserver(mockButtonObserver);

                            ButtonData buttonData = addToBookmarksToolbarButtonController.get(mTab);

                            Drawable originalDrawable = buttonData.getButtonSpec().getDrawable();
                            String originalDescription =
                                    buttonData.getButtonSpec().getContentDescription();

                            when(mBookmarkModel.hasBookmarkIdForTab(mTab)).thenReturn(true);
                            // Send and event notifying that something changed in the bookmark
                            // model.
                            bookmarkModelObserverCaptor.getValue().bookmarkModelChanged();

                            buttonData = addToBookmarksToolbarButtonController.get(mTab);

                            // The icon and description should be different.
                            Assert.assertNotEquals(
                                    originalDescription,
                                    buttonData.getButtonSpec().getContentDescription());
                            Assert.assertNotEquals(
                                    originalDrawable, buttonData.getButtonSpec().getDrawable());
                            // The provider should have notified of a change.
                            verify(mockButtonObserver).buttonDataChanged(true);
                        });
    }
}
