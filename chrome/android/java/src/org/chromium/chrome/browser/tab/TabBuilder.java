// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Builds {@link Tab} using builder pattern. All Tab classes should be instantiated
 * through this builder.
 */
public class TabBuilder {
    private final Profile mProfile;

    private int mId = Tab.INVALID_TAB_ID;
    private Tab mParent;
    private TabResolver mTabResolver;
    private WindowAndroid mWindow;
    // Should not be null when build() is called.
    private @Nullable @TabLaunchType Integer mLaunchType;
    private @TabCreationState Integer mCreationType;
    private boolean mFromFrozenState;
    private LoadUrlParams mLoadUrlParams;
    private String mTitle;

    private WebContents mWebContents;
    private TabDelegateFactory mDelegateFactory;
    private boolean mInitiallyHidden;
    private boolean mInitializeRenderer;
    private TabState mTabState;
    private Callback<Tab> mPreInitializeAction;

    public TabBuilder(Profile profile) {
        mProfile = profile;
    }

    /**
     * Sets the id with which the Tab to create should be identified.
     * @param id The id of the Tab.
     * @return {@link TabBuilder} creating the Tab.
     */
    public TabBuilder setId(int id) {
        mId = id;
        return this;
    }

    /**
     * Sets the tab from which the new one is opened.
     * @param parent The parent Tab.
     * @return {@link TabBuilder} creating the Tab.
     */
    public TabBuilder setParent(Tab parent) {
        mParent = parent;
        return this;
    }

    /**
     * Sets the tab resolver (tab id -> {@link Tab} mapping)
     * @param tabResolver the {@link TabResolver}
     * @return {@link TabBuilder} creating the Tab.
     */
    public TabBuilder setTabResolver(TabResolver tabResolver) {
        mTabResolver = tabResolver;
        return this;
    }

    /**
     * Sets window which the Tab will be attached to.
     * @param window An instance of a {@link WindowAndroid}.
     * @return {@link TabBuilder} creating the Tab.
     */
    public TabBuilder setWindow(WindowAndroid window) {
        mWindow = window;
        return this;
    }

    /**
     * Sets a flag indicating how this tab is launched (from a link, external app, etc).
     * @param type Launch type.
     * @return {@link TabBuilder} creating the Tab.
     */
    public TabBuilder setLaunchType(@TabLaunchType int type) {
        mLaunchType = type;
        return this;
    }

    /**
     * Sets a flag indicating to initialize renderer during WebContents creation.
     *
     * @return {@link TabBuilder} creating the Tab.
     */
    public TabBuilder setInitializeRenderer(boolean initializeRenderer) {
        mInitializeRenderer = initializeRenderer;
        return this;
    }

    /**
     * Sets a {@link WebContents} object to be used on the Tab. If not set, a new one
     * will be created.
     * @param webContents {@link WebContents} object.
     * @return {@link TabBuilder} creating the Tab.
     */
    public TabBuilder setWebContents(WebContents webContents) {
        mWebContents = webContents;
        return this;
    }

    /**
     * Sets a {@link TabDelegateFactory} object.
     * @param delegateFactory The factory delegated to create various Tab-related objects.
     * @return {@link TabBuilder} creating the Tab.
     */
    public TabBuilder setDelegateFactory(TabDelegateFactory delegateFactory) {
        mDelegateFactory = delegateFactory;
        return this;
    }

    /**
     * Sets a pre-initialization action to run.
     * @param action {@link Callback} object to invoke before {@link #initialize()}.
     * @return {@link TabBuilder} creating the Tab.
     */
    public TabBuilder setPreInitializeAction(Callback<Tab> action) {
        mPreInitializeAction = action;
        return this;
    }

    /**
     * Sets a flag indicating whether the Tab should start as hidden. Only used if
     * {@code webContents} is {@code null}.
     * @param initiallyHidden {@code true} if the newly created {@link WebContents} will be hidden.
     * @return {@link TabBuilder} creating the Tab.
     */
    public TabBuilder setInitiallyHidden(boolean initiallyHidden) {
        mInitiallyHidden = initiallyHidden;
        return this;
    }

    /**
     * Sets a {@link TabState} object containing information about this Tab, if it was persisted.
     * @param tabState State object.
     * @return {@link TabBuilder} creating the Tab.
     */
    public TabBuilder setTabState(TabState tabState) {
        mTabState = tabState;
        return this;
    }

