// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_dialog;

import android.app.Dialog;
import android.graphics.drawable.Drawable;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.support.test.filters.LargeTest;
import android.support.v4.util.ObjectsCompat;
import android.text.SpannableString;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/**
 * Tests for the ItemChooserDialog class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RetryOnFailure
public class ItemChooserDialogTest implements ItemChooserDialog.ItemSelectedCallback {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    ItemChooserDialog mChooserDialog;

    String mLastSelectedId = "None";

    Drawable mTestDrawable1;
    String mTestDrawableDescription1;

    Drawable mTestDrawable2;
    String mTestDrawableDescription2;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mChooserDialog = createDialog();

        mTestDrawable1 = getNewTestDrawable();
        mTestDrawableDescription1 = "icon1 description";

        mTestDrawable2 = getNewTestDrawable();
        mTestDrawableDescription2 = "icon2 description";

        Assert.assertFalse(ObjectsCompat.equals(mTestDrawable1, mTestDrawable2));
    }

    // ItemChooserDialog.ItemSelectedCallback:

    @Override
    public void onItemSelected(String id) {
        mLastSelectedId = id;
    }

    private Drawable getNewTestDrawable() {
        Drawable drawable =
                VectorDrawableCompat.create(mActivityTestRule.getActivity().getResources(),
                        R.drawable.ic_bluetooth_connected, null);
        // Calling mutate() on a Drawable should typically create a new ConstantState
        // for that Drawable. Ensure the new drawable doesn't share a state with other
        // drwables.
        return drawable.mutate();
    }

    private ItemChooserDialog createDialog() {
        SpannableString title = new SpannableString("title");
        SpannableString searching = new SpannableString("searching");
        SpannableString noneFound = new SpannableString("noneFound");
        SpannableString statusActive = new SpannableString("statusActive");
        SpannableString statusIdleNoneFound = new SpannableString("statusIdleNoneFound");
        SpannableString statusIdleSomeFound = new SpannableString("statusIdleSomeFound");
        String positiveButton = new String("positiveButton");
        final ItemChooserDialog.ItemChooserLabels labels =
                new ItemChooserDialog.ItemChooserLabels(title, searching, noneFound, statusActive,
                        statusIdleNoneFound, statusIdleSomeFound, positiveButton);
        ItemChooserDialog dialog = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            ItemChooserDialog dialog1 = new ItemChooserDialog(
                    mActivityTestRule.getActivity(), ItemChooserDialogTest.this, labels);
            return dialog1;
        });
        return dialog;
    }

    private void selectItem(Dialog dialog, int position, String expectedItemId,
            boolean expectedEnabledState) {
        final ListView items = (ListView) dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return items.getChildAt(0) != null;
            }
        });

        // Verify first item selected gets selected.
        TouchCommon.singleClickView(items.getChildAt(position - 1));

        CriteriaHelper.pollUiThread(
                Criteria.equals(expectedEnabledState, () -> button.isEnabled()));

        if (!expectedEnabledState) return;

        TouchCommon.singleClickView(button);

        CriteriaHelper.pollUiThread(
                Criteria.equals(expectedItemId, () -> mLastSelectedId));
    }

    private View getRowView(Dialog dialog, int position) {
        ListView items = (ListView) dialog.findViewById(R.id.items);
        int actualPosition = position - 1;
        int first = items.getFirstVisiblePosition();
        int last = items.getLastVisiblePosition();

        if (actualPosition < first || actualPosition > last) {
            return items.getAdapter().getView(actualPosition, null, items);
        } else {
            final int visiblePos = actualPosition - first;
            return items.getChildAt(visiblePos);
        }
    }

    private ImageView getIconImageView(Dialog dialog, int position) {
        return (ImageView) getRowView(dialog, position).findViewById(R.id.icon);
    }

    private TextView getDescriptionTextView(Dialog dialog, int position) {
        return (TextView) getRowView(dialog, position).findViewById(R.id.description);
    }

    @Test
    @LargeTest
    public void testAddItemsWithNoIcons() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Dialog dialog = mChooserDialog.getDialogForTesting();
            Assert.assertTrue(dialog.isShowing());

            {
                // Add item 1 with no icon.
                mChooserDialog.addOrUpdateItem("key1", "desc1");
                ImageView icon1 = getIconImageView(dialog, 1);
                Assert.assertEquals(View.GONE, icon1.getVisibility());
                Assert.assertEquals(null, icon1.getDrawable());
            }

            {
                // Add item 2 with no icon.
                mChooserDialog.addOrUpdateItem("key2", "desc2");
                ImageView icon2 = getIconImageView(dialog, 1);
                Assert.assertEquals(View.GONE, icon2.getVisibility());
                Assert.assertEquals(null, icon2.getDrawable());
            }

            mChooserDialog.setIdleState();
            mChooserDialog.dismiss();
        });
    }

    @Test
    @LargeTest
    public void testAddItemsWithIcons() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Dialog dialog = mChooserDialog.getDialogForTesting();
            Assert.assertTrue(dialog.isShowing());

            {
                // Add item 1 with icon.
                mChooserDialog.addOrUpdateItem(
                        "key1", "desc1", mTestDrawable1, mTestDrawableDescription1);
                ImageView icon1 = getIconImageView(dialog, 1);
                Assert.assertEquals(View.VISIBLE, icon1.getVisibility());
                Assert.assertEquals(mTestDrawable1, icon1.getDrawable());
                Assert.assertEquals(mTestDrawableDescription1, icon1.getContentDescription());
            }

            {
                // Add item 2 with icon.
                mChooserDialog.addOrUpdateItem(
                        "key2", "desc2", mTestDrawable2, mTestDrawableDescription2);
                ImageView icon1 = getIconImageView(dialog, 1);
                Assert.assertEquals(View.VISIBLE, icon1.getVisibility());
                Assert.assertEquals(mTestDrawable1, icon1.getDrawable());
                Assert.assertEquals(mTestDrawableDescription1, icon1.getContentDescription());
                ImageView icon2 = getIconImageView(dialog, 2);
                Assert.assertEquals(View.VISIBLE, icon2.getVisibility());
                Assert.assertEquals(mTestDrawable2, icon2.getDrawable());
                Assert.assertEquals(mTestDrawableDescription2, icon2.getContentDescription());
            }

            mChooserDialog.setIdleState();
            mChooserDialog.dismiss();
        });
    }

    @Test
    @LargeTest
    public void testAddItemWithIconAfterItemWithNoIcon() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Dialog dialog = mChooserDialog.getDialogForTesting();
            Assert.assertTrue(dialog.isShowing());

            {
                // Add item 1 with no icon.
                mChooserDialog.addOrUpdateItem("key1", "desc1");
                ImageView icon1 = getIconImageView(dialog, 1);
                Assert.assertEquals(View.GONE, icon1.getVisibility());
                Assert.assertEquals(null, icon1.getDrawable());
            }

            {
                // Add item 2 with icon.
                mChooserDialog.addOrUpdateItem(
                        "key2", "desc2", mTestDrawable2, mTestDrawableDescription2);
                ImageView icon1 = getIconImageView(dialog, 1);
                ImageView icon2 = getIconImageView(dialog, 2);
                Assert.assertEquals(View.INVISIBLE, icon1.getVisibility());
                Assert.assertEquals(View.VISIBLE, icon2.getVisibility());
                Assert.assertEquals(mTestDrawable2, icon2.getDrawable());
            }

            mChooserDialog.setIdleState();
            mChooserDialog.dismiss();
        });
    }

    @Test
    @LargeTest
    public void testAddItemWithNoIconAfterItemWithIcon() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Dialog dialog = mChooserDialog.getDialogForTesting();
            Assert.assertTrue(dialog.isShowing());

            {
                // Add item 1 with icon.
                mChooserDialog.addOrUpdateItem(
                        "key1", "desc1", mTestDrawable1, mTestDrawableDescription1);
                ImageView icon1 = getIconImageView(dialog, 1);
                Assert.assertEquals(View.VISIBLE, icon1.getVisibility());
                Assert.assertEquals(mTestDrawable1, icon1.getDrawable());
            }

            {
                // Add item 2 with no icon.
                mChooserDialog.addOrUpdateItem("key2", "desc2");
                ImageView icon1 = getIconImageView(dialog, 1);
                ImageView icon2 = getIconImageView(dialog, 2);
                Assert.assertEquals(View.VISIBLE, icon1.getVisibility());
                Assert.assertEquals(mTestDrawable1, icon1.getDrawable());
                Assert.assertEquals(View.INVISIBLE, icon2.getVisibility());
            }

            mChooserDialog.setIdleState();
            mChooserDialog.dismiss();
        });
    }

    @Test
    @LargeTest
    public void testRemoveItemWithIconNoItemsWithIconsLeft() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Dialog dialog = mChooserDialog.getDialogForTesting();
            Assert.assertTrue(dialog.isShowing());

            {
                // Add item 1 with icon.
                mChooserDialog.addOrUpdateItem(
                        "key1", "desc1", mTestDrawable1, mTestDrawableDescription1);
                ImageView icon1 = getIconImageView(dialog, 1);
                Assert.assertEquals(View.VISIBLE, icon1.getVisibility());
                Assert.assertEquals(mTestDrawable1, icon1.getDrawable());
            }

            {
                // Add item 2 with no icon.
                mChooserDialog.addOrUpdateItem("key2", "desc2");
                ImageView icon1 = getIconImageView(dialog, 1);
                ImageView icon2 = getIconImageView(dialog, 2);
                Assert.assertEquals(View.VISIBLE, icon1.getVisibility());
                Assert.assertEquals(View.INVISIBLE, icon2.getVisibility());
            }

            {
                // Remove item 1 with icon. No items with icons left.
                mChooserDialog.removeItemFromList("key1");
                ImageView icon2 = getIconImageView(dialog, 1);
                Assert.assertEquals(View.GONE, icon2.getVisibility());
            }

            mChooserDialog.setIdleState();
            mChooserDialog.dismiss();
        });
    }

    @Test
    @LargeTest
    public void testRemoveItemWithIconOneItemWithIconLeft() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Dialog dialog = mChooserDialog.getDialogForTesting();
            Assert.assertTrue(dialog.isShowing());

            {
                // Add item 1 with icon.
                mChooserDialog.addOrUpdateItem(
                        "key1", "desc1", mTestDrawable1, mTestDrawableDescription1);
                ImageView icon1 = getIconImageView(dialog, 1);
                Assert.assertEquals(View.VISIBLE, icon1.getVisibility());
            }

            {
                // Add item 2 with icon.
                mChooserDialog.addOrUpdateItem(
                        "key2", "desc2", mTestDrawable2, mTestDrawableDescription2);
                ImageView icon1 = getIconImageView(dialog, 1);
                ImageView icon2 = getIconImageView(dialog, 2);
                Assert.assertEquals(View.VISIBLE, icon1.getVisibility());
                Assert.assertEquals(View.VISIBLE, icon2.getVisibility());
            }

            {
                // Add item 3 with no icon.
                mChooserDialog.addOrUpdateItem("key3", "desc3");
                ImageView icon1 = getIconImageView(dialog, 1);
                ImageView icon2 = getIconImageView(dialog, 2);
                ImageView icon3 = getIconImageView(dialog, 3);
                Assert.assertEquals(View.VISIBLE, icon1.getVisibility());
                Assert.assertEquals(View.VISIBLE, icon2.getVisibility());
                Assert.assertEquals(View.INVISIBLE, icon3.getVisibility());
            }

            {
                mChooserDialog.removeItemFromList("key1");
                ImageView icon2 = getIconImageView(dialog, 1);
                ImageView icon3 = getIconImageView(dialog, 2);
                Assert.assertEquals(View.VISIBLE, icon2.getVisibility());
                Assert.assertEquals(mTestDrawable2, icon2.getDrawable());
                Assert.assertEquals(View.INVISIBLE, icon3.getVisibility());
            }

            mChooserDialog.setIdleState();
            mChooserDialog.dismiss();
        });
    }

    @Test
    @LargeTest
    public void testUpdateItemWithIconToNoIcon() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Dialog dialog = mChooserDialog.getDialogForTesting();
            Assert.assertTrue(dialog.isShowing());
            DeviceItemAdapter itemAdapter = mChooserDialog.getItemAdapterForTesting();

            {
                // Add item 1 with icon.
                mChooserDialog.addOrUpdateItem(
                        "key1", "desc1", mTestDrawable1, mTestDrawableDescription1);
                ImageView icon1 = getIconImageView(dialog, 1);
                Assert.assertEquals(View.VISIBLE, icon1.getVisibility());
                Assert.assertEquals(mTestDrawableDescription1, icon1.getContentDescription());
                Assert.assertTrue(itemAdapter.getItem(0).hasSameContents(
                        "key1", "desc1", mTestDrawable1, mTestDrawableDescription1));
            }

            {
                // Update item 1 to no icon.
                mChooserDialog.addOrUpdateItem("key1", "desc1");
                ImageView icon1 = getIconImageView(dialog, 1);
                Assert.assertEquals(View.GONE, icon1.getVisibility());
                Assert.assertEquals(null, icon1.getContentDescription());
                Assert.assertTrue(itemAdapter.getItem(0).hasSameContents(
                        "key1", "desc1", null /* icon */, null /* iconDescription */));
            }

            mChooserDialog.setIdleState();
            mChooserDialog.dismiss();
        });
    }

    @Test
    @LargeTest
    public void testUpdateItemWithNoIconToIcon() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Dialog dialog = mChooserDialog.getDialogForTesting();
            Assert.assertTrue(dialog.isShowing());
            DeviceItemAdapter itemAdapter = mChooserDialog.getItemAdapterForTesting();

            {
                // Add item 1 to no icon.
                mChooserDialog.addOrUpdateItem("key1", "desc1");
                ImageView icon1 = getIconImageView(dialog, 1);
                Assert.assertEquals(View.GONE, icon1.getVisibility());
                Assert.assertTrue(itemAdapter.getItem(0).hasSameContents(
                        "key1", "desc1", null /* icon */, null /* iconDescription */));
            }

            {
                // Update item 1 with icon.
                mChooserDialog.addOrUpdateItem(
                        "key1", "desc1", mTestDrawable1, mTestDrawableDescription1);
                ImageView icon1 = getIconImageView(dialog, 1);
                Assert.assertEquals(View.VISIBLE, icon1.getVisibility());
                Assert.assertEquals(mTestDrawable1, icon1.getDrawable());
                Assert.assertEquals(mTestDrawableDescription1, icon1.getContentDescription());
                Assert.assertTrue(itemAdapter.getItem(0).hasSameContents(
                        "key1", "desc1", mTestDrawable1, mTestDrawableDescription1));
            }

            mChooserDialog.setIdleState();
            mChooserDialog.dismiss();
        });
    }

    @Test
    @LargeTest
    public void testUpdateItemIcon() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Dialog dialog = mChooserDialog.getDialogForTesting();
            Assert.assertTrue(dialog.isShowing());
            DeviceItemAdapter itemAdapter = mChooserDialog.getItemAdapterForTesting();

            {
                // Update item 1 with icon.
                mChooserDialog.addOrUpdateItem(
                        "key1", "desc1", mTestDrawable1, mTestDrawableDescription1);
                ImageView icon1 = getIconImageView(dialog, 1);
                Assert.assertEquals(View.VISIBLE, icon1.getVisibility());
                Assert.assertEquals(mTestDrawable1, icon1.getDrawable());
                Assert.assertEquals(mTestDrawableDescription1, icon1.getContentDescription());
                Assert.assertTrue(itemAdapter.getItem(0).hasSameContents(
                        "key1", "desc1", mTestDrawable1, mTestDrawableDescription1));
            }

            {
                // Update item 1 with different icon.
                mChooserDialog.addOrUpdateItem(
                        "key1", "desc1", mTestDrawable2, mTestDrawableDescription2);
                ImageView icon1 = getIconImageView(dialog, 1);
                Assert.assertEquals(View.VISIBLE, icon1.getVisibility());
                Assert.assertEquals(mTestDrawable2, icon1.getDrawable());
                Assert.assertEquals(mTestDrawableDescription2, icon1.getContentDescription());
                Assert.assertTrue(itemAdapter.getItem(0).hasSameContents(
                        "key1", "desc1", mTestDrawable2, mTestDrawableDescription2));
            }

            mChooserDialog.setIdleState();
            mChooserDialog.dismiss();
        });
    }

    @Test
    @LargeTest
    @DisabledTest(message = "crbug.com/958558")
    public void testSimpleItemSelection() {
        Dialog dialog = mChooserDialog.getDialogForTesting();
        Assert.assertTrue(dialog.isShowing());

        TextViewWithClickableSpans statusView = (TextViewWithClickableSpans)
                dialog.findViewById(R.id.status);
        final ListView items = (ListView) dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);

        // Before we add items to the dialog, the 'searching' message should be
        // showing, the Commit button should be disabled and the list view hidden.
        Assert.assertEquals("searching", statusView.getText().toString());
        Assert.assertFalse(button.isEnabled());
        Assert.assertEquals(View.GONE, items.getVisibility());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mChooserDialog.addOrUpdateItem("key1", "desc1");
            mChooserDialog.addOrUpdateItem("key2", "desc2");
        });

        // Two items showing, the empty view should be no more and the button
        // should now be enabled.
        Assert.assertEquals(View.VISIBLE, items.getVisibility());
        Assert.assertEquals(View.GONE, items.getEmptyView().getVisibility());
        Assert.assertEquals("statusActive", statusView.getText().toString());
        Assert.assertFalse(button.isEnabled());

        TestThreadUtils.runOnUiThreadBlocking(() -> { mChooserDialog.setIdleState(); });

        // After discovery stops the list should be visible with two items,
        // it should not show the empty view and the button should not be enabled.
        // The chooser should show the status idle text.
        Assert.assertEquals(View.VISIBLE, items.getVisibility());
        Assert.assertEquals(View.GONE, items.getEmptyView().getVisibility());
        Assert.assertEquals("statusIdleSomeFound", statusView.getText().toString());
        Assert.assertFalse(button.isEnabled());

        // Select the first item and verify it got selected.
        selectItem(dialog, 1, "key1", true);
        Assert.assertTrue(getDescriptionTextView(dialog, 1).isSelected());

        mChooserDialog.dismiss();
    }

    @Test
    @LargeTest
    public void testNoItemsAddedDiscoveryIdle() {
        Dialog dialog = mChooserDialog.getDialogForTesting();
        Assert.assertTrue(dialog.isShowing());

        TextViewWithClickableSpans statusView = (TextViewWithClickableSpans)
                dialog.findViewById(R.id.status);
        final ListView items = (ListView) dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);

        // Before we add items to the dialog, the 'searching' message should be
        // showing, the Commit button should be disabled and the list view hidden.
        Assert.assertEquals("searching", statusView.getText().toString());
        Assert.assertFalse(button.isEnabled());
        Assert.assertEquals(View.GONE, items.getVisibility());

        TestThreadUtils.runOnUiThreadBlocking(() -> { mChooserDialog.setIdleState(); });

        // Listview should now be showing empty, with an empty view visible to
        // drive home the point and a status message at the bottom.
        Assert.assertEquals(View.GONE, items.getVisibility());
        Assert.assertEquals(View.VISIBLE, items.getEmptyView().getVisibility());
        Assert.assertEquals("statusIdleNoneFound", statusView.getText().toString());
        Assert.assertFalse(button.isEnabled());

        mChooserDialog.dismiss();
    }

    @Test
    @LargeTest
    public void testPairButtonDisabledAfterSelectedItemRemoved() throws Throwable {
        final Dialog dialog = TestThreadUtils.runOnUiThreadBlocking(() -> {
            Dialog dialog1 = mChooserDialog.getDialogForTesting();
            Assert.assertTrue(dialog1.isShowing());

            mChooserDialog.addOrUpdateItem("key1", "desc1");
            mChooserDialog.addOrUpdateItem("key2", "desc2");

            return dialog1;
        });

        selectItem(dialog, 1, "key1", true);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            final Button button = (Button) dialog.findViewById(R.id.positive);
            Assert.assertTrue(button.isEnabled());

            mChooserDialog.removeItemFromList("key1");
            Assert.assertFalse(button.isEnabled());

            mChooserDialog.dismiss();
        });
    }

    @Test
    @LargeTest
    public void testSelectAnItemAndRemoveAnotherItem() throws Throwable {
        final Dialog dialog = TestThreadUtils.runOnUiThreadBlocking(() -> {
            Dialog dialog1 = mChooserDialog.getDialogForTesting();
            Assert.assertTrue(dialog1.isShowing());

            mChooserDialog.addOrUpdateItem("key1", "desc1");
            mChooserDialog.addOrUpdateItem("key2", "desc2");
            mChooserDialog.addOrUpdateItem("key3", "desc3");
            return dialog1;
        });
        selectItem(dialog, 2, "key2", true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            final Button button = (Button) dialog.findViewById(R.id.positive);
            DeviceItemAdapter itemAdapter = mChooserDialog.getItemAdapterForTesting();

            Assert.assertTrue(button.isEnabled());

            // Remove the item before the currently selected item.
            mChooserDialog.removeItemFromList("key1");
            Assert.assertTrue(button.isEnabled());
            Assert.assertEquals("key2", itemAdapter.getSelectedItemKey());

            // Remove the item after the currently selected item.
            mChooserDialog.removeItemFromList("key3");
            Assert.assertTrue(button.isEnabled());
            Assert.assertEquals("key2", itemAdapter.getSelectedItemKey());

            mChooserDialog.dismiss();
        });
    }

    @Test
    @LargeTest
    public void testSelectAnItemAndRemoveTheSelectedItem() throws Throwable {
        final Dialog dialog = TestThreadUtils.runOnUiThreadBlocking(() -> {
            Dialog dialog1 = mChooserDialog.getDialogForTesting();
            Assert.assertTrue(dialog1.isShowing());

            mChooserDialog.addOrUpdateItem("key1", "desc1");
            mChooserDialog.addOrUpdateItem("key2", "desc2");
            mChooserDialog.addOrUpdateItem("key3", "desc3");
            return dialog1;
        });

        selectItem(dialog, 2, "key2", true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Button button = (Button) dialog.findViewById(R.id.positive);
            DeviceItemAdapter itemAdapter = mChooserDialog.getItemAdapterForTesting();
            Assert.assertTrue(button.isEnabled());

            // Remove the selected item.
            mChooserDialog.removeItemFromList("key2");
            Assert.assertFalse(button.isEnabled());
            Assert.assertEquals("", itemAdapter.getSelectedItemKey());

            mChooserDialog.dismiss();
        });
    }

    @Test
    @LargeTest
    public void testUpdateItemAndRemoveItemFromList() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Dialog dialog = mChooserDialog.getDialogForTesting();
            Assert.assertTrue(dialog.isShowing());

            TextViewWithClickableSpans statusView =
                    (TextViewWithClickableSpans) dialog.findViewById(R.id.status);
            final ListView items = (ListView) dialog.findViewById(R.id.items);
            final Button button = (Button) dialog.findViewById(R.id.positive);

            DeviceItemAdapter itemAdapter = mChooserDialog.getItemAdapterForTesting();
            final String nonExistentKey = "key";

            // Initially the itemAdapter is empty.
            Assert.assertTrue(itemAdapter.isEmpty());

            // Try removing an item from an empty itemAdapter.
            mChooserDialog.removeItemFromList(nonExistentKey);
            Assert.assertTrue(itemAdapter.isEmpty());

            // Add item 1.
            mChooserDialog.addOrUpdateItem("key1", "desc1");
            Assert.assertEquals(1, itemAdapter.getCount());
            Assert.assertTrue(itemAdapter.getItem(0).hasSameContents(
                    "key1", "desc1", null /* icon */, null /* iconDescription */));

            // Update item 1 with different description.
            mChooserDialog.addOrUpdateItem("key1", "desc2");
            Assert.assertEquals(1, itemAdapter.getCount());
            Assert.assertTrue(itemAdapter.getItem(0).hasSameContents(
                    "key1", "desc2", null /* icon */, null /* iconDescription */));

            mChooserDialog.setIdleState();

            // Remove item 1.
            mChooserDialog.removeItemFromList("key1");
            Assert.assertTrue(itemAdapter.isEmpty());

            // Listview should now be showing empty, with an empty view visible
            // and the button should not be enabled.
            // The chooser should show a status message at the bottom.
            Assert.assertEquals(View.GONE, items.getVisibility());
            Assert.assertEquals(View.VISIBLE, items.getEmptyView().getVisibility());
            Assert.assertEquals("statusIdleNoneFound", statusView.getText().toString());
            Assert.assertFalse(button.isEnabled());

            mChooserDialog.dismiss();
        });
    }

    @Test
    @LargeTest
    public void testAddItemAndRemoveItemFromList() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Dialog dialog = mChooserDialog.getDialogForTesting();
            Assert.assertTrue(dialog.isShowing());

            TextViewWithClickableSpans statusView =
                    (TextViewWithClickableSpans) dialog.findViewById(R.id.status);
            final ListView items = (ListView) dialog.findViewById(R.id.items);
            final Button button = (Button) dialog.findViewById(R.id.positive);

            DeviceItemAdapter itemAdapter = mChooserDialog.getItemAdapterForTesting();
            final String nonExistentKey = "key";

            // Initially the itemAdapter is empty.
            Assert.assertTrue(itemAdapter.isEmpty());

            // Try removing an item from an empty itemAdapter.
            mChooserDialog.removeItemFromList(nonExistentKey);
            Assert.assertTrue(itemAdapter.isEmpty());

            // Add item 1.
            mChooserDialog.addOrUpdateItem("key1", "desc1");
            Assert.assertEquals(1, itemAdapter.getCount());
            Assert.assertTrue(itemAdapter.getItem(0).hasSameContents(
                    "key1", "desc1", null /* icon */, null /* iconDescription */));

            // Add item 2.
            mChooserDialog.addOrUpdateItem("key2", "desc2");
            Assert.assertEquals(2, itemAdapter.getCount());
            Assert.assertTrue(itemAdapter.getItem(0).hasSameContents(
                    "key1", "desc1", null /* icon */, null /* iconDescription */));
            Assert.assertTrue(itemAdapter.getItem(1).hasSameContents(
                    "key2", "desc2", null /* icon */, null /* iconDescription */));

            mChooserDialog.setIdleState();

            // Try removing an item that doesn't exist.
            mChooserDialog.removeItemFromList(nonExistentKey);
            Assert.assertEquals(2, itemAdapter.getCount());

            // Remove item 2.
            mChooserDialog.removeItemFromList("key2");
            Assert.assertEquals(1, itemAdapter.getCount());
            Assert.assertTrue(itemAdapter.getItem(0).hasSameContents(
                    "key1", "desc1", null /* icon */, null /* iconDescription */));

            // The list should be visible with one item, it should not show
            // the empty view and the button should not be enabled.
            // The chooser should show a status message at the bottom.
            Assert.assertEquals(View.VISIBLE, items.getVisibility());
            Assert.assertEquals(View.GONE, items.getEmptyView().getVisibility());
            Assert.assertEquals("statusIdleSomeFound", statusView.getText().toString());
            Assert.assertFalse(button.isEnabled());

            // Remove item 1.
            mChooserDialog.removeItemFromList("key1");
            Assert.assertTrue(itemAdapter.isEmpty());

            // Listview should now be showing empty, with an empty view visible
            // and the button should not be enabled.
            // The chooser should show a status message at the bottom.
            Assert.assertEquals(View.GONE, items.getVisibility());
            Assert.assertEquals(View.VISIBLE, items.getEmptyView().getVisibility());
            Assert.assertEquals("statusIdleNoneFound", statusView.getText().toString());
            Assert.assertFalse(button.isEnabled());

            mChooserDialog.dismiss();
        });
    }

    @Test
    @LargeTest
    public void testAddItemWithSameNameToListAndRemoveItemFromList() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Dialog dialog = mChooserDialog.getDialogForTesting();
            Assert.assertTrue(dialog.isShowing());

            DeviceItemAdapter itemAdapter = mChooserDialog.getItemAdapterForTesting();

            // Add item 1.
            mChooserDialog.addOrUpdateItem("key1", "desc1");
            Assert.assertEquals(1, itemAdapter.getCount());
            // Add item 2.
            mChooserDialog.addOrUpdateItem("key2", "desc2");
            Assert.assertEquals(2, itemAdapter.getCount());
            // Add item 3 with same description as item 1.
            mChooserDialog.addOrUpdateItem("key3", "desc1");
            Assert.assertEquals(3, itemAdapter.getCount());
            Assert.assertTrue(itemAdapter.getItem(0).hasSameContents(
                    "key1", "desc1", null /* icon */, null /* iconDescription */));
            Assert.assertTrue(itemAdapter.getItem(1).hasSameContents(
                    "key2", "desc2", null /* icon */, null /* iconDescription */));
            Assert.assertTrue(itemAdapter.getItem(2).hasSameContents(
                    "key3", "desc1", null /* icon */, null /* iconDescription */));

            // Since two items have the same name, their display text should have their unique
            // keys appended.
            Assert.assertEquals("desc1 (key1)", itemAdapter.getDisplayText(0));
            Assert.assertEquals("desc2", itemAdapter.getDisplayText(1));
            Assert.assertEquals("desc1 (key3)", itemAdapter.getDisplayText(2));

            // Remove item 2.
            mChooserDialog.removeItemFromList("key2");
            Assert.assertEquals(2, itemAdapter.getCount());
            // Make sure the remaining items are item 1 and item 3.
            Assert.assertTrue(itemAdapter.getItem(0).hasSameContents(
                    "key1", "desc1", null /* icon */, null /* iconDescription */));
            Assert.assertTrue(itemAdapter.getItem(1).hasSameContents(
                    "key3", "desc1", null /* icon */, null /* iconDescription */));
            Assert.assertEquals("desc1 (key1)", itemAdapter.getDisplayText(0));
            Assert.assertEquals("desc1 (key3)", itemAdapter.getDisplayText(1));

            // Remove item 1.
            mChooserDialog.removeItemFromList("key1");
            Assert.assertEquals(1, itemAdapter.getCount());
            // Make sure the remaining item is item 3.
            Assert.assertTrue(itemAdapter.getItem(0).hasSameContents(
                    "key3", "desc1", null /* icon */, null /* iconDescription */));
            // After removing item 1, item 3 is the only remaining item, so its display text
            // also changed to its original description.
            Assert.assertEquals("desc1", itemAdapter.getDisplayText(0));

            mChooserDialog.dismiss();
        });
    }

    @Test
    @LargeTest
    public void testListHeight() {
        // 500 * .3 is 150, which is 48 * 3.125. 48 * 3.5 is 168.
        Assert.assertEquals(168, ItemChooserDialog.getListHeight(500, 1.0f));

        // 150 * .3 is 45, which rounds below the minimum height.
        Assert.assertEquals(72, ItemChooserDialog.getListHeight(150, 1.0f));

        // 1460 * .3 is 438, which rounds above the maximum height.
        Assert.assertEquals(408, ItemChooserDialog.getListHeight(1460, 1.0f));

        // 1100px is 500dp at a density of 2.2. 500 * .3 is 150dp, which is 48dp *
        // 3.125. 48dp * 3.5 is 168dp. 168dp * 2.2px/dp is 369.6, which rounds to
        // 370.
        Assert.assertEquals(370, ItemChooserDialog.getListHeight(1100, 2.2f));
    }
}
