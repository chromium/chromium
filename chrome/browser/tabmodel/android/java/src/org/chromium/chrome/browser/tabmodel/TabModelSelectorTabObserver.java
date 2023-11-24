// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;

import java.util.ArrayList;
import java.util.List;

/** Observer of tab changes for all tabs owned by a {@link TabModelSelector}. */
public class TabModelSelectorTabObserver extends EmptyTabObserver {
    private final TabModelSelectorTabRegistrationObserver mTabRegistrationObserver;
    private boolean mShouldDeferTabRegisterNotifications;
    private List<Tab> mDeferredTabs = new ArrayList<>();
    private boolean mIsDestroyed;
    private boolean mIsDeferredInitializationFinished;

    /**
     * Constructs an observer that should be notified of tab changes for all tabs owned
     * by a specified {@link TabModelSelector}.  Any Tabs created after this call will be
     * observed as well, and Tabs removed will no longer have their information broadcast.
     *
     * <p>
     * {@link #destroy()} must be called to unregister this observer.
     *
     * @param selector The selector that owns the Tabs that should notify this observer.
     */
    public TabModelSelectorTabObserver(TabModelSelector selector) {
        mTabRegistrationObserver = new TabModelSelectorTabRegistrationObserver(selector);
        mShouldDeferTabRegisterNotifications = true;
        mTabRegistrationObserver.addObserverAndNotifyExistingTabRegistration(
                createRegistrationObserver());
        mShouldDeferTabRegisterNotifications = false;
        // Run |onTabRegistered| asynchronously so it is done after the tasks in the
        // constructor of the inherited classes are completed and the relevant local
        // variables are ready.
        // TODO(jinsukkim): Consider making this class final, and introducing an inner
        //     class that extends EmptyTabObserver + provides onTab[Un]Registered instead.
        ThreadUtils.getUiThreadHandler()
                .postAtFrontOfQueue(
                        () -> {
                            assert !mShouldDeferTabRegisterNotifications;
                            for (Tab tab : mDeferredTabs) {
                                if (tab.isDestroyed()) continue;
                                onTabRegistered(tab);
                            }
                            mDeferredTabs.clear();
                            mIsDeferredInitializationFinished = true;
                        });
    }

    private TabModelSelectorTabRegistrationObserver.Observer createRegistrationObserver() {
        return new TabModelSelectorTabRegistrationObserver.Observer() {
            @Override
            public void onTabRegistered(Tab tab) {
                if (tab.isDestroyed()) return;
                tab.addObserver(TabModelSelectorTabObserver.this);
                if (mShouldDeferTabRegisterNotifications) {
                    mDeferredTabs.add(tab);
                } else {
                    TabModelSelectorTabObserver.this.onTabRegistered(tab);
                }
            }

            @Override
            public void onTabUnregistered(Tab tab) {
                if (mShouldDeferTabRegisterNotifications) {
                    boolean didExist = mDeferredTabs.remove(tab);
                    assert didExist
                            : "Attempting to remove a tab during deferred registration that "
                                    + "never was added";
                    return;
                }

                if (mIsDestroyed) {
                    performUnregister(tab);
                } else {
                    // Post the removal of the observer so that other tab events are
                    // notified before removing the tab observer (e.g. detach tab from
                    // activity).
                    PostTask.postTask(TaskTraits.UI_DEFAULT, () -> performUnregister(tab));
                }
                TabModelSelectorTabObserver.this.onTabUnregistered(tab);
            }

            private void performUnregister(Tab tab) {
                // If the tab as been destroyed we cannot access PersistedTabData.
                if (tab.isDestroyed()) return;
                tab.removeObserver(TabModelSelectorTabObserver.this);
            }
        };
    }

    /**
     * Called when a tab is registered to a tab model this selector is managing.
     * @param tab The registered Tab.
     */
    protected void onTabRegistered(Tab tab) {}

    /**
     * Called when a tab is unregistered from a tab model this selector is managing.
     * @param tab The unregistered Tab.
     */
    protected void onTabUnregistered(Tab tab) {}

    /** Destroys the observer and removes itself as a listener for Tab updates. */
    public void destroy() {
        mIsDestroyed = true;
        mTabRegistrationObserver.destroy();
    }

    boolean isDeferredInitializationFinishedForTesting() {
        return mIsDeferredInitializationFinished;
    }
}
