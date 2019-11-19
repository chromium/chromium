// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import android.content.Context;
import android.content.DialogInterface;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.GlobalDiscardableReferencePool;
import org.chromium.chrome.browser.ui.widget.RoundedIconGenerator;
import org.chromium.chrome.browser.util.BitmapCache;
import org.chromium.chrome.browser.util.ConversionUtils;
import org.chromium.chrome.browser.widget.selection.SelectableListLayout;
import org.chromium.chrome.browser.widget.selection.SelectableListToolbar;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.content.browser.contacts.ContactsPickerPropertiesRequested;
import org.chromium.ui.ContactsPickerListener;
import org.chromium.ui.UiUtils;
import org.chromium.ui.widget.OptimizedFrameLayout;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * A class for keeping track of common data associated with showing contact details in
 * the contacts picker, for example the RecyclerView.
 */
public class PickerCategoryView extends OptimizedFrameLayout
        implements View.OnClickListener, RecyclerView.RecyclerListener,
                   SelectionDelegate.SelectionObserver<ContactDetails>,
                   SelectableListToolbar.SearchDelegate, TopView.SelectAllToggleCallback,
                   CompressContactIconsWorkerTask.CompressContactIconsCallback {
    // These values are written to logs.  New enum values can be added, but existing
    // enums must never be renumbered or deleted and reused.
    private static final int ACTION_CANCEL = 0;
    private static final int ACTION_CONTACTS_SELECTED = 1;
    private static final int ACTION_BOUNDARY = 2;

    // Constants for the RoundedIconGenerator.
    private static final int ICON_SIZE_DP = 36;
    private static final int ICON_CORNER_RADIUS_DP = 20;
    private static final int ICON_TEXT_SIZE_DP = 12;

    // The dialog that owns us.
    private ContactsPickerDialog mDialog;

    // The view containing the RecyclerView and the toolbar, etc.
    private SelectableListLayout<ContactDetails> mSelectableListLayout;

    // Our activity.
    private ChromeActivity mActivity;

    // The callback to notify the listener of decisions reached in the picker.
    private ContactsPickerListener mListener;

    // The toolbar located at the top of the dialog.
    private ContactsPickerToolbar mToolbar;

    // The RecyclerView showing the images.
    private RecyclerView mRecyclerView;

    // The view at the top (showing the explanation and Select All checkbox).
    private TopView mTopView;

    // The {@link PickerAdapter} for the RecyclerView.
    private PickerAdapter mPickerAdapter;

    // The layout manager for the RecyclerView.
    private LinearLayoutManager mLayoutManager;

    // A helper class to draw the icon for each contact.
    private RoundedIconGenerator mIconGenerator;

    // The {@link SelectionDelegate} keeping track of which contacts are selected.
    private SelectionDelegate<ContactDetails> mSelectionDelegate;

    // A cache for contact images, lazily created.
    private ContactsBitmapCache mBitmapCache;

    // The search icon.
    private ImageView mSearchButton;

    // Keeps track of the set of last selected contacts in the UI.
    Set<ContactDetails> mPreviousSelection;

    // The Done text button that confirms the selection choice.
    private Button mDoneButton;

    // Whether the picker is in multi-selection mode.
    private boolean mMultiSelectionAllowed;

    // Whether the contacts data returned includes names.
    public final boolean includeNames;

    // Whether the contacts data returned includes emails.
    public final boolean includeEmails;

    // Whether the contacts data returned includes telephone numbers.
    public final boolean includeTel;

    // Whether the contacts data returned includes addresses.
    public final boolean includeAddresses;

    // Whether the contacts data returned includes icons.
    public final boolean includeIcons;

    /**
     * @param multiSelectionAllowed Whether the contacts picker should allow multiple items to be
     * selected.
     */
    @SuppressWarnings("unchecked") // mSelectableListLayout
    public PickerCategoryView(Context context, boolean multiSelectionAllowed,
            boolean shouldIncludeNames, boolean shouldIncludeEmails, boolean shouldIncludeTel,
            boolean shouldIncludeAddresses, boolean shouldIncludeIcons, String formattedOrigin,
            ContactsPickerToolbar.ContactsToolbarDelegate delegate) {
        super(context, null);

        mActivity = (ChromeActivity) context;
        mMultiSelectionAllowed = multiSelectionAllowed;
        includeNames = shouldIncludeNames;
        includeEmails = shouldIncludeEmails;
        includeTel = shouldIncludeTel;
        includeAddresses = shouldIncludeAddresses;
        includeIcons = shouldIncludeIcons;

        mSelectionDelegate = new SelectionDelegate<ContactDetails>();
        if (!multiSelectionAllowed) mSelectionDelegate.setSingleSelectionMode();
        mSelectionDelegate.addObserver(this);

        Resources resources = context.getResources();
        int iconColor =
                ApiCompatibilityUtils.getColor(resources, R.color.default_favicon_background_color);
        mIconGenerator = new RoundedIconGenerator(resources, ICON_SIZE_DP, ICON_SIZE_DP,
                ICON_CORNER_RADIUS_DP, iconColor, ICON_TEXT_SIZE_DP);

        View root = LayoutInflater.from(context).inflate(R.layout.contacts_picker_dialog, this);
        mSelectableListLayout =
                (SelectableListLayout<ContactDetails>) root.findViewById(R.id.selectable_list);
        mSelectableListLayout.initializeEmptyView(
                R.string.contacts_picker_no_contacts_found,
                R.string.contacts_picker_no_contacts_found);

        mPickerAdapter = new PickerAdapter(this, context, formattedOrigin);
        mRecyclerView = mSelectableListLayout.initializeRecyclerView(mPickerAdapter);
        int titleId = multiSelectionAllowed ? R.string.contacts_picker_select_contacts
                                            : R.string.contacts_picker_select_contact;
        mToolbar = (ContactsPickerToolbar) mSelectableListLayout.initializeToolbar(
                R.layout.contacts_picker_toolbar, mSelectionDelegate, titleId, 0, 0, null, false,
                false);
        mToolbar.setNavigationOnClickListener(this);
        mToolbar.initializeSearchView(this, R.string.contacts_picker_search, 0);
        mToolbar.setDelegate(delegate);
        mSelectableListLayout.configureWideDisplayStyle();

        mSearchButton = (ImageView) mToolbar.findViewById(R.id.search);
        mSearchButton.setOnClickListener(this);
        mDoneButton = (Button) mToolbar.findViewById(R.id.done);
        mDoneButton.setOnClickListener(this);

        mLayoutManager = new LinearLayoutManager(context);
        mRecyclerView.setHasFixedSize(true);
        mRecyclerView.setLayoutManager(mLayoutManager);

        mBitmapCache = new ContactsBitmapCache();
    }

    /**
     * Initializes the PickerCategoryView object.
     * @param dialog The dialog showing us.
     * @param listener The listener who should be notified of actions.
     */
    public void initialize(ContactsPickerDialog dialog, ContactsPickerListener listener) {
        mDialog = dialog;
        mListener = listener;

        mDialog.setOnCancelListener(new DialogInterface.OnCancelListener() {
            @Override
            public void onCancel(DialogInterface dialog) {
                executeAction(
                        ContactsPickerListener.ContactsPickerAction.CANCEL, null, ACTION_CANCEL);
            }
        });

        mPickerAdapter.notifyDataSetChanged();
    }

    private void onStartSearch() {
        mDoneButton.setVisibility(GONE);

        // Showing the search clears current selection. Save it, so we can restore it after the
        // search has completed.
        mPreviousSelection = new HashSet<ContactDetails>(mSelectionDelegate.getSelectedItems());
        mSearchButton.setVisibility(GONE);
        mPickerAdapter.setSearchMode(true);
        mToolbar.showSearchView();
    }

    // SelectableListToolbar.SearchDelegate:

    @Override
    public void onEndSearch() {
        mPickerAdapter.setSearchString("");
        mPickerAdapter.setSearchMode(false);
        mToolbar.setNavigationOnClickListener(this);
        mDoneButton.setVisibility(VISIBLE);
        mSearchButton.setVisibility(VISIBLE);

        // Hiding the search view clears the selection. Save it first and restore to the old
        // selection, with the new item added during search.
        // TODO(finnur): This needs to be revisited after UX is finalized.
        HashSet<ContactDetails> selection = new HashSet<>();
        for (ContactDetails item : mSelectionDelegate.getSelectedItems()) {
            selection.add(item);
        }
        mToolbar.hideSearchView();
        for (ContactDetails item : mPreviousSelection) {
            selection.add(item);
        }

        // Post a runnable to update the selection so that the update occurs after the search fully
        // finishes, ensuring the number roll shows the right number.
        getHandler().post(() -> mSelectionDelegate.setSelectedItems(selection));
    }

    @Override
    public void onSearchTextChanged(String query) {
        mPickerAdapter.setSearchString(query);
    }

    // SelectionDelegate.SelectionObserver:

    @Override
    public void onSelectionStateChange(List<ContactDetails> selectedItems) {
        // Once a selection is made, drop out of search mode. Note: This function is also called
        // when entering search mode (with selectedItems then being 0 in size).
        if (mToolbar.isSearching() && selectedItems.size() > 0) {
            mToolbar.hideSearchView();
        }

        boolean allSelected = selectedItems.size() == mPickerAdapter.getItemCount() - 1;
        if (mTopView != null) mTopView.updateSelectAllCheckbox(allSelected);
    }

    // RecyclerView.RecyclerListener:

    @Override
    public void onViewRecycled(RecyclerView.ViewHolder holder) {
        ContactViewHolder bitmapHolder = (ContactViewHolder) holder;
        bitmapHolder.cancelIconRetrieval();
    }

    // TopView.SelectAllToggleCallback:

    @Override
    public void onSelectAllToggled(boolean allSelected) {
        if (allSelected) {
            mPreviousSelection = mSelectionDelegate.getSelectedItems();
            mSelectionDelegate.setSelectedItems(
                    new HashSet<ContactDetails>(mPickerAdapter.getAllContacts()));
            mListener.onContactsPickerUserAction(
                    ContactsPickerListener.ContactsPickerAction.SELECT_ALL, /*contacts=*/null,
                    /*percentageShared=*/0, /*propertiesRequested=*/0);
        } else {
            mSelectionDelegate.setSelectedItems(new HashSet<ContactDetails>());
            mPreviousSelection = null;
            mListener.onContactsPickerUserAction(
                    ContactsPickerListener.ContactsPickerAction.UNDO_SELECT_ALL, /*contacts=*/null,
                    /*percentageShared=*/0, /*propertiesRequested=*/0);
        }
    }

    // OnClickListener:

    @Override
    public void onClick(View view) {
        int id = view.getId();
        if (id == R.id.done) {
            prepareContactsSelected();
        } else if (id == R.id.search) {
            onStartSearch();
        } else {
            executeAction(ContactsPickerListener.ContactsPickerAction.CANCEL, null, ACTION_CANCEL);
        }
    }

    // Simple getters and setters:

    SelectionDelegate<ContactDetails> getSelectionDelegate() {
        return mSelectionDelegate;
    }

    RoundedIconGenerator getIconGenerator() {
        return mIconGenerator;
    }

    ContactsBitmapCache getIconCache() {
        return mBitmapCache;
    }

    ChromeActivity getActivity() {
        return mActivity;
    }

    void setTopView(TopView topView) {
        mTopView = topView;
    }

    boolean multiSelectionAllowed() {
        return mMultiSelectionAllowed;
    }

    /**
     * Formats the selected contacts before notifying the listeners.
     */
    private void prepareContactsSelected() {
        List<ContactDetails> selectedContacts = mSelectionDelegate.getSelectedItemsAsList();
        Collections.sort(selectedContacts);

        if (includeIcons && PickerAdapter.includesIcons()) {
            // Fetch missing icons and compress them first.
            new CompressContactIconsWorkerTask(
                    mActivity.getContentResolver(), mBitmapCache, selectedContacts, this)
                    .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            return;
        }

        notifyContactsSelected(selectedContacts);
    }

    @Override
    public void iconsCompressed(List<ContactDetails> selectedContacts) {
        notifyContactsSelected(selectedContacts);
    }

    /**
     * @param isIncluded Whether the property was requested by the API.
     * @param isEnabled Whether the property was allowed to be shared by the user.
     * @param selected The property values that are currently selected.
     * @return The list of property values to share.
     */
    private <T> List<T> getContactPropertyValues(
            boolean isIncluded, boolean isEnabled, List<T> selected) {
        if (!isIncluded) {
            // The property wasn't requested in the API so return null.
            return null;
        }

        if (!isEnabled) {
            // The user doesn't want to share this property, so return an empty array.
            return new ArrayList<T>();
        }

        // Share whatever was selected.
        return selected;
    }

    /**
     * Notifies any listeners that one or more contacts have been selected.
     */
    private void notifyContactsSelected(List<ContactDetails> selectedContacts) {
        List<ContactsPickerListener.Contact> contacts = new ArrayList<>();

        for (ContactDetails contactDetails : selectedContacts) {
            contacts.add(new ContactsPickerListener.Contact(
                    getContactPropertyValues(includeNames, PickerAdapter.includesNames(),
                            contactDetails.getDisplayNames()),
                    getContactPropertyValues(includeEmails, PickerAdapter.includesEmails(),
                            contactDetails.getEmails()),
                    getContactPropertyValues(includeTel, PickerAdapter.includesTelephones(),
                            contactDetails.getPhoneNumbers()),
                    getContactPropertyValues(includeAddresses, PickerAdapter.includesAddresses(),
                            contactDetails.getAddresses()),
                    getContactPropertyValues(includeIcons, PickerAdapter.includesIcons(),
                            contactDetails.getIcons())));
        }

        executeAction(ContactsPickerListener.ContactsPickerAction.CONTACTS_SELECTED, contacts,
                ACTION_CONTACTS_SELECTED);
    }

    /**
     * Report back what the user selected in the dialog, report UMA and clean up.
     * @param action The action taken.
     * @param contacts The contacts that were selected (if any).
     * @param umaId The UMA value to record with the action.
     */
    private void executeAction(@ContactsPickerListener.ContactsPickerAction int action,
            List<ContactsPickerListener.Contact> contacts, int umaId) {
        int selectCount = contacts != null ? contacts.size() : 0;
        int contactCount = mPickerAdapter.getAllContacts().size();
        int percentageShared = contactCount > 0 ? (100 * selectCount) / contactCount : 0;

        int propertiesRequested = ContactsPickerPropertiesRequested.PROPERTIES_NONE;
        if (includeNames) propertiesRequested |= ContactsPickerPropertiesRequested.PROPERTIES_NAMES;
        if (includeEmails) {
            propertiesRequested |= ContactsPickerPropertiesRequested.PROPERTIES_EMAILS;
        }
        if (includeTel) propertiesRequested |= ContactsPickerPropertiesRequested.PROPERTIES_TELS;
        if (includeAddresses) {
            propertiesRequested |= ContactsPickerPropertiesRequested.PROPERTIES_ADDRESSES;
        }
        if (includeIcons) {
            propertiesRequested |= ContactsPickerPropertiesRequested.PROPERTIES_ICONS;
        }

        mListener.onContactsPickerUserAction(
                action, contacts, percentageShared, propertiesRequested);
        mDialog.dismiss();
        UiUtils.onContactsPickerDismissed();
        recordFinalUmaStats(
                umaId, contactCount, selectCount, percentageShared, propertiesRequested);
    }

    /**
     * Record UMA statistics (what action was taken in the dialog and other performance stats).
     * @param action The action the user took in the dialog.
     * @param contactCount The number of contacts in the contact list.
     * @param selectCount The number of contacts selected.
     * @param percentageShared The percentage shared (of the whole contact list).
     * @param propertiesRequested The properties (names/emails/tels) requested by the website.
     */
    private void recordFinalUmaStats(int action, int contactCount, int selectCount,
            int percentageShared, int propertiesRequested) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.ContactsPicker.DialogAction", action, ACTION_BOUNDARY);
        RecordHistogram.recordCountHistogram("Android.ContactsPicker.ContactCount", contactCount);
        RecordHistogram.recordCountHistogram("Android.ContactsPicker.SelectCount", selectCount);
        RecordHistogram.recordPercentageHistogram(
                "Android.ContactsPicker.SelectPercentage", percentageShared);
        RecordHistogram.recordEnumeratedHistogram("Android.ContactsPicker.PropertiesRequested",
                propertiesRequested, ContactsPickerPropertiesRequested.PROPERTIES_BOUNDARY);
    }

    @VisibleForTesting
    public SelectionDelegate<ContactDetails> getSelectionDelegateForTesting() {
        return mSelectionDelegate;
    }

    @VisibleForTesting
    public TopView getTopViewForTesting() {
        return mTopView;
    }

    // A wrapper around BitmapCache to keep track of contacts that don't have an icon.
    protected static class ContactsBitmapCache {
        public BitmapCache bitmapCache;
        public Set<String> noIconIds;

        public ContactsBitmapCache() {
            // Each image (on a Pixel 2 phone) is about 30-40K. Calculate a proportional amount of
            // the available memory, but cap it at 5MB.
            final long maxMemory =
                    ConversionUtils.bytesToKilobytes(Runtime.getRuntime().maxMemory());
            int iconCacheSizeKb = (int) (maxMemory / 8); // 1/8th of the available memory.
            bitmapCache = new BitmapCache(GlobalDiscardableReferencePool.getReferencePool(),
                    Math.min(iconCacheSizeKb, 5 * ConversionUtils.BYTES_PER_MEGABYTE));

            noIconIds = new HashSet<>();
        }

        public Bitmap getBitmap(String id) {
            return bitmapCache.getBitmap(id);
        }

        public void putBitmap(String id, Bitmap icon) {
            if (icon == null) {
                noIconIds.add(id);
            } else {
                bitmapCache.putBitmap(id, icon);
                noIconIds.remove(id);
            }
        }
    }
}
