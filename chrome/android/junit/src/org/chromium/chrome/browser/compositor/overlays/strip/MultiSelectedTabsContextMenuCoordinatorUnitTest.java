// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabOverflowMenuCoordinator.OnItemClickedCallback;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.List;

/** Unit tests for {@link MultiSelectedTabsContextMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.DATA_SHARING})
public class MultiSelectedTabsContextMenuCoordinatorUnitTest {
    private static final int TAB_1_ID = 1;
    private static final int TAB_2_ID = 2;
    private static final int TAB_3_ID = 3;
    private static final int TAB_OUTSIDE_OF_GROUP_ID = 4;
    private static final int NON_URL_TAB_ID = 5;
    private static final Token TAB_GROUP_ID = Token.createRandom();
    private static final GURL EXAMPLE_URL = new GURL("https://example.com");
    private static final GURL CHROME_SCHEME_URL = new GURL("chrome://history");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private MultiSelectedTabsContextMenuCoordinator mMultiSelectedTabsContextMenuCoordinator;
    private OnItemClickedCallback<List<Integer>> mOnItemClickedCallback;
    private MockTabModel mTabModel;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTab3;
    @Mock private Tab mTabOutsideOfGroup;
    @Mock private Tab mNonUrlTab;
    @Mock private TabRemover mTabRemover;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabUngrouper mTabUngrouper;
    @Mock private Profile mProfile;
    @Mock private TabGroupListBottomSheetCoordinator mBottomSheetCoordinator;
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private TabCreator mTabCreator;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private ServiceStatus mServiceStatus;
    @Mock private WeakReference<Activity> mWeakReferenceActivity;

    @Before
    public void setUp() {
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mCollaborationService.getServiceStatus()).thenReturn(mServiceStatus);
        when(mServiceStatus.isAllowedToCreate()).thenReturn(true);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        MultiWindowUtils.setInstanceCountForTesting(1);

        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        when(mWindowAndroid.getKeyboardDelegate()).thenReturn(mKeyboardVisibilityDelegate);
        when(mWindowAndroid.getActivity()).thenReturn(mWeakReferenceActivity);
        when(mWeakReferenceActivity.get()).thenReturn(activity);
        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabModel.getTabById(TAB_1_ID)).thenReturn(mTab1);
        when(mTabModel.getTabById(TAB_2_ID)).thenReturn(mTab2);
        when(mTabModel.getTabById(TAB_3_ID)).thenReturn(mTab3);
        when(mTabModel.getTabById(TAB_OUTSIDE_OF_GROUP_ID)).thenReturn(mTabOutsideOfGroup);
        when(mTabModel.getTabById(NON_URL_TAB_ID)).thenReturn(mNonUrlTab);
        mTabModel.setTabRemoverForTesting(mTabRemover);
        mTabModel.setTabCreatorForTesting(mTabCreator);

        when(mTab1.getId()).thenReturn(TAB_1_ID);
        when(mTab2.getId()).thenReturn(TAB_2_ID);
        when(mTab3.getId()).thenReturn(TAB_3_ID);

        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTab1.getUrl()).thenReturn(EXAMPLE_URL);
        when(mTab2.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTab2.getUrl()).thenReturn(EXAMPLE_URL);
        when(mTab3.getTabGroupId()).thenReturn(null);
        when(mTab3.getUrl()).thenReturn(EXAMPLE_URL);
        when(mTabOutsideOfGroup.getTabGroupId()).thenReturn(null);
        when(mTabOutsideOfGroup.getUrl()).thenReturn(EXAMPLE_URL);
        when(mNonUrlTab.getTabGroupId()).thenReturn(null);
        when(mNonUrlTab.getUrl()).thenReturn(CHROME_SCHEME_URL);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilter.getTabUngrouper()).thenReturn(mTabUngrouper);
        setupWithIncognito(/* incognito= */ false); // Most tests will run not in incognito mode
        initializeCoordinator();
    }

    private void setupWithIncognito(boolean incognito) {
        when(mTabModel.isIncognito()).thenReturn(incognito);
        when(mTabModel.isIncognitoBranded()).thenReturn(incognito);
        when(mProfile.isOffTheRecord()).thenReturn(incognito);
    }

    private void initializeCoordinator() {
        mOnItemClickedCallback =
                MultiSelectedTabsContextMenuCoordinator.getMenuItemClickedCallback(
                        () -> mTabModel,
                        mTabGroupModelFilter,
                        mBottomSheetCoordinator,
                        mMultiInstanceManager);
        mMultiSelectedTabsContextMenuCoordinator =
                MultiSelectedTabsContextMenuCoordinator.createContextMenuCoordinator(
                        mTabModel,
                        mTabGroupModelFilter,
                        mBottomSheetCoordinator,
                        mMultiInstanceManager,
                        mWindowAndroid);
    }

    @Test
    public void fakeTest() {
        // This is just to appease CQ. will be removed once context menu gets fleshed out.
    }
}
