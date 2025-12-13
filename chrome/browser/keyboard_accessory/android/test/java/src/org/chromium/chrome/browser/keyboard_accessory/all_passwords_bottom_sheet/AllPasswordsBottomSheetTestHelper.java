// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import android.app.Activity;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager.ScrimClient;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;

import java.util.List;

/** Helpers in this class simplify interactions with the all passwords bottom sheet component. */
@NullMarked
public class AllPasswordsBottomSheetTestHelper {
    public static final Credential ANA =
            new Credential(
                    /* username= */ "ana@gmail.com",
                    /* password= */ "S3cr3t",
                    /* formattedUsername= */ "ana@gmail.com",
                    /* originUrl= */ "https://example.com",
                    /* isAndroidCredential= */ false,
                    /* appDisplayName= */ "",
                    /* isPlusAddressUsername= */ true);
    public static final Credential NO_ONE =
            new Credential(
                    /* username= */ "",
                    /* password= */ "***",
                    /* formattedUsername= */ "No Username",
                    /* originUrl= */ "https://m.example.xyz",
                    /* isAndroidCredential= */ false,
                    /* appDisplayName= */ "",
                    /* isPlusAddressUsername= */ false);
    public static final Credential BOB =
            new Credential(
                    /* username= */ "Bob",
                    /* password= */ "***",
                    /* formattedUsername= */ "Bob",
                    /* originUrl= */ "android://com.facebook.org",
                    /* isAndroidCredential= */ true,
                    /* appDisplayName= */ "facebook",
                    /* isPlusAddressUsername= */ false);
    public static final List<Credential> TEST_CREDENTIALS = List.of(ANA, NO_ONE, BOB);

    private AllPasswordsBottomSheetTestHelper() {}

    /**
     * Creates a new {@link BottomSheetController} for testing purposes.
     *
     * @param activity The {@link Activity} to create the controller in.
     * @return A new {@link BottomSheetController}.
     */
    public static BottomSheetController createBottomSheetController(Activity activity) {
        assert activity != null : "Activity must not be null!";
        ViewGroup contentView = activity.findViewById(android.R.id.content);
        ScrimManager scrimManager = new ScrimManager(activity, contentView, ScrimClient.NONE);
        BottomSheetController bottomSheetController =
                BottomSheetControllerFactory.createBottomSheetController(
                        () -> scrimManager,
                        (unused) -> {},
                        activity.getWindow(),
                        KeyboardVisibilityDelegate.getInstance(),
                        () -> contentView,
                        () -> 0,
                        /* desktopWindowStateManager= */ null);
        return bottomSheetController;
    }

    /**
     * Creates a new {@link ListItem} for a credential in the all passwords bottom sheet.
     *
     * @param credential The {@link Credential} to create the item for.
     * @param isPasswordField Whether the credential is for a password field.
     * @return A new {@link ListItem}.
     */
    public static ListItem createAllPasswordsSheetCredential(
            Credential credential, boolean isPasswordField) {
        return new ListItem(
                AllPasswordsBottomSheetProperties.ItemType.CREDENTIAL,
                AllPasswordsBottomSheetProperties.CredentialProperties.createCredentialModel(
                        credential, (credentialFillRequest) -> {}, isPasswordField));
    }
}
