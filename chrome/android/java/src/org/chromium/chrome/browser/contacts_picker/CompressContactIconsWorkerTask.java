// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import android.content.ContentResolver;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.blink.mojom.ContactIconBlob;

import java.io.ByteArrayOutputStream;
import java.util.HashMap;
import java.util.List;
import java.util.Set;

/**
 * A worker task to retrieve images for contacts.
 */
public class CompressContactIconsWorkerTask extends AsyncTask<Void> {
    private ContentResolver mContentResolver;
    private Set<String> mNoIconIds;
    private HashMap<String, Bitmap> mBitmaps;
    private List<ContactDetails> mSelectedContacts;
    private CompressContactIconsCallback mCallback;

    public static boolean sDisableForTesting;

    /**
     * An interface to use to communicate back the results to the client.
     */
    public interface CompressContactIconsCallback {
        /**
         * @param selectedContacts The list of selected contacts with their icons.
         */
        void iconsCompressed(List<ContactDetails> selectedContacts);
    }

    /**
     * @param contentResolver The context's content resolver.
     * @param bitmapCache The bitmap cache holding the icon bitmaps.
     * @param selectedContacts The list of contacts selected by the user.
     * @param callback The callback to return the results to.
     */
    public CompressContactIconsWorkerTask(ContentResolver contentResolver,
            PickerCategoryView.ContactsBitmapCache bitmapCache,
            List<ContactDetails> selectedContacts, CompressContactIconsCallback callback) {
        mContentResolver = contentResolver;
        mNoIconIds = bitmapCache.noIconIds;
        mBitmaps = new HashMap<>();
        for (ContactDetails contact : selectedContacts) {
            mBitmaps.put(contact.getId(), bitmapCache.getBitmap(contact.getId()));
        }
        mSelectedContacts = selectedContacts;
        mCallback = callback;
    }

    @Override
    protected Void doInBackground() {
        assert !ThreadUtils.runningOnUiThread();

        if (sDisableForTesting) {
            return null;
        }

        for (ContactDetails contact : mSelectedContacts) {
            if (mNoIconIds.contains(contact.getId())) {
                continue;
            }
            Bitmap icon = mBitmaps.get(contact.getId());
            if (icon == null) {
                Drawable drawable = contact.isSelf() ? contact.getSelfIcon() : null;
                if (drawable != null && drawable instanceof BitmapDrawable) {
                    icon = ((BitmapDrawable) drawable).getBitmap();
                } else if (!contact.isSelf()) {
                    icon = new FetchIconWorkerTask(contact.getId(), mContentResolver, null)
                                   .doInBackground();
                }
            }

            if (icon == null) {
                continue;
            }

            ByteArrayOutputStream stream = new ByteArrayOutputStream();
            icon.compress(Bitmap.CompressFormat.PNG, 100, stream);

            ContactIconBlob blob = new ContactIconBlob();
            blob.data = stream.toByteArray();
            blob.mimeType = "image/png";
            contact.setIcon(blob);
        }

        return null;
    }

    /**
     * Communicates the results back to the client. Called on the UI thread.
     * @param result Unused Void variable.
     */
    @Override
    protected void onPostExecute(Void result) {
        assert ThreadUtils.runningOnUiThread();

        if (isCancelled()) return;

        mCallback.iconsCompressed(mSelectedContacts);
    }
}