    public Tab build() {
        assert mLaunchType != null : "TabBuilder#setLaunchType() must be called.";

        // Pre-condition check
        if (mCreationType != null) {
            if (!mFromFrozenState) {
                assert mCreationType != TabCreationState.FROZEN_ON_RESTORE;
            } else {
                assert mLaunchType == TabLaunchType.FROM_RESTORE
                        && mCreationType == TabCreationState.FROZEN_ON_RESTORE;
            }
        } else {
            if (mFromFrozenState) assert mLaunchType == TabLaunchType.FROM_RESTORE;
        }

        TabImpl tab = new TabImpl(mId, mProfile, mLaunchType);
        Tab parent = null;
        if (mParent != null) {
            parent = mParent;
        } else if (mTabResolver != null) {
            if (mTabState != null) {
                parent = mTabResolver.resolve(mTabState.parentId);
            }
        }
        tab.updateWindowAndroid(mWindow);

        if (parent != null && mDelegateFactory == null) {
            mDelegateFactory = ((TabImpl) parent).getDelegateFactory();
        }

        if (mPreInitializeAction != null) mPreInitializeAction.onResult(tab);

        // Initializes Tab. Its user data objects are also initialized through the event
        // |onInitialized| of TabObserver they register.
        tab.initialize(
                parent,
                mCreationType,
                mLoadUrlParams,
                mTitle,
                mWebContents,
                mDelegateFactory,
                mInitiallyHidden,
                mTabState,
                mInitializeRenderer);
        return tab;
    }

    private TabBuilder setCreationType(@TabCreationState int type) {
        mCreationType = type;
        return this;
    }

    private TabBuilder setFromFrozenState(boolean frozenState) {
        mFromFrozenState = frozenState;
        return this;
    }

    private TabBuilder setLoadUrlParams(LoadUrlParams loadUrlParams) {
        mLoadUrlParams = loadUrlParams;
        return this;
    }

    private TabBuilder setTitle(String title) {
        mTitle = title;
        return this;
    }

    /**
     * Creates a TabBuilder for a new, "frozen" tab from a saved state. This can be used for
     * background tabs restored on cold start that should be loaded when switched to. initialize()
     * needs to be called afterwards to complete the second level initialization.
     *
     * @param profile The Profile associated with the Tab.
     */
    public static TabBuilder createFromFrozenState(Profile profile) {
        return new TabBuilder(profile)
                .setLaunchType(TabLaunchType.FROM_RESTORE)
                .setCreationType(TabCreationState.FROZEN_ON_RESTORE)
                .setFromFrozenState(true);
    }

    /**
     * Creates a TabBuilder for a new tab to be loaded lazily. This can be used for tabs opened in
     * the background that should be loaded when switched to. initialize() needs to be called
     * afterwards to complete the second level initialization.
     *
     * @param profile The Profile associated with the Tab.
     * @param loadUrlParams Params specifying the conditions for loading url.
     * @param title The title to use for the load.
     */
    public static TabBuilder createForLazyLoad(
            Profile profile, LoadUrlParams loadUrlParams, @Nullable String title) {
        return new TabBuilder(profile)
                .setLoadUrlParams(loadUrlParams)
                .setTitle(title)
                .setCreationType(TabCreationState.FROZEN_FOR_LAZY_LOAD);
    }

    /**
     * Creates a TabBuilder for a tab from a web contents with no renderer. initialize() needs to be
     * called afterwards to complete the second level initialization.
     *
     * @param profile The Profile associated with the Tab.
     */
    public static TabBuilder createLazyTabWithWebContents(Profile profile) {
        return new TabBuilder(profile).setCreationType(TabCreationState.FROZEN_FOR_LAZY_LOAD);
    }

    /**
     * Creates a TabBuilder for a fresh tab. initialize() needs to be called afterwards to complete
     * the second level initialization.
     *
     * @param profile The Profile associated with the Tab.
     * @param initiallyHidden true iff the tab being created is initially in background
     */
    public static TabBuilder createLiveTab(Profile profile, boolean initiallyHidden) {
        return new TabBuilder(profile)
                .setCreationType(
                        initiallyHidden
                                ? TabCreationState.LIVE_IN_BACKGROUND
                                : TabCreationState.LIVE_IN_FOREGROUND);
    }
}
