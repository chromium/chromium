// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import static android.content.res.Configuration.HARDKEYBOARDHIDDEN_UNDEFINED;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doNothing;
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
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.SHOULD_EXTEND_KEYBOARD;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.SHOW_WHEN_VISIBLE;
import static org.chromium.chrome.browser.tab.Tab.INVALID_TAB_ID;
import static org.chromium.chrome.browser.tab.TabLaunchType.FROM_BROWSER_ACTIONS;
import static org.chromium.chrome.browser.tab.TabSelectionType.FROM_NEW;
import static org.chromium.chrome.browser.tab.TabSelectionType.FROM_USER;

import android.content.res.Configuration;
import android.graphics.drawable.Drawable;
import android.view.Surface;
import android.view.View;

import androidx.annotation.Nullable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.CallbackUtils;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeWindow;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent.UpdateAccessorySheetDelegate;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.UserInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.PropertyProvider;
import org.chromium.chrome.browser.keyboard_accessory.data.UserInfoField;
import org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabCoordinator;
import org.chromium.chrome.browser.password_manager.ConfirmationDialogHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.mojom.VirtualKeyboardMode;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.atomic.AtomicReference;

/** Controller tests for the root controller for interactions with the manual filling UI. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ManualFillingControllerTest {
    private static final int sKeyboardHeightDp = 100;
    private static final int sAccessoryHeightDp = 48;

    @Mock private ChromeWindow mMockWindow;
    @Mock private ChromeActivity mMockActivity;
    private WebContents mLastMockWebContents;
    @Mock private Profile mMockProfile;
    @Mock private Profile.Natives mProfileJniMock;
    @Mock private ContentView mMockContentView;
    @Mock private TabModelSelector mMockTabModelSelector;
    @Mock private android.content.res.Resources mMockResources;
    @Mock private KeyboardAccessoryCoordinator mMockKeyboardAccessory;
    @Mock private AccessorySheetCoordinator mMockAccessorySheet;
    @Mock private CompositorViewHolder mMockCompositorViewHolder;
    @Mock private BottomSheetController mMockBottomSheetController;
    @Mock private ManualFillingComponent.SoftKeyboardDelegate mMockSoftKeyboardDelegate;
    @Mock private ConfirmationDialogHelper mMockConfirmationHelper;
    @Mock private FullscreenManager mMockFullscreenManager;
    @Mock private InsetObserver mInsetObserver;
    @Mock private BackPressManager mMockBackPressManager;
    @Mock private EdgeToEdgeController mMockEdgeToEdgeController;

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Captor ArgumentCaptor<FullscreenManager.Observer> mFullscreenObserverCaptor;

    private final ManualFillingCoordinator mController = new ManualFillingCoordinator();
    private final ManualFillingMediator mMediator = mController.getMediatorForTesting();
    private final ManualFillingStateCache mCache = mMediator.getStateCacheForTesting();
    private final PropertyModel mModel = mMediator.getModelForTesting();
    private final UserDataHost mUserDataHost = new UserDataHost();
    private final ApplicationViewportInsetSupplier mInsetSupplier =
            ApplicationViewportInsetSupplier.createForTests();
    private final ObservableSupplierImpl<Integer> mKeyboardInsetSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<EdgeToEdgeController> mMockEdgeToEdgeControllerSupplier =
            new ObservableSupplierImpl<EdgeToEdgeController>();

    private static class MockActivityTabProvider extends ActivityTabProvider {
        public Tab mTab;

        @Override
        public void set(Tab tab) {
            mTab = tab;
        }

        @Override
        public Tab get() {
            return mTab;
        }
    }

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
         * Can be used to capture data from an observer. Retrieve the last captured data with {@link
         * #getRecordedActions()} and {@link #getFirstRecordedAction()}.
         *
         * @param unusedTypeId Unused but necessary to enable use as method reference.
         * @param item The {@link Action[]} provided by a {@link PropertyProvider<Action[]>}.
         */
        void record(int unusedTypeId, Action[] item) {
            mRecordedActionNotifications++;
            mRecordedActions.clear();
            mRecordedActions.addAll(Arrays.asList(item));
        }

        /**
         * Can be used to capture data from an observer. Retrieve the last captured data with {@link
         * #getRecordedSheetData()} and {@link #getFirstRecordedPassword()}.
         *
         * @param unusedTypeId Unused but necessary to enable use as method reference.
         * @param data The {@link AccessorySheetData} provided by a {@link PropertyProvider}.
         */
        void record(int unusedTypeId, AccessorySheetData data) {
            mRecordedSheetData.set(data);
        }

        /**
         * Uses the provider as returned by {@link #getActionListProvider()} to provide an Action.
         *
         * @param actionType The type for the provided generation action.
         */
        void provideAction(@AccessoryAction int actionType) {
            provideActions(new Action[] {new Action(actionType, action -> {})});
        }

        /**
         * Uses the provider as returned by {@link #getActionListProvider()} to provide Actions.
         *
         * @param actions The {@link Action}s to provide.
         */
        void provideActions(Action[] actions) {
            mActionListProvider.notifyObservers(actions);
        }

        /**
         * Uses the provider as returned by {@link #getSheetDataProvider()} to provide an simple
         * password sheet with one credential pair.
         *
         * @param passwordString The only provided password in the new sheet.
         */
        void providePasswordSheet(String passwordString) {
            AccessorySheetData sheetData =
                    new AccessorySheetData(
                            AccessoryTabType.PASSWORDS,
                            /* userInfoTitle= */ "Passwords",
                            /* plusAddressTitle= */ "",
                            /* warning= */ "");
            UserInfo userInfo = new UserInfo("", false);
            userInfo.addField(
                    new UserInfoField("(No username)", "No username", /* id= */ "", false, null));
            userInfo.addField(
                    new UserInfoField(passwordString, "Password", /* id= */ "", true, null));
            sheetData.getUserInfoList().add(userInfo);
            mAccessorySheetDataProvider.notifyObservers(sheetData);
        }

        /**
         * @return The {@link Action} last captured with {@link #record(int, Action[])}.
         */
        Action getFirstRecordedAction() {
            int firstNonTabLayoutAction = 0;
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
         *
         * @return A {@link PropertyProvider}.
         */
        PropertyProvider<Action[]> getActionListProvider() {
            return mActionListProvider;
        }

        /**
         * The returned provider is the same used by {@link #providePasswordSheet(String)}.
         *
         * @return A {@link PropertyProvider}.
         */
        PropertyProvider<AccessorySheetData> getSheetDataProvider() {
            return mAccessorySheetDataProvider;
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mMockWindow.getActivity()).thenReturn(new WeakReference<>(mMockActivity));
        mInsetSupplier.setKeyboardInsetSupplier(mKeyboardInsetSupplier);
        mInsetSupplier.setKeyboardAccessoryInsetSupplier(mController.getBottomInsetSupplier());
        when(mMockWindow.getApplicationBottomInsetSupplier()).thenReturn(mInsetSupplier);
        when(mMockSoftKeyboardDelegate.calculateSoftKeyboardHeight(any())).thenReturn(0);
        when(mMockActivity.getTabModelSelector()).thenReturn(mMockTabModelSelector);
        when(mMockActivity.getActivityTabProvider()).thenReturn(mActivityTabProvider);
        BrowserControlsManager browserControlsManager =
                new BrowserControlsManager(mMockActivity, 0);
        when(mMockActivity.getBrowserControlsManager()).thenReturn(browserControlsManager);
        when(mMockActivity.getFullscreenManager()).thenReturn(mMockFullscreenManager);
        doNothing().when(mMockFullscreenManager).addObserver(mFullscreenObserverCaptor.capture());
        ObservableSupplierImpl<CompositorViewHolder> compositorViewHolderSupplier =
                new ObservableSupplierImpl<>();
        compositorViewHolderSupplier.set(mMockCompositorViewHolder);
        when(mMockActivity.getCompositorViewHolderSupplier())
                .thenReturn(compositorViewHolderSupplier);
        when(mMockActivity.getResources()).thenReturn(mMockResources);
        when(mMockActivity.getPackageManager())
                .thenReturn(RuntimeEnvironment.application.getPackageManager());
        when(mMockActivity.findViewById(android.R.id.content)).thenReturn(mMockContentView);
        when(mMockContentView.getRootView()).thenReturn(mock(View.class));
        mLastMockWebContents = mock(WebContents.class);
        when(mMockActivity.getCurrentWebContents()).then(i -> mLastMockWebContents);

        mJniMocker.mock(ProfileJni.TEST_HOOKS, mProfileJniMock);
        when(mProfileJniMock.fromWebContents(any())).thenReturn(mMockProfile);

        when(mMockWindow.getInsetObserver()).thenReturn(mInsetObserver);
        simulateLayoutSizeChange(
                2.f, 80, 128, /* keyboardShown= */ false, VirtualKeyboardMode.RESIZES_VISUAL);
        Configuration config = new Configuration();
        config.hardKeyboardHidden = HARDKEYBOARDHIDDEN_UNDEFINED;
        when(mMockResources.getConfiguration()).thenReturn(config);
        AccessorySheetTabCoordinator.IconProvider.setIconForTesting(mock(Drawable.class));
        doNothing()
                .when(mMockBackPressManager)
                .addHandler(any(), eq(BackPressHandler.Type.MANUAL_FILLING));
        when(mMockEdgeToEdgeController.getBottomInset()).thenReturn(0);
        mMockEdgeToEdgeControllerSupplier.set(mMockEdgeToEdgeController);
        mController.initialize(
                mMockWindow,
                mMockKeyboardAccessory,
                mMockAccessorySheet,
                mMockBottomSheetController,
                /* isContextualSearchOpened= */ () -> false,
                mMockBackPressManager,
                mMockEdgeToEdgeControllerSupplier,
                mMockSoftKeyboardDelegate,
                mMockConfirmationHelper);
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
        mController.registerSheetDataProvider(
                mLastMockWebContents, AccessoryTabType.PASSWORDS, new PropertyProvider<>());

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
        UpdateAccessorySheetDelegate firstSheetUpdater = mock(UpdateAccessorySheetDelegate.class);
        UpdateAccessorySheetDelegate secondSheetUpdater = mock(UpdateAccessorySheetDelegate.class);

        // Simulate opening a new tab which automatically triggers the registration:
        Tab firstTab = addBrowserTab(mMediator, 1111, null);
        mController.registerSheetDataProvider(
                mLastMockWebContents,
                AccessoryTabType.PASSWORDS,
                firstTabHelper.getSheetDataProvider());
        mController.registerSheetUpdateDelegate(mLastMockWebContents, firstSheetUpdater);
        getStateForBrowserTab()
                .getSheetDataProvider(AccessoryTabType.PASSWORDS)
                .addObserver(firstTabHelper::record);
        firstTabHelper.providePasswordSheet("FirstPassword");
        assertThat(firstTabHelper.getFirstRecordedPassword(), is("FirstPassword"));

        // Simulate creating a second tab:
        Tab secondTab = addBrowserTab(mMediator, 2222, firstTab);
        mController.registerSheetDataProvider(
                mLastMockWebContents,
                AccessoryTabType.PASSWORDS,
                secondTabHelper.getSheetDataProvider());
        mController.registerSheetUpdateDelegate(mLastMockWebContents, secondSheetUpdater);
        getStateForBrowserTab()
                .getSheetDataProvider(AccessoryTabType.PASSWORDS)
                .addObserver(secondTabHelper::record);
        secondTabHelper.providePasswordSheet("SecondPassword");
        assertThat(secondTabHelper.getFirstRecordedPassword(), is("SecondPassword"));

        // Simulate switching back to the first tab:
        switchBrowserTab(mMediator, /* from= */ secondTab, /* to= */ firstTab);
        // Wiring affects the same sheet only and is triggered after switching
        verify(firstSheetUpdater).requestSheet(AccessoryTabType.PASSWORDS);
        firstTabHelper.providePasswordSheet("FirstPassword");
        assertThat(firstTabHelper.getFirstRecordedPassword(), is("FirstPassword"));

        // And back to the second:
        switchBrowserTab(mMediator, /* from= */ firstTab, /* to= */ secondTab);
        // Wiring affects the same sheet only and is triggered after switching
        verify(secondSheetUpdater).requestSheet(AccessoryTabType.PASSWORDS);
        secondTabHelper.providePasswordSheet("SecondPassword");
        assertThat(secondTabHelper.getFirstRecordedPassword(), is("SecondPassword"));
    }

    @Test
    public void testKeyboardAccessoryActionsPersistAfterSwitchingBrowserTabs() {
        SheetProviderHelper firstTabHelper = new SheetProviderHelper();
        SheetProviderHelper secondTabHelper = new SheetProviderHelper();

        // Simulate opening a new tab which automatically triggers the registration:
        Tab firstTab = addBrowserTab(mMediator, 1111, null);
        mController.registerActionProvider(
                mLastMockWebContents, firstTabHelper.getActionListProvider());
        getStateForBrowserTab().getActionsProvider().addObserver(firstTabHelper::record);
        firstTabHelper.provideAction(AccessoryAction.GENERATE_PASSWORD_AUTOMATIC);
        assertThat(
                firstTabHelper.getFirstRecordedAction().getActionType(),
                is(AccessoryAction.GENERATE_PASSWORD_AUTOMATIC));

        // Simulate creating a second tab:
        Tab secondTab = addBrowserTab(mMediator, 2222, firstTab);
        mController.registerActionProvider(
                mLastMockWebContents, secondTabHelper.getActionListProvider());
        getStateForBrowserTab().getActionsProvider().addObserver(secondTabHelper::record);
        secondTabHelper.provideActions(new Action[0]);
        assertThat(secondTabHelper.getRecordedActions().size(), is(0));

        // Simulate switching back to the first tab:
        switchBrowserTab(mMediator, /* from= */ secondTab, /* to= */ firstTab);
        assertThat(
                firstTabHelper.getFirstRecordedAction().getActionType(),
                is(AccessoryAction.GENERATE_PASSWORD_AUTOMATIC));

        // And back to the second:
        switchBrowserTab(mMediator, /* from= */ firstTab, /* to= */ secondTab);
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
        mController.registerSheetDataProvider(
                mLastMockWebContents, AccessoryTabType.PASSWORDS, new PropertyProvider<>());

        // Simulate creating a second tab without any tabs:
        Tab secondTab = addBrowserTab(mMediator, 2222, firstTab);

        // Simulate switching back to the first tab:
        switchBrowserTab(mMediator, /* from= */ secondTab, /* to= */ firstTab);

        // And back to the second:
        switchBrowserTab(mMediator, /* from= */ firstTab, /* to= */ secondTab);

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
        mController.registerSheetDataProvider(
                mLastMockWebContents, AccessoryTabType.PASSWORDS, new PropertyProvider<>());

        // Simulate closing the tab (uncommitted):
        mMediator.getTabModelObserverForTesting().willCloseTab(tab, true);
        mMediator.getTabObserverForTesting().onHidden(tab, TabHidingType.CHANGED_TABS);
        getStateForBrowserTab()
                .getWebContentsObserverForTesting()
                .onVisibilityChanged(Visibility.HIDDEN);
        // The state should be kept if the closure wasn't committed.
        assertThat(getStateForBrowserTab().getTabs().length, is(1));
        mLastMockWebContents = null;

        // Simulate undo closing the tab and selecting it:
        mMediator.getTabModelObserverForTesting().tabClosureUndone(tab);
        switchBrowserTab(mMediator, null, tab);

        // Simulate closing the tab and committing to it (i.e. wait out undo message):
        WebContents oldWebContents = mLastMockWebContents;
        closeBrowserTab(mMediator, tab);
        // The state should be cleaned up, now that it was committed.
        assertThat(
                mMediator.getStateCacheForTesting().getStateFor(oldWebContents).getTabs().length,
                is(0));

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
        mController.registerActionProvider(
                mLastMockWebContents, new PropertyProvider<>(GENERATE_PASSWORD_AUTOMATIC));
        getStateForBrowserTab().getActionsProvider().addObserver(firstTabHelper::record);

        // Create a new tab with an action:
        Tab secondTab = addBrowserTab(mMediator, 1111, tab);
        mController.registerActionProvider(
                mLastMockWebContents, secondTabHelper.getActionListProvider());
        getStateForBrowserTab().getActionsProvider().addObserver(secondTabHelper::record);
        secondTabHelper.provideAction(AccessoryAction.CREDMAN_CONDITIONAL_UI_REENTRY);
        assertThat(
                secondTabHelper.getFirstRecordedAction().getActionType(),
                is(AccessoryAction.CREDMAN_CONDITIONAL_UI_REENTRY));

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
        mController.registerActionProvider(
                mLastMockWebContents, delayedTabHelper.getActionListProvider());
        getStateForBrowserTab().getActionsProvider().addObserver(delayedTabHelper::record);
        assertThat(delayedTabHelper.hasRecordedActions(), is(false));

        // Create and switch to a new tab:
        Tab secondTab = addBrowserTab(mMediator, 2222, delayedTab);
        mController.registerActionProvider(
                mLastMockWebContents, secondTabHelper.getActionListProvider());
        getStateForBrowserTab().getActionsProvider().addObserver(secondTabHelper::record);

        // And provide data to the active browser tab.
        secondTabHelper.provideAction(AccessoryAction.CREDMAN_CONDITIONAL_UI_REENTRY);
        // Now, have the delayed provider provide data for the backgrounded browser tab.
        delayedTabHelper.provideAction(AccessoryAction.GENERATE_PASSWORD_AUTOMATIC);

        // The current tab should not be influenced by the delayed provider.
        assertThat(secondTabHelper.getRecordedActions().size(), is(1));
        assertThat(
                secondTabHelper.getFirstRecordedAction().getActionType(),
                is(AccessoryAction.CREDMAN_CONDITIONAL_UI_REENTRY));

        // Switching tabs back should only show the action that was received in the background.
        switchBrowserTab(mMediator, secondTab, delayedTab);
        assertThat(delayedTabHelper.getRecordedActions().size(), is(1));
        assertThat(
                delayedTabHelper.getFirstRecordedAction().getActionType(),
                is(AccessoryAction.GENERATE_PASSWORD_AUTOMATIC));
    }

    @Test
    public void testDestroyingTabCleansModelForThisTab() {
        // Clear any calls that happened during initialization:
        reset(mMockKeyboardAccessory);
        reset(mMockAccessorySheet);
        SheetProviderHelper firstTabHelper = new SheetProviderHelper();
        SheetProviderHelper secondTabHelper = new SheetProviderHelper();
        UpdateAccessorySheetDelegate secondSheetUpdater = mock(UpdateAccessorySheetDelegate.class);

        // Simulate opening a new tab:
        Tab firstTab = addBrowserTab(mMediator, 1111, null);
        mController.registerSheetDataProvider(
                mLastMockWebContents,
                AccessoryTabType.PASSWORDS,
                firstTabHelper.getSheetDataProvider());
        mController.registerActionProvider(
                mLastMockWebContents, firstTabHelper.getActionListProvider());
        getStateForBrowserTab()
                .getSheetDataProvider(AccessoryTabType.PASSWORDS)
                .addObserver(firstTabHelper::record);
        getStateForBrowserTab().getActionsProvider().addObserver(firstTabHelper::record);
        firstTabHelper.providePasswordSheet("FirstPassword");
        firstTabHelper.provideAction(AccessoryAction.CREDMAN_CONDITIONAL_UI_REENTRY);

        // Create and switch to a new tab: (because destruction shouldn't rely on tab to be active)
        Tab secondTab = addBrowserTab(mMediator, 2222, firstTab);
        mController.registerSheetDataProvider(
                mLastMockWebContents,
                AccessoryTabType.PASSWORDS,
                secondTabHelper.getSheetDataProvider());
        mController.registerSheetUpdateDelegate(mLastMockWebContents, secondSheetUpdater);
        mController.registerActionProvider(
                mLastMockWebContents, secondTabHelper.getActionListProvider());
        getStateForBrowserTab()
                .getSheetDataProvider(AccessoryTabType.PASSWORDS)
                .addObserver(secondTabHelper::record);
        getStateForBrowserTab().getActionsProvider().addObserver(secondTabHelper::record);
        secondTabHelper.providePasswordSheet("SecondPassword");
        secondTabHelper.provideAction(AccessoryAction.CREDMAN_CONDITIONAL_UI_REENTRY);

        // The newly created tab should be valid.
        assertThat(secondTabHelper.getFirstRecordedPassword(), is("SecondPassword"));
        assertThat(
                secondTabHelper.getFirstRecordedAction().getActionType(),
                is(AccessoryAction.CREDMAN_CONDITIONAL_UI_REENTRY));

        // Request destruction of the first Tab:
        mMediator.getTabObserverForTesting().onDestroyed(firstTab);

        // The current tab should not be influenced by the destruction...
        // Wiring affects the same sheet only and is triggered after switching
        verify(secondSheetUpdater).requestSheet(AccessoryTabType.PASSWORDS);
        secondTabHelper.providePasswordSheet("SecondPassword");
        assertThat(secondTabHelper.getFirstRecordedPassword(), is("SecondPassword"));
        assertThat(
                secondTabHelper.getFirstRecordedAction().getActionType(),
                is(AccessoryAction.CREDMAN_CONDITIONAL_UI_REENTRY));
        assertThat(getStateForBrowserTab(), is(mCache.getStateFor(secondTab)));
        // ... but the other tab's data should be gone.
        assertThat(mCache.getStateFor(firstTab).getActionsProvider(), nullValue());
        assertThat(mCache.getStateFor(firstTab).getTabs().length, is(0));
    }

    @Test
    public void testDisplaysAccessoryOnlyWhenSpaceIsSufficient() {
        reset(mMockKeyboardAccessory);

        addBrowserTab(mMediator, 1234, null);
        SheetProviderHelper tabHelper = new SheetProviderHelper();
        mController.registerSheetDataProvider(
                mLastMockWebContents, AccessoryTabType.PASSWORDS, tabHelper.getSheetDataProvider());
        when(mMockSoftKeyboardDelegate.isSoftKeyboardShowing(any(), any())).thenReturn(true);
        mKeyboardInsetSupplier.set(sKeyboardHeightDp * /* density= */ 2);
        when(mMockSoftKeyboardDelegate.calculateSoftKeyboardHeight(any()))
                .thenReturn(sKeyboardHeightDp * /* density= */ 2);
        when(mMockKeyboardAccessory.empty()).thenReturn(false);

        // Show the accessory bar for the default dimensions (300x128@2.f).
        mController.show(true);
        verify(mMockKeyboardAccessory).show();

        // The accessory is shown and the content area plus bar size don't exceed the threshold.
        simulateLayoutSizeChange(
                2.f, 180, 128, /* keyboardShown= */ true, VirtualKeyboardMode.RESIZES_VISUAL);

        verify(mMockKeyboardAccessory, never()).dismiss();
    }

    @Test
    public void testDisplaysAccessoryOnlyWhenSpaceIsSufficient_KeyboardResizesContent() {
        mInsetSupplier.setVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_CONTENT);
        reset(mMockKeyboardAccessory);

        addBrowserTab(mMediator, 1234, null);
        SheetProviderHelper tabHelper = new SheetProviderHelper();
        mController.registerSheetDataProvider(
                mLastMockWebContents, AccessoryTabType.PASSWORDS, tabHelper.getSheetDataProvider());
        when(mMockSoftKeyboardDelegate.isSoftKeyboardShowing(any(), any())).thenReturn(true);
        mKeyboardInsetSupplier.set(sKeyboardHeightDp * /* density= */ 2);
        when(mMockSoftKeyboardDelegate.calculateSoftKeyboardHeight(any()))
                .thenReturn(sKeyboardHeightDp * /* density= */ 2);
        when(mMockKeyboardAccessory.empty()).thenReturn(false);

        // Show the accessory bar for the default dimensions (300x128@2.f).
        mController.show(true);
        verify(mMockKeyboardAccessory).show();

        // The accessory is shown and the content area plus bar size don't exceed the threshold.
        simulateLayoutSizeChange(
                2.f, 180, 128, /* keyboardShown= */ true, VirtualKeyboardMode.RESIZES_CONTENT);

        verify(mMockKeyboardAccessory, never()).dismiss();
    }

    @Test
    public void testHidesAccessoryAfterRotation() {
        reset(mMockKeyboardAccessory);
        setContentAreaDimensions(2.f, 180, 320);
        addBrowserTab(mMediator, 1234, null);
        SheetProviderHelper tabHelper = new SheetProviderHelper();
        mController.registerSheetDataProvider(
                mLastMockWebContents, AccessoryTabType.PASSWORDS, tabHelper.getSheetDataProvider());
        when(mMockSoftKeyboardDelegate.isSoftKeyboardShowing(any(), any())).thenReturn(true);
        when(mMockKeyboardAccessory.empty()).thenReturn(false);

        mController.show(true);
        setContentAreaDimensions(2.f, 180, 220);
        mMediator.onLayoutChange(mMockContentView, 0, 0, 540, 360, 0, 0, 640, 360);
        verify(mMockKeyboardAccessory).show();
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), not(is(HIDDEN)));

        // Rotating the screen causes a relayout:
        setContentAreaDimensions(2.f, 320, 128, Surface.ROTATION_90);
        when(mMockSoftKeyboardDelegate.isSoftKeyboardShowing(eq(mMockActivity), any()))
                .thenReturn(false);
        mMediator.onLayoutChange(mMockContentView, 0, 0, 160, 640, 0, 0, 540, 360);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(HIDDEN));
    }

    @Test
    public void testDisplaysAccessoryOnlyWhenVerticalSpaceIsSufficient() {
        mInsetSupplier.setVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_CONTENT);
        reset(mMockKeyboardAccessory);
        addBrowserTab(mMediator, 1234, null);
        SheetProviderHelper tabHelper = new SheetProviderHelper();
        mController.registerSheetDataProvider(
                mLastMockWebContents, AccessoryTabType.PASSWORDS, tabHelper.getSheetDataProvider());
        when(mMockSoftKeyboardDelegate.isSoftKeyboardShowing(eq(mMockActivity), any()))
                .thenReturn(true);
        mKeyboardInsetSupplier.set(sKeyboardHeightDp * /* density= */ 2);
        when(mMockSoftKeyboardDelegate.calculateSoftKeyboardHeight(any()))
                .thenReturn(sKeyboardHeightDp * /* density= */ 2);
        when(mMockKeyboardAccessory.empty()).thenReturn(false);

        // Show the accessory bar for the dimensions exactly at the threshold: 300x128@2.f.
        simulateLayoutSizeChange(
                2.0f, 300, 128, /* keyboardShown= */ true, VirtualKeyboardMode.RESIZES_CONTENT);
        mController.show(true);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), not(is(HIDDEN)));
        verify(mMockKeyboardAccessory).show();

        // The height is now reduced by the 48dp high accessory -- it should remain visible.
        simulateLayoutSizeChange(
                2.0f, 300, 128, /* keyboardShown= */ true, VirtualKeyboardMode.RESIZES_CONTENT);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), not(is(HIDDEN)));

        // Use a height that is too small but with a valid width (e.g. resized multi-window window).
        simulateLayoutSizeChange(
                2.0f, 300, 127, /* keyboardShown= */ true, VirtualKeyboardMode.RESIZES_CONTENT);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(HIDDEN));

        // Also test in RESIZES_VISUAL mode where the keyboard and accessory won't resize the
        // WebContents.
        mInsetSupplier.setVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_VISUAL);
        simulateLayoutSizeChange(
                2.0f, 300, 127, /* keyboardShown= */ true, VirtualKeyboardMode.RESIZES_VISUAL);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(HIDDEN));

        simulateLayoutSizeChange(
                2.0f, 300, 128, /* keyboardShown= */ true, VirtualKeyboardMode.RESIZES_VISUAL);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), not(is(HIDDEN)));
    }

    @Test
    public void testDisplaysAccessoryOnlyWhenHorizontalSpaceIsSufficient() {
        reset(mMockKeyboardAccessory);

        addBrowserTab(mMediator, 1234, null);
        SheetProviderHelper tabHelper = new SheetProviderHelper();
        mController.registerSheetDataProvider(
                mLastMockWebContents, AccessoryTabType.PASSWORDS, tabHelper.getSheetDataProvider());
        when(mMockSoftKeyboardDelegate.isSoftKeyboardShowing(eq(mMockActivity), any()))
                .thenReturn(true);
        when(mMockKeyboardAccessory.empty()).thenReturn(false);

        // Show the accessory bar for the dimensions exactly at the threshold: 180x128@2.f.
        simulateLayoutSizeChange(
                2.0f, 180, 128, /* keyboardShown= */ true, VirtualKeyboardMode.RESIZES_VISUAL);
        mController.show(true);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), not(is(HIDDEN)));

        // Use a width that is too small but with a valid height (e.g. resized multi-window window).
        simulateLayoutSizeChange(
                2.0f, 179, 128, /* keyboardShown= */ true, VirtualKeyboardMode.RESIZES_VISUAL);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(HIDDEN));
    }

    /**
     * This tests the case where an accessory sheet is showing instead of a keyboard. The screen is
     * rotated so that the amount of vertical space shrinks below the minimum allowed. Confirm that
     * the accessory sheet's height is shrunken.
     */
    @Test
    public void testRestrictsSheetSizeIfVerticalSpaceChanges() {
        final int density = 2;
        final int accessorySheetHeightDp = 100; // The height of a large keyboard.
        final int minimumVisibleHeightDp = 128; // This is a constant from ManualFillingMediator.
        final int initialWidthDp = 200;
        final int initialHeightDp = 300;

        addBrowserTab(mMediator, 1234, null);

        // Resize the screen to 200x300@2.f.
        simulateLayoutSizeChange(
                density,
                initialWidthDp,
                initialHeightDp,
                /* keyboardShown= */ false,
                VirtualKeyboardMode.RESIZES_VISUAL);

        // Now simulate showing the accessory sheet.
        when(mMockKeyboardAccessory.empty()).thenReturn(false);
        when(mMockKeyboardAccessory.isShown()).thenReturn(true);
        when(mMockKeyboardAccessory.hasActiveTab()).thenReturn(true);
        when(mMockAccessorySheet.getHeight()).thenReturn(accessorySheetHeightDp * density);
        mModel.set(SHOW_WHEN_VISIBLE, true);
        mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_SHEET);
        mController.registerSheetDataProvider(
                mLastMockWebContents, AccessoryTabType.PASSWORDS, new PropertyProvider<>());
        reset(mMockKeyboardAccessory, mMockAccessorySheet);

        when(mMockKeyboardAccessory.empty()).thenReturn(false);
        when(mMockKeyboardAccessory.isShown()).thenReturn(true);
        when(mMockKeyboardAccessory.hasActiveTab()).thenReturn(true);
        when(mMockAccessorySheet.isShown()).thenReturn(true);
        when(mMockAccessorySheet.getHeight()).thenReturn(accessorySheetHeightDp * density);

        // Set layout as if it was rotated: 300x200@2f. The sheet does not inset WebContents since
        // we're in the default RESIZES_VISUAL VirtualKeyboardMode. Even though contentsHeightDp >
        // minimumVisibleHeightDp, test that the visible area is correctly deduced to be 200 - 100 <
        // minimumVisibleHeightDp so the sheet should be restricted in height.
        assertEquals(
                (int) mController.getBottomInsetSupplier().get(), accessorySheetHeightDp * density);
        simulateLayoutSizeChange(
                density,
                initialHeightDp,
                initialWidthDp,
                /* keyboardShown= */ false,
                VirtualKeyboardMode.RESIZES_VISUAL);
        assertEquals(mLastMockWebContents.getHeight(), initialWidthDp);

        // 200 - 128 = 72
        int expectedSheetHeightDp = initialWidthDp - minimumVisibleHeightDp;
        verify(mMockAccessorySheet).setHeight(density * expectedSheetHeightDp);
    }

    /**
     * This tests the case where an accessory sheet is showing instead of a keyboard. The screen is
     * rotated so that the amount of vertical space shrinks below the minimum allowed. Confirm that
     * the accessory sheet's height is shrunken.
     *
     * <p>This is the same test as above but with the keyboard in RESIZES_CONTENT mode, so that the
     * WebContents height is insetted by the keyboard and its accessories.
     */
    @Test
    public void testRestrictsSheetSizeIfVerticalSpaceChangesWithResizesContent() {
        final int density = 2;
        final int accessorySheetHeightDp = 100; // The height of a large keyboard.
        final int minimumVisibleHeightDp = 128; // This is a constant from ManualFillingMediator.
        final int initialWidthDp = 200;
        final int initialHeightDp = 300;

        mInsetSupplier.setVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_CONTENT);
        addBrowserTab(mMediator, 1234, null);
        // Resize the screen to 200x300@2.f.
        simulateLayoutSizeChange(
                density,
                initialWidthDp,
                initialHeightDp,
                /* keyboardShown= */ false,
                VirtualKeyboardMode.RESIZES_CONTENT);

        // Now simulate showing the accessory sheet.
        when(mMockKeyboardAccessory.empty()).thenReturn(false);
        when(mMockKeyboardAccessory.isShown()).thenReturn(true);
        when(mMockKeyboardAccessory.hasActiveTab()).thenReturn(true);
        when(mMockAccessorySheet.getHeight()).thenReturn(accessorySheetHeightDp * density);
        mModel.set(SHOW_WHEN_VISIBLE, true);
        mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_SHEET);
        mController.registerSheetDataProvider(
                mLastMockWebContents, AccessoryTabType.PASSWORDS, new PropertyProvider<>());
        reset(mMockKeyboardAccessory, mMockAccessorySheet);

        when(mMockKeyboardAccessory.empty()).thenReturn(false);
        when(mMockKeyboardAccessory.isShown()).thenReturn(true);
        when(mMockKeyboardAccessory.hasActiveTab()).thenReturn(true);
        when(mMockAccessorySheet.isShown()).thenReturn(true);
        when(mMockAccessorySheet.getHeight()).thenReturn(accessorySheetHeightDp * density);

        // Set layout as if it was rotated: 300x200@2f. Since we're in RESIZES_CONTENT mode, the
        // sheet will cause a resize to the web contents.  WebContents.getHeight <
        // minimumVisibleHeightDp so the sheet should be restricted in height.
        assertEquals(
                (int) mController.getBottomInsetSupplier().get(), accessorySheetHeightDp * density);
        simulateLayoutSizeChange(
                density,
                initialHeightDp,
                initialWidthDp,
                /* keyboardShown= */ false,
                VirtualKeyboardMode.RESIZES_CONTENT);
        assertEquals(mLastMockWebContents.getHeight(), initialWidthDp - accessorySheetHeightDp);

        // 200 - 128 = 72
        int expectedSheetHeightDp = initialWidthDp - minimumVisibleHeightDp;
        verify(mMockAccessorySheet).setHeight(density * expectedSheetHeightDp);
    }

    @Test
    public void testAdjustsOffsetAndHeightForFullscreen() {
        final int density = 2;
        // Turn off E2E mode
        mMockEdgeToEdgeControllerSupplier.set(null);

        mInsetSupplier.setVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_CONTENT);
        Tab tab = addBrowserTab(mMediator, 1234, null);

        // Now simulate showing the accessory bar.
        when(mMockKeyboardAccessory.empty()).thenReturn(false);
        when(mMockKeyboardAccessory.isShown()).thenReturn(true);
        when(mMockKeyboardAccessory.hasActiveTab()).thenReturn(false);
        mModel.set(SHOW_WHEN_VISIBLE, true);
        when(mMockSoftKeyboardDelegate.isSoftKeyboardShowing(eq(mMockActivity), any()))
                .thenReturn(true);
        mModel.set(KEYBOARD_EXTENSION_STATE, EXTENDING_KEYBOARD);

        // Ensure it's bottom-aligned and insetting the page with its height.
        assertEquals(
                sAccessoryHeightDp * density, (int) mController.getBottomInsetSupplier().get());
        verify(mMockKeyboardAccessory).setBottomOffset(0);
        reset(mMockKeyboardAccessory, mMockAccessorySheet);

        // Simulate entering fullscreen mode which makes the keyboard overlaying.
        mFullscreenObserverCaptor
                .getValue()
                .onEnterFullscreen(tab, new FullscreenOptions(false, false));

        // Ensure it's not insetting the page.
        assertEquals(0, (int) mController.getBottomInsetSupplier().get());
    }

    @Test
    public void testAdjustsOffsetAndHeightForFullscreenOnE2EMode() {
        final int density = 2;

        mInsetSupplier.setVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_CONTENT);
        Tab tab = addBrowserTab(mMediator, 1234, null);

        // Now simulate showing the accessory bar.
        when(mMockKeyboardAccessory.empty()).thenReturn(false);
        when(mMockKeyboardAccessory.isShown()).thenReturn(true);
        when(mMockKeyboardAccessory.hasActiveTab()).thenReturn(false);

        mModel.set(SHOW_WHEN_VISIBLE, true);
        when(mMockSoftKeyboardDelegate.isSoftKeyboardShowing(eq(mMockActivity), any()))
                .thenReturn(true);
        mModel.set(KEYBOARD_EXTENSION_STATE, EXTENDING_KEYBOARD);

        // Ensure it's bottom-aligned and insetting the page with its height.
        assertEquals(
                sAccessoryHeightDp * density, (int) mController.getBottomInsetSupplier().get());
        verify(mMockKeyboardAccessory).setBottomOffset(0);
        reset(mMockKeyboardAccessory, mMockAccessorySheet);

        // Simulate entering fullscreen mode which makes the keyboard overlaying.
        mFullscreenObserverCaptor
                .getValue()
                .onEnterFullscreen(tab, new FullscreenOptions(false, false));

        // Ensure it's not insetting the page.
        assertEquals(
                sAccessoryHeightDp * density, (int) mController.getBottomInsetSupplier().get());
    }

    @Test
    public void testIsFillingViewShownReturnsTargetValueAheadOfComponentUpdate() {
        // After initialization with one tab, the accessory sheet is closed.
        addBrowserTab(mMediator, 1234, null);
        mController.registerSheetDataProvider(
                mLastMockWebContents, AccessoryTabType.PASSWORDS, new PropertyProvider<>());
        when(mMockKeyboardAccessory.hasActiveTab()).thenReturn(false);
        assertThat(mController.isFillingViewShown(null), is(false));

        // As soon as active tab and keyboard change, |isFillingViewShown| returns the expected
        // state - even if the sheet component wasn't updated yet.
        when(mMockKeyboardAccessory.hasActiveTab()).thenReturn(true);
        when(mMockSoftKeyboardDelegate.isSoftKeyboardShowing(eq(mMockActivity), any()))
                .thenReturn(false);
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
        reset(mMockKeyboardAccessory, mMockAccessorySheet);

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
        reset(mMockKeyboardAccessory, mMockAccessorySheet);

        when(mMockSoftKeyboardDelegate.isSoftKeyboardShowing(eq(mMockActivity), any()))
                .thenReturn(true);
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
        when(mMockSoftKeyboardDelegate.isSoftKeyboardShowing(eq(mMockActivity), any()))
                .thenReturn(false);
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
        reset(mMockKeyboardAccessory, mMockAccessorySheet);

        // Set the model FLOATING_BAR. This should update keyboard and subcomponents.
        mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_BAR);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(FLOATING_BAR));

        verify(mMockSoftKeyboardDelegate, atLeastOnce()).showSoftKeyboard(any());
        verify(mMockAccessorySheet).hide();
        verify(mMockKeyboardAccessory).closeActiveTab();
        verify(mMockCompositorViewHolder).requestLayout(); // Triggered as if it was a keyboard.
        verify(mMockKeyboardAccessory).show();
    }

    @Test
    public void testTransitionToFloatingBarWithShouldExtendKeyboardFalse() {
        addBrowserTab(mMediator, 1111, null);
        mModel.set(SHOW_WHEN_VISIBLE, true);
        // Make sure the model is in a non-FLOATING_BAR state first.
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
        reset(mMockKeyboardAccessory, mMockAccessorySheet);

        // Set the model FLOATING_BAR but not extend the keyboard with SHOULD_EXTEND_KEYBOARD
        mModel.set(SHOULD_EXTEND_KEYBOARD, false);
        mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_BAR);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(FLOATING_BAR));

        verify(mMockSoftKeyboardDelegate, never()).showSoftKeyboard(any());
        verify(mMockAccessorySheet).hide();
        verify(mMockKeyboardAccessory).closeActiveTab();
        verify(mMockKeyboardAccessory).show();
    }

    @Test
    public void testTransitionToFloatingSheetShowsSheet() {
        addBrowserTab(mMediator, 1111, null);
        // Make sure the model is in a non-FLOATING_SHEET state first.
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
        reset(mMockKeyboardAccessory, mMockAccessorySheet);

        // Set the model FLOATING_SHEET. This should update keyboard and subcomponents.
        mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_SHEET);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(FLOATING_SHEET));

        verify(mMockSoftKeyboardDelegate).showSoftKeyboard(any());
        verify(mMockAccessorySheet).show();
        verify(mMockKeyboardAccessory, never()).show();
    }

    @Test
    public void testTransitionToReplacingShowsSheet() {
        addBrowserTab(mMediator, 1111, null);
        // Make sure the model is in a non-REPLACING_KEYBOARD state first.
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
        reset(mMockKeyboardAccessory, mMockAccessorySheet);

        // Set the model REPLACING_KEYBOARD. This should update keyboard and subcomponents.
        mModel.set(KEYBOARD_EXTENSION_STATE, REPLACING_KEYBOARD);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(REPLACING_KEYBOARD));

        verify(mMockAccessorySheet).show();
        verify(mMockKeyboardAccessory, never()).show();
    }

    @Test
    public void testTransitionToWaitingHidesKeyboardAndShowsSheet() {
        addBrowserTab(mMediator, 1111, null);
        // Make sure the model is in a non-REPLACING_KEYBOARD state first.
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
        reset(mMockKeyboardAccessory, mMockAccessorySheet);

        // Set the model REPLACING_KEYBOARD. This should update keyboard and subcomponents.
        mModel.set(KEYBOARD_EXTENSION_STATE, WAITING_TO_REPLACE);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(WAITING_TO_REPLACE));

        verify(mMockSoftKeyboardDelegate).hideSoftKeyboardOnly(any());
        verify(mMockAccessorySheet, never()).hide();
        verify(mMockKeyboardAccessory, never()).closeActiveTab();
        verify(mMockKeyboardAccessory, never()).show();
    }

    @Test
    public void testTransitionFromHiddenToExtendingByKeyboard() {
        // Prepare a tab and register a new tab, so there is a reason to display the bar.
        addBrowserTab(mMediator, 1111, null);
        mController.registerSheetDataProvider(
                mLastMockWebContents, AccessoryTabType.PASSWORDS, new PropertyProvider<>());
        mModel.set(SHOW_WHEN_VISIBLE, true);
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
        reset(mMockKeyboardAccessory, mMockAccessorySheet);
        when(mMockKeyboardAccessory.empty()).thenReturn(false);

        // Showing the keyboard should now trigger a transition into EXTENDING state.
        when(mMockSoftKeyboardDelegate.isSoftKeyboardShowing(any(), any())).thenReturn(true);
        mMediator.onLayoutChange(mMockContentView, 0, 0, 320, 90, 0, 0, 320, 180);

        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(EXTENDING_KEYBOARD));
    }

    @Test
    public void testTransitionFromHiddenToExtendingByAvailableData() {
        // Prepare a tab and register a new tab, so there is a reason to display the bar.
        addBrowserTab(mMediator, 1111, null);
        mController.registerSheetDataProvider(
                mLastMockWebContents, AccessoryTabType.PASSWORDS, new PropertyProvider<>());
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
        reset(mMockKeyboardAccessory, mMockAccessorySheet);
        when(mMockKeyboardAccessory.empty()).thenReturn(false);
        when(mMockSoftKeyboardDelegate.isSoftKeyboardShowing(any(), any())).thenReturn(true);

        // Showing the keyboard should now trigger a transition into EXTENDING state.
        mController.show(true);

        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(EXTENDING_KEYBOARD));
    }

    @Test
    public void testTransitionFromHiddenToFloatingBarByAvailableData() {
        // Prepare a tab and register a new tab, so there is a reason to display the bar.
        addBrowserTab(mMediator, 1111, null);
        mController.registerSheetDataProvider(
                mLastMockWebContents, AccessoryTabType.PASSWORDS, new PropertyProvider<>());
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
        reset(mMockKeyboardAccessory, mMockAccessorySheet);
        when(mMockKeyboardAccessory.empty()).thenReturn(false);

        // Showing the keyboard should now trigger a transition into EXTENDING state.
        mController.show(true);

        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(FLOATING_BAR));
    }

    @Test
    public void testTransitionFromFloatingBarToExtendingByKeyboard() {
        // Prepare a tab and register a new tab, so there is a reason to display the bar.
        addBrowserTab(mMediator, 1111, null);
        mController.registerSheetDataProvider(
                mLastMockWebContents, AccessoryTabType.PASSWORDS, new PropertyProvider<>());
        mModel.set(SHOW_WHEN_VISIBLE, true);
        mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_BAR);
        reset(mMockKeyboardAccessory, mMockAccessorySheet);
        when(mMockKeyboardAccessory.empty()).thenReturn(false);
        when(mMockKeyboardAccessory.isShown()).thenReturn(true);

        // Simulate opening a keyboard:
        when(mMockSoftKeyboardDelegate.isSoftKeyboardShowing(any(), any())).thenReturn(true);
        mMediator.onLayoutChange(mMockContentView, 0, 0, 320, 90, 0, 0, 320, 180);

        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(EXTENDING_KEYBOARD));
    }

    @Test
    public void testTransitionFromFloatingBarToFloatingSheetByActivatingTab() {
        // Prepare a tab and register a new tab, so there is a reason to display the bar.
        addBrowserTab(mMediator, 1111, null);
        mController.registerSheetDataProvider(
                mLastMockWebContents, AccessoryTabType.PASSWORDS, new PropertyProvider<>());
        mModel.set(SHOW_WHEN_VISIBLE, true);
        mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_BAR);
        reset(mMockKeyboardAccessory, mMockAccessorySheet);
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
        mController.registerSheetDataProvider(
                mLastMockWebContents, AccessoryTabType.PASSWORDS, new PropertyProvider<>());
        mModel.set(SHOW_WHEN_VISIBLE, true);
        mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_SHEET);
        reset(mMockKeyboardAccessory, mMockAccessorySheet);
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
        mController.registerSheetDataProvider(
                mLastMockWebContents, AccessoryTabType.PASSWORDS, new PropertyProvider<>());
        mModel.set(SHOW_WHEN_VISIBLE, true);
        when(mMockSoftKeyboardDelegate.isSoftKeyboardShowing(any(), any())).thenReturn(true);
        mModel.set(KEYBOARD_EXTENSION_STATE, EXTENDING_KEYBOARD);
        reset(mMockKeyboardAccessory, mMockAccessorySheet);
        when(mMockKeyboardAccessory.empty()).thenReturn(false);
        when(mMockKeyboardAccessory.isShown()).thenReturn(true);

        // Simulate selecting a bottom sheet:
        when(mMockKeyboardAccessory.hasActiveTab()).thenReturn(true);
        mMediator.onChangeAccessorySheet(0);

        // Now the filling component waits for the keyboard to disappear before changing the stat:
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(WAITING_TO_REPLACE));
        // Layout changes but the keyboard is still there, so nothing happens:
        mMediator.onLayoutChange(mMockContentView, 0, 0, 320, 90, 0, 0, 320, 90);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(WAITING_TO_REPLACE));

        // The keyboard finally hides completely and the state changes to REPLACING.
        when(mMockSoftKeyboardDelegate.isSoftKeyboardShowing(any(), any())).thenReturn(false);
        mMediator.onLayoutChange(mMockContentView, 0, 0, 320, 90, 0, 0, 320, 180);
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(REPLACING_KEYBOARD));
    }

    @Test
    public void testTransitionFromReplacingKeyboardToExtendingByClosingSheet() {
        // Prepare a tab and register a new tab, so there is a reason to display the bar.
        addBrowserTab(mMediator, 1111, null);
        mController.registerSheetDataProvider(
                mLastMockWebContents, AccessoryTabType.PASSWORDS, new PropertyProvider<>());
        mModel.set(SHOW_WHEN_VISIBLE, true);
        mModel.set(KEYBOARD_EXTENSION_STATE, REPLACING_KEYBOARD);
        reset(mMockKeyboardAccessory, mMockAccessorySheet);
        when(mMockKeyboardAccessory.empty()).thenReturn(false);
        when(mMockKeyboardAccessory.isShown()).thenReturn(true);
        when(mMockKeyboardAccessory.hasActiveTab()).thenReturn(true);

        // Simulate closing the bottom sheet:
        when(mMockKeyboardAccessory.hasActiveTab()).thenReturn(false);
        mMediator.onCloseAccessorySheet();

        // This will cause a temporary floating sheet state which allows a nicer animation:
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(FLOATING_SHEET));
        // This must trigger the keyboard to open, so the transition into EXTENDING can proceed.
        verify(mMockSoftKeyboardDelegate).showSoftKeyboard(any());

        // Simulate the keyboard opening:
        when(mMockSoftKeyboardDelegate.isSoftKeyboardShowing(any(), any())).thenReturn(true);
        mMediator.onLayoutChange(mMockContentView, 0, 0, 320, 90, 0, 0, 320, 180);

        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(EXTENDING_KEYBOARD));
    }

    @Test
    public void testCallsHelperToConfirmDeletion() {
        Runnable testConfirmRunnable = CallbackUtils.emptyRunnable();
        Runnable testDeclineRunnable = CallbackUtils.emptyRunnable();
        mMediator.confirmOperation(
                "Suggestion", "Delete it?", testConfirmRunnable, testDeclineRunnable);
        verify(mMockConfirmationHelper)
                .showConfirmation(
                        "Suggestion",
                        "Delete it?",
                        R.string.ok,
                        testConfirmRunnable,
                        testDeclineRunnable);
    }

    @Test
    public void testScrollsPageUpAfterBarIsFullyShown() {
        mMediator.onBarFadeInAnimationEnd();
        verify(mLastMockWebContents).scrollFocusedEditableNodeIntoView();
    }

    @Test
    public void testShowAccessorySheetTab() {
        // Prepare a tab and register a new tab, so there is a reason to display the bar.
        addBrowserTab(mMediator, 1111, null);
        mController.registerSheetDataProvider(
                mLastMockWebContents, AccessoryTabType.PASSWORDS, new PropertyProvider<>());
        assertThat(mModel.get(SHOW_WHEN_VISIBLE), is(false));
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(HIDDEN));

        mController.showAccessorySheetTab(AccessoryTabType.PASSWORDS);

        // Verify that the states are updated correctly and the active tab is set.
        assertThat(mModel.get(SHOW_WHEN_VISIBLE), is(true));
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(REPLACING_KEYBOARD));
        verify(mMockKeyboardAccessory, times(1)).setActiveTab(AccessoryTabType.PASSWORDS);

        // Simulate the callback once active tab is set.
        mMediator.onChangeAccessorySheet(0);

        // Assert tha the keyboard extension state continues to be REPLACING_KEYBOARD as we're
        // showing the sheet.
        assertThat(mModel.get(KEYBOARD_EXTENSION_STATE), is(REPLACING_KEYBOARD));
    }

    /**
     * Creates a tab and calls the observer events as if it was just created and switched to.
     *
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
            mCache.getStateFor(mLastMockWebContents)
                    .getWebContentsObserverForTesting()
                    .onVisibilityChanged(Visibility.HIDDEN);
        }
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(id);
        when(tab.getUserDataHost()).thenReturn(mUserDataHost);
        mLastMockWebContents = mock(WebContents.class);
        when(tab.getWebContents()).thenReturn(mLastMockWebContents);
        mCache.getStateFor(tab)
                .getWebContentsObserverForTesting()
                .onVisibilityChanged(Visibility.VISIBLE);
        when(tab.getContentView()).thenReturn(mMockContentView);
        when(mMockTabModelSelector.getCurrentTab()).thenReturn(tab);
        mActivityTabProvider.set(tab);
        mediator.getTabModelObserverForTesting()
                .didAddTab(tab, FROM_BROWSER_ACTIONS, TabCreationState.LIVE_IN_FOREGROUND, false);
        mediator.getTabObserverForTesting().onShown(tab, FROM_NEW);
        mediator.getTabModelObserverForTesting().didSelectTab(tab, FROM_NEW, lastId);
        mInsetSupplier.setVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_CONTENT);
        simulateLayoutSizeChange(
                2.f, 300, 128, /* keyboardShown= */ true, VirtualKeyboardMode.RESIZES_VISUAL);
        return tab;
    }

    /**
     * Simulates switching to a different tab by calling observer events on the given |mediator|.
     *
     * @param mediator The mediator providing the observer instances.
     * @param from The mocked {@link Tab} to be switched from. Needs |getId()|. May be null.
     * @param to The mocked {@link Tab} to be switched to. Needs |getId()|.
     */
    private void switchBrowserTab(ManualFillingMediator mediator, @Nullable Tab from, Tab to) {
        int lastId = INVALID_TAB_ID;
        if (from != null) {
            lastId = from.getId();
            mediator.getTabObserverForTesting().onHidden(from, TabHidingType.CHANGED_TABS);
            mCache.getStateFor(mLastMockWebContents)
                    .getWebContentsObserverForTesting()
                    .onVisibilityChanged(Visibility.HIDDEN);
        }
        mLastMockWebContents = to.getWebContents();
        mCache.getStateFor(to)
                .getWebContentsObserverForTesting()
                .onVisibilityChanged(Visibility.VISIBLE);
        when(mMockTabModelSelector.getCurrentTab()).thenReturn(to);
        mediator.getTabModelObserverForTesting().didSelectTab(to, FROM_USER, lastId);
        mediator.getTabObserverForTesting().onShown(to, FROM_USER);
    }

    /**
     * Simulates destroying the given tab by calling observer events on the given |mediator|.
     *
     * @param mediator The mediator providing the observer instances.
     * @param tabToBeClosed The mocked {@link Tab} to be closed. Needs |getId()|.
     */
    private void closeBrowserTab(ManualFillingMediator mediator, Tab tabToBeClosed) {
        mediator.getTabModelObserverForTesting().willCloseTab(tabToBeClosed, true);
        mediator.getTabObserverForTesting().onHidden(tabToBeClosed, TabHidingType.CHANGED_TABS);
        mCache.getStateFor(mLastMockWebContents)
                .getWebContentsObserverForTesting()
                .onVisibilityChanged(Visibility.HIDDEN);
        mLastMockWebContents = null;
        mediator.getTabModelObserverForTesting().tabClosureCommitted(tabToBeClosed);
        mediator.getTabObserverForTesting().onDestroyed(tabToBeClosed);
    }

    /**
     * Prefer to use simulateLayoutSizeChange which more faithfully sets the WebContents and layout
     * sizes in the presence of a keyboard.
     */
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
     * layout resize event (e.g. when extending/shrinking a multi-window window). It sets the
     * correct {@link WebContents} size according to the current VirtualKeyboardMode and calls
     * |onLayoutChange| with the new bounds.
     *
     * @param density The logical screen density (e.g. 1.f).
     * @param width The new mediator layout width in dp.
     * @param height The new mediator layout height in dp.
     * @param keyboardShown Whether the keyboard is considered shown - if true, the WebContents will
     *     be adjusted by the sKeyboardHeightDp depending on the vkMode.
     * @param vkMode The current virtual keyboard mode, affecting how WebContents reacts to the View
     *     size.
     */
    private void simulateLayoutSizeChange(
            float density,
            int width,
            int height,
            boolean keyboardShown,
            @VirtualKeyboardMode.EnumType int vkMode) {
        mInsetSupplier.setVirtualKeyboardMode(vkMode);
        int oldHeight = mLastMockWebContents.getHeight();
        int oldWidth = mLastMockWebContents.getWidth();

        int webContentsHeight = height;
        // In VISUAL/OVERLAYS, the keyboard shouldn't resize the WebContents so it must be
        // outsetted from the layout height by the keyboard. Otherwise, we must add to the
        // View's existing keyboard inset by insetting the accessory height as well. See
        // ApplicationViewportInsetSupplier for details on how this works.
        if (vkMode == VirtualKeyboardMode.RESIZES_VISUAL
                || vkMode == VirtualKeyboardMode.OVERLAYS_CONTENT) {
            webContentsHeight += keyboardShown ? sKeyboardHeightDp : 0;
        } else {
            int manualFillingInset =
                    Math.round(mController.getBottomInsetSupplier().get() / density);
            webContentsHeight -= manualFillingInset;
        }
        setContentAreaDimensions(2.f, width, webContentsHeight);

        int newHeight = (int) (density * height);
        int newWidth = (int) (density * width);
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
