// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener.DialogType;
import org.chromium.chrome.browser.ui.native_page.BeforeUnloadCallback;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link PassthroughTabRemover}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PassthroughTabRemoverUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private TabModelActionListener mListener;
    @Mock private Callback<TabClosureParams> mTabClosureCallback;

    @Captor private ArgumentCaptor<TabClosureParams> mParamsCaptor;

    private MockTabModel mTabModel;
    private PassthroughTabRemover mPassthroughTabRemover;

    @Before
    public void setUp() {
        mTabModel = spy(new MockTabModel(mProfile, null));
        mPassthroughTabRemover = new PassthroughTabRemover(() -> mTabModel);
    }

    @Test
    public void testCloseTabs_NoDialog() {
        Tab tab0 = mTabModel.addTab(/* id= */ 0);
        TabClosureParams params = TabClosureParams.closeTab(tab0).build();

        mPassthroughTabRemover.closeTabs(params, /* allowDialog= */ false, mListener);
        verify(mTabModel).closeTabs(params);
        verify(mListener)
                .willPerformActionOrShowDialog(DialogType.NONE, /* willSkipDialog= */ true);
        verify(mListener)
                .onConfirmationDialogResult(
                        DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
    }

    @Test
    public void testCloseTabs_BeforeUnload_Proceed() {
        Tab tab0 = mTabModel.addTab(/* id= */ 0);
        BeforeUnloadCallback callback =
                (onProceed, onCancel) -> {
                    onProceed.run();
                    return true;
                };
        tab0.getUserDataHost().setUserData(BeforeUnloadCallback.class, callback);
        TabClosureParams params = TabClosureParams.closeTab(tab0).build();

        mPassthroughTabRemover.closeTabs(params, /* allowDialog= */ true, mListener);
        verify(mTabModel).closeTabs(params);
        verify(mListener)
                .willPerformActionOrShowDialog(DialogType.NONE, /* willSkipDialog= */ true);
        verify(mListener)
                .onConfirmationDialogResult(
                        DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
    }

    @Test
    public void testCloseTabs_BeforeUnload_Cancel() {
        Tab tab0 = mTabModel.addTab(/* id= */ 0);
        BeforeUnloadCallback callback =
                (onProceed, onCancel) -> {
                    onCancel.run();
                    return true;
                };
        tab0.getUserDataHost().setUserData(BeforeUnloadCallback.class, callback);
        TabClosureParams params = TabClosureParams.closeTab(tab0).build();

        mPassthroughTabRemover.closeTabs(params, /* allowDialog= */ true, mListener);
        verify(mTabModel, never()).closeTabs(any());
        verify(mListener)
                .onConfirmationDialogResult(
                        DialogType.NONE, ActionConfirmationResult.CONFIRMATION_NEGATIVE);
    }

    @Test
    public void testPrepareCloseTabs_BeforeUnload_DisallowedDialog() {
        Tab tab0 = mTabModel.addTab(/* id= */ 0);
        BeforeUnloadCallback callback = mock(BeforeUnloadCallback.class);
        tab0.getUserDataHost().setUserData(BeforeUnloadCallback.class, callback);
        TabClosureParams params = TabClosureParams.closeTab(tab0).build();

        mPassthroughTabRemover.prepareCloseTabs(
                params, /* allowDialog= */ false, mListener, mTabClosureCallback);
        verify(callback, never()).handleBeforeUnload(any(), any());
        verify(mTabClosureCallback).onResult(params);
    }

    @Test
    public void testPrepareCloseTabs_MultipleTabs_BeforeUnload_ProceedAll() {
        Tab tab0 = mTabModel.addTab(/* id= */ 0);
        Tab tab1 = mTabModel.addTab(/* id= */ 1);
        BeforeUnloadCallback callback0 =
                (onProceed, onCancel) -> {
                    onProceed.run();
                    return true;
                };
        BeforeUnloadCallback callback1 =
                (onProceed, onCancel) -> {
                    onProceed.run();
                    return true;
                };
        tab0.getUserDataHost().setUserData(BeforeUnloadCallback.class, callback0);
        tab1.getUserDataHost().setUserData(BeforeUnloadCallback.class, callback1);
        TabClosureParams params = TabClosureParams.closeTabs(List.of(tab0, tab1)).build();

        mPassthroughTabRemover.prepareCloseTabs(
                params, /* allowDialog= */ true, mListener, mTabClosureCallback);
        verify(mTabClosureCallback).onResult(params);
    }

    @Test
    public void testPrepareCloseTabs_MultipleTabs_BeforeUnload_CancelSecondTab() {
        Tab tab0 = mTabModel.addTab(/* id= */ 0);
        Tab tab1 = mTabModel.addTab(/* id= */ 1);
        BeforeUnloadCallback callback0 =
                (onProceed, onCancel) -> {
                    onProceed.run();
                    return true;
                };
        BeforeUnloadCallback callback1 =
                (onProceed, onCancel) -> {
                    onCancel.run();
                    return true;
                };
        tab0.getUserDataHost().setUserData(BeforeUnloadCallback.class, callback0);
        tab1.getUserDataHost().setUserData(BeforeUnloadCallback.class, callback1);
        TabClosureParams params = TabClosureParams.closeTabs(List.of(tab0, tab1)).build();

        mPassthroughTabRemover.prepareCloseTabs(
                params, /* allowDialog= */ true, mListener, mTabClosureCallback);

        // tab1 refused, so the closure narrows to tab0 rather than being abandoned.
        verify(mTabClosureCallback).onResult(mParamsCaptor.capture());
        assertEquals(List.of(tab0), mParamsCaptor.getValue().tabs);
    }

    @Test
    public void testForceCloseTabs_BypassesBeforeUnload() {
        Tab tab0 = mTabModel.addTab(/* id= */ 0);
        BeforeUnloadCallback callback = mock(BeforeUnloadCallback.class);
        tab0.getUserDataHost().setUserData(BeforeUnloadCallback.class, callback);
        TabClosureParams params = TabClosureParams.closeTab(tab0).build();

        mPassthroughTabRemover.forceCloseTabs(params);
        verify(callback, never()).handleBeforeUnload(any(), any());
        verify(mTabModel).closeTabs(params);
    }

    @Test
    public void testPrepareCloseTabs_AllTabs_BeforeUnload() {
        Tab tab0 = mTabModel.addTab(/* id= */ 0);
        BeforeUnloadCallback callback =
                (onProceed, onCancel) -> {
                    onCancel.run();
                    return true;
                };
        tab0.getUserDataHost().setUserData(BeforeUnloadCallback.class, callback);
        TabClosureParams params = TabClosureParams.closeAllTabs().build();

        mPassthroughTabRemover.prepareCloseTabs(
                params, /* allowDialog= */ true, mListener, mTabClosureCallback);
        verify(mTabClosureCallback, never()).onResult(any());
        verify(mListener)
                .onConfirmationDialogResult(
                        DialogType.NONE, ActionConfirmationResult.CONFIRMATION_NEGATIVE);
    }

    @Test
    public void testPrepareCloseTabs_MultipleTabs_BeforeUnload_CallbackReturnsFalse() {
        Tab tab0 = mTabModel.addTab(/* id= */ 0);
        Tab tab1 = mTabModel.addTab(/* id= */ 1);
        BeforeUnloadCallback callback0 = (onProceed, onCancel) -> false;
        tab0.getUserDataHost().setUserData(BeforeUnloadCallback.class, callback0);
        TabClosureParams params = TabClosureParams.closeTabs(List.of(tab0, tab1)).build();

        mPassthroughTabRemover.prepareCloseTabs(
                params, /* allowDialog= */ true, mListener, mTabClosureCallback);
        verify(mTabClosureCallback).onResult(params);
    }

    @Test
    public void testPrepareCloseTabs_TabClosePrompter_Proceed() {
        Tab tab0 = mTabModel.addTab(/* id= */ 0);
        TabClosureParams params = TabClosureParams.closeTab(tab0).build();
        PassthroughTabRemover remover =
                new PassthroughTabRemover(
                        () -> mTabModel,
                        (promptedParams, tab, onProceed, onCancel) -> {
                            onProceed.run();
                            return true;
                        });

        remover.prepareCloseTabs(params, /* allowDialog= */ true, mListener, mTabClosureCallback);
        verify(mTabClosureCallback).onResult(params);
    }

    @Test
    public void testPrepareCloseTabs_TabClosePrompter_CancelSparesThatTab() {
        Tab tab0 = mTabModel.addTab(/* id= */ 0);
        Tab tab1 = mTabModel.addTab(/* id= */ 1);
        TabClosureParams params = TabClosureParams.closeTabs(List.of(tab0, tab1)).build();
        PassthroughTabRemover remover =
                new PassthroughTabRemover(
                        () -> mTabModel,
                        (promptedParams, tab, onProceed, onCancel) -> {
                            if (tab == tab1) {
                                onCancel.run();
                            } else {
                                onProceed.run();
                            }
                            return true;
                        });

        remover.prepareCloseTabs(params, /* allowDialog= */ true, mListener, mTabClosureCallback);

        verify(mTabClosureCallback).onResult(mParamsCaptor.capture());
        assertEquals(List.of(tab0), mParamsCaptor.getValue().tabs);
    }

    @Test
    public void testPrepareCloseTabs_TabClosePrompter_TabLeftModelWhileAsking() {
        Tab tab0 = mTabModel.addTab(/* id= */ 0);
        Tab tab1 = mTabModel.addTab(/* id= */ 1);
        TabClosureParams params = TabClosureParams.closeTabs(List.of(tab0, tab1)).build();
        PassthroughTabRemover remover =
                new PassthroughTabRemover(
                        () -> mTabModel,
                        (promptedParams, tab, onProceed, onCancel) -> {
                            // window.close() and closures started elsewhere can take a tab out of
                            // the model while an earlier tab is still being asked.
                            if (tab == tab0) mTabModel.removeTab(tab1);
                            onProceed.run();
                            return true;
                        });

        remover.prepareCloseTabs(params, /* allowDialog= */ true, mListener, mTabClosureCallback);

        verify(mTabClosureCallback).onResult(mParamsCaptor.capture());
        assertEquals(List.of(tab0), mParamsCaptor.getValue().tabs);
    }

    @Test
    public void testPrepareCloseTabs_TabClosePrompter_NotAskedWhenNativePageAsks() {
        Tab tab0 = mTabModel.addTab(/* id= */ 0);
        BeforeUnloadCallback callback =
                (onProceed, onCancel) -> {
                    onProceed.run();
                    return true;
                };
        tab0.getUserDataHost().setUserData(BeforeUnloadCallback.class, callback);
        TabClosureParams params = TabClosureParams.closeTab(tab0).build();
        List<Tab> promptedTabs = new ArrayList<>();
        PassthroughTabRemover remover =
                new PassthroughTabRemover(
                        () -> mTabModel,
                        (promptedParams, tab, onProceed, onCancel) -> {
                            promptedTabs.add(tab);
                            onProceed.run();
                            return true;
                        });

        remover.prepareCloseTabs(params, /* allowDialog= */ true, mListener, mTabClosureCallback);

        // One prompt per tab: the native page already asked, and it agreed.
        assertTrue(promptedTabs.isEmpty());
        verify(mTabClosureCallback).onResult(params);
    }

    @Test
    public void testPrepareCloseTabs_TabClosePrompter_NothingToAsk() {
        Tab tab0 = mTabModel.addTab(/* id= */ 0);
        TabClosureParams params = TabClosureParams.closeTab(tab0).build();
        PassthroughTabRemover remover =
                new PassthroughTabRemover(
                        () -> mTabModel, (promptedParams, tab, onProceed, onCancel) -> false);

        remover.prepareCloseTabs(params, /* allowDialog= */ true, mListener, mTabClosureCallback);

        // A tab no mechanism claims agrees by default.
        verify(mTabClosureCallback).onResult(params);
    }

    // No mechanism is registered, so the walk iterates rather than recursing. This covers the
    // unprompted bulk path only; the nesting that recursion would cause needs a mechanism that
    // answers synchronously, which the walk's own asserts reject.
    @Test
    public void testPrepareCloseTabs_BulkTabs_NoPromptWalksEveryTab() {
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < 150; i++) {
            tabs.add(mTabModel.addTab(i));
        }
        TabClosureParams params = TabClosureParams.closeTabs(tabs).build();

        mPassthroughTabRemover.prepareCloseTabs(
                params, /* allowDialog= */ true, mListener, mTabClosureCallback);
        verify(mTabClosureCallback).onResult(params);
    }
}
