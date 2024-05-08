// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;

import java.util.List;

/**
 * An implementation of TabModelObserver that forwards notifications over a JNI bridge
 * to a corresponding native implementation. Objects of this type are created and owned by
 * the native TabModelJniBridge implementation when native observers are added.
 */
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
                .didSelectTab(
                        mNativeTabModelObserverJniBridge,
                        TabModelObserverJniBridge.this,
                        tab,
                        type,
                        lastId);
    }

    @Override
    public final void willCloseTab(Tab tab, boolean didCloseAlone) {
        assert mNativeTabModelObserverJniBridge != 0;
        assert tab.isInitialized();
        TabModelObserverJniBridgeJni.get()
                .willCloseTab(
                        mNativeTabModelObserverJniBridge, TabModelObserverJniBridge.this, tab);
    }

    @Override
    public final void onFinishingTabClosure(Tab tab) {
        assert mNativeTabModelObserverJniBridge != 0;
        TabModelObserverJniBridgeJni.get()
                .onFinishingTabClosure(
                        mNativeTabModelObserverJniBridge,
                        TabModelObserverJniBridge.this,
                        tab.getId(),
                        tab.isIncognito());
    }

    @Override
    public final void onFinishingMultipleTabClosure(List<Tab> tabs, boolean canRestore) {
        assert mNativeTabModelObserverJniBridge != 0;
        TabModelObserverJniBridgeJni.get()
                .onFinishingMultipleTabClosure(
                        mNativeTabModelObserverJniBridge,
                        TabModelObserverJniBridge.this,
                        tabs.toArray(new Tab[0]),
                        canRestore);
    }

    @Override
    public final void willAddTab(Tab tab, @TabLaunchType int type) {
        assert mNativeTabModelObserverJniBridge != 0;
        assert tab.isInitialized();
        TabModelObserverJniBridgeJni.get()
                .willAddTab(
                        mNativeTabModelObserverJniBridge,
                        TabModelObserverJniBridge.this,
                        tab,
                        type);
    }

    @Override
    public final void didAddTab(
            Tab tab,
            @TabLaunchType int type,
            @TabCreationState int creationState,
            boolean markedForSelection) {
        assert mNativeTabModelObserverJniBridge != 0;
        assert tab.isInitialized();
        TabModelObserverJniBridgeJni.get()
                .didAddTab(
                        mNativeTabModelObserverJniBridge,
                        TabModelObserverJniBridge.this,
                        tab,
                        type);
    }

    @Override
    public final void didMoveTab(Tab tab, int newIndex, int curIndex) {
        assert mNativeTabModelObserverJniBridge != 0;
        assert tab.isInitialized();
        TabModelObserverJniBridgeJni.get()
                .didMoveTab(
                        mNativeTabModelObserverJniBridge,
                        TabModelObserverJniBridge.this,
                        tab,
                        newIndex,
                        curIndex);
    }

    @Override
    public final void tabPendingClosure(Tab tab) {
        assert mNativeTabModelObserverJniBridge != 0;
        assert tab.isInitialized();
        TabModelObserverJniBridgeJni.get()
                .tabPendingClosure(
                        mNativeTabModelObserverJniBridge, TabModelObserverJniBridge.this, tab);
    }

    @Override
    public final void tabClosureUndone(Tab tab) {
        assert mNativeTabModelObserverJniBridge != 0;
        assert tab.isInitialized();
        TabModelObserverJniBridgeJni.get()
                .tabClosureUndone(
                        mNativeTabModelObserverJniBridge, TabModelObserverJniBridge.this, tab);
    }

    @Override
    public final void tabClosureCommitted(Tab tab) {
        assert mNativeTabModelObserverJniBridge != 0;
        assert tab.isInitialized();
        TabModelObserverJniBridgeJni.get()
                .tabClosureCommitted(
                        mNativeTabModelObserverJniBridge, TabModelObserverJniBridge.this, tab);
    }

    @Override
    public final void multipleTabsPendingClosure(List<Tab> tabs, boolean isAllTabs) {
        // Convert the List to an array of objects. This makes the corresponding C++ code much
        // easier.
        assert mNativeTabModelObserverJniBridge != 0;
        TabModelObserverJniBridgeJni.get()
                .allTabsPendingClosure(
                        mNativeTabModelObserverJniBridge,
                        TabModelObserverJniBridge.this,
                        tabs.toArray(new Tab[0]));
    }

    @Override
    public final void allTabsClosureCommitted(boolean isIncognito) {
        assert mNativeTabModelObserverJniBridge != 0;
        TabModelObserverJniBridgeJni.get()
                .allTabsClosureCommitted(
                        mNativeTabModelObserverJniBridge, TabModelObserverJniBridge.this);
    }

    @Override
    public final void tabRemoved(Tab tab) {
        assert mNativeTabModelObserverJniBridge != 0;
        assert tab.isInitialized();
        TabModelObserverJniBridgeJni.get()
                .tabRemoved(mNativeTabModelObserverJniBridge, TabModelObserverJniBridge.this, tab);
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
                TabModelObserverJniBridge caller,
                Tab tab,
                int type,
                int lastId);

        void willCloseTab(
                long nativeTabModelObserverJniBridge, TabModelObserverJniBridge caller, Tab tab);

        void onFinishingTabClosure(
                long nativeTabModelObserverJniBridge,
                TabModelObserverJniBridge caller,
                int tabId,
                boolean incognito);

        void onFinishingMultipleTabClosure(
                long nativeTabModelObserverJniBridge,
                TabModelObserverJniBridge caller,
                Tab[] tabs,
                boolean canRestore);

        void willAddTab(
                long nativeTabModelObserverJniBridge,
                TabModelObserverJniBridge caller,
                Tab tab,
                int type);

        void didAddTab(
                long nativeTabModelObserverJniBridge,
                TabModelObserverJniBridge caller,
                Tab tab,
                int type);

        void didMoveTab(
                long nativeTabModelObserverJniBridge,
                TabModelObserverJniBridge caller,
                Tab tab,
                int newIndex,
                int curIndex);

        void tabPendingClosure(
                long nativeTabModelObserverJniBridge, TabModelObserverJniBridge caller, Tab tab);

        void tabClosureUndone(
                long nativeTabModelObserverJniBridge, TabModelObserverJniBridge caller, Tab tab);

        void tabClosureCommitted(
                long nativeTabModelObserverJniBridge, TabModelObserverJniBridge caller, Tab tab);

        void allTabsPendingClosure(
                long nativeTabModelObserverJniBridge, TabModelObserverJniBridge caller, Tab[] tabs);

        void allTabsClosureCommitted(
                long nativeTabModelObserverJniBridge, TabModelObserverJniBridge caller);

        void tabRemoved(
                long nativeTabModelObserverJniBridge, TabModelObserverJniBridge caller, Tab tab);
    }
}
