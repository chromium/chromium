// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import static android.content.res.Configuration.HARDKEYBOARDHIDDEN_UNDEFINED;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.keyboard_accessory.AccessoryAction.GENERATE_PASSWORD_AUTOMATIC;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.KEYBOARD_EXTENSION_STATE;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.EXTENDING_KEYBOARD;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.FLOATING_BAR;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.FLOATING_SHEET;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.HIDDEN;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.REPLACING_KEYBOARD;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.WAITING_TO_REPLACE;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.SHOW_WHEN_VISIBLE;
import static org.chromium.chrome.browser.tab.Tab.INVALID_TAB_ID;
import static org.chromium.chrome.browser.tabmodel.TabLaunchType.FROM_BROWSER_ACTIONS;
import static org.chromium.chrome.browser.tabmodel.TabSelectionType.FROM_NEW;
import static org.chromium.chrome.browser.tabmodel.TabSelectionType.FROM_USER;

import android.content.res.Configuration;
import android.graphics.drawable.Drawable;
import android.view.Surface;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.UserDataHost;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeKeyboardVisibilityDelegate;
import org.chromium.chrome.browser.ChromeWindow;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.UserInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.PropertyProvider;
import org.chromium.chrome.browser.keyboard_accessory.data.UserInfoField;
import org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.TabHidingType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Controller tests for the root controller for interactions with the manual filling UI.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
@EnableFeatures({ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY,
        ChromeFeatureList.AUTOFILL_MANUAL_FALLBACK_ANDROID})
public class ManualFillingControllerTest {
    @Mock
    private ChromeWindow mMockWindow;
    @Mock
    private ChromeActivity mMockActivity;
    private WebContents mLastMockWebContents;
    @Mock
    private ViewGroup mMockContentView;
    @Mock
    private TabModelSelector mMockTabModelSelector;
    @Mock
    private android.content.res.Resources mMockResources;
    @Mock
    private ChromeKeyboardVisibilityDelegate mMockKeyboard;
    @Mock
    private KeyboardAccessoryCoordinator mMockKeyboardAccessory;
    @Mock
    private AccessorySheetCoordinator mMockAccessorySheet;
    @Mock
    private CompositorViewHolder mMockCompositorViewHolder;

    @Rule
    public Features.JUnitProcessor mFeaturesProcessor = new Features.JUnitProcessor();

    private final ManualFillingCoordinator mController = new ManualFillingCoordinator();
    private final ManualFillingMediator mMediator = mController.getMediatorForTesting();
    private final ManualFillingStateCache mCache = mMediator.getStateCacheForTesting();
    private final PropertyModel mModel = mMediator.getModelForTesting();
    private final UserDataHost mUserDataHost = new UserDataHost();

    private static class MockActivityTabProvider extends ActivityTabProvider {
        public Tab mTab;

        public void set(Tab tab) {
            mTab = tab;
        }

        @Override
        public Tab get() {
            return mTab;
        }
    };

    private final MockActivityTabProvider mActivityTabProvider = new MockActivityTabProvider();

    /**
     * Helper class that provides shortcuts to providing and observing AccessorySheetData and
     * Actions.
     */
    private static class SheetProviderHelper {
        private final PropertyProvider<Action[]> mActionListProvider =
                new PropertyProvider<>(GENERATE_PASSWORD_AUTOMATIC);
        private final PropertyProvider<AccessorySheetData> mAccessorySheetDataProvider =
                new PropertyProvider<>();

        private final ArrayList<Action> mRecordedActions = new ArrayList<>();
        private int mRecordedActionNotifications;
        private final AtomicReference<AccessorySheetData> mRecordedSheetData =
                new AtomicReference<>();

        /**
         * Can be used to capture data from an observer. Retrieve the last captured data with
         * {@link #getRecordedActions()} and {@link #getFirstRecordedAction()}.
         * @param unusedTypeId Unused but necessary to enable use as method reference.
         * @param item The {@link Action[]} provided by a {@link PropertyProvider<Action[]>}.
         */
        void record(int unusedTypeId, Action[] item) {
            mRecordedActionNotifications++;
            mRecordedActions.clear();
            mRecordedActions.addAll(Arrays.asList(item));
        }

        /**
         * Can be used to capture data from an observer. Retrieve the last captured data with
         * {@link #getRecordedSheetData()} and {@link #getFirstRecordedPassword()}.
         * @param unusedTypeId Unused but necessary to enable use as method reference.
         * @param data The {@link AccessorySheetData} provided by a {@link PropertyProvider}.
         */
        void record(int unusedTypeId, AccessorySheetData data) {
            mRecordedSheetData.set(data);
        }

        /**
         * Uses the provider as returned by {@link #getActionListProvider()} to provide an Action.
         * @param actionCaption The caption for the provided generation action.
         */
        void provideAction(String actionCaption) {
            provideActions(new Action[] {
                    new Action(actionCaption, GENERATE_PASSWORD_AUTOMATIC, action -> {})});
        }

        /**
         * Uses the provider as returned by {@link #getActionListProvider()} to provide Actions.
         * @param actions The {@link Action}s to provide.
         */
        void provideActions(Action[] actions) {
            mActionListProvider.notifyObservers(actions);
        }

        /**
         * Uses the provider as returned by {@link #getSheetDataProvider()} to provide an simple
         * password sheet with one credential pair.
         * @param passwordString The only provided password in the new sheet.
         */
        void providePasswordSheet(String passwordString) {
            AccessorySheetData sheetData =
                    new AccessorySheetData(AccessoryTabType.PASSWORDS, "Passwords", "");
            UserInfo userInfo = new UserInfo("", null);
            userInfo.addField(
                    new UserInfoField("(No username)", "No username", /*id=*/"", false, null));
            userInfo.addField(new UserInfoField(passwordString, "Password", /*id=*/"", true, null));
            sheetData.getUserInfoList().add(userInfo);
            mAccessorySheetDataProvider.notifyObservers(sheetData);
        }

