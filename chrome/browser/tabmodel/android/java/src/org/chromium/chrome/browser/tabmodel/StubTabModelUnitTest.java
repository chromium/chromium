// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.fail;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/** Tests for {@link StubTabModel} to ensure all mutating methods are overridden. */
@RunWith(BaseRobolectricTestRunner.class)
public class StubTabModelUnitTest {

    // Allowlist of methods that are safe reads or NO-OP listeners, and don't need to be
    // overridden in StubTabModel. They will inherit benign defaults from EmptyTabModel.
    private static final Set<String> SAFE_READ_METHODS =
            new HashSet<>(
                    Arrays.asList(
                            "getTabModelType",
                            "getProfile",
                            "isIncognito",
                            "isOffTheRecord",
                            "isIncognitoBranded",
                            "getCount",
                            "getTabAt",
                            "getTabById",
                            "indexOf",
                            "iterator",
                            "index",
                            "isActiveModel",
                            "isClosurePending",
                            "getComprehensiveModel",
                            "supportsPendingClosures",
                            "getMostRecentlyClosedEntryType",
                            "getMostRecentClosureTime",
                            "isTabMultiSelected",
                            "getMultiSelectedTabsCount",
                            "findFirstNonPinnedTabIndex",
                            "getPinnedTabsCount",
                            "getNativeSessionIdForTesting",
                            "isMuted",
                            "getTabStripCollection",
                            "isClosingAllTabs",
                            "getTabModel",
                            "getRepresentativeTabList",
                            "getIndividualTabAndGroupCount",
                            "getCurrentRepresentativeTabIndex",
                            "getCurrentRepresentativeTab",
                            "getRepresentativeTabAt",
                            "representativeIndexOf",
                            "getTabGroupCount",
                            "getTabCountForGroup",
                            "tabGroupExists",
                            "getRelatedTabList",
                            "getTabsInGroup",
                            "isTabInTabGroup",
                            "getIndexOfTabInGroup",
                            "getGroupLastShownTabId",
                            "getAllTabGroupIds",
                            "getLazyAllTabGroupIds",
                            "getTabGroupTitle",
                            "getTabGroupColor",
                            "getTabGroupColorWithFallback",
                            "getTabGroupCollapsed",
                            "isTabGroupHiding",
                            "isTabModelRestored",
                            "associateWithBrowserWindow",
                            "dissociateWithBrowserWindow",
                            "addDelegateModelObserver",
                            "addIncognitoObserver",
                            "removeIncognitoObserver",
                            "markTabStateInitialized",
                            "addObserver",
                            "removeObserver",
                            "addTabGroupObserver",
                            "removeTabGroupObserver",
                            "getCurrentTabSupplier",
                            "getTabCountSupplier",
                            "getTabByIdChecked",
                            "getNextTabIfClosed",
                            "commitAllTabClosures",
                            "commitTabClosure",
                            "cancelTabClosure",
                            "broadcastSessionRestoreComplete",
                            "setTabsMultiSelected",
                            "clearMultiSelection",
                            "setMuteSetting",
                            "recordPinTimestamp",
                            "recordPinnedDuration",
                            "getActivityTypeForTesting",
                            "setTabGroupCollapsed",
                            "willMergingCreateNewGroup",
                            "performUndoGroupOperation",
                            "undoGroupOperationExpired",
                            "getValidPosition",
                            "setTabGroupTitle",
                            "deleteTabGroupTitle",
                            "setTabGroupColor",
                            "deleteTabGroupColor",
                            "deleteTabGroupCollapsed",
                            "getTabAtChecked",
                            "spliterator",
                            "forEach",
                            "destroy",
                            // Default methods in interface that delegate to throwing methods.
                            // They are safe to inherit because they will eventually throw.
                            "pinTab",
                            "createSingleTabGroup",
                            "mergeTabsToGroup",
                            "mergeListOfTabsToGroup"));

    @Test
    public void testAllMutatingMethodsOverridden() {
        Method[] methods = IncognitoTabModelInternal.class.getMethods();
        StringBuilder missingOverrides = new StringBuilder();

        for (Method method : methods) {
            String methodName = method.getName();
            if (SAFE_READ_METHODS.contains(methodName)) {
                continue;
            }

            try {
                // Check if StubTabModel declares this method itself.
                Method unused =
                        StubTabModel.class.getDeclaredMethod(
                                methodName, method.getParameterTypes());
            } catch (NoSuchMethodException e) {
                missingOverrides.append(methodName).append("\n");
            }
        }

        if (missingOverrides.length() > 0) {
            fail(
                    "The following mutating methods in TabModel are not overridden in"
                            + " StubTabModel:\n"
                            + missingOverrides.toString()
                            + "Please override them to throw error(), or add them to the safe"
                            + " read allowlist in this test.");
        }
    }
}
