// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.text.TextUtils;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.ProfileDataUtils;
import org.chromium.components.browser_ui.contacts_picker.ContactDetails;
import org.chromium.components.browser_ui.contacts_picker.PickerAdapter;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.google_apis.gaia.CoreAccountId;

import java.util.ArrayList;
import java.util.Collections;

/**
 * A {@link PickerAdapter} with special behavior tailored for Chrome.
 *
 * <p>Owner email is looked up in the {@link ProfileDataCache}, or, failing that, via the {@link
 * AccountManagerFacade}.
 */
@NullMarked
public class ChromePickerAdapter extends PickerAdapter implements ProfileDataCache.Observer {
    private final IdentityManager mIdentityManager;

    // The profile data cache to consult when figuring out the signed in user.
    private final ProfileDataCache mProfileDataCache;

    // Whether an observer for ProfileDataCache has been registered.
    private boolean mObserving;

    // Whether owner info is being fetched asynchronously.
    private boolean mWaitingOnOwnerInfo;

    public ChromePickerAdapter(Context context, Profile profile) {
        mIdentityManager =
                assertNonNull(
                        IdentityServicesProvider.get()
                                .getIdentityManager(profile.getOriginalProfile()));
        mProfileDataCache =
                ProfileDataCache.createWithoutBadge(
                        context, mIdentityManager, R.dimen.contact_picker_icon_size);
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
    // TODO(finnur): crbug.com/40106180 - Maintain an member instance of this.
    public void onProfileDataUpdated(DisplayableProfileData profileData) {
        String ownerEmail = getOwnerEmail();
        if (!mWaitingOnOwnerInfo || !TextUtils.equals(profileData.getAccountEmail(), ownerEmail)) {
            return;
        }
        assumeNonNull(ownerEmail);

        // Now that we've received an update for the right accountId, we can stop listening and
        // update our records.
        mWaitingOnOwnerInfo = false;
        removeProfileDataObserver();
        assumeNonNull(getAllContacts());
        ContactDetails contact = getAllContacts().get(0);
        contact.setSelfIcon(profileData.getImage());
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
    protected @Nullable String findOwnerEmail() {
        @Nullable AccountInfo signedInAccountInfo = mIdentityManager.getPrimaryAccountInfo();
        if (signedInAccountInfo != null) {
            return signedInAccountInfo.getEmail();
        }
        @Nullable DisplayableProfileData profileData =
                ProfileDataUtils.getFirstIfFulfilledAndNotEmpty(mProfileDataCache.getAccounts());
        if (profileData != null) {
            return profileData.getAccountEmail();
        }
        return null;
    }

    @Override
    protected void addOwnerInfoToContacts(ArrayList<ContactDetails> contacts, String ownerEmail) {
        // Processing was not complete, finish the rest asynchronously. Flow continues in
        // onProfileDataUpdated.
        mWaitingOnOwnerInfo = true;
        addProfileDataObserver();
        var ownerAccount = mIdentityManager.findExtendedAccountInfoByEmailAddress(ownerEmail);
        if (ownerAccount != null) {
            contacts.add(0, constructOwnerInfo(ownerAccount.getId()));
        }
    }

    /**
     * Constructs a {@link ContactDetails} record for the currently signed in user. Name is obtained
     * via the {@link DisplayableProfileData}.
     *
     * @param ownerEmail The email for the currently signed in user.
     * @return The contact info for the currently signed in user.
     */
    private ContactDetails constructOwnerInfo(CoreAccountId ownerAccountId) {
        final DisplayableProfileData profileData = mProfileDataCache.getById(ownerAccountId);
        final ContactDetails contact =
                new ContactDetails(
                        /* id= */ ContactDetails.SELF_CONTACT_ID,
                        // TODO(crbug.com/500657003): Use getFullNameOrEmail() only. Currently it's
                        // not possible because that method checks only if full name is null, not if
                        // it's empty.
                        /* displayName= */ TextUtils.isEmpty(profileData.getFullName())
                                ? profileData.getAccountEmail()
                                : profileData.getFullNameOrEmail(),
                        /* emails= */ Collections.singletonList(profileData.getAccountEmail()),
                        /* phoneNumbers= */ null,
                        /* addresses= */ null);
        contact.setIsSelf(true);
        contact.setSelfIcon(profileData.getImage());
        return contact;
    }
}
