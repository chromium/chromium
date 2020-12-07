// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import android.content.Context;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.NonOverlappingStack;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;

import java.util.ArrayList;

/**
 * Layout that displays all normal tabs in one stack and all incognito tabs in a second.
 */
public class StackLayout extends StackLayoutBase {
    public static final int NORMAL_STACK_INDEX = 0;
    public static final int INCOGNITO_STACK_INDEX = 1;

    /** Whether the current fling animation is the result of switching stacks. */
    private boolean mFlingFromModelChange;

    /** Disable the incognito button while selecting a tab. */
    private boolean mAnimatingStackSwitch;

    /**
     * @param context                              The current Android's context.
     * @param updateHost                           The {@link LayoutUpdateHost} view for this
     *                                             layout.
     * @param renderHost                           The {@link LayoutRenderHost} view for this
     *                                             layout.
     * @param browserControlsStateProviderSupplier The {@link ObservableSupplier} for the
     *                                             {@link BrowserControlsStateProvider}.
     */
    public StackLayout(Context context, LayoutUpdateHost updateHost, LayoutRenderHost renderHost,
            ObservableSupplier<BrowserControlsStateProvider> browserControlsStateProviderSupplier) {
        super(context, updateHost, renderHost, browserControlsStateProviderSupplier);
    }

    @Override
    protected boolean shouldIgnoreTouchInput() {
        return mAnimatingStackSwitch;
    }

    @Override
    public void setTabModelSelector(TabModelSelector modelSelector, TabContentManager manager) {
        super.setTabModelSelector(modelSelector, manager);
        if (modelSelector.getTabModelFilterProvider().getCurrentTabModelFilter() == null) {
            // Registers an observer of the TabModel's creation if it hasn't been created yet. Once
            // the TabModel is ready, we will call setTablists() immediately.
            // See https://crbug.com/1142858.
            TabModelSelectorObserver selectorObserver = new EmptyTabModelSelectorObserver() {
                @Override
                public void onChange() {
                    mTabModelSelector.removeObserver(this);
                    setTablists();
                }
            };
            mTabModelSelector.addObserver(selectorObserver);
        } else {
            setTablists();
        }
    }

    private void setTablists() {
        ArrayList<TabList> tabLists = new ArrayList<TabList>();
        tabLists.add(mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(false));
        tabLists.add(mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(true));
        setTabLists(tabLists);
    }

    @Override
    protected int getTabStackIndex(int tabId) {
        if (tabId == Tab.INVALID_TAB_ID) {
            if (mTemporarySelectedStack != INVALID_STACK_INDEX) return mTemporarySelectedStack;

            return mTabModelSelector.isIncognitoSelected() ? INCOGNITO_STACK_INDEX
                                                           : NORMAL_STACK_INDEX;
        } else {
            return TabModelUtils.getTabById(mTabModelSelector.getModel(true), tabId) != null
                    ? INCOGNITO_STACK_INDEX
                    : NORMAL_STACK_INDEX;
        }
    }

    @Override
    public void onTabClosing(long time, int id) {
        super.onTabClosing(time, id);
        // Just in case we closed the last tab of a stack we need to trigger the overlap animation.
        startMarginAnimation(true);
        // Animate the stack to leave incognito mode.
        if (!mStacks.get(INCOGNITO_STACK_INDEX).isDisplayable()) onTabModelSwitched(false);
    }

    @Override
    public void onTabsAllClosing(long time, boolean incognito) {
        super.onTabsAllClosing(time, incognito);
        getTabStackAtIndex(incognito ? INCOGNITO_STACK_INDEX : NORMAL_STACK_INDEX)
                .tabsAllClosingEffect(time);
        // trigger the overlap animation.
        startMarginAnimation(true);
        // Animate the stack to leave incognito mode.
        if (!mStacks.get(INCOGNITO_STACK_INDEX).isDisplayable()) onTabModelSwitched(false);
    }

    @Override
    public void onTabClosureCancelled(long time, int id, boolean incognito) {
        super.onTabClosureCancelled(time, id, incognito);
        getTabStackAtIndex(incognito ? INCOGNITO_STACK_INDEX : NORMAL_STACK_INDEX)
                .undoClosure(time, id);
    }

    @Override
    public void onTabCreated(long time, int id, int tabIndex, int sourceId, boolean newIsIncognito,
            boolean background, float originX, float originY) {
        super.onTabCreated(
                time, id, tabIndex, sourceId, newIsIncognito, background, originX, originY);
        onTabModelSwitched(newIsIncognito);
    }

