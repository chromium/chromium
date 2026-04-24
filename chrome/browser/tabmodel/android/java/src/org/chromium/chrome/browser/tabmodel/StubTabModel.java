// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;

import java.util.List;

/**
 * A TabModel implementation that stubs out all mutating operations and throws exceptions. Used to
 * enforce that a window only uses one type of profile (regular or incognito).
 */
@NullMarked
public class StubTabModel extends EmptyTabModel {
    private final boolean mIsIncognito;
    private final String mMessage;
    private final @Nullable Profile mProfile;

    public StubTabModel(boolean isIncognito, @Nullable Profile profile) {
        super();
        mIsIncognito = isIncognito;
        mProfile = profile;
        mMessage =
                isIncognito
                        ? "Cannot use Incognito tabs in a Regular-only window."
                        : "Cannot use Regular tabs in an Incognito-only window.";
    }

    private UnsupportedOperationException error() {
        return new UnsupportedOperationException(mMessage);
    }

    @Override
    public @Nullable Profile getProfile() {
        return mProfile;
    }

    @Override
    public boolean isIncognito() {
        return mIsIncognito;
    }

    @Override
    public boolean isOffTheRecord() {
        return mIsIncognito;
    }

    @Override
    public boolean isIncognitoBranded() {
        return mIsIncognito;
    }

    // Mutating methods that must throw to enforce isolation

    @Override
    public void setIndex(int i, @TabSelectionType int type) {
        throw error();
    }

    @Override
    public void moveTab(int id, int newIndex) {
        throw error();
    }

    @Override
    public void pinTab(
            int tabId,
            boolean showUngroupDialog,
            @Nullable TabModelActionListener tabModelActionListener) {
        throw error();
    }

    @Override
    public void unpinTab(int tabId) {
        throw error();
    }

    @Override
    public TabCreator getTabCreator() {
        throw error();
    }

    @Override
    public void addTab(
            Tab tab, int index, @TabLaunchType int type, @TabCreationState int creationState) {
        throw error();
    }

    @Override
    public void removeTab(Tab tab) {
        throw error();
    }

    @Override
    public @Nullable Tab duplicateTab(Tab tab) {
        throw error();
    }

    @Override
    public void moveRelatedTabs(int id, int newIndex) {
        throw error();
    }

    @Override
    public void createSingleTabGroup(Tab tab) {
        throw error();
    }

    @Override
    public void createTabGroupForTabGroupSync(List<Tab> tabs, Token tabGroupId) {
        throw error();
    }

    @Override
    public void mergeTabsToGroup(
            int sourceTabId, int destinationTabId, boolean skipUpdateTabModel) {
        throw error();
    }

    @Override
    public void mergeListOfTabsToGroup(
            List<Tab> tabs, Tab destinationTab, @Nullable Integer indexInGroup, int notify) {
        throw error();
    }

    @Override
    public void moveTabOutOfGroupInDirection(int sourceTabId, boolean trailing) {
        throw error();
    }

    @Override
    public boolean closeTabs(TabClosureParams tabClosureParams) {
        throw error();
    }

    @Override
    public TabRemover getTabRemover() {
        throw error();
    }

    @Override
    public TabUngrouper getTabUngrouper() {
        throw error();
    }

    @Override
    public void openMostRecentlyClosedEntry() {
        throw error();
    }

    @Override
    public void setActive(boolean active) {
        throw error();
    }
}
