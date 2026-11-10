// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.util.ArrayMap;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;

import java.util.Arrays;
import java.util.List;
import java.util.Map;

/**
 * An implementation of {@link TabModelObserver} and {@link TabGroupObserver} that forwards
 * notifications over a JNI bridge to a corresponding native implementation. Objects of this type
 * are created and owned by the native TabModelJniBridge implementation when native observers are
 * added.
 */
@NullMarked
class TabModelObserverJniBridge implements TabModelObserver, TabGroupObserver {
    @JniType("::TabModelObserverJniBridge")
    interface NativeTabModelObserverJniBridge extends JniTypeToken {}

    /** Native TabModelObserverJniBridge pointer, set by the constructor. */
    private @Nullable JniRawPtr<NativeTabModelObserverJniBridge> mNativeTabModelObserverJniBridge;

    /** TabModel to which this observer is attached, set by the constructor. */
    private TabModel mTabModel;

    /**
     * Constructor. Only intended to be used by the static create factory function.
     *
     * @param nativeTabModelObserverJniBridge The address of the corresponding native object.
     * @param tabModel The tab model to which the observer bridge will be associated.
     */
    private TabModelObserverJniBridge(
            JniRawPtr<NativeTabModelObserverJniBridge> nativeTabModelObserverJniBridge,
            TabModel tabModel) {
        mNativeTabModelObserverJniBridge = nativeTabModelObserverJniBridge;
        mTabModel = tabModel;
    }

    // TabModelObserver implementation.
    // These simply forward events to the corresponding native implementation.

