// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.contacts_picker.ContactsPickerDialog;
import org.chromium.content_public.browser.ContactsFetcher;
import org.chromium.content_public.browser.ContactsPickerDelegate;
import org.chromium.content_public.browser.ContactsPickerListener;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.edge_to_edge.EdgeToEdgeStateProvider;

/** A delegate for the ContactsPicker that shows the Chrome-specific implementation. */
@NullMarked
public class ChromeContactsPickerDelegate implements ContactsPickerDelegate {
    @Override
    public Object showContactsPicker(
            WebContents webContents,
            ContactsPickerListener listener,
            boolean allowMultiple,
            boolean includeNames,
            boolean includeEmails,
            boolean includeTel,
            boolean includeAddresses,
            boolean includeIcons,
            String formattedOrigin,
            ContactsFetcher contactsFetcher) {
        WindowAndroid windowAndroid = webContents.getTopLevelNativeWindow();
        assumeNonNull(windowAndroid);
        boolean shouldDialogPadForContent =
                EdgeToEdgeStateProvider.isEdgeToEdgeEnabledForWindow(windowAndroid);
        Context context = windowAndroid.getContext().get();
        assumeNonNull(context);
        ContactsPickerDialog dialog =
                new ContactsPickerDialog(
                        windowAndroid,
                        new ChromePickerAdapter(context, Profile.fromWebContents(webContents)),
                        listener,
                        allowMultiple,
                        includeNames,
                        includeEmails,
                        includeTel,
                        includeAddresses,
                        includeIcons,
                        formattedOrigin,
                        shouldDialogPadForContent,
                        contactsFetcher);
        assumeNonNull(dialog.getWindow()).getAttributes().windowAnimations =
                R.style.PickerDialogAnimation;
        dialog.show();
        return dialog;
    }
}
