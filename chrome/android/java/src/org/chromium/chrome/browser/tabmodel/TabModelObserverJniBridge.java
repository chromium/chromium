// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabClosingSource;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;

import java.util.List;

/**
 * An implementation of TabModelObserver that forwards notifications over a JNI bridge to a
 * corresponding native implementation. Objects of this type are created and owned by the native
 * TabModelJniBridge implementation when native observers are added.
 */
@NullMarked
class TabModelObserverJniBridge implements TabModelObserver {
    /** Native TabModelObserverJniBridge pointer, set by the constructor. */
    private long mNativeTabModelObserverJniBridge;

    /** TabModel to which this observer is attached, set by the constructor. */
    private TabModel mTabModel;

    /** Constructor. Only intended to be used by the static create factory function.
     *
     * @param nativeTabModelObserverJniBridge The address of the corresponding native object.
     * @param tabModel The tab model to which the observer bridge will be associated.
     */
    private TabModelObserverJniBridge(long nativeTabModelObserverJniBridge, TabModel tabModel) {
        mNativeTabModelObserverJniBridge = nativeTabModelObserverJniBridge;
        mTabModel = tabModel;
    }

    // TabModelObserver implementation.
    // These simply forward events to the corresponding native implementation.

    @Override
    public final void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
        assert mNativeTabModelObserverJniBridge != 0;
        assert tab.isInitialized();
        TabModelObserverJniBridgeJni.get()
                .didSelectTab(mNativeTabModelObserverJniBridge, tab, type, lastId);
    }

    @Override
    public final void willCloseTab(Tab tab, boolean didCloseAlone) {
        assert mNativeTabModelObserverJniBridge != 0;
        assert tab.isInitialized();
        TabModelObserverJniBridgeJni.get().willCloseTab(mNativeTabModelObserverJniBridge, tab);
    }

    @Override
    public final void onFinishingTabClosure(Tab tab, @TabClosingSource int source) {
        assert mNativeTabModelObserverJniBridge != 0;
        TabModelObserverJniBridgeJni.get()
                .onFinishingTabClosure(mNativeTabModelObserverJniBridge, tab, source);
    }

    @Override
    public final void onFinishingMultipleTabClosure(List<Tab> tabs, boolean canRestore) {
        assert mNativeTabModelObserverJniBridge != 0;
        TabModelObserverJniBridgeJni.get()
                .onFinishingMultipleTabClosure(
                        mNativeTabModelObserverJniBridge, tabs.toArray(new Tab[0]), canRestore);
    }

    @Override
    public final void willAddTab(Tab tab, @TabLaunchType int type) {
        assert mNativeTabModelObserverJniBridge != 0;
        assert tab.isInitialized();
        TabModelObserverJniBridgeJni.get().willAddTab(mNativeTabModelObserverJniBridge, tab, type);
    }

    @Override
    public final void didAddTab(
            Tab tab,
            @TabLaunchType int type,
            @TabCreationState int creationState,
            boolean markedForSelection) {
        assert mNativeTabModelObserverJniBridge != 0;
        assert tab.isInitialized();
        TabModelObserverJniBridgeJni.get().didAddTab(mNativeTabModelObserverJniBridge, tab, type);
    }

    @Override
    public final void didMoveTab(Tab tab, int newIndex, int curIndex) {
        assert mNativeTabModelObserverJniBridge != 0;
        assert tab.isInitialized();
        TabModelObserverJniBridgeJni.get()
                .didMoveTab(mNativeTabModelObserverJniBridge, tab, newIndex, curIndex);
    }

    @Override
    public final void tabClosureUndone(Tab tab) {
        assert mNativeTabModelObserverJniBridge != 0;
        assert tab.isInitialized();
        TabModelObserverJniBridgeJni.get().tabClosureUndone(mNativeTabModelObserverJniBridge, tab);
    }

    @Override
    public final void tabClosureCommitted(Tab tab) {
        assert mNativeTabModelObserverJniBridge != 0;
        assert tab.isInitialized();
        TabModelObserverJniBridgeJni.get()
                .tabClosureCommitted(mNativeTabModelObserverJniBridge, tab);
    }

    @Override
    public final void onTabClosePending(
            List<Tab> tabs, boolean isAllTabs, @TabClosingSource int closingSource) {
        // Convert the List to an array of objects. This makes the corresponding C++ code much
        // easier.
        assert mNativeTabModelObserverJniBridge != 0;
        TabModelObserverJniBridgeJni.get()
                .onTabClosePending(mNativeTabModelObserverJniBridge, tabs, closingSource);
    }

    @Override
    public void onTabCloseUndone(List<Tab> tabs, boolean isAllTabs) {
        assert mNativeTabModelObserverJniBridge != 0;
        TabModelObserverJniBridgeJni.get()
                .onTabCloseUndone(mNativeTabModelObserverJniBridge, tabs.toArray(new Tab[0]));
    }

    @Override
    public final void allTabsClosureCommitted(boolean isIncognito) {
        assert mNativeTabModelObserverJniBridge != 0;
        TabModelObserverJniBridgeJni.get()
                .allTabsClosureCommitted(mNativeTabModelObserverJniBridge);
    }

    @Override
    public final void tabRemoved(Tab tab) {
        assert mNativeTabModelObserverJniBridge != 0;
        assert tab.isInitialized();
        TabModelObserverJniBridgeJni.get().tabRemoved(mNativeTabModelObserverJniBridge, tab);
    }

    @Override
    public void restoreCompleted() {}

    /**
     * Creates an observer bridge for the given tab model. The native counterpart to this object
     * will hold a global reference to the Java endpoint and manage its lifetime. This is private as
     * it is only intended to be called from native code.
     *
     * @param nativeTabModelObserverJniBridge The address of the corresponding native object.
     * @param tabModel The tab model to which the observer bridge will be associated.
     */
    @CalledByNative
    private static TabModelObserverJniBridge create(
            long nativeTabModelObserverJniBridge, TabModel tabModel) {
        TabModelObserverJniBridge bridge =
                new TabModelObserverJniBridge(nativeTabModelObserverJniBridge, tabModel);
        tabModel.addObserver(bridge);
        return bridge;
    }

    /**
     * Causes the observer to be removed from the associated tab model. The native counterpart calls
     * this prior to cleaning up its last reference to the Java endpoint so that it can be correctly
     * torn down.
     */
    @SuppressWarnings("NullAway")
    @CalledByNative
    private void detachFromTabModel() {
        assert mNativeTabModelObserverJniBridge != 0;
        assert mTabModel != null;
        mTabModel.removeObserver(this);
        mNativeTabModelObserverJniBridge = 0;
        mTabModel = null;
    }

    // Native functions that are implemented by
    // browser/ui/android/tab_model/tab_model_observer_jni_bridge.*.
    @NativeMethods
    interface Natives {
        void didSelectTab(
                long nativeTabModelObserverJniBridge,
                @JniType("TabAndroid*") Tab tab,
                int type,
                int lastId);

        void willCloseTab(long nativeTabModelObserverJniBridge, @JniType("TabAndroid*") Tab tab);

        void onFinishingTabClosure(
                long nativeTabModelObserverJniBridge,
                @JniType("TabAndroid*") Tab tab,
                @TabClosingSource int source);

        void onFinishingMultipleTabClosure(
                long nativeTabModelObserverJniBridge,
                @JniType("std::vector<TabAndroid*>") Tab[] tabs,
                boolean canRestore);

        void willAddTab(
                long nativeTabModelObserverJniBridge, @JniType("TabAndroid*") Tab tab, int type);

        void didAddTab(
                long nativeTabModelObserverJniBridge, @JniType("TabAndroid*") Tab tab, int type);

        void didMoveTab(
                long nativeTabModelObserverJniBridge,
                @JniType("TabAndroid*") Tab tab,
                int newIndex,
                int curIndex);

        void tabClosureUndone(
                long nativeTabModelObserverJniBridge, @JniType("TabAndroid*") Tab tab);

        void onTabCloseUndone(
                long nativeTabModelObserverJniBridge,
                @JniType("std::vector<TabAndroid*>") Tab[] tab);

        void tabClosureCommitted(
                long nativeTabModelObserverJniBridge, @JniType("TabAndroid*") Tab tab);

        void onTabClosePending(
                long nativeTabModelObserverJniBridge,
                @JniType("std::vector<TabAndroid*>") List<Tab> tabs,
                int source);

        void allTabsClosureCommitted(long nativeTabModelObserverJniBridge);

        void tabRemoved(long nativeTabModelObserverJniBridge, @JniType("TabAndroid*") Tab tab);
    }
}
