// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import android.content.ContentResolver;
import android.graphics.Bitmap;
import android.support.v7.widget.RecyclerView.ViewHolder;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.task.AsyncTask;

/**
 * Holds on to a {@link ContactView} that displays information about a contact.
 */
public class ContactViewHolder
        extends ViewHolder implements FetchIconWorkerTask.IconRetrievedCallback {
    // Our parent category.
    private final PickerCategoryView mCategoryView;

    // The Content Resolver to use for the lookup.
    private final ContentResolver mContentResolver;

    // The contact view we are holding on to.
    private final ContactView mItemView;

    // The details for the contact.
    private ContactDetails mContact;

    // A worker task for asynchronously retrieving icons off the main thread.
    private FetchIconWorkerTask mWorkerTask;

    // The icon to use when testing.
    private static Bitmap sIconForTest;

    /**
     * The PickerBitmapViewHolder.
     * @param itemView The {@link ContactView} for the contact.
     * @param categoryView The {@link PickerCategoryView} showing the contacts.
     * @param contentResolver The {@link ContentResolver} to use for the lookup.
     */
    public ContactViewHolder(ContactView itemView, PickerCategoryView categoryView,
            ContentResolver contentResolver) {
        super(itemView);
        mCategoryView = categoryView;
        mContentResolver = contentResolver;
        mItemView = itemView;
    }

    /**
     * Sets the contact details to show in the itemview. If the image is not found in the cache,
     * an asynchronous worker task is created to load it.
     * @param contact The contact details to show.
     */
    public void setContactDetails(ContactDetails contact) {
        mContact = contact;

        if (sIconForTest != null) {
            mItemView.initialize(contact, sIconForTest);
            return;
        }

        Bitmap icon = mCategoryView.getIconCache().getBitmap(mContact.getId());
        if (icon == null) {
            mWorkerTask = new FetchIconWorkerTask(mContact.getId(), mContentResolver, this);
            mWorkerTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }
        mItemView.initialize(contact, icon);
    }

    /**
     * Cancels the worker task to retrieve the icon.
     */
    public void cancelIconRetrieval() {
        mWorkerTask.cancel(true);
        mWorkerTask = null;
    }

    // FetchIconWorkerTask.IconRetrievedCallback:

    @Override
    public void iconRetrieved(Bitmap icon, String contactId) {
        if (icon == null) return;
        if (!contactId.equals(mContact.getId())) return;

        if (mCategoryView.getIconCache().getBitmap(contactId) == null) {
            mCategoryView.getIconCache().putBitmap(contactId, icon);
        }

        mItemView.setIconBitmap(icon);
    }

    /** Sets the icon to use when testing. */
    @VisibleForTesting
    public static void setIconForTesting(Bitmap icon) {
        sIconForTest = icon;
    }
}
