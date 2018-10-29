// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import android.content.ContentResolver;
import android.graphics.Bitmap;
import android.support.v7.widget.RecyclerView.Adapter;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;

import java.util.ArrayList;
import java.util.Locale;

/**
 * A data adapter for the Contacts Picker.
 */
public class PickerAdapter extends Adapter<ContactViewHolder>
        implements ContactsFetcherWorkerTask.ContactsRetrievedCallback {
    // The category view to use to show the contacts.
    private PickerCategoryView mCategoryView;

    // The content resolver to query data from.
    private ContentResolver mContentResolver;

    // The full list of all registered contacts on the device.
    private ArrayList<ContactDetails> mContactDetails;

    // The async worker task to use for fetching the contact details.
    private ContactsFetcherWorkerTask mWorkerTask;

    // A list of search result indices into the larger data set.
    private ArrayList<Integer> mSearchResults;

    // A list of contacts to use for testing (instead of querying Android).
    private static ArrayList<ContactDetails> sTestContacts;

    /**
     * The PickerAdapter constructor.
     * @param categoryView The category view to use to show the contacts.
     * @param contentResolver The content resolver to use to fetch the data.
     */
    public PickerAdapter(PickerCategoryView categoryView, ContentResolver contentResolver) {
        mCategoryView = categoryView;
        mContentResolver = contentResolver;

        if (getAllContacts() == null && sTestContacts == null) {
            mWorkerTask = new ContactsFetcherWorkerTask(mContentResolver, this);
            mWorkerTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        } else {
            mContactDetails = sTestContacts;
            notifyDataSetChanged();
        }
    }

    /**
     * Sets the search query (filter) for the contact list. Filtering is by display name.
     * @param query The search term to use.
     */
    public void setSearchString(String query) {
        if (query.equals("")) {
            if (mSearchResults == null) return;
            mSearchResults.clear();
            mSearchResults = null;
        } else {
            mSearchResults = new ArrayList<Integer>();
            Integer count = 0;
            String query_lower = query.toLowerCase(Locale.getDefault());
            for (ContactDetails contact : mContactDetails) {
                if (contact.getDisplayName().toLowerCase(Locale.getDefault()).contains(query_lower)
                        || contact.getContactDetailsAsString()
                                   .toLowerCase(Locale.getDefault())
                                   .contains(query_lower)) {
                    mSearchResults.add(count);
                }
                count++;
            }
        }
        notifyDataSetChanged();
    }

    /**
     * Fetches all known contacts.
     * @return The contact list as an array.
     */
    public ArrayList<ContactDetails> getAllContacts() {
        return mContactDetails;
    }

    // ContactsFetcherWorkerTask.ContactsRetrievedCallback:

    @Override
    public void contactsRetrieved(ArrayList<ContactDetails> contacts) {
        mContactDetails = contacts;
        notifyDataSetChanged();
    }

    // RecyclerView.Adapter:

    @Override
    public ContactViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        ContactView itemView = (ContactView) LayoutInflater.from(parent.getContext())
                                       .inflate(R.layout.contact_view, parent, false);
        itemView.setCategoryView(mCategoryView);
        return new ContactViewHolder(itemView, mCategoryView, mContentResolver);
    }

    @Override
    public void onBindViewHolder(ContactViewHolder holder, int position) {
        ContactDetails contact;
        if (mSearchResults == null) {
            contact = mContactDetails.get(position);
        } else {
            Integer index = mSearchResults.get(position);
            contact = mContactDetails.get(index);
        }

        holder.setContactDetails(contact);
    }

    private Bitmap getPhoto() {
        return null;
    }

    @Override
    public int getItemCount() {
        if (mSearchResults != null) return mSearchResults.size();
        if (mContactDetails == null) return 0;
        return mContactDetails.size();
    }

    /** Sets a list of contacts to use as data for the dialog. For testing use only. */
    @VisibleForTesting
    public static void setTestContacts(ArrayList<ContactDetails> contacts) {
        sTestContacts = contacts;
    }
}
