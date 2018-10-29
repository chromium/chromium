// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.v7.widget.RecyclerView;
import android.util.JsonWriter;
import android.view.View;
import android.widget.Button;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate.SelectionObserver;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.ui.ContactsPickerListener;

import java.io.StringWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Callable;

/**
 * Tests for the ContactsPickerDialog class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ContactsPickerDialogTest
        implements ContactsPickerListener, SelectionObserver<ContactDetails> {
    // The timeout (in seconds) to wait for the decoder service to be ready.
    private static final long WAIT_TIMEOUT_SECONDS = scaleTimeout(30);

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    // The dialog we are testing.
    private ContactsPickerDialog mDialog;

    // The data to show in the dialog.
    private ArrayList<ContactDetails> mTestContacts;

    // The selection delegate for the dialog.
    private SelectionDelegate<ContactDetails> mSelectionDelegate;

    // The last action recorded in the dialog (e.g. photo selected).
    private @ContactsPickerAction int mLastActionRecorded;

    // The final set of contacts picked by the dialog (json string).
    private String mLastSelectedContacts;

    // The list of currently selected photos (built piecemeal).
    private List<ContactDetails> mCurrentContactSelection;

    // A callback that fires when something is selected in the dialog.
    public final CallbackHelper onSelectionCallback = new CallbackHelper();

    // A callback that fires when an action is taken in the dialog (cancel/done etc).
    public final CallbackHelper onActionCallback = new CallbackHelper();

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestContacts = new ArrayList<ContactDetails>();
        mTestContacts.add(new ContactDetails("0", "Contact 0", null, null));
        mTestContacts.add(new ContactDetails("1", "Contact 1", null, null));
        mTestContacts.add(new ContactDetails("2", "Contact 2", null, null));
        mTestContacts.add(new ContactDetails("3", "Contact 3", null, null));
        mTestContacts.add(new ContactDetails("4", "Contact 4", null, null));
        mTestContacts.add(new ContactDetails("5", "Contact 5", null, null));
        PickerAdapter.setTestContacts(mTestContacts);

        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        canvas.drawColor(Color.BLUE);
        ContactViewHolder.setIconForTesting(bitmap);
    }

    // ContactsPickerDialog.ContactsPickerListener:

    @Override
    public void onContactsPickerUserAction(@ContactsPickerAction int action, String contacts) {
        mLastActionRecorded = action;
        mLastSelectedContacts = contacts;
        onActionCallback.notifyCalled();
    }

    // SelectionObserver:

    @Override
    public void onSelectionStateChange(List<ContactDetails> photosSelected) {
        mCurrentContactSelection = photosSelected;
        onSelectionCallback.notifyCalled();
    }

    private RecyclerView getRecyclerView() {
        return (RecyclerView) mDialog.findViewById(R.id.recycler_view);
    }

    private ContactsPickerDialog createDialog(
            final boolean multiselect, final List<String> mimeTypes) throws Exception {
        final ContactsPickerDialog dialog =
                ThreadUtils.runOnUiThreadBlocking(new Callable<ContactsPickerDialog>() {
                    @Override
                    public ContactsPickerDialog call() {
                        final ContactsPickerDialog dialog =
                                new ContactsPickerDialog(mActivityTestRule.getActivity(),
                                        ContactsPickerDialogTest.this, multiselect, mimeTypes);
                        dialog.show();
                        return dialog;
                    }
                });

        mSelectionDelegate = dialog.getCategoryViewForTesting().getSelectionDelegateForTesting();
        if (!multiselect) mSelectionDelegate.setSingleSelectionMode();
        mSelectionDelegate.addObserver(this);
        mDialog = dialog;

        return dialog;
    }

    private void clickView(final int position, final int expectedSelectionCount,
            final boolean expectSelection) throws Exception {
        RecyclerView recyclerView = getRecyclerView();
        RecyclerViewTestUtils.scrollToView(recyclerView, position);

        int callCount = onSelectionCallback.getCallCount();
        TestTouchUtils.performClickOnMainSync(InstrumentationRegistry.getInstrumentation(),
                recyclerView.findViewHolderForAdapterPosition(position).itemView);
        onSelectionCallback.waitForCallback(callCount, 1);

        // Validate the correct selection took place.
        Assert.assertEquals(expectedSelectionCount, mCurrentContactSelection.size());
        Assert.assertEquals(
                expectSelection, mSelectionDelegate.isItemSelected(mTestContacts.get(position)));
    }

    private void clickDone() throws Exception {
        mLastActionRecorded = ContactsPickerAction.NUM_ENTRIES;

        ContactsPickerToolbar toolbar =
                (ContactsPickerToolbar) mDialog.findViewById(R.id.action_bar);
        Button done = (Button) toolbar.findViewById(R.id.done);
        int callCount = onActionCallback.getCallCount();
        TestTouchUtils.performClickOnMainSync(InstrumentationRegistry.getInstrumentation(), done);
        onActionCallback.waitForCallback(callCount, 1);
        Assert.assertEquals(ContactsPickerAction.CONTACTS_SELECTED, mLastActionRecorded);
    }

    public void clickCancel() throws Exception {
        mLastActionRecorded = ContactsPickerAction.NUM_ENTRIES;

        PickerCategoryView categoryView = mDialog.getCategoryViewForTesting();
        View cancel = new View(mActivityTestRule.getActivity());
        int callCount = onActionCallback.getCallCount();
        categoryView.onClick(cancel);
        onActionCallback.waitForCallback(callCount, 1);
        Assert.assertEquals(ContactsPickerAction.CANCEL, mLastActionRecorded);
    }

    private void clickActionButton(final int expectedSelectionCount,
            final @ContactsPickerAction int expectedAction) throws Exception {
        mLastActionRecorded = ContactsPickerAction.NUM_ENTRIES;

        ContactsPickerToolbar toolbar =
                (ContactsPickerToolbar) mDialog.findViewById(R.id.action_bar);
        View action = toolbar.findViewById(R.id.action);
        int callCount = onActionCallback.getCallCount();
        TestTouchUtils.performClickOnMainSync(InstrumentationRegistry.getInstrumentation(), action);
        onActionCallback.waitForCallback(callCount, 1);
        Assert.assertEquals(expectedSelectionCount, mSelectionDelegate.getSelectedItems().size());
        Assert.assertEquals(expectedAction, mLastActionRecorded);
    }

    private void clickSearchButton() throws Exception {
        ContactsPickerToolbar toolbar =
                (ContactsPickerToolbar) mDialog.findViewById(R.id.action_bar);
        View search = toolbar.findViewById(R.id.search);
        TestTouchUtils.performClickOnMainSync(InstrumentationRegistry.getInstrumentation(), search);
    }

    private void dismissDialog() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mDialog.dismiss();
            }
        });
    }

    private void addContact(JsonWriter writer, String displayName) throws Throwable {
        writer.beginObject();
        writer.name("name");
        writer.value(displayName);
        writer.name("emails");
        writer.beginArray();
        writer.endArray();
        writer.name("phoneNumbers");
        writer.beginArray();
        writer.endArray();
        writer.endObject();
    }

    @Test
    @LargeTest
    public void testNoSelection() throws Throwable {
        createDialog(/* multiselect = */ false, Arrays.asList("image/*"));
        Assert.assertTrue(mDialog.isShowing());

        int expectedSelectionCount = 1;
        clickView(0, expectedSelectionCount, /* expectSelection = */ true);
        clickCancel();

        Assert.assertNull(mLastSelectedContacts);
        Assert.assertEquals(ContactsPickerAction.CANCEL, mLastActionRecorded);

        dismissDialog();
    }

    @Test
    @LargeTest
    public void testSingleSelectionContacts() throws Throwable {
        createDialog(/* multiselect = */ false, Arrays.asList("image/*"));
        Assert.assertTrue(mDialog.isShowing());

        // Expected selection count is 1 because clicking on a new view deselects other.
        int expectedSelectionCount = 1;
        clickView(0, expectedSelectionCount, /* expectSelection = */ true);
        clickView(1, expectedSelectionCount, /* expectSelection = */ true);
        clickDone();

        StringWriter out = new StringWriter();
        final JsonWriter writer = new JsonWriter(out);
        writer.beginArray();
        addContact(writer, mTestContacts.get(1).getDisplayName());
        writer.endArray();

        Assert.assertEquals(ContactsPickerAction.CONTACTS_SELECTED, mLastActionRecorded);
        Assert.assertEquals(out.toString(), mLastSelectedContacts);

        dismissDialog();
    }

    @Test
    @LargeTest
    public void testMultiSelectionContacts() throws Throwable {
        createDialog(/* multiselect = */ true, Arrays.asList("image/*"));
        Assert.assertTrue(mDialog.isShowing());

        // Multi-selection is enabled, so each click is counted.
        int expectedSelectionCount = 0;
        clickView(0, ++expectedSelectionCount, /* expectSelection = */ true);
        clickView(2, ++expectedSelectionCount, /* expectSelection = */ true);
        clickView(4, ++expectedSelectionCount, /* expectSelection = */ true);
        clickDone();

        Assert.assertEquals(ContactsPickerAction.CONTACTS_SELECTED, mLastActionRecorded);

        StringWriter out = new StringWriter();
        final JsonWriter writer = new JsonWriter(out);
        writer.beginArray();
        addContact(writer, mTestContacts.get(4).getDisplayName());
        addContact(writer, mTestContacts.get(2).getDisplayName());
        addContact(writer, mTestContacts.get(0).getDisplayName());
        writer.endArray();
        Assert.assertEquals(out.toString(), mLastSelectedContacts);

        dismissDialog();
    }

    @Test
    @LargeTest
    public void testSelectAll() throws Throwable {
        createDialog(/* multiselect = */ true, Arrays.asList("image/*"));
        Assert.assertTrue(mDialog.isShowing());

        ContactsPickerToolbar toolbar =
                (ContactsPickerToolbar) mDialog.findViewById(R.id.action_bar);
        View action = toolbar.findViewById(R.id.action);
        Assert.assertEquals(View.VISIBLE, action.getVisibility());

        clickActionButton(6, ContactsPickerAction.SELECT_ALL);
        clickActionButton(0, ContactsPickerAction.UNDO_SELECT_ALL);

        // Manually select one item.
        int expectedSelectionCount = 0;
        clickView(0, ++expectedSelectionCount, /* expectSelection = */ true);

        clickActionButton(6, ContactsPickerAction.SELECT_ALL);
        clickActionButton(1, ContactsPickerAction.UNDO_SELECT_ALL);

        // Select the rest of the items manually.
        clickView(1, ++expectedSelectionCount, /* expectSelection = */ true);
        clickView(2, ++expectedSelectionCount, /* expectSelection = */ true);
        clickView(3, ++expectedSelectionCount, /* expectSelection = */ true);
        clickView(4, ++expectedSelectionCount, /* expectSelection = */ true);
        clickView(5, ++expectedSelectionCount, /* expectSelection = */ true);

        // Select all should no longer be visible (nothing left to select).
        Assert.assertEquals(View.GONE, action.getVisibility());

        // Deselect one item. The Select All button should re-appear.
        clickView(5, --expectedSelectionCount, /* expectSelection = */ false);
        Assert.assertEquals(View.VISIBLE, action.getVisibility());

        clickActionButton(6, ContactsPickerAction.SELECT_ALL);
        clickActionButton(5, ContactsPickerAction.UNDO_SELECT_ALL);
    }

    @Test
    @LargeTest
    public void testNoSearchStringNoCrash() throws Throwable {
        createDialog(/* multiselect = */ true, Arrays.asList("image/*"));
        Assert.assertTrue(mDialog.isShowing());

        clickSearchButton();
        dismissDialog();
    }
}
