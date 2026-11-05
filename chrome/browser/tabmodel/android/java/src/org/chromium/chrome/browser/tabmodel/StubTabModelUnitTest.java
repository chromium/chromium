// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.mock;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;

import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.Collections;
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
                            "getOrderedMultiSelectedTabIds",
                            "getOrderedMultiSelectedTabs",
                            "findFirstNonPinnedTabIndex",
                            "getPinnedTabsCount",
                            "getNativeSessionId",
                            "isMuted",
                            "getTabStripCollection",
                            "getTabModel",
                            "getRepresentativeTabList",
                            "getIndividualTabAndGroupCount",
                            "getRepresentativeTabAt",
                            "representativeIndexOf",
                            "getTabGroupCount",
                            "getTabCountForGroup",
                            "containsTabGroup",
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
                            "isTabStateInitialized",
                            "associateWithBrowserWindow",
                            "dissociateWithBrowserWindow",
                            "addIncognitoObserver",
                            "removeIncognitoObserver",
                            "addObserver",
                            "removeObserver",
                            "addTabGroupObserver",
                            "removeTabGroupObserver",
                            "getNextTabPolicySupplier",
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
                            "performUndoGroupOperation",
                            "undoGroupOperationExpired",
                            "setTabGroupTitle",
                            "deleteTabGroupTitle",
                            "setTabGroupColor",
                            "getTabAtChecked",
                            "spliterator",
                            "forEach",
                            "destroy",
                            // Default methods in interface that delegate to throwing methods.
                            // They are safe to inherit because they will eventually throw.
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
                var _ =
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

    @Test
    public void testSuppliers() {
        StubTabModel stubTabModel = new StubTabModel(/* isIncognito= */ false, /* profile= */ null);
        assertNull(stubTabModel.getCurrentTabSupplier().get());
        assertEquals(Integer.valueOf(0), stubTabModel.getTabCountSupplier().get());
    }

    @Test
    public void testCloseAllTabsIsNoOp() {
        StubTabModel stubTabModel = new StubTabModel(/* isIncognito= */ false, /* profile= */ null);
        TabClosureParams closeAllParams = TabClosureParams.closeAllTabs().build();

        assertFalse(stubTabModel.closeTabs(closeAllParams));
        stubTabModel.getTabRemover().closeTabs(closeAllParams, /* allowDialog= */ false);
        stubTabModel
                .getTabRemover()
                .prepareCloseTabs(
                        closeAllParams,
                        /* allowDialog= */ false,
                        /* listener= */ null,
                        params -> fail("onPreparedCallback should not be invoked on StubTabModel"));
        stubTabModel.getTabRemover().forceCloseTabs(closeAllParams);
    }

    @Test
    public void testCloseNonAllTabsThrows() {
        StubTabModel stubTabModel = new StubTabModel(/* isIncognito= */ true, /* profile= */ null);
        Tab mockTab = mock(Tab.class);
        TabClosureParams singleParams = TabClosureParams.closeTab(mockTab).build();
        TabClosureParams multipleParams =
                TabClosureParams.closeTabs(Collections.singletonList(mockTab)).build();

        assertThrows(
                UnsupportedOperationException.class, () -> stubTabModel.closeTabs(singleParams));
        assertThrows(
                UnsupportedOperationException.class, () -> stubTabModel.closeTabs(multipleParams));

        TabRemover remover = stubTabModel.getTabRemover();
        assertThrows(
                UnsupportedOperationException.class,
                () -> remover.closeTabs(singleParams, /* allowDialog= */ false));
        assertThrows(
                UnsupportedOperationException.class,
                () -> remover.closeTabs(multipleParams, /* allowDialog= */ false));
        assertThrows(
                UnsupportedOperationException.class,
                () ->
                        remover.prepareCloseTabs(
                                singleParams,
                                /* allowDialog= */ false,
                                /* listener= */ null,
                                params -> {}));
        assertThrows(
                UnsupportedOperationException.class, () -> remover.forceCloseTabs(singleParams));
        assertThrows(
                UnsupportedOperationException.class,
                () -> remover.removeTab(mockTab, /* allowDialog= */ false));
    }
}
