//// Copyright 2019 The Chromium Authors. All rights reserved.
//// Use of this source code is governed by a BSD-style license that can be
//// found in the LICENSE file.
//
//package com.ark.browser.tab;
//
//import androidx.annotation.Nullable;
//
//import com.ark.browser.ArkWindowAndroid;
//import com.ark.browser.tab.core.ITab;
//
//import org.chromium.base.Callback;
//import org.chromium.chrome.browser.tab.Tab;
//import org.chromium.chrome.browser.tab.TabCreationState;
//import org.chromium.chrome.browser.tab.TabDelegateFactory;
//import org.chromium.chrome.browser.tab.TabLaunchType;
//import org.chromium.chrome.browser.tab.TabResolver;
//import org.chromium.chrome.browser.tab.TabState;
//import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
//import org.chromium.chrome.browser.tab.state.SerializedCriticalPersistedTabData;
//import org.chromium.content_public.browser.LoadUrlParams;
//import org.chromium.content_public.browser.WebContents;
//import org.chromium.ui.base.WindowAndroid;
//
///**
// * Builds {@link Tab} using builder pattern. All Tab classes should be instantiated
// * through this builder.
// */
//public class ArkTabBuilder {
//
//    private Tab mParent;
//    private Integer mLaunchType;
//
//    private Callback<Tab> mPreInitializeAction;
//
//    private final ITab mTab;
//
//    public ArkTabBuilder(ITab tab) {
//        mTab = tab;
//    }
//
//    /**
//     * Sets the tab from which the new one is opened.
//     * @param parent The parent Tab.
//     * @return {@link ArkTabBuilder} creating the Tab.
//     */
//    public ArkTabBuilder setParent(Tab parent) {
//        mParent = parent;
//        return this;
//    }
//
//    /**
//     * Sets a flag indicating how this tab is launched (from a link, external app, etc).
//     * @param type Launch type.
//     * @return {@link ArkTabBuilder} creating the Tab.
//     */
//    public ArkTabBuilder setLaunchType(@TabLaunchType int type) {
//        mLaunchType = type;
//        return this;
//    }
//
//    /**
//     * Sets a pre-initialization action to run.
//     * @param action {@link Callback} object to invoke before {@link #initialize()}.
//     * @return {@link ArkTabBuilder} creating the Tab.
//     */
//    public ArkTabBuilder setPreInitializeAction(Callback<Tab> action) {
//        mPreInitializeAction = action;
//        return this;
//    }
//
//    public ArkTabImpl build() {
//        // Pre-condition check
////        if (mCreationType != null) {
////            if (!mFromFrozenState) {
////                assert mCreationType != TabCreationState.FROZEN_ON_RESTORE;
////            } else {
////                assert mLaunchType == TabLaunchType.FROM_RESTORE
////                        && mCreationType == TabCreationState.FROZEN_ON_RESTORE;
////            }
////        } else {
////            if (mFromFrozenState) assert mLaunchType == TabLaunchType.FROM_RESTORE;
////        }
//
//        if (mLaunchType == null) {
//            mLaunchType = TabLaunchType.FROM_CHROME_UI;
//        }
//        mTab.getTabInfo().setLaunchType(mLaunchType);
//        ArkTabImpl tab = new ArkTabImpl(mTab);
//        Tab parent = null;
//        if (mParent != null) {
//            parent = mParent;
//        }
//
//        if (mPreInitializeAction != null) mPreInitializeAction.onResult(tab);
//
//        // Initializes Tab. Its user data objects are also initialized through the event
//        // |onInitialized| of TabObserver they register.
//        tab.initialize(parent);
//        return tab;
//    }
//
////    private ArkTabBuilder setCreationType(@TabCreationState int type) {
////        mCreationType = type;
////        return this;
////    }
//
////    private ArkTabBuilder setFromFrozenState(boolean frozenState) {
////        mFromFrozenState = frozenState;
////        return this;
////    }
////
////    /**
////     * Creates a TabBuilder for a new, "frozen" tab from a saved state. This can be used for
////     * background tabs restored on cold start that should be loaded when switched to. initialize()
////     * needs to be called afterwards to complete the second level initialization.
////     */
////    public static ArkTabBuilder createFromFrozenState(ITab tab) {
////        return new ArkTabBuilder(tab)
////                .setLaunchType(TabLaunchType.FROM_RESTORE)
////                .setCreationType(TabCreationState.FROZEN_ON_RESTORE)
////                .setFromFrozenState(true);
////    }
////
////    /**
////     * Creates a TabBuilder for a new tab to be loaded lazily. This can be used for tabs opened
////     * in the background that should be loaded when switched to. initialize() needs to be called
////     * afterwards to complete the second level initialization.
////     * @param loadUrlParams Params specifying the conditions for loading url.
////     */
////    public static ArkTabBuilder createForLazyLoad(LoadUrlParams loadUrlParams) {
////        return new ArkTabBuilder()
////                .setLoadUrlParams(loadUrlParams)
////                .setCreationType(TabCreationState.FROZEN_FOR_LAZY_LOAD);
////    }
//
//    /**
//     * Creates a TabBuilder for a fresh tab. initialize() needs to be called afterwards to
//     * complete the second level initialization.
//     * @param initiallyHidden true iff the tab being created is initially in background
//     */
//    public static ArkTabBuilder createLiveTab(ITab tab, boolean initiallyHidden) {
//        return new ArkTabBuilder(tab);
//    }
//}
