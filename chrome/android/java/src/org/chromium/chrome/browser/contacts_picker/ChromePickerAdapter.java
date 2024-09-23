// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.components.browser_ui.contacts_picker.ContactDetails;
import org.chromium.components.browser_ui.contacts_picker.PickerAdapter;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;

import java.util.ArrayList;
import java.util.Collections;

/**
 * A {@link PickerAdapter} with special behavior tailored for Chrome.
 *
 * <p>Owner email is looked up in the {@link ProfileDataCache}, or, failing that, via the {@link
 * AccountManagerFacade}.
 */
public class ChromePickerAdapter extends PickerAdapter implements ProfileDataCache.Observer {
    private final Profile mProfile;

    // The profile data cache to consult when figuring out the signed in user.
    private ProfileDataCache mProfileDataCache;

    // Whether an observer for ProfileDataCache has been registered.
    private boolean mObserving;

    // Whether owner info is being fetched asynchronously.
    private boolean mWaitingOnOwnerInfo;

    public ChromePickerAdapter(Context context, Profile profile) {
        mProfile = profile;
        mProfileDataCache =
                ProfileDataCache.createWithoutBadge(context, R.dimen.contact_picker_icon_size);
    }

    // Adapter:

    @Override
    public void onAttachedToRecyclerView(RecyclerView recyclerView) {
        super.onAttachedToRecyclerView(recyclerView);
        addProfileDataObserver();
    }

    @Override
    public void onDetachedFromRecyclerView(RecyclerView recyclerView) {
        super.onDetachedFromRecyclerView(recyclerView);
        removeProfileDataObserver();
    }

    // ProfileDataCache.Observer:

    @Override
    public void onProfileDataUpdated(String accountEmail) {
        if (!mWaitingOnOwnerInfo || !TextUtils.equals(accountEmail, getOwnerEmail())) {
            return;
        }

        // Now that we've received an update for the right accountId, we can stop listening and
        // update our records.
        mWaitingOnOwnerInfo = false;
        removeProfileDataObserver();
        // TODO(finnur): crbug.com/1021477 - Maintain an member instance of this.
        DisplayableProfileData profileData =
                mProfileDataCache.getProfileDataOrDefault(getOwnerEmail());
        ContactDetails contact = getAllContacts().get(0);
        Drawable icon = profileData.getImage();
        contact.setSelfIcon(icon);
        update();
    }

    private void addProfileDataObserver() {
        if (!mObserving) {
            mObserving = true;
            mProfileDataCache.addObserver(this);
        }
    }

    private void removeProfileDataObserver() {
        if (mObserving) {
            mObserving = false;
            mProfileDataCache.removeObserver(this);
        }
    }

    // PickerAdapter:

    /**
     * Returns the email for the currently signed-in user. If that is not available, return the
     * first Google account associated with this phone instead.
     */
    @Override
    protected String findOwnerEmail() {
        CoreAccountInfo coreAccountInfo = getCoreAccountInfo();
        if (coreAccountInfo != null) {
            return coreAccountInfo.getEmail();
        }
        final @Nullable CoreAccountInfo defaultCoreAccountInfo =
                AccountUtils.getDefaultCoreAccountInfoIfFulfilled(
                        AccountManagerFacadeProvider.getInstance().getCoreAccountInfos());
        return defaultCoreAccountInfo != null ? defaultCoreAccountInfo.getEmail() : null;
    }

    @Override
    protected void addOwnerInfoToContacts(ArrayList<ContactDetails> contacts) {
        // Processing was not complete, finish the rest asynchronously. Flow continues in
        // onProfileDataUpdated.
        mWaitingOnOwnerInfo = true;
        addProfileDataObserver();
        contacts.add(0, constructOwnerInfo(getOwnerEmail()));
    }

    /**
     * Constructs a {@link ContactDetails} record for the currently signed in user. Name is obtained
     * via the {@link DisplayableProfileData}, if available, or (alternatively) using the signed in
     * information.
     *
     * @param ownerEmail The email for the currently signed in user.
     * @return The contact info for the currently signed in user.
     */
    @SuppressLint("HardwareIds")
    private ContactDetails constructOwnerInfo(String ownerEmail) {
        DisplayableProfileData profileData = mProfileDataCache.getProfileDataOrDefault(ownerEmail);
        String name = profileData.getFullNameOrEmail();
        if (TextUtils.isEmpty(name) || TextUtils.equals(name, ownerEmail)) {
            name = CoreAccountInfo.getEmailFrom(getCoreAccountInfo());
        }

        ContactDetails contact =
                new ContactDetails(
                        ContactDetails.SELF_CONTACT_ID,
                        name,
                        Collections.singletonList(ownerEmail),
                        /* phoneNumbers= */ null,
                        /* addresses= */ null);
        Drawable icon = profileData.getImage();
        contact.setIsSelf(true);
        contact.setSelfIcon(icon);
        return contact;
    }

    private CoreAccountInfo getCoreAccountInfo() {
        // Since this is read-only operation to obtain email address, always using regular profile
        // for both regular and off-the-record profile is safe.
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile.getOriginalProfile());
        return identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
    }
}
