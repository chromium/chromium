// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;

import java.util.Objects;

/**
 * Contains an object holding a reference to either tab ID or sync GUID tied to a selection editor
 * item.
 */
@NullMarked
public class TabListEditorItemSelectionId {
    private final @TabId int mTabId;
    private final @Nullable String mTabGroupSyncId;

    /**
     * Only set one of tabId or tabGroupSyncId as a representative item id.
     *
     * @param tabId The tabId of the selection item or INVALID_TAB_ID if not set.
     * @param tabGroupSyncId The syncId of the selection item or null if not set.
     */
    private TabListEditorItemSelectionId(@TabId int tabId, @Nullable String tabGroupSyncId) {
        mTabId = tabId;
        mTabGroupSyncId = tabGroupSyncId;
    }

    /** The tabId cannot be INVALID_TAB_ID. */
    public static TabListEditorItemSelectionId createTabId(@TabId int tabId) {
        assert tabId != Tab.INVALID_TAB_ID;
        return new TabListEditorItemSelectionId(tabId, null);
    }

    /** The tabGroupSyncId cannot be null. */
    public static TabListEditorItemSelectionId createTabGroupSyncId(String tabGroupSyncId) {
        assert tabGroupSyncId != null;
        return new TabListEditorItemSelectionId(Tab.INVALID_TAB_ID, tabGroupSyncId);
    }

    /** Checks whether this object represents a Tab through the associated tabId. */
    public boolean isTabId() {
        return mTabId != Tab.INVALID_TAB_ID;
    }

    /** Checks whether this object represents a Tab Group through the associated syncId. */
    public boolean isTabGroupSyncId() {
        return mTabGroupSyncId != null;
    }

    /** Returns the tabId associated with the underlying Tab selection item. */
    public @TabId int getTabId() {
        assert isTabId();
        return mTabId;
    }

    /** Returns the syncId associated with the underlying Tab Group selection item. */
    public @Nullable String getTabGroupSyncId() {
        assert isTabGroupSyncId();
        return mTabGroupSyncId;
    }

    @Override
    public boolean equals(Object obj) {
        return (obj instanceof TabListEditorItemSelectionId other)
                && this.mTabId == other.mTabId
                && Objects.equals(this.mTabGroupSyncId, other.mTabGroupSyncId);
    }

    @Override
    public int hashCode() {
        return Objects.hash(this.mTabId, this.mTabGroupSyncId);
    }
}