    @Override
    public void onTabModelSwitched(boolean toIncognitoTabModel) {
        if (isHorizontalTabSwitcherFlagEnabled()) {
            // Don't allow switching between normal and incognito again until the animations finish.
            mAnimatingStackSwitch = true;

            // Make sure we update the tab switcher's background color even if no tabs are open and
            // therefore neither the switch away nor switch to animations run.
            requestUpdate();

            NonOverlappingStack oldStack = (NonOverlappingStack) mStacks.get(
                    toIncognitoTabModel ? NORMAL_STACK_INDEX : INCOGNITO_STACK_INDEX);
            oldStack.runSwitchAwayAnimation(toIncognitoTabModel
                            ? NonOverlappingStack.SwitchDirection.LEFT
                            : NonOverlappingStack.SwitchDirection.RIGHT);
        } else {
            flingStacks(toIncognitoTabModel ? INCOGNITO_STACK_INDEX : NORMAL_STACK_INDEX);
            mFlingFromModelChange = true;
        }
    }

    @Override
    public void onSwitchAwayFinished() {
        int newStackIndex = getTabStackIndex(Tab.INVALID_TAB_ID);
        mRenderedScrollOffset = -newStackIndex;
        NonOverlappingStack newStack = (NonOverlappingStack) mStacks.get(newStackIndex);
        newStack.runSwitchToAnimation(newStackIndex == INCOGNITO_STACK_INDEX
                        ? NonOverlappingStack.SwitchDirection.LEFT
                        : NonOverlappingStack.SwitchDirection.RIGHT);
    }

    @Override
    public void onSwitchToFinished() {
        mAnimatingStackSwitch = false;
    }

    @Override
    protected void onAnimationFinished() {
        super.onAnimationFinished();
        mFlingFromModelChange = false;
        if (mTemporarySelectedStack != INVALID_STACK_INDEX) {
            mTabModelSelector.selectModel(mTemporarySelectedStack == INCOGNITO_STACK_INDEX);
            mTemporarySelectedStack = INVALID_STACK_INDEX;
        }
    }

    @Override
    protected int getMinRenderedScrollOffset() {
        // If the horizontal tab switcher flag is enabled, we let the user tap the incognito button
        // to switch to incognito mode, even if no incognito tabs are open.
        if (isHorizontalTabSwitcherFlagEnabled()) return -1;

        // If there's at least one incognito tab open, or we're in the process of switching back
        // from incognito to normal mode, return -1 so we don't cause any clamping. Otherwise,
        // return 0 to prevent scrolling.
        if (mStacks.get(INCOGNITO_STACK_INDEX).isDisplayable() || mFlingFromModelChange) return -1;
        return 0;
    }

    @Override
    public void uiRequestingCloseTab(long time, int id) {
        super.uiRequestingCloseTab(time, id);

        int incognitoCount = mTabModelSelector.getModel(true).getCount();
        TabModel model = mTabModelSelector.getModelForTabId(id);
        if (model != null && model.isIncognito()) incognitoCount--;
        boolean incognitoVisible = incognitoCount > 0;

        // Make sure we show/hide both stacks depending on which tab we're closing.
        startMarginAnimation(true, incognitoVisible);
        if (!incognitoVisible) onTabModelSwitched(false);
    }

    @Override
    protected @SwipeMode int computeInputMode(long time, float x, float y, float dx, float dy) {
        // If this experiment flag is enabled, we add an incognito toggle button to the toolbar, and
        // disable swiping between the stacks.
        if (isHorizontalTabSwitcherFlagEnabled()) return SwipeMode.SEND_TO_STACK;

        if (mStacks.size() == 2 && !mStacks.get(1).isDisplayable()) return SwipeMode.SEND_TO_STACK;
        return super.computeInputMode(time, x, y, dx, dy);
    }

    @Override
    public void setActiveStackState(int stackIndex) {
        if (stackIndex != getTabStackIndex(Tab.INVALID_TAB_ID)) {
            if (stackIndex == NORMAL_STACK_INDEX) {
                RecordUserAction.record("MobileStackViewNormalMode");
            } else {
                RecordUserAction.record("MobileStackViewIncognitoMode");
            }
        }

        super.setActiveStackState(stackIndex);
    }

    @Override
    public boolean shouldAllowIncognitoSwitching() {
        return !mAnimatingStackSwitch;
    }
}
