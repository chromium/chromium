// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.provider.Browser;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Token;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.customtabs.PopupCreatorFactory;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tabmodel.AsyncTabParams;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/** Unit test for {@link ReparentingTask}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ReparentingTaskUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private ReparentingTask.Natives mReparentingTaskNatives;
    @Mock private WindowAndroid mWindowAndroid;

    @Mock private Tab mTab;
    private final UserDataHost mUserDataHost = new UserDataHost();
    @Mock private ReparentingTask mTabReparentingTask;
    @Mock private WebContents mWebContents;

    private static final Class<ReparentingTask> USER_DATA_KEY = ReparentingTask.class;
    private static final String EXTRA_TEST_EXTRA = "ReparentingTaskUnitTest#EXTRA_TEST_EXTRA";
    private static final int EXTRA_TEST_VALUE = 37;
    private static final int TAB_ID = 56;
    private static final int TAB_ID_2 = 57;
    private static final String TAB_URL_SPEC = "https://url1.com/";
    private static final GURL TAB_URL = new GURL(TAB_URL_SPEC);

    @Before
    public void setup() {
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.getUrl()).thenReturn(TAB_URL);
        ReparentingTaskJni.setInstanceForTesting(mReparentingTaskNatives);
        PopupCreatorFactory.setInstanceForTesting(new PopupCreatorImpl());
    }

    @After
    public void tearDown() {
        AsyncTabParamsManagerSingleton.getInstance().remove(TAB_ID);
        AsyncTabParamsManagerSingleton.getInstance().remove(TAB_ID_2);
        ReparentingTaskJni.setInstanceForTesting(null);
    }

    @Test
    public void testGet_returnsReparentingTaskFromTab() {
        mUserDataHost.setUserData(USER_DATA_KEY, mTabReparentingTask);

        assertEquals(
                "#get(...) should return the ReparentingTask from Tab's UserDataHost",
                mTabReparentingTask,
                ReparentingTask.get(mTab));
    }

    @Test
    public void testFrom_tabHasReparentingTask_returnsIt() {
        mUserDataHost.setUserData(USER_DATA_KEY, mTabReparentingTask);

        assertEquals(
                "#from(...) should return the ReparentingTask from Tab's UserDataHost",
                mTabReparentingTask,
                ReparentingTask.from(mTab));
    }

    @Test
    public void testFrom_tabDoesNotHaveReparentingTask_createsReparentingTaskAndReturnsIt() {
        final ReparentingTask result = ReparentingTask.from(mTab);

        assertNotNull("#from(...) should return a non-null ReparentingTask", result);
        assertEquals(
                "The ReparentingTask returned from #from(...) should correctly set its Tab",
                mTab,
                result.getTabForTesting());
    }

    @Test
    public void testBegin_detaches() {
        final ReparentingTask task = ReparentingTask.from(mTab);

        assertTrue(
                "#begin(...) should return true if successful",
                task.begin(mContext, new Intent(), null, null));

        verify(mWebContents).setTopLevelNativeWindow(null);
        verify(mTab).updateAttachment(null, null);
    }

    @Test
    public void testBegin_setsIntentParameters() {
        final ReparentingTask task = ReparentingTask.from(mTab);
        final Intent intent = new Intent();
        intent.putExtra(EXTRA_TEST_EXTRA, EXTRA_TEST_VALUE);

        task.begin(mContext, intent, null, null);

        final ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(mContext).startActivity(captor.capture(), any());
        final Intent sentIntent = captor.getValue();

        assertEquals(
                "#begin(...) should set the correct Intent class if the component was null",
                ChromeLauncherActivity.class.getName(),
                sentIntent.getComponent().getClassName());
        assertEquals(
                "#begin(...) should set the correct Intent action",
                Intent.ACTION_VIEW,
                sentIntent.getAction());
        assertEquals(
                "#begin(...) should set the correct Intent data if the data string was null",
                TAB_URL_SPEC,
                sentIntent.getDataString());
        assertTrue(
                "#begin(...) should make the Intent trusted",
                IntentUtils.isTrustedIntentFromSelf(sentIntent));
        assertEquals(
                "#begin(...) should set the correct Tab ID to the Intent",
                TAB_ID,
                IntentHandler.getTabId(sentIntent));
        assertEquals(
                "#begin(...) should preserve preexisting extras of the Intent",
                EXTRA_TEST_VALUE,
                sentIntent.getIntExtra(EXTRA_TEST_EXTRA, 0));
    }

    @Test
    public void testBegin_setsIntentParameters_incognito() {
        when(mTab.isIncognito()).thenReturn(true);
        final ReparentingTask task = ReparentingTask.from(mTab);
        final Intent intent = new Intent();

        task.begin(mContext, intent, null, null);

        final ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(mContext).startActivity(captor.capture(), any());
        final Intent sentIntent = captor.getValue();

        assertEquals(
                "#begin(...) should populate the EXTRA_APPLICATION_ID extra of the Intent if the"
                        + " Tab is incognito",
                ContextUtils.getApplicationContext().getPackageName(),
                sentIntent.getStringExtra(Browser.EXTRA_APPLICATION_ID));
        assertTrue(
                "#begin(...) should set the EXTRA_OPEN_NEW_INCOGNITO_TAB extra of the Intent if"
                        + " the Tab is incognito",
                sentIntent.hasExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB));
        assertTrue(
                "#begin(...) should set the EXTRA_OPEN_NEW_INCOGNITO_TAB extra of the Intent if"
                        + " the Tab is incognito",
                sentIntent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false));
    }

    @Test
    public void testBegin_passesActivityOptions() {
        final ReparentingTask task = ReparentingTask.from(mTab);
        final Intent intent = new Intent();
        final Bundle options = new Bundle();

        task.begin(mContext, intent, options, null);

        final ArgumentCaptor<Bundle> captor = ArgumentCaptor.forClass(Bundle.class);
        verify(mContext).startActivity(any(), captor.capture());
        final Bundle sentOptions = captor.getValue();

        assertEquals(
                "#begin(...) should start the new Activity with provided ActivityOptions",
                options,
                sentOptions);
    }

    @Test
    public void testBegin_addsParamsToAsyncTabParamsManager() {
        final ReparentingTask task = ReparentingTask.from(mTab);
        final Intent intent = new Intent();
        final Runnable finalizeCallback = mock(Runnable.class);

        task.begin(mContext, intent, null, finalizeCallback);

        final AsyncTabParamsManager asyncTabParamsManager =
                AsyncTabParamsManagerSingleton.getInstance();
        assertTrue(
                "#begin(...) should add TabReparentingParams to AsyncTabParamsManager",
                asyncTabParamsManager.hasParamsForTabId(TAB_ID));
        final AsyncTabParams asyncTabParams = asyncTabParamsManager.getAsyncTabParams().get(TAB_ID);
        assertTrue(
                "#begin(...) should add TabReparentingParams to AsyncTabParamsManager",
                asyncTabParams instanceof TabReparentingParams);
        final TabReparentingParams tabReparentingParams = (TabReparentingParams) asyncTabParams;
        assertEquals(
                "#begin(...) should add TabReparentingParams with correct Tab set to"
                        + " AsyncTabParamsManager",
                mTab,
                tabReparentingParams.getTabToReparent());
        assertEquals(
                "#begin(...) should add TabReparentingParams with correct FinalizeCallback set to"
                        + " AsyncTabParamsManager",
                finalizeCallback,
                tabReparentingParams.getFinalizeCallback());
    }

    @Test
    public void testBegin_finishesAsNoOpIfActivityDoesNotStart() {
        final ReparentingTask task = ReparentingTask.from(mTab);
        final Intent intent = new Intent();
        doThrow(new SecurityException()).when(mContext).startActivity(eq(intent), any());
        when(mTab.getWindowAndroidChecked()).thenReturn(mWindowAndroid);

        assertFalse(
                "#begin(...) should return false if the Activity has not started",
                task.begin(mContext, intent, null, null));

        final AsyncTabParamsManager asyncTabParamsManager =
                AsyncTabParamsManagerSingleton.getInstance();
        assertFalse(
                "#begin(...) should not keep TabReparentingParams in AsyncTabParamsManager if the"
                        + " Activity has not started",
                asyncTabParamsManager.hasParamsForTabId(TAB_ID));

        final InOrder inOrder = inOrder(mTab, mWebContents);
        inOrder.verify(mWebContents).setTopLevelNativeWindow(null);
        inOrder.verify(mTab).updateAttachment(null, null);
        inOrder.verify(mTab).updateAttachment(mWindowAndroid, null);
    }

    @Test
    public void testReparentingTabsTaskBegin_finishesAsNoOpIfActivityDoesNotStart() {
        final Tab tab2 = mock(Tab.class);
        final WebContents webContents2 = mock(WebContents.class);
        final WindowAndroid windowAndroid2 = mock(WindowAndroid.class);
        final UserDataHost userDataHost2 = new UserDataHost();
        final int tabId2 = 57;
        when(tab2.getUserDataHost()).thenReturn(userDataHost2);
        when(tab2.getWebContents()).thenReturn(webContents2);
        when(tab2.getId()).thenReturn(tabId2);
        when(tab2.getUrl()).thenReturn(TAB_URL);
        when(tab2.getWindowAndroidChecked()).thenReturn(windowAndroid2);
        when(mTab.getWindowAndroidChecked()).thenReturn(mWindowAndroid);

        final Intent intent = new Intent();
        doThrow(new SecurityException()).when(mContext).startActivity(eq(intent), any());

        final ReparentingTabsTask tabsTask = ReparentingTabsTask.from(List.of(mTab, tab2));
        assertFalse(
                "#begin(...) should return false if the Activity has not started",
                tabsTask.begin(mContext, intent, null, null));

        final AsyncTabParamsManager asyncTabParamsManager =
                AsyncTabParamsManagerSingleton.getInstance();
        assertFalse(
                "#begin(...) should not keep TabReparentingParams in AsyncTabParamsManager for"
                        + " tab1",
                asyncTabParamsManager.hasParamsForTabId(TAB_ID));
        assertFalse(
                "#begin(...) should not keep TabReparentingParams in AsyncTabParamsManager for"
                        + " tab2",
                asyncTabParamsManager.hasParamsForTabId(tabId2));

        final InOrder inOrder1 = inOrder(mTab, mWebContents);
        inOrder1.verify(mWebContents).setTopLevelNativeWindow(null);
        inOrder1.verify(mTab).updateAttachment(null, null);
        inOrder1.verify(mTab).updateAttachment(mWindowAndroid, null);

        final InOrder inOrder2 = inOrder(tab2, webContents2);
        inOrder2.verify(webContents2).setTopLevelNativeWindow(null);
        inOrder2.verify(tab2).updateAttachment(null, null);
        inOrder2.verify(tab2).updateAttachment(windowAndroid2, null);
    }

    private static TabGroupMetadata createTabGroupMetadata(
            int sourceWindowId, Token tabGroupId, int tabId1, int tabId2) {
        ArrayList<Map.Entry<Integer, String>> tabIdsToUrls =
                new ArrayList<>(
                        List.of(
                                Map.entry(tabId1, TAB_URL_SPEC),
                                Map.entry(tabId2, "https://url2.com/")));
        return new TabGroupMetadata(
                /* selectedTabId= */ tabId1,
                sourceWindowId,
                tabGroupId,
                tabIdsToUrls,
                /* tabGroupColor= */ 0,
                /* tabGroupTitle= */ "Group",
                /* mhtmlTabTitle= */ null,
                /* tabGroupCollapsed= */ false,
                /* isGroupShared= */ false,
                /* isIncognito= */ false);
    }

    @Test
    public void testReparentingTabGroupTaskBegin_finishesAsNoOpIfActivityDoesNotStart() {
        final Tab tab2 = mock(Tab.class);
        final WebContents webContents2 = mock(WebContents.class);
        final UserDataHost userDataHost2 = new UserDataHost();
        when(tab2.getUserDataHost()).thenReturn(userDataHost2);
        when(tab2.getWebContents()).thenReturn(webContents2);
        when(tab2.getId()).thenReturn(TAB_ID_2);
        when(tab2.getUrl()).thenReturn(TAB_URL);
        when(tab2.getWindowAndroidChecked()).thenReturn(mWindowAndroid);
        when(mTab.getWindowAndroidChecked()).thenReturn(mWindowAndroid);

        final int sourceWindowId = 1;
        final Token tabGroupId = new Token(123L, 456L);
        final TabWindowManager tabWindowManager = mock(TabWindowManager.class);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(tabWindowManager);
        when(tabWindowManager.getGroupedTabsByWindow(sourceWindowId, tabGroupId, false))
                .thenReturn(List.of(mTab, tab2));

        final TabGroupMetadata metadata =
                createTabGroupMetadata(sourceWindowId, tabGroupId, TAB_ID, TAB_ID_2);
        final ReparentingTabGroupTask tabGroupTask = ReparentingTabGroupTask.from(metadata);

        final Intent intent = new Intent();
        tabGroupTask.setupIntent(intent, null);

        final AsyncTabParamsManager asyncTabParamsManager =
                AsyncTabParamsManagerSingleton.getInstance();
        assertTrue(asyncTabParamsManager.hasParamsForTabId(TAB_ID));
        assertTrue(asyncTabParamsManager.hasParamsForTabId(TAB_ID_2));

        doThrow(new SecurityException()).when(mContext).startActivity(eq(intent), any());

        assertFalse(
                "#begin(...) should return false if the Activity has not started",
                tabGroupTask.begin(mContext, intent));

        assertFalse(
                "#begin(...) should not keep TabReparentingParams in AsyncTabParamsManager for"
                        + " tab1",
                asyncTabParamsManager.hasParamsForTabId(TAB_ID));
        assertFalse(
                "#begin(...) should not keep TabReparentingParams in AsyncTabParamsManager for"
                        + " tab2",
                asyncTabParamsManager.hasParamsForTabId(TAB_ID_2));

        final InOrder inOrder1 = inOrder(mTab, mWebContents);
        inOrder1.verify(mWebContents).setTopLevelNativeWindow(null);
        inOrder1.verify(mTab).updateAttachment(null, null);
        inOrder1.verify(mTab).updateAttachment(mWindowAndroid, null);

        final InOrder inOrder2 = inOrder(tab2, webContents2);
        inOrder2.verify(webContents2).setTopLevelNativeWindow(null);
        inOrder2.verify(tab2).updateAttachment(null, null);
        inOrder2.verify(tab2).updateAttachment(mWindowAndroid, null);
    }

    @Test
    public void testReparentingTabGroupTaskBegin_nullContext_finishesAsNoOp() {
        final Tab tab2 = mock(Tab.class);
        final WebContents webContents2 = mock(WebContents.class);
        final UserDataHost userDataHost2 = new UserDataHost();
        when(tab2.getUserDataHost()).thenReturn(userDataHost2);
        when(tab2.getWebContents()).thenReturn(webContents2);
        when(tab2.getId()).thenReturn(TAB_ID_2);
        when(tab2.getUrl()).thenReturn(TAB_URL);
        when(tab2.getWindowAndroidChecked()).thenReturn(mWindowAndroid);
        when(mTab.getWindowAndroidChecked()).thenReturn(mWindowAndroid);

        final int sourceWindowId = 1;
        final Token tabGroupId = new Token(123L, 456L);
        final TabWindowManager tabWindowManager = mock(TabWindowManager.class);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(tabWindowManager);
        when(tabWindowManager.getGroupedTabsByWindow(sourceWindowId, tabGroupId, false))
                .thenReturn(List.of(mTab, tab2));

        final TabGroupMetadata metadata =
                createTabGroupMetadata(sourceWindowId, tabGroupId, TAB_ID, TAB_ID_2);
        final ReparentingTabGroupTask tabGroupTask = ReparentingTabGroupTask.from(metadata);

        final Intent intent = new Intent();
        tabGroupTask.setupIntent(intent, null);

        final AsyncTabParamsManager asyncTabParamsManager =
                AsyncTabParamsManagerSingleton.getInstance();
        assertTrue(asyncTabParamsManager.hasParamsForTabId(TAB_ID));
        assertTrue(asyncTabParamsManager.hasParamsForTabId(TAB_ID_2));

        assertFalse(
                "#begin(...) should return false when context is null",
                tabGroupTask.begin(null, intent));

        assertFalse(
                "#begin(...) should not keep TabReparentingParams in AsyncTabParamsManager for"
                        + " tab1",
                asyncTabParamsManager.hasParamsForTabId(TAB_ID));
        assertFalse(
                "#begin(...) should not keep TabReparentingParams in AsyncTabParamsManager for"
                        + " tab2",
                asyncTabParamsManager.hasParamsForTabId(TAB_ID_2));

        final InOrder inOrder1 = inOrder(mTab, mWebContents);
        inOrder1.verify(mWebContents).setTopLevelNativeWindow(null);
        inOrder1.verify(mTab).updateAttachment(null, null);
        inOrder1.verify(mTab).updateAttachment(mWindowAndroid, null);

        final InOrder inOrder2 = inOrder(tab2, webContents2);
        inOrder2.verify(webContents2).setTopLevelNativeWindow(null);
        inOrder2.verify(tab2).updateAttachment(null, null);
        inOrder2.verify(tab2).updateAttachment(mWindowAndroid, null);
    }

    @Test
    public void testFinish_attaches() {
        final ReparentingTask task = ReparentingTask.from(mTab);
        final Intent intent = new Intent();
        final WindowAndroid targetWindow = mock(WindowAndroid.class);
        final TabDelegateFactory targetTabDelegateFactory = mock(TabDelegateFactory.class);
        final Runnable finalizeCallback = mock(Runnable.class);
        final CompositorViewHolder compositorViewHolder = mock(CompositorViewHolder.class);
        final ReparentingTask.Delegate delegate =
                new ReparentingTask.Delegate() {
                    @Override
                    public @Nullable CompositorViewHolder getCompositorViewHolder() {
                        return compositorViewHolder;
                    }

                    @Override
                    public WindowAndroid getWindowAndroid() {
                        return targetWindow;
                    }

                    @Override
                    public @Nullable TabDelegateFactory getTabDelegateFactory() {
                        return targetTabDelegateFactory;
                    }
                };

        task.begin(mContext, intent, null, null);
        task.finish(delegate, finalizeCallback);

        final InOrder inOrder = inOrder(mTab);
        inOrder.verify(mTab).updateAttachment(null, null);
        inOrder.verify(mTab).updateAttachment(targetWindow, targetTabDelegateFactory);

        verify(compositorViewHolder).prepareForTabReparenting();
        verify(mReparentingTaskNatives).attachTab(mWebContents);
        verify(finalizeCallback).run();
    }
}
