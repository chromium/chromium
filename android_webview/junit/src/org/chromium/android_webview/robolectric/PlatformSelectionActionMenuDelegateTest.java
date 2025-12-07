// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import android.content.Context;
import android.content.pm.ResolveInfo;
import android.view.MenuItem;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.android_webview.selection.PlatformSelectionActionMenuDelegate;
import org.chromium.base.SelectionActionMenuClientWrapper;
import org.chromium.base.SelectionActionMenuClientWrapper.DefaultItem;
import org.chromium.base.SelectionActionMenuClientWrapper.MenuType;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.autofill.AutofillProvider;
import org.chromium.components.autofill.AutofillSelectionActionMenuDelegate;
import org.chromium.components.autofill.AutofillSelectionMenuItemHelper;
import org.chromium.content_public.browser.SelectionMenuItem;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;

/**
 * Tests for PlatformSelectionActionMenuDelegate. The template for most of the tests here is:
 *
 * <ol>
 *   <li>Override TestClient with the method you want to test.
 *   <li>Create an instance of PlatformSelectionActionMenuDelegate with the test client.
 *   <li>Assert that the behavior of the delegate method is as expected.
 * </ol>
 */
@RunWith(BaseRobolectricTestRunner.class)
public class PlatformSelectionActionMenuDelegateTest {

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void usesDefaultMenuItemOrderFromClient() {
        final @DefaultItem int[] floatingOrder =
                new @DefaultItem int[] {
                    DefaultItem.WEB_SEARCH,
                    DefaultItem.SELECT_ALL,
                    DefaultItem.SHARE,
                    DefaultItem.PASTE_AS_PLAIN_TEXT,
                    DefaultItem.PASTE,
                    DefaultItem.COPY,
                    DefaultItem.CUT
                };
        final @DefaultItem int[] dropdownOrder =
                new @DefaultItem int[] {
                    DefaultItem.PASTE_AS_PLAIN_TEXT,
                    DefaultItem.COPY,
                    DefaultItem.SHARE,
                    DefaultItem.CUT,
                    DefaultItem.SELECT_ALL,
                    DefaultItem.PASTE,
                    DefaultItem.WEB_SEARCH,
                };
        TestClient client =
                new TestClient() {
                    @Override
                    public @DefaultItem int[] getDefaultMenuItemOrder(int menuType) {
                        if (menuType == MenuType.FLOATING) return floatingOrder;
                        if (menuType == MenuType.DROPDOWN) return dropdownOrder;
                        Assert.fail("Wrong MenuType value was passed to getDefaultMenuItemOrder");
                        return new int[] {};
                    }
                };
        PlatformSelectionActionMenuDelegate delegate =
                new PlatformSelectionActionMenuDelegate(client);

        Assert.assertArrayEquals(
                floatingOrder, delegate.getDefaultMenuItemOrder(MenuType.FLOATING));
        Assert.assertArrayEquals(
                dropdownOrder, delegate.getDefaultMenuItemOrder(MenuType.DROPDOWN));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void hasAdditionalItemsFromSuper_noClientItems() {
        // Create an instance of the superclass and store the returned additional items.
        AutofillSelectionActionMenuDelegate parent = new AutofillSelectionActionMenuDelegate();
        AutofillSelectionMenuItemHelper helper =
                new AutofillSelectionMenuItemHelper(getFakeAutofillProvider());
        parent.setAutofillSelectionMenuItemHelper(helper);
        List<SelectionMenuItem> expected =
                parent.getAdditionalMenuItems(MenuType.FLOATING, false, false, "");

        // Use default client with no extra items.
        PlatformSelectionActionMenuDelegate delegate =
                new PlatformSelectionActionMenuDelegate(new TestClient());
        // Set the same helper so that we should end up with the menu items from the super class.
        delegate.setAutofillSelectionMenuItemHelper(helper);
        List<SelectionMenuItem> actual =
                delegate.getAdditionalMenuItems(MenuType.FLOATING, false, false, "");

        Context context = ApplicationProvider.getApplicationContext();
        Assert.assertFalse(expected.isEmpty());
        assertContainsItemsWithTitles(expected, actual, context);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void hasAdditionalItemsFromSuper_withClientItems() {
        // Create an instance of the superclass and store the returned additional items.
        AutofillSelectionActionMenuDelegate parent = new AutofillSelectionActionMenuDelegate();
        AutofillSelectionMenuItemHelper helper =
                new AutofillSelectionMenuItemHelper(getFakeAutofillProvider());
        parent.setAutofillSelectionMenuItemHelper(helper);
        List<SelectionMenuItem> expected =
                parent.getAdditionalMenuItems(MenuType.FLOATING, false, false, "");
        // Use client with extra items.
        TestClient client =
                new TestClient() {
                    @Override
                    public List<MenuItem> getAdditionalMenuItems(
                            Context context,
                            int menuType,
                            boolean isSelectionPassword,
                            boolean isSelectionReadOnly,
                            String selectedText) {
                        Assert.assertTrue(
                                menuType == MenuType.FLOATING || menuType == MenuType.DROPDOWN);
                        return List.of(getFakeMenuItem("Hello"), getFakeMenuItem("World"));
                    }
                };
        PlatformSelectionActionMenuDelegate delegate =
                new PlatformSelectionActionMenuDelegate(client);
        // Set the same helper so that we should end up with the menu items from the super class.
        delegate.setAutofillSelectionMenuItemHelper(helper);
        List<SelectionMenuItem> actual =
                delegate.getAdditionalMenuItems(MenuType.FLOATING, false, false, "");

        Context context = ApplicationProvider.getApplicationContext();
        Assert.assertFalse(expected.isEmpty());
        // First check that the autofill items are returned.
        assertContainsItemsWithTitles(expected, actual, context);
        assertContainsItemsWithTitles(new HashSet<>(List.of("Hello", "World")), actual, context);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void usesFilteringFromClient() {
        ResolveInfo extraResolveInfo = new ResolveInfo();
        TestClient clientFilteringAllActivities =
                new TestClient() {
                    @Override
                    public List<ResolveInfo> filterTextProcessingActivities(
                            Context context, int menuType, List<ResolveInfo> activities) {
                        return List.of();
                    }
                };
        TestClient clientAddingExtraResolveInfo =
                new TestClient() {
                    @Override
                    public List<ResolveInfo> filterTextProcessingActivities(
                            Context context, int menuType, List<ResolveInfo> activities) {
                        ArrayList<ResolveInfo> extendedActivities = new ArrayList<>(activities);
                        extendedActivities.add(extraResolveInfo);
                        return extendedActivities;
                    }
                };

        PlatformSelectionActionMenuDelegate delegateFilteringAllActivities =
                new PlatformSelectionActionMenuDelegate(clientFilteringAllActivities);
        PlatformSelectionActionMenuDelegate delegateAddingExtraResolveInfo =
                new PlatformSelectionActionMenuDelegate(clientAddingExtraResolveInfo);
        List<ResolveInfo> activities = List.of(new ResolveInfo(), new ResolveInfo());

        Assert.assertTrue(
                delegateFilteringAllActivities
                        .filterTextProcessingActivities(MenuType.FLOATING, activities)
                        .isEmpty());
        Assert.assertTrue(
                delegateAddingExtraResolveInfo
                        .filterTextProcessingActivities(MenuType.DROPDOWN, activities)
                        .contains(extraResolveInfo));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void handlesAddedMenuItems() {
        MenuItem extraItem1 = getFakeMenuItem("Item1", 1);
        MenuItem extraItem2 = getFakeMenuItem("Item2", 2);
        TestClient client =
                new TestClient() {
                    @Override
                    public List<MenuItem> getAdditionalMenuItems(
                            Context context,
                            int menuType,
                            boolean isSelectionPassword,
                            boolean isSelectionReadOnly,
                            String selectedText) {
                        return List.of(extraItem1, extraItem2);
                    }

                    @Override
                    public boolean handleMenuItemClick(Context context, MenuItem item) {
                        // CTS tests enforce that clients handle their own menu items.
                        return item.getItemId() == extraItem1.getItemId()
                                || item.getItemId() == extraItem2.getItemId();
                    }
                };
        PlatformSelectionActionMenuDelegate delegate =
                new PlatformSelectionActionMenuDelegate(client);
        delegate.getAdditionalMenuItems(MenuType.DROPDOWN, false, false, "");

        Assert.assertTrue(
                delegate.handleMenuItemClick(
                        new SelectionMenuItem.Builder("Item1")
                                .setId(extraItem1.getItemId())
                                .build(),
                        null,
                        null));
        Assert.assertTrue(
                delegate.handleMenuItemClick(
                        new SelectionMenuItem.Builder("Item2")
                                .setId(extraItem2.getItemId())
                                .build(),
                        null,
                        null));
        Assert.assertFalse(
                delegate.handleMenuItemClick(
                        new SelectionMenuItem.Builder("Item3").setId(3).build(), null, null));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void handlesItemsFromSuper() {
        // Create an instance of the superclass and store the returned additional items.
        AutofillSelectionActionMenuDelegate parent = new AutofillSelectionActionMenuDelegate();
        AutofillSelectionMenuItemHelper helper =
                new AutofillSelectionMenuItemHelper(getFakeAutofillProvider());
        parent.setAutofillSelectionMenuItemHelper(helper);
        List<SelectionMenuItem> additionalItems =
                parent.getAdditionalMenuItems(MenuType.FLOATING, false, false, "");
        PlatformSelectionActionMenuDelegate delegate =
                new PlatformSelectionActionMenuDelegate(new TestClient());
        delegate.setAutofillSelectionMenuItemHelper(helper);

        Assert.assertTrue(delegate.handleMenuItemClick(additionalItems.get(0), null, null));
    }

    private static MenuItem getFakeMenuItem(String title) {
        return getFakeMenuItem(title, null);
    }

    private static MenuItem getFakeMenuItem(String title, @Nullable Integer id) {
        MenuItem item = Mockito.spy(MenuItem.class);
        Mockito.when(item.getItemId()).thenReturn(id != null ? id : View.generateViewId());
        Mockito.when(item.getTitle()).thenReturn(title);
        Mockito.when(item.isEnabled()).thenReturn(true);
        Mockito.when(item.isVisible()).thenReturn(true);
        return item;
    }

    private static AutofillProvider getFakeAutofillProvider() {
        AutofillProvider provider = Mockito.mock(AutofillProvider.class);
        Mockito.when(provider.shouldOfferPasskeyEntry()).thenReturn(true);
        Mockito.when(provider.shouldQueryAutofillSuggestion()).thenReturn(true);
        return provider;
    }

    private static void assertContainsItemsWithTitles(
            List<SelectionMenuItem> expected, List<SelectionMenuItem> actual, Context context) {
        HashSet<String> expectedTitles = new HashSet<>();
        for (SelectionMenuItem item : expected) {
            expectedTitles.add(item.getTitle(context).toString());
        }
        assertContainsItemsWithTitles(expectedTitles, actual, context);
    }

    private static void assertContainsItemsWithTitles(
            HashSet<String> expectedTitles, List<SelectionMenuItem> actual, Context context) {
        for (SelectionMenuItem item : actual) {
            expectedTitles.remove(item.getTitle(context).toString());
        }
        Assert.assertTrue(
                "Items with the following titles were not found: " + expectedTitles,
                expectedTitles.isEmpty());
    }

    private static class TestClient implements SelectionActionMenuClientWrapper {
        @Override
        public @DefaultItem int[] getDefaultMenuItemOrder(int menuType) {
            return new int[0];
        }

        @Override
        public List<MenuItem> getAdditionalMenuItems(
                Context context,
                int menuType,
                boolean isSelectionPassword,
                boolean isSelectionReadOnly,
                String selectedText) {
            return Collections.emptyList();
        }

        @Override
        public List<ResolveInfo> filterTextProcessingActivities(
                Context context, int menuType, List<ResolveInfo> activities) {
            return Collections.emptyList();
        }

        @Override
        public boolean handleMenuItemClick(Context context, MenuItem item) {
            return false;
        }
    }
}
