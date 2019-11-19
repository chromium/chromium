// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.os.Bundle;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ResourceId;
import org.chromium.chrome.browser.permissions.AndroidPermissionRequester;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.website.SingleCategoryPreferences;
import org.chromium.chrome.browser.preferences.website.SiteSettingsCategory;
import org.chromium.chrome.browser.tab.Tab;

/**
 * An infobar used for prompting the user to grant a web API permission.
 */
public class PermissionInfoBar
        extends ConfirmInfoBar implements AndroidPermissionRequester.RequestDelegate {

    /** The tab which this infobar will be displayed upon. */
    protected final Tab mTab;

    /** The content settings types corresponding to the permission requested in this infobar. */
    protected int[] mContentSettingsTypes;

    /** Whether the last clicked button was the "Manage" (secondary) button. */
    protected boolean mManageButtonLastClicked;

    /** Whether the infobar should be shown as a compact mini-infobar or a classic expanded one. */
    private boolean mIsExpanded;

    /** The text of the link shown in the compact state. */
    private String mCompactLinkText;

    /** The message text in the compact state. */
    private String mCompactMessage;

    /** The secondary text shown below the message in the expanded state. */
    private String mDescription;

    protected PermissionInfoBar(Tab tab, int[] contentSettingsTypes, int iconDrawableId,
            String compactMessage, String compactLinkText, String message, String description,
            String primaryButtonText, String secondaryButtonText) {
        super(iconDrawableId, R.color.infobar_icon_drawable_color, null /* iconBitmap */, message,
                null /* linkText */, primaryButtonText, secondaryButtonText);
        mTab = tab;
        mContentSettingsTypes = contentSettingsTypes;
        mManageButtonLastClicked = false;
        mIsExpanded = false;
        mCompactLinkText = compactLinkText;
        mCompactMessage = compactMessage;
        mDescription = description;
    }

    @Override
    protected boolean usesCompactLayout() {
        return !mIsExpanded;
    }

    @Override
    protected void createCompactLayoutContent(InfoBarCompactLayout layout) {
        new InfoBarCompactLayout.MessageBuilder(layout)
                .withText(mCompactMessage)
                .withLink(mCompactLinkText, view -> onLinkClicked())
                .buildAndInsert();
    }

    @Override
    public boolean areControlsEnabled() {
        // The controls need to be enbled after the user clicks `manage` since they will return to
        // the page and the infobar still needs to be kept active.
        return super.areControlsEnabled() || mManageButtonLastClicked;
    }

    @Override
    public void onButtonClicked(final boolean isPrimaryButton) {
        mManageButtonLastClicked = !isPrimaryButton;
        if (getContext() == null) {
            onButtonClickedInternal(isPrimaryButton);
            return;
        }

        if (isPrimaryButton) {
            // requestAndroidPermissions will call back into this class to finalize the action if it
            // returns true.
            if (AndroidPermissionRequester.requestAndroidPermissions(
                        mTab, mContentSettingsTypes.clone(), this)) {
                return;
            }
        } else {
            launchNotificationsSettingsPage();
        }
        onButtonClickedInternal(isPrimaryButton);
    }

    @Override
    public void onLinkClicked() {
        if (!mIsExpanded) {
            mIsExpanded = true;
            replaceView(createView());
        }

        super.onLinkClicked();
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);
        layout.getMessageLayout().addDescription(mDescription);
    }

    @Override
    public void onAndroidPermissionAccepted() {
        onButtonClickedInternal(true);
    }

    @Override
    public void onAndroidPermissionCanceled() {
        onCloseButtonClicked();
    }

    private void onButtonClickedInternal(boolean isPrimaryButton) {
        super.onButtonClicked(isPrimaryButton);
    }

    private void launchNotificationsSettingsPage() {
        Bundle fragmentArguments = new Bundle();
        fragmentArguments.putString(SingleCategoryPreferences.EXTRA_CATEGORY,
                SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.NOTIFICATIONS));
        PreferencesLauncher.launchSettingsPage(
                getContext(), SingleCategoryPreferences.class, fragmentArguments);
    }

    /**
     * Creates and begins the process for showing a PermissionInfoBar.
     * @param tab                   The owning tab for the infobar.
     * @param contentSettingsTypes  The list of ContentSettingTypes being requested by this infobar.
     * @param enumeratedIconId      ID corresponding to the icon that will be shown for the infobar.
     *                              The ID must have been mapped using the ResourceMapper class
     *                              before passing it to this function.
     * @param compactMessage        Message to show in the compact state.
     * @param compactLinkText       Text of link displayed right to the message in compact state.
     * @param message               Primary message in the extended state.
     * @param description           Secondary message (description) in the expanded state.
     * @param buttonOk              String to display on the OK button.
     * @param buttonManage          String to display on the Manage button.
     */
    @CalledByNative
    private static PermissionInfoBar create(Tab tab, int[] contentSettingsTypes,
            int enumeratedIconId, String compactMessage, String compactLinkText, String message,
            String description, String buttonOk, String buttonManage) {
        int drawableId = ResourceId.mapToDrawableId(enumeratedIconId);

        PermissionInfoBar infoBar = new PermissionInfoBar(tab, contentSettingsTypes, drawableId,
                compactMessage, compactLinkText, message, description, buttonOk, buttonManage);

        return infoBar;
    }
}