        /**
         * @return The {@link Action} last captured with {@link #record(int, Action[])}.
         */
        Action getFirstRecordedAction() {
            int firstNonTabLayoutAction = 1;
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)) {
                firstNonTabLayoutAction = 0;
            }
            assert mRecordedActions.size() >= firstNonTabLayoutAction;
            return mRecordedActions.get(firstNonTabLayoutAction);
        }

        /**
         * @return First password in a sheet captured by {@link #record(int, AccessorySheetData)}.
         */
        String getFirstRecordedPassword() {
            assert getRecordedSheetData() != null;
            assert getRecordedSheetData().getUserInfoList() != null;
            UserInfo info = getRecordedSheetData().getUserInfoList().get(0);
            assert info != null;
            assert info.getFields() != null;
            assert info.getFields().size() > 1;
            return info.getFields().get(1).getDisplayText();
        }

        /**
         * @return True if {@link #record(int, Action[])} was notified.
         */
        boolean hasRecordedActions() {
            return mRecordedActionNotifications > 0;
        }

        /**
         * @return The {@link Action}s last captured with {@link #record(int, Action[])}.
         */
        ArrayList<Action> getRecordedActions() {
            return mRecordedActions;
        }

        /**
         * @return {@link AccessorySheetData} captured by {@link #record(int, AccessorySheetData)}.
         */
        AccessorySheetData getRecordedSheetData() {
            return mRecordedSheetData.get();
        }

        /**
         * The returned provider is the same used by {@link #provideActions(Action[])}.
         * @return A {@link PropertyProvider}.
         */
        PropertyProvider<Action[]> getActionListProvider() {
            return mActionListProvider;
        }

        /**
         * The returned provider is the same used by {@link #providePasswordSheet(String)}.
         * @return A {@link PropertyProvider}.
         */
        PropertyProvider<AccessorySheetData> getSheetDataProvider() {
            return mAccessorySheetDataProvider;
        }
    }

    @Before
    public void setUp() {
        ShadowRecordHistogram.reset();
        MockitoAnnotations.initMocks(this);
        KeyboardVisibilityDelegate.setInstance(mMockKeyboard);
        when(mMockWindow.getKeyboardDelegate()).thenReturn(mMockKeyboard);
        when(mMockWindow.getActivity()).thenReturn(new WeakReference<>(mMockActivity));
        when(mMockKeyboard.calculateKeyboardHeight(any())).thenReturn(0);
        when(mMockActivity.getTabModelSelector()).thenReturn(mMockTabModelSelector);
        when(mMockActivity.getActivityTabProvider()).thenReturn(mActivityTabProvider);
        ChromeFullscreenManager fullscreenManager = new ChromeFullscreenManager(mMockActivity, 0) {
            @Override
            protected boolean isInVr() {
                return false;
            }
            @Override
            protected boolean bootsToVr() {
                return false;
            }
            @Override
            protected void rawTopContentOffsetChangedForVr(int topContentOffset) {}
        };
        when(mMockActivity.getFullscreenManager()).thenReturn(fullscreenManager);
        when(mMockActivity.getCompositorViewHolder()).thenReturn(mMockCompositorViewHolder);
        when(mMockActivity.getResources()).thenReturn(mMockResources);
        when(mMockActivity.getPackageManager())
                .thenReturn(RuntimeEnvironment.application.getPackageManager());
        when(mMockActivity.findViewById(android.R.id.content)).thenReturn(mMockContentView);
        when(mMockContentView.getRootView()).thenReturn(mock(View.class));
        mLastMockWebContents = mock(WebContents.class);
        when(mMockActivity.getCurrentWebContents()).then(i -> mLastMockWebContents);
        setContentAreaDimensions(2.f, 80, 300);
        Configuration config = new Configuration();
        config.hardKeyboardHidden = HARDKEYBOARDHIDDEN_UNDEFINED;
        when(mMockResources.getConfiguration()).thenReturn(config);
        AccessorySheetTabCoordinator.IconProvider.setIconForTesting(mock(Drawable.class));
        mController.initialize(mMockWindow, mMockKeyboardAccessory, mMockAccessorySheet);
    }

    @Test
    public void testCreatesValidSubComponents() {
        assertThat(mController, is(notNullValue()));
        assertThat(mMediator, is(notNullValue()));
        assertThat(mCache, is(notNullValue()));
    }

    @Test
    public void testAddingNewTabIsAddedToAccessoryAndSheet() {
        // Clear any calls that happened during initialization:
        reset(mMockKeyboardAccessory);
        reset(mMockAccessorySheet);

        // Create a new tab with a passwords tab:
        addBrowserTab(mMediator, 1111, null);

        // Registering a provider creates a new passwords tab:
        mController.registerSheetDataProvider(AccessoryTabType.PASSWORDS, new PropertyProvider<>());

        // Now check the how many tabs were sent to the sub components:
        ArgumentCaptor<KeyboardAccessoryData.Tab[]> barTabCaptor =
                ArgumentCaptor.forClass(KeyboardAccessoryData.Tab[].class);
        ArgumentCaptor<KeyboardAccessoryData.Tab[]> sheetTabCaptor =
                ArgumentCaptor.forClass(KeyboardAccessoryData.Tab[].class);
        verify(mMockKeyboardAccessory, times(2)).setTabs(barTabCaptor.capture());
        verify(mMockAccessorySheet, times(2)).setTabs(sheetTabCaptor.capture());

        // Initial empty state:
        assertThat(barTabCaptor.getAllValues().get(0).length, is(0));
        assertThat(sheetTabCaptor.getAllValues().get(0).length, is(0));

        // When creating the password sheet:
        assertThat(barTabCaptor.getAllValues().get(1).length, is(1));
        assertThat(sheetTabCaptor.getAllValues().get(1).length, is(1));
    }

    @Test
    public void testAddingBrowserTabsCreatesValidAccessoryState() {
        // Emulate adding a browser tab. Expect the model to have another entry.
        Tab firstTab = addBrowserTab(mMediator, 1111, null);
        ManualFillingState firstState = mCache.getStateFor(firstTab);
        assertThat(firstState, notNullValue());

        // Emulate adding a second browser tab. Expect the model to have another entry.
        Tab secondTab = addBrowserTab(mMediator, 2222, firstTab);
        ManualFillingState secondState = mCache.getStateFor(secondTab);
        assertThat(secondState, notNullValue());

        assertThat(firstState, not(equalTo(secondState)));
    }

    @Test
    public void testPasswordItemsPersistAfterSwitchingBrowserTabs() {
        SheetProviderHelper firstTabHelper = new SheetProviderHelper();
        SheetProviderHelper secondTabHelper = new SheetProviderHelper();

        // Simulate opening a new tab which automatically triggers the registration:
        Tab firstTab = addBrowserTab(mMediator, 1111, null);
        mController.registerSheetDataProvider(
                AccessoryTabType.PASSWORDS, firstTabHelper.getSheetDataProvider());
        getStateForBrowserTab()
                .getSheetDataProvider(AccessoryTabType.PASSWORDS)
                .addObserver(firstTabHelper::record);
        firstTabHelper.providePasswordSheet("FirstPassword");
        assertThat(firstTabHelper.getFirstRecordedPassword(), is("FirstPassword"));

        // Simulate creating a second tab:
        Tab secondTab = addBrowserTab(mMediator, 2222, firstTab);
        mController.registerSheetDataProvider(
                AccessoryTabType.PASSWORDS, secondTabHelper.getSheetDataProvider());
        getStateForBrowserTab()
                .getSheetDataProvider(AccessoryTabType.PASSWORDS)
                .addObserver(secondTabHelper::record);
        secondTabHelper.providePasswordSheet("SecondPassword");
        assertThat(secondTabHelper.getFirstRecordedPassword(), is("SecondPassword"));

        // Simulate switching back to the first tab:
        switchBrowserTab(mMediator, /*from=*/secondTab, /*to=*/firstTab);
        assertThat(firstTabHelper.getFirstRecordedPassword(), is("FirstPassword"));

        // And back to the second:
        switchBrowserTab(mMediator, /*from=*/firstTab, /*to=*/secondTab);
        assertThat(secondTabHelper.getFirstRecordedPassword(), is("SecondPassword"));
    }

    @Test
    public void testKeyboardAccessoryActionsPersistAfterSwitchingBrowserTabs() {
        SheetProviderHelper firstTabHelper = new SheetProviderHelper();
        SheetProviderHelper secondTabHelper = new SheetProviderHelper();

        // Simulate opening a new tab which automatically triggers the registration:
        Tab firstTab = addBrowserTab(mMediator, 1111, null);
        mController.registerActionProvider(firstTabHelper.getActionListProvider());
        getStateForBrowserTab().getActionsProvider().addObserver(firstTabHelper::record);
        firstTabHelper.provideAction("Generate Password");
        assertThat(firstTabHelper.getFirstRecordedAction().getCaption(), is("Generate Password"));

        // Simulate creating a second tab:
        Tab secondTab = addBrowserTab(mMediator, 2222, firstTab);
        mController.registerActionProvider(secondTabHelper.getActionListProvider());
        getStateForBrowserTab().getActionsProvider().addObserver(secondTabHelper::record);
        secondTabHelper.provideActions(new Action[0]);
        assertThat(secondTabHelper.getRecordedActions().size(), is(0));

        // Simulate switching back to the first tab:
        switchBrowserTab(mMediator, /*from=*/secondTab, /*to=*/firstTab);
        assertThat(firstTabHelper.getFirstRecordedAction().getCaption(), is("Generate Password"));

        // And back to the second:
        switchBrowserTab(mMediator, /*from=*/firstTab, /*to=*/secondTab);
        assertThat(secondTabHelper.getRecordedActions().size(), is(0));
    }

    @Test
    public void testPasswordTabRestoredWhenSwitchingBrowserTabs() {
        // Clear any calls that happened during initialization:
        reset(mMockKeyboardAccessory);
        reset(mMockAccessorySheet);

        // Create a new tab:
        Tab firstTab = addBrowserTab(mMediator, 1111, null);

        // Create a new passwords tab:
        mController.registerSheetDataProvider(AccessoryTabType.PASSWORDS, new PropertyProvider<>());

        // Simulate creating a second tab without any tabs:
        Tab secondTab = addBrowserTab(mMediator, 2222, firstTab);

        // Simulate switching back to the first tab:
        switchBrowserTab(mMediator, /*from=*/secondTab, /*to=*/firstTab);

        // And back to the second:
        switchBrowserTab(mMediator, /*from=*/firstTab, /*to=*/secondTab);

        ArgumentCaptor<KeyboardAccessoryData.Tab[]> barTabCaptor =
                ArgumentCaptor.forClass(KeyboardAccessoryData.Tab[].class);
        ArgumentCaptor<KeyboardAccessoryData.Tab[]> sheetTabCaptor =
                ArgumentCaptor.forClass(KeyboardAccessoryData.Tab[].class);
        verify(mMockKeyboardAccessory, times(5)).setTabs(barTabCaptor.capture());
        verify(mMockAccessorySheet, times(5)).setTabs(sheetTabCaptor.capture());

        // Initial empty state:
        assertThat(barTabCaptor.getAllValues().get(0).length, is(0));
        assertThat(sheetTabCaptor.getAllValues().get(0).length, is(0));

        // When creating the password sheet in 1st tab:
        assertThat(barTabCaptor.getAllValues().get(1).length, is(1));
        assertThat(sheetTabCaptor.getAllValues().get(1).length, is(1));

        // When switching to empty 2nd tab:
        assertThat(barTabCaptor.getAllValues().get(2).length, is(0));
        assertThat(sheetTabCaptor.getAllValues().get(2).length, is(0));

        // When switching back to 1st tab with password sheet:
        assertThat(barTabCaptor.getAllValues().get(3).length, is(1));
        assertThat(sheetTabCaptor.getAllValues().get(3).length, is(1));

        // When switching back to empty 2nd tab:
        assertThat(barTabCaptor.getAllValues().get(4).length, is(0));
        assertThat(sheetTabCaptor.getAllValues().get(4).length, is(0));
    }

    @Test
    public void testPasswordTabRestoredWhenClosingTabIsUndone() {
        // Clear any calls that happened during initialization:
        reset(mMockKeyboardAccessory);
        reset(mMockAccessorySheet);

        // Create a new tab with a passwords tab:
        Tab tab = addBrowserTab(mMediator, 1111, null);

        // Create a new passwords tab:
        mController.registerSheetDataProvider(AccessoryTabType.PASSWORDS, new PropertyProvider<>());

        // Simulate closing the tab (uncommitted):
        mMediator.getTabModelObserverForTesting().willCloseTab(tab, false);
        mMediator.getTabObserverForTesting().onHidden(tab, TabHidingType.CHANGED_TABS);
        getStateForBrowserTab().getWebContentsObserverForTesting().wasHidden();
        // The state should be kept if the closure wasn't committed.
        assertThat(getStateForBrowserTab().getAccessorySheet(AccessoryTabType.PASSWORDS),
                is(not(nullValue())));
        mLastMockWebContents = null;

        // Simulate undo closing the tab and selecting it:
        mMediator.getTabModelObserverForTesting().tabClosureUndone(tab);
        switchBrowserTab(mMediator, null, tab);

        // Simulate closing the tab and committing to it (i.e. wait out undo message):
        WebContents oldWebContents = mLastMockWebContents;
        closeBrowserTab(mMediator, tab);
        // The state should be cleaned up, now that it was committed.
        assertThat(mCache.getStateFor(oldWebContents).getAccessorySheet(AccessoryTabType.PASSWORDS),
                is(nullValue()));

        ArgumentCaptor<KeyboardAccessoryData.Tab[]> barTabCaptor =
                ArgumentCaptor.forClass(KeyboardAccessoryData.Tab[].class);
        ArgumentCaptor<KeyboardAccessoryData.Tab[]> sheetTabCaptor =
                ArgumentCaptor.forClass(KeyboardAccessoryData.Tab[].class);
        verify(mMockKeyboardAccessory, times(4)).setTabs(barTabCaptor.capture());
        verify(mMockAccessorySheet, times(4)).setTabs(sheetTabCaptor.capture());

        // Initial empty state:
        assertThat(barTabCaptor.getAllValues().get(0).length, is(0));
        assertThat(sheetTabCaptor.getAllValues().get(0).length, is(0));

        // When creating the password sheet:
        assertThat(barTabCaptor.getAllValues().get(1).length, is(1));
        assertThat(sheetTabCaptor.getAllValues().get(1).length, is(1));

        // When restoring the tab:
        assertThat(barTabCaptor.getAllValues().get(2).length, is(1));
        assertThat(sheetTabCaptor.getAllValues().get(2).length, is(1));

        // When committing to close the tab:
        assertThat(barTabCaptor.getAllValues().get(3).length, is(0));
        assertThat(sheetTabCaptor.getAllValues().get(3).length, is(0));
    }

    @Test
    public void testTreatNeverProvidedActionsAsEmptyActionList() {
        SheetProviderHelper firstTabHelper = new SheetProviderHelper();
        SheetProviderHelper secondTabHelper = new SheetProviderHelper();

        // Open a tab.
        Tab tab = addBrowserTab(mMediator, 1111, null);
        // Add an action provider that never provides any actions.
        mController.registerActionProvider(new PropertyProvider<>(GENERATE_PASSWORD_AUTOMATIC));
        getStateForBrowserTab().getActionsProvider().addObserver(firstTabHelper::record);

        // Create a new tab with an action:
        Tab secondTab = addBrowserTab(mMediator, 1111, tab);
        mController.registerActionProvider(secondTabHelper.getActionListProvider());
        getStateForBrowserTab().getActionsProvider().addObserver(secondTabHelper::record);
        secondTabHelper.provideAction("Test Action");
        assertThat(secondTabHelper.getFirstRecordedAction().getCaption(), is("Test Action"));

        // Switching back should notify the accessory about the still empty state of the accessory.
        switchBrowserTab(mMediator, secondTab, tab);
        assertThat(firstTabHelper.hasRecordedActions(), is(true));
        assertThat(firstTabHelper.getRecordedActions().size(), is(0));
    }

    @Test
    public void testUpdatesInactiveAccessory() {
        SheetProviderHelper delayedTabHelper = new SheetProviderHelper();
        SheetProviderHelper secondTabHelper = new SheetProviderHelper();

        // Open a tab.
        Tab delayedTab = addBrowserTab(mMediator, 1111, null);
        // Add an action provider that hasn't provided actions yet.
        mController.registerActionProvider(delayedTabHelper.getActionListProvider());
        getStateForBrowserTab().getActionsProvider().addObserver(delayedTabHelper::record);
        assertThat(delayedTabHelper.hasRecordedActions(), is(false));

        // Create and switch to a new tab:
        Tab secondTab = addBrowserTab(mMediator, 2222, delayedTab);
        mController.registerActionProvider(secondTabHelper.getActionListProvider());
        getStateForBrowserTab().getActionsProvider().addObserver(secondTabHelper::record);

        // And provide data to the active browser tab.
        secondTabHelper.provideAction("Test Action");
        // Now, have the delayed provider provide data for the backgrounded browser tab.
        delayedTabHelper.provideAction("Delayed");

        // The current tab should not be influenced by the delayed provider.
        assertThat(secondTabHelper.getRecordedActions().size(), is(1));
        assertThat(secondTabHelper.getFirstRecordedAction().getCaption(), is("Test Action"));

        // Switching tabs back should only show the action that was received in the background.
        switchBrowserTab(mMediator, secondTab, delayedTab);
        assertThat(delayedTabHelper.getRecordedActions().size(), is(1));
        assertThat(delayedTabHelper.getFirstRecordedAction().getCaption(), is("Delayed"));
    }

    @Test
    public void testDestroyingTabCleansModelForThisTab() {
        // Clear any calls that happened during initialization:
        reset(mMockKeyboardAccessory);
        reset(mMockAccessorySheet);
        SheetProviderHelper firstTabHelper = new SheetProviderHelper();
        SheetProviderHelper secondTabHelper = new SheetProviderHelper();

        // Simulate opening a new tab:
        Tab firstTab = addBrowserTab(mMediator, 1111, null);
        mController.registerSheetDataProvider(
                AccessoryTabType.PASSWORDS, firstTabHelper.getSheetDataProvider());
        mController.registerActionProvider(firstTabHelper.getActionListProvider());
        getStateForBrowserTab()
                .getSheetDataProvider(AccessoryTabType.PASSWORDS)
                .addObserver(firstTabHelper::record);
        getStateForBrowserTab().getActionsProvider().addObserver(firstTabHelper::record);
        firstTabHelper.providePasswordSheet("FirstPassword");
        firstTabHelper.provideAction("2BDestroyed");

        // Create and switch to a new tab: (because destruction shouldn't rely on tab to be active)
        Tab secondTab = addBrowserTab(mMediator, 2222, firstTab);
        mController.registerSheetDataProvider(
                AccessoryTabType.PASSWORDS, secondTabHelper.getSheetDataProvider());
        mController.registerActionProvider(secondTabHelper.getActionListProvider());
        getStateForBrowserTab()
                .getSheetDataProvider(AccessoryTabType.PASSWORDS)
                .addObserver(secondTabHelper::record);
        getStateForBrowserTab().getActionsProvider().addObserver(secondTabHelper::record);
        secondTabHelper.providePasswordSheet("SecondPassword");
        secondTabHelper.provideAction("2BKept");

        // The newly created tab should be valid.
        assertThat(secondTabHelper.getFirstRecordedPassword(), is("SecondPassword"));
        assertThat(secondTabHelper.getFirstRecordedAction().getCaption(), is("2BKept"));

        // Request destruction of the first Tab:
        mMediator.getTabObserverForTesting().onDestroyed(firstTab);

        // The current tab should not be influenced by the destruction...
        assertThat(secondTabHelper.getFirstRecordedPassword(), is("SecondPassword"));
        assertThat(secondTabHelper.getFirstRecordedAction().getCaption(), is("2BKept"));
        assertThat(getStateForBrowserTab(), is(mCache.getStateFor(secondTab)));
        // ... but the other tab's data should be gone.
        assertThat(mCache.getStateFor(firstTab).getActionsProvider(), nullValue());
        assertThat(mCache.getStateFor(firstTab).getAccessorySheet(AccessoryTabType.PASSWORDS),
                nullValue());
    }

    @Test
    public void testDisplaysAccessoryOnlyWhenSpaceIsSufficient() {
        reset(mMockKeyboardAccessory);

        addBrowserTab(mMediator, 1234, null);
        SheetProviderHelper tabHelper = new SheetProviderHelper();
        mController.registerSheetDataProvider(
                AccessoryTabType.PASSWORDS, tabHelper.getSheetDataProvider());
        when(mMockKeyboard.isSoftKeyboardShowing(any(), any())).thenReturn(true);
        when(mMockKeyboardAccessory.empty()).thenReturn(false);

        // Show the accessory bar for the default dimensions (300x128@2.f).
        mController.showWhenKeyboardIsVisible();
        verify(mMockKeyboardAccessory).show();

        // The accessory is shown and the content area plus bar size don't exceed the threshold.
        simulateLayoutSizeChange(3.f, 180, 128);

        verify(mMockKeyboardAccessory, never()).dismiss();
    }

    @Test
    public void testHidesAccessoryAfterRotation() {
        reset(mMockKeyboardAccessory);
        setContentAreaDimensions(2.f, 180, 320);
        addBrowserTab(mMediator, 1234, null);
        SheetProviderHelper tabHelper = new SheetProviderHelper();
        mController.registerSheetDataProvider(
                AccessoryTabType.PASSWORDS, tabHelper.getSheetDataProvider());
        when(mMockKeyboard.isSoftKeyboardShowing(any(), any())).thenReturn(true);
        when(mMockKeyboardAccessory.empty()).thenReturn(false);

        mController.showWhenKeyboardIsVisible();
        setContentAreaDimensions(2.f, 180, 220);
        mMediator.onLayoutChange(mMockContentView, 0, 0, 540, 360, 0, 0, 640, 360);
        verify(mMockKeyboardAccessory).show();
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), not(is(HIDDEN)));

        // Rotating the screen causes a relayout:
        setContentAreaDimensions(2.f, 320, 128, Surface.ROTATION_90);
        mMediator.onLayoutChange(mMockContentView, 0, 0, 160, 640, 0, 0, 540, 360);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(HIDDEN));
    }

    @Test
    public void testDisplaysAccessoryOnlyWhenVerticalSpaceIsSufficient() {
        reset(mMockKeyboardAccessory);
        addBrowserTab(mMediator, 1234, null);
        SheetProviderHelper tabHelper = new SheetProviderHelper();
        mController.registerSheetDataProvider(
                AccessoryTabType.PASSWORDS, tabHelper.getSheetDataProvider());
        when(mMockKeyboard.isSoftKeyboardShowing(eq(mMockActivity), any())).thenReturn(true);
        when(mMockKeyboardAccessory.empty()).thenReturn(false);

        // Show the accessory bar for the dimensions exactly at the threshold: 300x128@2.f.
        simulateLayoutSizeChange(2.0f, 300, 128);
        mController.showWhenKeyboardIsVisible();
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), not(is(HIDDEN)));
        verify(mMockKeyboardAccessory).show();

        // The height is now reduced by the 48dp high accessory -- it should remain visible.
        simulateLayoutSizeChange(2.0f, 300, 80);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), not(is(HIDDEN)));

        // Use a height that is too small but with a valid width (e.g. resized multi-window window).
        simulateLayoutSizeChange(2.0f, 300, 79);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(HIDDEN));
    }

    @Test
    public void testDisplaysAccessoryOnlyWhenHorizontalSpaceIsSufficient() {
        reset(mMockKeyboardAccessory);

        addBrowserTab(mMediator, 1234, null);
        SheetProviderHelper tabHelper = new SheetProviderHelper();
        mController.registerSheetDataProvider(
                AccessoryTabType.PASSWORDS, tabHelper.getSheetDataProvider());
        when(mMockKeyboard.isSoftKeyboardShowing(eq(mMockActivity), any())).thenReturn(true);
        when(mMockKeyboardAccessory.empty()).thenReturn(false);

        // Show the accessory bar for the dimensions exactly at the threshold: 180x128@2.f.
        simulateLayoutSizeChange(2.0f, 180, 128);
        mController.showWhenKeyboardIsVisible();
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), not(is(HIDDEN)));

        // Use a width that is too small but with a valid height (e.g. resized multi-window window).
        simulateLayoutSizeChange(2.0f, 179, 128);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(HIDDEN));
    }

    @Test
    public void testRestrictsSheetSizeIfVerticalSpaceChanges() {
        addBrowserTab(mMediator, 1234, null);
        // Resize the screen from 300x128@2.f to 300x200@2.f.
        setContentAreaDimensions(2.f, 200, 300);
        mMediator.onLayoutChange(mMockContentView, 0, 0, 400, 600, 0, 0, 256, 600);

        when(mMockKeyboardAccessory.empty()).thenReturn(false);
        when(mMockKeyboardAccessory.isShown()).thenReturn(true);
        when(mMockKeyboardAccessory.hasActiveTab()).thenReturn(true);
        when(mMockAccessorySheet.getHeight()).thenReturn(200); // Return height of a large keyboard.
        mModel.set(SHOW_WHEN_VISIBLE, true);
        mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_SHEET);
        mController.registerSheetDataProvider(AccessoryTabType.PASSWORDS, new PropertyProvider<>());
        reset(mMockKeyboardAccessory, mMockAccessorySheet, mMockKeyboard);

        when(mMockKeyboardAccessory.empty()).thenReturn(false);
        when(mMockKeyboardAccessory.isShown()).thenReturn(true);
        when(mMockKeyboardAccessory.hasActiveTab()).thenReturn(true);
        when(mMockAccessorySheet.isShown()).thenReturn(true);
        when(mMockAccessorySheet.getHeight()).thenReturn(200); // Return height of a large keyboard.

        // Set layout as if it was rotated: 200x300@2f minus the 100dp+48dp high filling ui.
        setContentAreaDimensions(2.f, 300, 52);
        mMediator.onLayoutChange(mMockContentView, 0, 0, 600, 104, 0, 0, 400, 600);
        verify(mMockAccessorySheet).setHeight(144); // == 2f * (200dp - 128dp)
    }

    @Test
    public void testClosingTabDoesntAffectUnitializedComponents() {
        // A leftover tab is closed before the filling component could pick up the active tab.
        closeBrowserTab(mMediator, mock(Tab.class));

        // Without any tab, there should be no state that would allow creating a sheet.
        assertThat(mMediator.getOrCreateSheet(AccessoryTabType.PASSWORDS), is(nullValue()));
    }

    @Test
    public void testIsFillingViewShownReturnsTargetValueAheadOfComponentUpdate() {
        // After initialization with one tab, the accessory sheet is closed.
        addBrowserTab(mMediator, 1234, null);
        mController.registerSheetDataProvider(AccessoryTabType.PASSWORDS, new PropertyProvider<>());
        when(mMockKeyboardAccessory.hasActiveTab()).thenReturn(false);
        assertThat(mController.isFillingViewShown(null), is(false));

        // As soon as active tab and keyboard change, |isFillingViewShown| returns the expected
        // state - even if the sheet component wasn't updated yet.
        when(mMockKeyboardAccessory.hasActiveTab()).thenReturn(true);
        when(mMockKeyboard.isSoftKeyboardShowing(eq(mMockActivity), any())).thenReturn(false);
        assertThat(mController.isFillingViewShown(null), is(true));

        // The layout change impacts the component, but not the coordinator method.
        mMediator.onLayoutChange(null, 0, 0, 0, 0, 0, 0, 0, 0);
        assertThat(mController.isFillingViewShown(null), is(true));
    }

    @Test
    public void testTransitionToHiddenHidesEverything() {
        addBrowserTab(mMediator, 1111, null);
        // Make sure the model is in a non-HIDDEN state first.
        mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_SHEET);
        reset(mMockKeyboard, mMockKeyboardAccessory, mMockAccessorySheet);

        // Set the model HIDDEN. This should update keyboard and subcomponents.
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(HIDDEN));

        verify(mMockAccessorySheet).hide();
        verify(mMockKeyboardAccessory).closeActiveTab();
        verify(mMockKeyboardAccessory).dismiss();
        verify(mMockCompositorViewHolder).requestLayout(); // Triggered as if it was a keyboard.
    }

    @Test
    public void testTransitionToExtendingShowsBarAndHidesSheet() {
        addBrowserTab(mMediator, 1111, null);
        mModel.set(SHOW_WHEN_VISIBLE, true);
        // Make sure the model is in a non-EXTENDING_KEYBOARD state first.
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
        reset(mMockKeyboard, mMockKeyboardAccessory, mMockAccessorySheet);

        // Set the model EXTENDING_KEYBOARD. This should update keyboard and subcomponents.
        mModel.set(KEYBOARD_EXTENSION_STATE, EXTENDING_KEYBOARD);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(EXTENDING_KEYBOARD));

        verify(mMockAccessorySheet).hide();
        verify(mMockKeyboardAccessory).closeActiveTab();
        verify(mMockKeyboardAccessory).show();
    }

    @Test
    public void testTransitionToFloatingBarShowsBarAndHidesSheet() {
        addBrowserTab(mMediator, 1111, null);
        mModel.set(SHOW_WHEN_VISIBLE, true);
        // Make sure the model is in a non-FLOATING_BAR state first.
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
        reset(mMockKeyboard, mMockKeyboardAccessory, mMockAccessorySheet);

        // Set the model FLOATING_BAR. This should update keyboard and subcomponents.
        mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_BAR);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(FLOATING_BAR));

        verify(mMockKeyboard).showKeyboard(any());
        verify(mMockAccessorySheet).hide();
        verify(mMockKeyboardAccessory).closeActiveTab();
        verify(mMockCompositorViewHolder).requestLayout(); // Triggered as if it was a keyboard.
        verify(mMockKeyboardAccessory).show();
    }

    @Test
    public void testTransitionToFloatingSheetShowsBarAndSheet() {
        addBrowserTab(mMediator, 1111, null);
        // Make sure the model is in a non-FLOATING_SHEET state first.
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
        reset(mMockKeyboard, mMockKeyboardAccessory, mMockAccessorySheet);

        // Set the model FLOATING_SHEET. This should update keyboard and subcomponents.
        mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_SHEET);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(FLOATING_SHEET));

        verify(mMockKeyboard).showKeyboard(any());
        verify(mMockAccessorySheet).show();
        verify(mMockKeyboardAccessory).show();
    }

    @Test
    public void testTransitionToReplacingShowsBarAndSheet() {
        addBrowserTab(mMediator, 1111, null);
        // Make sure the model is in a non-REPLACING_KEYBOARD state first.
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
        reset(mMockKeyboard, mMockKeyboardAccessory, mMockAccessorySheet);

        // Set the model REPLACING_KEYBOARD. This should update keyboard and subcomponents.
        mModel.set(KEYBOARD_EXTENSION_STATE, REPLACING_KEYBOARD);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(REPLACING_KEYBOARD));

        verify(mMockAccessorySheet).show();
        verify(mMockKeyboardAccessory).show();
    }

    @Test
    public void testTransitionToWaitingHidesKeyboardAndShowsBarAndSheet() {
        addBrowserTab(mMediator, 1111, null);
        // Make sure the model is in a non-REPLACING_KEYBOARD state first.
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
        reset(mMockKeyboard, mMockKeyboardAccessory, mMockAccessorySheet);

        // Set the model REPLACING_KEYBOARD. This should update keyboard and subcomponents.
        mModel.set(KEYBOARD_EXTENSION_STATE, WAITING_TO_REPLACE);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(WAITING_TO_REPLACE));

        verify(mMockKeyboard).hideSoftKeyboardOnly(any());
        verify(mMockAccessorySheet, never()).hide();
        verify(mMockKeyboardAccessory, never()).closeActiveTab();
        verify(mMockKeyboardAccessory).show();
    }

    @Test
    public void testTransitionFromHiddenToExtendingByKeyboard() {
        // Prepare a tab and register a new tab, so there is a reason to display the bar.
        addBrowserTab(mMediator, 1111, null);
        mController.registerSheetDataProvider(AccessoryTabType.PASSWORDS, new PropertyProvider<>());
        mModel.set(SHOW_WHEN_VISIBLE, true);
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
        reset(mMockKeyboard, mMockKeyboardAccessory, mMockAccessorySheet);
        when(mMockKeyboardAccessory.empty()).thenReturn(false);

        // Showing the keyboard should now trigger a transition into EXTENDING state.
        when(mMockKeyboard.isSoftKeyboardShowing(any(), any())).thenReturn(true);
        mMediator.onLayoutChange(mMockContentView, 0, 0, 320, 90, 0, 0, 320, 180);

        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(EXTENDING_KEYBOARD));
    }

    @Test
    public void testTransitionFromHiddenToExtendingByAvailableData() {
        // Prepare a tab and register a new tab, so there is a reason to display the bar.
        addBrowserTab(mMediator, 1111, null);
        mController.registerSheetDataProvider(AccessoryTabType.PASSWORDS, new PropertyProvider<>());
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
        reset(mMockKeyboard, mMockKeyboardAccessory, mMockAccessorySheet);
        when(mMockKeyboardAccessory.empty()).thenReturn(false);
        when(mMockKeyboard.isSoftKeyboardShowing(any(), any())).thenReturn(true);

        // Showing the keyboard should now trigger a transition into EXTENDING state.
        mController.showWhenKeyboardIsVisible();

        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(EXTENDING_KEYBOARD));
    }

    @Test
    public void testTransitionFromHiddenToFloatingBarByAvailableData() {
        // Prepare a tab and register a new tab, so there is a reason to display the bar.
        addBrowserTab(mMediator, 1111, null);
        mController.registerSheetDataProvider(AccessoryTabType.PASSWORDS, new PropertyProvider<>());
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
        reset(mMockKeyboard, mMockKeyboardAccessory, mMockAccessorySheet);
        when(mMockKeyboardAccessory.empty()).thenReturn(false);

        // Showing the keyboard should now trigger a transition into EXTENDING state.
        mController.showWhenKeyboardIsVisible();

        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(FLOATING_BAR));
    }

    @Test
    public void testTransitionFromFloatingBarToExtendingByKeyboard() {
        // Prepare a tab and register a new tab, so there is a reason to display the bar.
        addBrowserTab(mMediator, 1111, null);
        mController.registerSheetDataProvider(AccessoryTabType.PASSWORDS, new PropertyProvider<>());
        mModel.set(SHOW_WHEN_VISIBLE, true);
        mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_BAR);
        reset(mMockKeyboard, mMockKeyboardAccessory, mMockAccessorySheet);
        when(mMockKeyboardAccessory.empty()).thenReturn(false);
        when(mMockKeyboardAccessory.isShown()).thenReturn(true);

        // Simulate opening a keyboard:
        when(mMockKeyboard.isSoftKeyboardShowing(any(), any())).thenReturn(true);
        mMediator.onLayoutChange(mMockContentView, 0, 0, 320, 90, 0, 0, 320, 180);

        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(EXTENDING_KEYBOARD));
    }

    @Test
    public void testTransitionFromFloatingBarToFloatingSheetByActivatingTab() {
        // Prepare a tab and register a new tab, so there is a reason to display the bar.
        addBrowserTab(mMediator, 1111, null);
        mController.registerSheetDataProvider(AccessoryTabType.PASSWORDS, new PropertyProvider<>());
        mModel.set(SHOW_WHEN_VISIBLE, true);
        mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_BAR);
        reset(mMockKeyboard, mMockKeyboardAccessory, mMockAccessorySheet);
        when(mMockKeyboardAccessory.empty()).thenReturn(false);
        when(mMockKeyboardAccessory.isShown()).thenReturn(true);

        // Simulate selecting a bottom sheet:
        when(mMockKeyboardAccessory.hasActiveTab()).thenReturn(true);
        mMediator.onChangeAccessorySheet(0);

        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(FLOATING_SHEET));
    }

    @Test
    public void testTransitionFromFloatingSheetToFloatingBarByClosingSheet() {
        // Prepare a tab and register a new tab, so there is a reason to display the bar.
        addBrowserTab(mMediator, 1111, null);
        mController.registerSheetDataProvider(AccessoryTabType.PASSWORDS, new PropertyProvider<>());
        mModel.set(SHOW_WHEN_VISIBLE, true);
        mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_SHEET);
        reset(mMockKeyboard, mMockKeyboardAccessory, mMockAccessorySheet);
        when(mMockKeyboardAccessory.empty()).thenReturn(false);
        when(mMockKeyboardAccessory.isShown()).thenReturn(true);
        when(mMockKeyboardAccessory.hasActiveTab()).thenReturn(true);

        // Simulate closing the bottom sheet:
        when(mMockKeyboardAccessory.hasActiveTab()).thenReturn(false);
        mMediator.onCloseAccessorySheet();

        // This will cause a temporary floating sheet state which allows a nicer animation:
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(FLOATING_BAR));
    }

    @Test
    public void testTransitionFromExtendingToReplacingKeyboardByActivatingSheet() {
        // Prepare a tab and register a new tab, so there is a reason to display the bar.
        addBrowserTab(mMediator, 1111, null);
        mController.registerSheetDataProvider(AccessoryTabType.PASSWORDS, new PropertyProvider<>());
        mModel.set(SHOW_WHEN_VISIBLE, true);
        mModel.set(KEYBOARD_EXTENSION_STATE, EXTENDING_KEYBOARD);
        reset(mMockKeyboard, mMockKeyboardAccessory, mMockAccessorySheet);
        when(mMockKeyboardAccessory.empty()).thenReturn(false);
        when(mMockKeyboardAccessory.isShown()).thenReturn(true);
        when(mMockKeyboard.isSoftKeyboardShowing(any(), any())).thenReturn(true);

        // Simulate selecting a bottom sheet:
        when(mMockKeyboardAccessory.hasActiveTab()).thenReturn(true);
        mMediator.onChangeAccessorySheet(0);

        // Now the filling component waits for the keyboard to disappear before changing the stat:
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(WAITING_TO_REPLACE));
        // Layout changes but the keyboard is still there, so nothing happens:
        mMediator.onLayoutChange(mMockContentView, 0, 0, 320, 90, 0, 0, 320, 90);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(WAITING_TO_REPLACE));

        // The keyboard finally hides completely and the state changes to REPLACING.
        when(mMockKeyboard.isSoftKeyboardShowing(any(), any())).thenReturn(false);
        mMediator.onLayoutChange(mMockContentView, 0, 0, 320, 90, 0, 0, 320, 180);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(REPLACING_KEYBOARD));
    }

    @Test
    public void testTransitionFromReplacingKeyboardToExtendingByClosingSheet() {
        // Prepare a tab and register a new tab, so there is a reason to display the bar.
        addBrowserTab(mMediator, 1111, null);
        mController.registerSheetDataProvider(AccessoryTabType.PASSWORDS, new PropertyProvider<>());
        mModel.set(SHOW_WHEN_VISIBLE, true);
        mModel.set(KEYBOARD_EXTENSION_STATE, REPLACING_KEYBOARD);
        reset(mMockKeyboard, mMockKeyboardAccessory, mMockAccessorySheet);
        when(mMockKeyboardAccessory.empty()).thenReturn(false);
        when(mMockKeyboardAccessory.isShown()).thenReturn(true);
        when(mMockKeyboardAccessory.hasActiveTab()).thenReturn(true);

        // Simulate closing the bottom sheet:
        when(mMockKeyboardAccessory.hasActiveTab()).thenReturn(false);
        mMediator.onCloseAccessorySheet();

        // This will cause a temporary floating sheet state which allows a nicer animation:
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(FLOATING_SHEET));
        // This must trigger the keyboard to open, so the transition into EXTENDING can proceed.
        verify(mMockKeyboard).showKeyboard(any());

        // Simulate the keyboard opening:
        when(mMockKeyboard.isSoftKeyboardShowing(any(), any())).thenReturn(true);
        mMediator.onLayoutChange(mMockContentView, 0, 0, 320, 90, 0, 0, 320, 180);

        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(EXTENDING_KEYBOARD));
    }

    /**
     * Creates a tab and calls the observer events as if it was just created and switched to.
     * @param mediator The {@link ManualFillingMediator} whose observers should be triggered.
     * @param id The id of the new browser tab.
     * @param lastTab A previous mocked {@link Tab} to be hidden. Needs |getId()|. May be null.
     * @return Returns a mock of the newly added {@link Tab}. Provides |getId()|.
     */
    private Tab addBrowserTab(ManualFillingMediator mediator, int id, @Nullable Tab lastTab) {
        int lastId = INVALID_TAB_ID;
        if (lastTab != null) {
            lastId = lastTab.getId();
            mediator.getTabObserverForTesting().onHidden(lastTab, TabHidingType.CHANGED_TABS);
            mCache.getStateFor(mLastMockWebContents).getWebContentsObserverForTesting().wasHidden();
        }
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(id);
        when(tab.getUserDataHost()).thenReturn(mUserDataHost);
        mLastMockWebContents = mock(WebContents.class);
        when(tab.getWebContents()).thenReturn(mLastMockWebContents);
        mCache.getStateFor(tab).getWebContentsObserverForTesting().wasShown();
        when(tab.getContentView()).thenReturn(mMockContentView);
        when(mMockTabModelSelector.getCurrentTab()).thenReturn(tab);
        mActivityTabProvider.set(tab);
        mediator.getTabModelObserverForTesting().didAddTab(tab, FROM_BROWSER_ACTIONS);
        mediator.getTabObserverForTesting().onShown(tab, FROM_NEW);
        mediator.getTabModelObserverForTesting().didSelectTab(tab, FROM_NEW, lastId);
        setContentAreaDimensions(2.f, 300, 128);
        return tab;
    }

    /**
     * Simulates switching to a different tab by calling observer events on the given |mediator|.
     * @param mediator The mediator providing the observer instances.
     * @param from The mocked {@link Tab} to be switched from. Needs |getId()|. May be null.
     * @param to The mocked {@link Tab} to be switched to. Needs |getId()|.
     */
    private void switchBrowserTab(ManualFillingMediator mediator, @Nullable Tab from, Tab to) {
        int lastId = INVALID_TAB_ID;
        if (from != null) {
            lastId = from.getId();
            mediator.getTabObserverForTesting().onHidden(from, TabHidingType.CHANGED_TABS);
            mCache.getStateFor(mLastMockWebContents).getWebContentsObserverForTesting().wasHidden();
        }
        mLastMockWebContents = to.getWebContents();
        mCache.getStateFor(to).getWebContentsObserverForTesting().wasShown();
        when(mMockTabModelSelector.getCurrentTab()).thenReturn(to);
        mediator.getTabModelObserverForTesting().didSelectTab(to, FROM_USER, lastId);
        mediator.getTabObserverForTesting().onShown(to, FROM_USER);
    }

    /**
     * Simulates destroying the given tab by calling observer events on the given |mediator|.
     * @param mediator The mediator providing the observer instances.
     * @param tabToBeClosed The mocked {@link Tab} to be closed. Needs |getId()|.
     */
    private void closeBrowserTab(ManualFillingMediator mediator, Tab tabToBeClosed) {
        mediator.getTabModelObserverForTesting().willCloseTab(tabToBeClosed, false);
        mediator.getTabObserverForTesting().onHidden(tabToBeClosed, TabHidingType.CHANGED_TABS);
        mCache.getStateFor(mLastMockWebContents).getWebContentsObserverForTesting().wasHidden();
        mLastMockWebContents = null;
        mediator.getTabModelObserverForTesting().tabClosureCommitted(tabToBeClosed);
        mediator.getTabObserverForTesting().onDestroyed(tabToBeClosed);
    }

    private void setContentAreaDimensions(float density, int widthDp, int heightDp) {
        setContentAreaDimensions(density, widthDp, heightDp, Surface.ROTATION_0);
    }

    private void setContentAreaDimensions(float density, int widthDp, int heightDp, int rotation) {
        DisplayAndroid mockDisplay = mock(DisplayAndroid.class);
        when(mockDisplay.getDipScale()).thenReturn(density);
        when(mockDisplay.getRotation()).thenReturn(rotation);
        when(mMockWindow.getDisplay()).thenReturn(mockDisplay);
        when(mLastMockWebContents.getHeight()).thenReturn(heightDp);
        when(mLastMockWebContents.getWidth()).thenReturn(widthDp);
        // Return the correct keyboard_accessory_height for the current density:
        when(mMockResources.getDimensionPixelSize(anyInt())).thenReturn((int) (density * 48));
    }

    /**
     * This function initializes mocks and then calls the given mediator events in the order of a
     * layout resize event (e.g. when extending/shrinking a multi-window window).
     * It mains sets the {@link WebContents} size and calls |onLayoutChange| with the new bounds.
     * @param density The logical screen density (e.g. 1.f).
     * @param width The new {@link WebContents} width in dp.
     * @param height The new {@link WebContents} height in dp.
     */
    private void simulateLayoutSizeChange(float density, int width, int height) {
        int oldHeight = mLastMockWebContents.getHeight();
        int oldWidth = mLastMockWebContents.getWidth();
        int newHeight = (int) (density * height);
        int newWidth = (int) (density * width);
        setContentAreaDimensions(2.f, width, height);
        mMediator.onLayoutChange(
                mMockContentView, 0, 0, newWidth, newHeight, 0, 0, oldWidth, oldHeight);
    }

    /**
     * @return A {@link ManualFillingState} that is never null.
     */
    private ManualFillingState getStateForBrowserTab() {
        assert mLastMockWebContents != null : "In testing, WebContents should never be null!";
        return mCache.getStateFor(mLastMockWebContents);
    }
}
