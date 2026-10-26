// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;
import android.view.View;

import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A preference showing a verified email address with a "More" button on the right, which opens a
 * menu with the option to remove the permission.
 *
 * <p>TODO(crbug.com/576739997): Show the issuer's icon on the left.
 */
@NullMarked
public class AutofillVerifiedEmailPreference extends ChromeBasePreference {
    private static final int MENU_ID_REMOVE_PERMISSION = 1;

    private final String mEmail;
    private @Nullable Runnable mOnRemovePermissionClicked;

    /** Creates a row showing the verified {@code email}. */
    public AutofillVerifiedEmailPreference(Context context, String email) {
        super(context);
        mEmail = email;
        setKey(email);
        setTitle(email);
        setSelectable(false);
        setWidgetLayoutResource(R.layout.autofill_verified_email_more_button);
    }

    /** Sets the callback invoked when "Remove permission" is chosen from the "More" menu. */
    public void setOnRemovePermissionClicked(Runnable onRemovePermissionClicked) {
        mOnRemovePermissionClicked = onRemovePermissionClicked;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        ListMenuButton moreButton =
                (ListMenuButton) holder.findViewById(R.id.verified_email_more_button);
        moreButton.setContentDescriptionContext(mEmail);
        moreButton.setDelegate(this::createMenu);
    }

    /** Creates the "More" menu. Only called when the "More" button is tapped. */
    private BasicListMenu createMenu() {
        ModelList modelList = new ModelList();
        modelList.add(
                new ListItemBuilder()
                        .withTitleRes(R.string.autofill_settings_remove_verified_email_permission)
                        .withMenuId(MENU_ID_REMOVE_PERMISSION)
                        .build());
        return BrowserUiListMenuUtils.getBasicListMenu(
                getContext(),
                modelList,
                (PropertyModel item, View view) -> {
                    if (item.get(ListMenuItemProperties.MENU_ITEM_ID) == MENU_ID_REMOVE_PERMISSION
                            && mOnRemovePermissionClicked != null) {
                        mOnRemovePermissionClicked.run();
                    }
                });
    }
}