    @Override
    public final void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
        assert mNativeTabModelObserverJniBridge != null;
        assert tab.isInitialized();
        TabModelObserverJniBridgeJni.get().didSelectTab(mNativeTabModelObserverJniBridge, tab);
    }

    @Override
    public final void willCloseTab(Tab tab, boolean didCloseAlone) {
        assert mNativeTabModelObserverJniBridge != null;
        assert tab.isInitialized();
        TabModelObserverJniBridgeJni.get().willCloseTab(mNativeTabModelObserverJniBridge, tab);
    }

    @Override
    public void willCloseTabs(List<Tab> tabs, boolean isAllTabs, boolean allowUndo) {
        assert mNativeTabModelObserverJniBridge != null;
        TabModelObserverJniBridgeJni.get()
                .willCloseTabs(mNativeTabModelObserverJniBridge, tabs, isAllTabs, allowUndo);
    }

    @Override
    public final void didRemoveTabForClosure(Tab tab) {
        assert mNativeTabModelObserverJniBridge != null;
        assert tab.isInitialized();
        TabModelObserverJniBridgeJni.get()
                .didRemoveTabForClosure(mNativeTabModelObserverJniBridge, tab);
    }

    @Override
    public final void onFinishingTabClosure(Tab tab, @TabClosingSource int source) {
        assert mNativeTabModelObserverJniBridge != null;
        TabModelObserverJniBridgeJni.get()
                .onFinishingTabClosure(mNativeTabModelObserverJniBridge, tab, source);
    }

    @Override
    public final void onFinishingMultipleTabClosure(List<Tab> tabs, boolean canRestore) {
        assert mNativeTabModelObserverJniBridge != null;
        TabModelObserverJniBridgeJni.get()
                .onFinishingMultipleTabClosure(mNativeTabModelObserverJniBridge, tabs, canRestore);
    }

    @Override
    public void onTabCloseCommitted(
            List<Tab> tabs,
            boolean isAllTabs,
            boolean canRestore,
            @TabClosingSource int closingSource) {
        assert mNativeTabModelObserverJniBridge != null;
        TabModelObserverJniBridgeJni.get()
                .onTabCloseCommitted(
                        mNativeTabModelObserverJniBridge,
                        tabs,
                        isAllTabs,
                        canRestore,
                        closingSource);
    }

    @Override
    public final void didAddTab(
            Tab tab,
            @TabLaunchType int type,
            @TabCreationState int creationState,
            boolean markedForSelection) {
        assert mNativeTabModelObserverJniBridge != null;
        assert tab.isInitialized();
        int index = mTabModel.indexOf(tab);
        TabModelObserverJniBridgeJni.get()
                .didAddTab(mNativeTabModelObserverJniBridge, tab, type, index);
    }

    @Override
    public final void didMoveTab(Tab tab, int newIndex, int curIndex) {
        assert mNativeTabModelObserverJniBridge != null;
        assert tab.isInitialized();
        TabModelObserverJniBridgeJni.get()
                .didMoveTab(mNativeTabModelObserverJniBridge, tab, newIndex, curIndex);
    }

    @Override
    public final void tabClosureUndone(Tab tab) {
        assert mNativeTabModelObserverJniBridge != null;
        assert tab.isInitialized();
        int index = mTabModel.indexOf(tab);
        TabModelObserverJniBridgeJni.get()
                .tabClosureUndone(mNativeTabModelObserverJniBridge, tab, index);
    }

    @Override
    public final void tabClosureCommitted(Tab tab) {
        assert mNativeTabModelObserverJniBridge != null;
        assert tab.isInitialized();
        TabModelObserverJniBridgeJni.get()
                .tabClosureCommitted(mNativeTabModelObserverJniBridge, tab);
    }

    @Override
    public final void onTabClosePending(
            List<Tab> tabs, boolean isAllTabs, @TabClosingSource int closingSource) {
        assert mNativeTabModelObserverJniBridge != null;
        TabModelObserverJniBridgeJni.get()
                .onTabClosePending(mNativeTabModelObserverJniBridge, tabs);
    }

    @Override
    public void onTabCloseUndone(List<Tab> tabs, boolean isAllTabs) {
        assert mNativeTabModelObserverJniBridge != null;
        int[] indices = new int[tabs.size()];
        Arrays.fill(indices, TabList.INVALID_TAB_INDEX);

        Map<Tab, Integer> tabToTargetIndex = new ArrayMap<>(tabs.size());
        for (int i = 0; i < tabs.size(); i++) {
            tabToTargetIndex.put(tabs.get(i), i);
        }

        int i = 0;
        for (Tab tab : mTabModel) {
            Integer targetIdx = tabToTargetIndex.remove(tab);
            if (targetIdx != null) {
                indices[targetIdx] = i;
                if (tabToTargetIndex.isEmpty()) {
                    break;
                }
            }
            i++;
        }

        TabModelObserverJniBridgeJni.get()
                .onTabCloseUndone(mNativeTabModelObserverJniBridge, tabs, indices);
    }

    @Override
    public void onTabsSelectionChanged() {
        assert mNativeTabModelObserverJniBridge != null;
        TabModelObserverJniBridgeJni.get().onTabsSelectionChanged(mNativeTabModelObserverJniBridge);
    }

    @Override
    public void onWillActiveStateChange(TabModel tabModel, boolean active) {
        assert mNativeTabModelObserverJniBridge != null;
        TabModelObserverJniBridgeJni.get()
                .onWillActiveStateChange(mNativeTabModelObserverJniBridge, tabModel, active);
    }

    @Override
    public void onDidActiveStateChange(TabModel tabModel, boolean active) {
        assert mNativeTabModelObserverJniBridge != null;
        TabModelObserverJniBridgeJni.get()
                .onDidActiveStateChange(mNativeTabModelObserverJniBridge, tabModel, active);
    }

    @Override
    public final void allTabsClosureCommitted(boolean isIncognito) {
        assert mNativeTabModelObserverJniBridge != null;
        TabModelObserverJniBridgeJni.get()
                .allTabsClosureCommitted(mNativeTabModelObserverJniBridge);
    }

    @Override
    public final void allTabsAreClosing() {
        assert mNativeTabModelObserverJniBridge != null;
        TabModelObserverJniBridgeJni.get().allTabsAreClosing(mNativeTabModelObserverJniBridge);
    }

    @Override
    public final void tabRemoved(Tab tab) {
        assert mNativeTabModelObserverJniBridge != null;
        assert tab.isInitialized();
        TabModelObserverJniBridgeJni.get().tabRemoved(mNativeTabModelObserverJniBridge, tab);
    }

    // TabGroupObserver implementation.

    @Override
    public final void onTabGroupCreated(Token tabGroupId) {
        assert mNativeTabModelObserverJniBridge != null;
        TabModelObserverJniBridgeJni.get()
                .onTabGroupCreated(mNativeTabModelObserverJniBridge, tabGroupId);
    }

    @Override
    public final void onTabGroupRemoving(Token tabGroupId) {
        assert mNativeTabModelObserverJniBridge != null;
        TabModelObserverJniBridgeJni.get()
                .onTabGroupRemoving(mNativeTabModelObserverJniBridge, tabGroupId);
    }

    @Override
    public final void onTabGroupMoved(
            Token tabGroupId, int tabModelOldIndex, int tabModelNewIndex) {
        assert mNativeTabModelObserverJniBridge != null;
        TabModelObserverJniBridgeJni.get()
                .onTabGroupMoved(mNativeTabModelObserverJniBridge, tabGroupId);
    }

    @Override
    public final void onTabGroupVisualsChanged(Token tabGroupId) {
        assert mNativeTabModelObserverJniBridge != null;
        TabModelObserverJniBridgeJni.get()
                .onTabGroupVisualsChanged(mNativeTabModelObserverJniBridge, tabGroupId);
    }

    /**
     * Creates an observer bridge for the given tab model. The native counterpart to this object
     * will hold a global reference to the Java endpoint and manage its lifetime.
     *
     * @param nativeTabModelObserverJniBridge The address of the corresponding native object.
     * @param tabModel The tab model to which the observer bridge will be associated.
     */
    @CalledByNative
    static TabModelObserverJniBridge create(
            JniRawPtr<NativeTabModelObserverJniBridge> nativeTabModelObserverJniBridge,
            TabModel tabModel) {
        TabModelObserverJniBridge bridge =
                new TabModelObserverJniBridge(nativeTabModelObserverJniBridge, tabModel);
        tabModel.addObserver(bridge);
        tabModel.addTabGroupObserver(bridge);
        return bridge;
    }

    /**
     * Causes the observer to be removed from the associated tab model. The native counterpart calls
     * this prior to cleaning up its last reference to the Java endpoint so that it can be correctly
     * torn down.
     */
    @SuppressWarnings("NullAway")
    @CalledByNative
    void detachFromTabModel() {
        assert mNativeTabModelObserverJniBridge != null;
        assert mTabModel != null;
        mTabModel.removeObserver(this);
        mTabModel.removeTabGroupObserver(this);
        mNativeTabModelObserverJniBridge.release();
        mNativeTabModelObserverJniBridge = null;
        mTabModel = null;
    }

    // Native functions that are implemented by
    // browser/ui/android/tab_model/tab_model_observer_jni_bridge.*.
    @NativeMethods
    interface Natives {
        void didSelectTab(
                JniPtr<NativeTabModelObserverJniBridge> self, @JniType("TabAndroid*") Tab tab);

        void willCloseTabs(
                JniPtr<NativeTabModelObserverJniBridge> self,
                @JniType("std::vector<TabAndroid*>") List<Tab> tabs,
                boolean isAllTabs,
                boolean allowUndo);

        void willCloseTab(
                JniPtr<NativeTabModelObserverJniBridge> self, @JniType("TabAndroid*") Tab tab);

        void didRemoveTabForClosure(
                JniPtr<NativeTabModelObserverJniBridge> self, @JniType("TabAndroid*") Tab tab);

        void onFinishingTabClosure(
                JniPtr<NativeTabModelObserverJniBridge> self,
                @JniType("TabAndroid*") Tab tab,
                @TabClosingSource int source);

        void onFinishingMultipleTabClosure(
                JniPtr<NativeTabModelObserverJniBridge> self,
                @JniType("std::vector<TabAndroid*>") List<Tab> tabs,
                boolean canRestore);

        void onTabCloseCommitted(
                JniPtr<NativeTabModelObserverJniBridge> self,
                @JniType("std::vector<TabAndroid*>") List<Tab> tabs,
                boolean isAllTabs,
                boolean canRestore,
                @TabClosingSource int closingSource);

        void didAddTab(
                JniPtr<NativeTabModelObserverJniBridge> self,
                @JniType("TabAndroid*") Tab tab,
                int type,
                int index);

        void didMoveTab(
                JniPtr<NativeTabModelObserverJniBridge> self,
                @JniType("TabAndroid*") Tab tab,
                int newIndex,
                int curIndex);

        void tabClosureUndone(
                JniPtr<NativeTabModelObserverJniBridge> self,
                @JniType("TabAndroid*") Tab tab,
                int index);

        void onTabCloseUndone(
                JniPtr<NativeTabModelObserverJniBridge> self,
                @JniType("std::vector<TabAndroid*>") List<Tab> tabs,
                @JniType("std::vector<int32_t>") int[] indices);

        void onTabsSelectionChanged(JniPtr<NativeTabModelObserverJniBridge> self);

        void tabClosureCommitted(
                JniPtr<NativeTabModelObserverJniBridge> self, @JniType("TabAndroid*") Tab tab);

        void onTabClosePending(
                JniPtr<NativeTabModelObserverJniBridge> self,
                @JniType("std::vector<TabAndroid*>") List<Tab> tabs);

        void allTabsClosureCommitted(JniPtr<NativeTabModelObserverJniBridge> self);

        void allTabsAreClosing(JniPtr<NativeTabModelObserverJniBridge> self);

        void tabRemoved(
                JniPtr<NativeTabModelObserverJniBridge> self, @JniType("TabAndroid*") Tab tab);

        void onTabGroupCreated(
                JniPtr<NativeTabModelObserverJniBridge> self,
                @JniType("base::Token") Token groupId);

        void onTabGroupRemoving(
                JniPtr<NativeTabModelObserverJniBridge> self,
                @JniType("base::Token") Token groupId);

        void onTabGroupMoved(
                JniPtr<NativeTabModelObserverJniBridge> self,
                @JniType("base::Token") Token groupId);

        void onTabGroupVisualsChanged(
                JniPtr<NativeTabModelObserverJniBridge> self,
                @JniType("base::Token") Token groupId);

        void onWillActiveStateChange(
                JniPtr<NativeTabModelObserverJniBridge> self,
                @JniType("TabModel*") TabModel tabModel,
                boolean active);

        void onDidActiveStateChange(
                JniPtr<NativeTabModelObserverJniBridge> self,
                @JniType("TabModel*") TabModel tabModel,
                boolean active);
    }
}
