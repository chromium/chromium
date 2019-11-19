// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.blink.mojom.ContactIconBlob;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate.SelectionObserver;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.payments.mojom.PaymentAddress;
import org.chromium.ui.ContactsPickerListener;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Callable;

/**
 * Tests for the ContactsPickerDialog class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Features.EnableFeatures({ChromeFeatureList.CONTACTS_PICKER_SELECT_ALL})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ContactsPickerDialogTest
        implements ContactsPickerListener, SelectionObserver<ContactDetails> {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    // The dialog we are testing.
    private ContactsPickerDialog mDialog;

    // The data to show in the dialog.
    private ArrayList<ContactDetails> mTestContacts;

    // The selection delegate for the dialog.
    private SelectionDelegate<ContactDetails> mSelectionDelegate;

    // The last action recorded in the dialog (e.g. contacts selected).
    private @ContactsPickerAction int mLastActionRecorded;

    // The final set of contacts picked by the dialog.
    private List<ContactsPickerListener.Contact> mLastSelectedContacts;

    // The percentage of contacts shared.
    private int mLastPercentageShared;

    // The properties requested (names, emails, telephone numbers) when the
    // dialog was opened (not to be confused with the properties that will
    // eventually be returned, after the user opts-out of some). Note: This is
    // a bitmask, where the first bit is 1 if telephone numbers were requested,
    // second bit is for emails and third bit is for names.
    private int mLastPropertiesRequested;

    // Whether the dialog is being closed as a result of an explicit user action.
    private boolean mClosing;

    // The list of currently selected contacts (built piecemeal).
    private List<ContactDetails> mCurrentContactSelection;

    // An icon to use as contact avatar during testing.
    private Bitmap mIcon;

    // A callback that fires when something is selected in the dialog.
    public final CallbackHelper onSelectionCallback = new CallbackHelper();

    // A callback that fires when an action is taken in the dialog (cancel/done etc).
    public final CallbackHelper onActionCallback = new CallbackHelper();

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mIcon = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(mIcon);
        canvas.drawColor(Color.BLUE);
        ContactViewHolder.setIconForTesting(mIcon);
        // Disable the async task since it tries to access contact icons which would fail in tests.
        CompressContactIconsWorkerTask.sDisableForTesting = true;
    }

    // ContactsPickerDialog.ContactsPickerListener:

    @Override
    public void onContactsPickerUserAction(@ContactsPickerAction int action,
            List<ContactsPickerListener.Contact> contacts, int percentageShared,
            int propertiesRequested) {
        mLastActionRecorded = action;
        mLastSelectedContacts = (contacts != null) ? new ArrayList<>(contacts) : null;
        mLastPercentageShared = percentageShared;
        mLastPropertiesRequested = propertiesRequested;
        onActionCallback.notifyCalled();
    }

    // SelectionObserver:

    @Override
    public void onSelectionStateChange(List<ContactDetails> contactsSelected) {
        mCurrentContactSelection = contactsSelected;
        onSelectionCallback.notifyCalled();
    }

    private RecyclerView getRecyclerView() {
        return (RecyclerView) mDialog.findViewById(R.id.recycler_view);
    }

    /**
     * Creates a ContactPicker dialog with no filtering (default case). For a more fine-grained
     * version, see below.
     */
    private ContactsPickerDialog createDialog(final boolean multiselect) throws Exception {
        return createDialog(multiselect, /* includeNames = */ true,
                /* includeEmails = */ true,
                /* includeTel = */ true,
                /* includeAddresses = */ true,
                /* includeIcons = */ true);
    }

    private ContactsPickerDialog createDialog(final boolean multiselect, final boolean includeNames,
            final boolean includeEmails, final boolean includeTel, final boolean includeAddresses,
            final boolean includeIcons) throws Exception {
        mClosing = false;
        final ContactsPickerDialog dialog =
                TestThreadUtils.runOnUiThreadBlocking(new Callable<ContactsPickerDialog>() {
                    @Override
                    public ContactsPickerDialog call() {
                        final ContactsPickerDialog dialog = new ContactsPickerDialog(
                                mActivityTestRule.getActivity(), ContactsPickerDialogTest.this,
                                multiselect, includeNames, includeEmails, includeTel,
                                includeAddresses, includeIcons, "example.com");
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

    /**
     * Clicks a single view in the RecyclerView that occupies slot |position|. Note that even though
     * the first entry in the RecyclerView is a Select All checkbox, this function automatically
     * skips past it. This way, calling clickView for |position| 0 will click the first Contact in
     * the list (and not the Select All checkbox).
     * @param position The position of the item to click (zero-based), or -1 if the intention is to
     *                 toggle the Select All checkbox.
     * @param expectedSelectionCount The expected selection count after the view has been clicked.
     * @param expectSelection True if the clicked-on view should become selected.
     */
    private void clickView(final int position, final int expectedSelectionCount,
            final boolean expectSelection) throws Exception {
        RecyclerView recyclerView = getRecyclerView();
        RecyclerViewTestUtils.scrollToView(recyclerView, position + 1);

        int callCount = onSelectionCallback.getCallCount();
        TestTouchUtils.performClickOnMainSync(InstrumentationRegistry.getInstrumentation(),
                recyclerView.findViewHolderForAdapterPosition(position + 1).itemView);
        onSelectionCallback.waitForCallback(callCount, 1);

        // Validate the correct selection took place.
        if (position != -1) {
            Assert.assertEquals(expectedSelectionCount, mCurrentContactSelection.size());
            Assert.assertEquals(expectSelection,
                    mSelectionDelegate.isItemSelected(mTestContacts.get(position)));
        }
    }

    /**
     * Clicks a single view in the Recyclerview in search mode.
     * @param position The position of the item to click (zero-based)..
     * @param expectedSelectionCount The expected selection count after the view has been clicked.
     * @param expectSelection True if the clicked-on view should become selected.
     */
    private void clickViewInSearchMode(final int position, final int expectedSelectionCount,
            final boolean expectSelection) throws Exception {
        // Search mode does not have the Select All checkbox, so we don't need to skip it.
        clickView(position - 1, expectedSelectionCount, expectSelection);
    }

    private void clickDone() throws Exception {
        Assert.assertEquals(false, mClosing);
        mClosing = true;

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
        Assert.assertEquals(false, mClosing);
        mClosing = true;

        mLastActionRecorded = ContactsPickerAction.NUM_ENTRIES;

        PickerCategoryView categoryView = mDialog.getCategoryViewForTesting();
        View cancel = new View(mActivityTestRule.getActivity());
        int callCount = onActionCallback.getCallCount();
        categoryView.onClick(cancel);
        onActionCallback.waitForCallback(callCount, 1);
        Assert.assertEquals(ContactsPickerAction.CANCEL, mLastActionRecorded);
    }

    private void toggleSelectAll(final int expectedSelectionCount,
            final @ContactsPickerAction int expectedAction) throws Exception {
        mLastActionRecorded = ContactsPickerAction.NUM_ENTRIES;

        int callCount = onActionCallback.getCallCount();

        // The clickView function automatically skips the Select All checkbox, which is the first
        // item in the list. Compensate for that by passing in -1.
        clickView(-1, expectedSelectionCount,
                expectedAction == ContactsPickerAction.CONTACTS_SELECTED);
        onActionCallback.waitForCallback(callCount, 1);
        Assert.assertEquals(expectedSelectionCount, mSelectionDelegate.getSelectedItems().size());
        Assert.assertEquals(expectedAction, mLastActionRecorded);
    }

    private void notifyChipToggled(@PickerAdapter.FilterType int filter) {
        TopView topView = mDialog.getCategoryViewForTesting().getTopViewForTesting();
        topView.notifyChipToggled(filter);
    }

    private void toggleFilter(@PickerAdapter.FilterType int filter) {
        RecyclerView recyclerView = getRecyclerView();
        RecyclerViewTestUtils.waitForView(recyclerView, 0);

        TestThreadUtils.runOnUiThreadBlocking(() -> notifyChipToggled(filter));
    }

    private void clickSearchButton() {
        ContactsPickerToolbar toolbar =
                (ContactsPickerToolbar) mDialog.findViewById(R.id.action_bar);
        View search = toolbar.findViewById(R.id.search);
        TestTouchUtils.performClickOnMainSync(InstrumentationRegistry.getInstrumentation(), search);
    }

    private void setSearchString(String query, int expectedMatches) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mDialog.getCategoryViewForTesting().onSearchTextChanged(query));
        Assert.assertEquals(expectedMatches, getRecyclerView().getAdapter().getItemCount());
    }

    private void dismissDialog() throws Exception {
        Assert.assertEquals(false, mClosing);
        mClosing = true;

        int callCount = onActionCallback.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> mDialog.cancel());
        onActionCallback.waitForCallback(callCount, 1);
    }

    private TopView getTopView() {
        RecyclerView recyclerView = getRecyclerView();
        RecyclerViewTestUtils.waitForView(recyclerView, 0);
        View view = recyclerView.getLayoutManager().findViewByPosition(0);
        Assert.assertNotNull(view);
        Assert.assertTrue(view instanceof TopView);

        return (TopView) view;
    }

    /**
     * Sets the contacts to use during the test.
     * @param ownerEmail If not null, includes a few contact entries representing owners.
     */
    private void setTestContacts(String ownerEmail) {
        mTestContacts = new ArrayList<ContactDetails>();
        PaymentAddress address = new PaymentAddress();
        address.city = "city";
        address.country = "country";
        address.addressLine = new String[] {"formattedAddress"};
        address.postalCode = "postalCode";
        address.region = "region";
        address.dependentLocality = "";
        address.sortingCode = "";
        address.organization = "";
        address.recipient = "";
        address.phone = "";

        ContactIconBlob icon = new ContactIconBlob();
        icon.data = new byte[] {0x42};
        icon.mimeType = "image/test";

        mTestContacts.add(new ContactDetails("0", "Contact 0", Arrays.asList("0@example.com"),
                Arrays.asList("555-1234"), Arrays.asList(address)));
        mTestContacts.add(new ContactDetails("1", "Contact 1", /*emails=*/null,
                /*phoneNumbers=*/null, /*addresses=*/null));
        mTestContacts.add(new ContactDetails("2", "Contact 2", /*emails=*/null,
                /*phoneNumbers=*/null, /*addresses=*/null));
        mTestContacts.add(new ContactDetails("3", "Contact 3", /*emails=*/null,
                /*phoneNumbers=*/null, /*addresses=*/null));
        mTestContacts.add(new ContactDetails("4", "Contact 4", /*emails=*/null,
                /*phoneNumbers=*/null, /*addresses=*/null));
        mTestContacts.add(new ContactDetails("5", "Contact 5", /*emails=*/null,
                /*phoneNumbers=*/null, /*addresses=*/null));

        if (ownerEmail != null) {
            // Note: The dialog will move Contact 6 (owner) to the top of the list.
            ContactDetails owner = new ContactDetails("6", "Contact 6",
                    Arrays.asList("owner@example.com"), /*phoneNumbers=*/null, /*addresses=*/null);
            owner.setSelfIcon(new BitmapDrawable(mIcon));
            ContactDetails owner2 = new ContactDetails("7", "Contact 7",
                    Arrays.asList("owner@example.com"), /*phoneNumbers=*/null, /*addresses=*/null);
            mTestContacts.add(owner);
            mTestContacts.add(owner2);
        }

        PickerAdapter.setTestContactsAndOwner(mTestContacts, ownerEmail);
    }

    @Test
    @LargeTest
    public void testOriginString() throws Throwable {
        setTestContacts(/*ownerEmail=*/null);
        createDialog(/* multiselect = */ true);
        Assert.assertTrue(mDialog.isShowing());

        TopView topView = getTopView();
        Assert.assertNotNull(topView);

        TextView explanation = (TextView) topView.findViewById(R.id.explanation);
        Assert.assertNotNull(explanation);
        Assert.assertEquals(explanation.getText().toString(),
                "The contacts you select will be shared with example.com.");

        dismissDialog();
    }

    @Test
    @LargeTest
    public void testFilterVisibilityForDataInclusion() throws Throwable {
        setTestContacts(/*ownerEmail=*/null);
        createDialog(/* multiselect = */ false, /* includeNames = */ true,
                /* includeEmails = */ false,
                /* includeTel = */ true,
                /* includeAddresses = */ false,
                /* includeIcons = */ true);
        Assert.assertTrue(mDialog.isShowing());

        TopView topView = getTopView();
        Assert.assertNotNull(topView);

        View namesFilter = topView.findViewById(R.id.names_filter);
        Assert.assertNotNull(namesFilter);
        View emailFilter = topView.findViewById(R.id.email_filter);
        Assert.assertNotNull(emailFilter);
        View telFilter = topView.findViewById(R.id.tel_filter);
        Assert.assertNotNull(telFilter);
        View addrFilter = topView.findViewById(R.id.address_filter);
        Assert.assertNotNull(addrFilter);
        View iconFilter = topView.findViewById(R.id.icon_filter);
        Assert.assertNotNull(iconFilter);

        // Per configuration given in the createDialog() call, the names and telephone filters
        // should be visible, but the e-mail and address filter should be gone.
        Assert.assertEquals(namesFilter.getVisibility(), View.VISIBLE);
        Assert.assertEquals(emailFilter.getVisibility(), View.GONE);
        Assert.assertEquals(telFilter.getVisibility(), View.VISIBLE);
        Assert.assertEquals(addrFilter.getVisibility(), View.GONE);
        Assert.assertEquals(iconFilter.getVisibility(), View.VISIBLE);
    }

    @Test
    @LargeTest
    public void testFilterVisibilityReverseForDataInclusion() throws Throwable {
        setTestContacts(/*ownerEmail=*/null);
        createDialog(/* multiselect = */ false, /* includeNames = */ false,
                /* includeEmails = */ true,
                /* includeTel = */ false,
                /* includeAddresses = */ true,
                /* includeIcons = */ false);
        Assert.assertTrue(mDialog.isShowing());

        TopView topView = getTopView();
        Assert.assertNotNull(topView);

        View namesFilter = topView.findViewById(R.id.names_filter);
        Assert.assertNotNull(namesFilter);
        View emailFilter = topView.findViewById(R.id.email_filter);
        Assert.assertNotNull(emailFilter);
        View telFilter = topView.findViewById(R.id.tel_filter);
        Assert.assertNotNull(telFilter);
        View addrFilter = topView.findViewById(R.id.address_filter);
        Assert.assertNotNull(addrFilter);
        View iconFilter = topView.findViewById(R.id.icon_filter);
        Assert.assertNotNull(iconFilter);

        // Per configuration given in the createDialog() call, the names and telephone filters
        // should be hidden, but the e-mail and address filter should be visible.
        Assert.assertEquals(namesFilter.getVisibility(), View.GONE);
        Assert.assertEquals(emailFilter.getVisibility(), View.VISIBLE);
        Assert.assertEquals(telFilter.getVisibility(), View.GONE);
        Assert.assertEquals(addrFilter.getVisibility(), View.VISIBLE);
        Assert.assertEquals(iconFilter.getVisibility(), View.GONE);
    }

    @Test
    @LargeTest
    public void testNoSelection() throws Throwable {
        setTestContacts(/*ownerEmail=*/"notanowner@example.com");
        createDialog(/* multiselect = */ false);
        Assert.assertTrue(mDialog.isShowing());

        int expectedSelectionCount = 1;
        clickView(0, expectedSelectionCount, /* expectSelection = */ true);
        clickCancel();

        Assert.assertEquals(null, mLastSelectedContacts);
        Assert.assertEquals(0, mLastPercentageShared);
        Assert.assertEquals(31, mLastPropertiesRequested);
        Assert.assertEquals(ContactsPickerAction.CANCEL, mLastActionRecorded);
    }

    @Test
    @LargeTest
    public void testOwnerContact() throws Throwable {
        setTestContacts(/*ownerEmail=*/"owner@example.com");
        createDialog(/* multiselect = */ false);
        Assert.assertTrue(mDialog.isShowing());

        int expectedSelectionCount = 1;
        clickView(0, expectedSelectionCount, /* expectSelection = */ true);
        clickDone();

        Assert.assertEquals(ContactsPickerAction.CONTACTS_SELECTED, mLastActionRecorded);
        Assert.assertEquals(1, mLastSelectedContacts.size());
        Assert.assertEquals(
                mTestContacts.get(0).getDisplayName(), mLastSelectedContacts.get(0).names.get(0));
        Assert.assertEquals(12, mLastPercentageShared);
        Assert.assertEquals(31, mLastPropertiesRequested);
    }

    @Test
    @LargeTest
    public void testSingleSelectionContacts() throws Throwable {
        setTestContacts(/*ownerEmail=*/null);
        createDialog(/* multiselect = */ false);
        Assert.assertTrue(mDialog.isShowing());

        // Expected selection count is 1 because clicking on a new view deselects other.
        int expectedSelectionCount = 1;
        clickView(0, expectedSelectionCount, /* expectSelection = */ true);
        clickView(1, expectedSelectionCount, /* expectSelection = */ true);
        clickDone();

        Assert.assertEquals(ContactsPickerAction.CONTACTS_SELECTED, mLastActionRecorded);
        Assert.assertEquals(1, mLastSelectedContacts.size());
        Assert.assertEquals(
                mTestContacts.get(1).getDisplayName(), mLastSelectedContacts.get(0).names.get(0));
        Assert.assertEquals(16, mLastPercentageShared);
        Assert.assertEquals(31, mLastPropertiesRequested);
    }

    @Test
    @LargeTest
    public void testMultiSelectionContacts() throws Throwable {
        setTestContacts(/*ownerEmail=*/null);
        createDialog(/* multiselect = */ true);
        Assert.assertTrue(mDialog.isShowing());

        // Multi-selection is enabled, so each click is counted.
        int expectedSelectionCount = 0;
        clickView(0, ++expectedSelectionCount, /* expectSelection = */ true);
        clickView(2, ++expectedSelectionCount, /* expectSelection = */ true);
        clickView(4, ++expectedSelectionCount, /* expectSelection = */ true);
        clickDone();

        Assert.assertEquals(ContactsPickerAction.CONTACTS_SELECTED, mLastActionRecorded);
        Assert.assertEquals(3, mLastSelectedContacts.size());
        Assert.assertEquals(
                mTestContacts.get(4).getDisplayName(), mLastSelectedContacts.get(0).names.get(0));
        Assert.assertEquals(
                mTestContacts.get(2).getDisplayName(), mLastSelectedContacts.get(1).names.get(0));
        Assert.assertEquals(
                mTestContacts.get(0).getDisplayName(), mLastSelectedContacts.get(2).names.get(0));
        Assert.assertEquals(50, mLastPercentageShared);
        Assert.assertEquals(31, mLastPropertiesRequested);
    }

    @Test
    @LargeTest
    public void testNamesRemoved() throws Throwable {
        setTestContacts(/*ownerEmail=*/null);
        createDialog(/* multiselect = */ false);
        Assert.assertTrue(mDialog.isShowing());

        toggleFilter(PickerAdapter.FilterType.NAMES);

        int expectedSelectionCount = 1;
        clickView(0, expectedSelectionCount, /* expectSelection = */ true);
        clickDone();

        Assert.assertEquals(ContactsPickerAction.CONTACTS_SELECTED, mLastActionRecorded);
        Assert.assertEquals(1, mLastSelectedContacts.size());
        Assert.assertEquals(new ArrayList<String>(), mLastSelectedContacts.get(0).names);
        Assert.assertEquals(mTestContacts.get(0).getEmails().get(0),
                mLastSelectedContacts.get(0).emails.get(0));
        Assert.assertEquals(mTestContacts.get(0).getPhoneNumbers().get(0),
                mLastSelectedContacts.get(0).tel.get(0));

        PaymentAddress selectedAddress =
                PaymentAddress.deserialize(mLastSelectedContacts.get(0).serializedAddresses.get(0));
        PaymentAddress testAddress = mTestContacts.get(0).getAddresses().get(0);
        Assert.assertEquals(selectedAddress.city, testAddress.city);
        Assert.assertEquals(selectedAddress.country, testAddress.country);
        Assert.assertEquals(selectedAddress.addressLine[0], testAddress.addressLine[0]);
        Assert.assertEquals(selectedAddress.postalCode, testAddress.postalCode);
        Assert.assertEquals(selectedAddress.region, testAddress.region);

        Assert.assertEquals(16, mLastPercentageShared);
        Assert.assertEquals(31, mLastPropertiesRequested);
    }

    @Test
    @LargeTest
    public void testEmailsRemoved() throws Throwable {
        setTestContacts(/*ownerEmail=*/null);
        createDialog(/* multiselect = */ false);
        Assert.assertTrue(mDialog.isShowing());

        toggleFilter(PickerAdapter.FilterType.EMAILS);

        int expectedSelectionCount = 1;
        clickView(0, expectedSelectionCount, /* expectSelection = */ true);
        clickDone();

        Assert.assertEquals(ContactsPickerAction.CONTACTS_SELECTED, mLastActionRecorded);
        Assert.assertEquals(1, mLastSelectedContacts.size());
        Assert.assertEquals(
                mTestContacts.get(0).getDisplayName(), mLastSelectedContacts.get(0).names.get(0));
        Assert.assertEquals(new ArrayList<String>(), mLastSelectedContacts.get(0).emails);
        Assert.assertEquals(16, mLastPercentageShared);
        Assert.assertEquals(31, mLastPropertiesRequested);
    }

    @Test
    @LargeTest
    public void testTelephonesRemoved() throws Throwable {
        setTestContacts(/*ownerEmail=*/null);
        createDialog(/* multiselect = */ false);
        Assert.assertTrue(mDialog.isShowing());

        toggleFilter(PickerAdapter.FilterType.TELEPHONES);

        int expectedSelectionCount = 1;
        clickView(0, expectedSelectionCount, /* expectSelection = */ true);
        clickDone();

        Assert.assertEquals(ContactsPickerAction.CONTACTS_SELECTED, mLastActionRecorded);
        Assert.assertEquals(1, mLastSelectedContacts.size());
        Assert.assertEquals(
                mTestContacts.get(0).getDisplayName(), mLastSelectedContacts.get(0).names.get(0));
        Assert.assertEquals(new ArrayList<String>(), mLastSelectedContacts.get(0).tel);
        Assert.assertEquals(16, mLastPercentageShared);
        Assert.assertEquals(31, mLastPropertiesRequested);
    }

    // TODO(crbug.com/1020564): Add a test (once icons have been implemented end-to-end) that
    //                          validates that icons are not returned when the filter chip is off.

    @Test
    @LargeTest
    public void testPropertiesRequested() throws Throwable {
        setTestContacts(/*ownerEmail=*/null);
        // Create a dialog showing names only.
        createDialog(/* multiselect = */ false, /* includeNames = */ true,
                /* includeEmails = */ false,
                /* includeTel = */ false,
                /* includeAddresses = */ false,
                /* includeIcons = */ false);
        Assert.assertTrue(mDialog.isShowing());
        clickCancel();
        Assert.assertEquals(4, mLastPropertiesRequested);

        // Create a dialog showing emails only.
        createDialog(/* multiselect = */ false, /* includeNames = */ false,
                /* includeEmails = */ true,
                /* includeTel = */ false,
                /* includeAddresses = */ false,
                /* includeIcons = */ false);
        Assert.assertTrue(mDialog.isShowing());
        clickCancel();
        Assert.assertEquals(2, mLastPropertiesRequested);

        // Create a dialog showing telephone numbers only.
        createDialog(/* multiselect = */ false, /* includeNames = */ false,
                /* includeEmails = */ false,
                /* includeTel = */ true,
                /* includeAddresses = */ false,
                /* includeIcons = */ false);
        Assert.assertTrue(mDialog.isShowing());
        clickCancel();
        Assert.assertEquals(1, mLastPropertiesRequested);

        // Create a dialog showing addresses only.
        createDialog(/* multiselect = */ false, /* includeNames = */ false,
                /* includeEmails = */ false,
                /* includeTel = */ false,
                /* includeAddresses = */ true,
                /* includeIcons = */ false);
        Assert.assertTrue(mDialog.isShowing());
        clickCancel();
        Assert.assertEquals(8, mLastPropertiesRequested);

        // Create a dialog showing icons only.
        createDialog(/* multiselect = */ false, /* includeNames = */ false,
                /* includeEmails = */ false,
                /* includeTel = */ false,
                /* includeAddresses = */ false,
                /* includeIcons = */ true);
        Assert.assertTrue(mDialog.isShowing());
        clickCancel();
        Assert.assertEquals(16, mLastPropertiesRequested);
    }

    @Test
    @LargeTest
    public void testSelfIconCompressed() throws Throwable {
        CompressContactIconsWorkerTask.sDisableForTesting = false;

        setTestContacts(/* ownerEmail = */ "owner@example.com");
        createDialog(/* multiselect = */ false);
        Assert.assertTrue(mDialog.isShowing());

        int expectedSelectionCount = 1;
        clickView(0, expectedSelectionCount, /* expectSelection = */ true);
        clickDone();

        Assert.assertEquals(ContactsPickerAction.CONTACTS_SELECTED, mLastActionRecorded);
        Assert.assertEquals(1, mLastSelectedContacts.size());

        ContactIconBlob icon =
                ContactIconBlob.deserialize(mLastSelectedContacts.get(0).serializedIcons.get(0));
        Assert.assertTrue(icon.data.length > 0);
        Assert.assertEquals("image/png", icon.mimeType);
    }

    @Test
    @LargeTest
    public void testSelectAll() throws Throwable {
        setTestContacts(/*ownerEmail=*/"owner@example.com");
        createDialog(/* multiselect = */ true);
        Assert.assertTrue(mDialog.isShowing());

        toggleSelectAll(8, ContactsPickerAction.SELECT_ALL);
        toggleSelectAll(0, ContactsPickerAction.UNDO_SELECT_ALL);

        // Manually select one item.
        clickView(0, /* expectedSelectionCount = */ 1, /* expectSelection = */ true);

        toggleSelectAll(8, ContactsPickerAction.SELECT_ALL);
        toggleSelectAll(0, ContactsPickerAction.UNDO_SELECT_ALL);

        // Select the rest of the items manually.
        int expectedSelectionCount = 0;
        clickView(1, ++expectedSelectionCount, /* expectSelection = */ true);
        clickView(2, ++expectedSelectionCount, /* expectSelection = */ true);
        clickView(3, ++expectedSelectionCount, /* expectSelection = */ true);
        clickView(4, ++expectedSelectionCount, /* expectSelection = */ true);
        clickView(5, ++expectedSelectionCount, /* expectSelection = */ true);
        clickView(6, ++expectedSelectionCount, /* expectSelection = */ true);
        clickView(7, ++expectedSelectionCount, /* expectSelection = */ true);

        toggleSelectAll(8, ContactsPickerAction.SELECT_ALL);
        toggleSelectAll(0, ContactsPickerAction.UNDO_SELECT_ALL);
    }

    @Test
    @LargeTest
    public void testSearchString() throws Throwable {
        setTestContacts(/*ownerEmail=*/null);
        createDialog(/* multiselect = */ false);
        Assert.assertTrue(mDialog.isShowing());

        clickSearchButton();
        setSearchString("NoMatches", /*expectedMatches=*/0);
        setSearchString("Contact", /*expectedMatches=*/6);
        setSearchString("Contact 3", /*expectedMatches=*/1);

        int expectedSelectionCount = 1;
        clickViewInSearchMode(0, expectedSelectionCount, /* expectSelection = */ true);
        clickDone();

        Assert.assertEquals(ContactsPickerAction.CONTACTS_SELECTED, mLastActionRecorded);
        Assert.assertEquals(1, mLastSelectedContacts.size());
        Assert.assertEquals(
                mTestContacts.get(3).getDisplayName(), mLastSelectedContacts.get(0).names.get(0));
        Assert.assertEquals(16, mLastPercentageShared);
        Assert.assertEquals(31, mLastPropertiesRequested);
    }

    @Test
    @LargeTest
    public void testNoSearchStringNoCrash() throws Throwable {
        setTestContacts(/*ownerEmail=*/null);
        createDialog(/* multiselect = */ true);
        Assert.assertTrue(mDialog.isShowing());

        clickSearchButton();
        dismissDialog();
    }

    @Test
    @LargeTest
    public void testEmptyContactListCrash() throws Throwable {
        PickerAdapter.setTestContactsAndOwner(new ArrayList<ContactDetails>(), null);

        createDialog(/* multiselect = */ true);
        Assert.assertTrue(mDialog.isShowing());

        dismissDialog();
    }
}
