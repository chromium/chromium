// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.os.Bundle;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.Spanned;

import org.jni_zero.CalledByNative;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.infobars.ConfirmInfoBar;
import org.chromium.components.infobars.InfoBarCompactLayout;
import org.chromium.components.infobars.InfoBarLayout;
import org.chromium.components.permissions.AndroidPermissionRequester;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.text.NoUnderlineClickableSpan;

/** An infobar used for prompting the user to grant a web API permission. */
public class PermissionInfoBar extends ConfirmInfoBar
        implements AndroidPermissionRequester.RequestDelegate {
    /** The window which this infobar will be displayed upon. */
    protected final WindowAndroid mWindow;

    /** The content settings types corresponding to the permission requested in this infobar. */
    protected int[] mContentSettingsTypes;

    /** Whether the secondary button should act as a "Manage" button to open settings. */
    protected boolean mSecondaryButtonShouldOpenSettings;

    /**
     * Whether the last clicked button opened settings, requiring the dialog to stay interactive
     * when the user switches back.
     */
    protected boolean mLastClickOpenedSettings;

    /** Whether the infobar should be shown as a compact mini-infobar or a classic expanded one. */
    private boolean mIsExpanded;

    /** The text of the link shown in the compact state. */
    private String mCompactLinkText;

    /** The message text in the compact state. */
    private String mCompactMessage;

    /** The secondary text shown below the message in the expanded state. */
    private String mDescription;

    /** The text of the `Learn more` link shown in the expanded state after the description. */
    private String mLearnMoreLinkText;

    protected PermissionInfoBar(
            WindowAndroid window,
            int[] contentSettingsTypes,
            int iconDrawableId,
            String compactMessage,
            String compactLinkText,
            String message,
            String description,
            String learnMoreLinktext,
            String primaryButtonText,
            String secondaryButtonText,
            boolean secondaryButtonShouldOpenSettings) {
        super(
                iconDrawableId,
                R.color.infobar_icon_drawable_color,
                /* iconBitmap= */ null,
                message,
                /* linkText= */ null,
                primaryButtonText,
                secondaryButtonText);
        mWindow = window;
        mContentSettingsTypes = contentSettingsTypes;
        mSecondaryButtonShouldOpenSettings = secondaryButtonShouldOpenSettings;
        mLastClickOpenedSettings = false;
        mIsExpanded = false;
        mCompactLinkText = compactLinkText;
        mCompactMessage = compactMessage;
        mDescription = description;
        mLearnMoreLinkText = learnMoreLinktext;
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
        return super.areControlsEnabled() || mLastClickOpenedSettings;
    }

    @Override
    public void onButtonClicked(final boolean isPrimaryButton) {
        mLastClickOpenedSettings = false;
        if (getContext() == null) {
            onButtonClickedInternal(isPrimaryButton);
            return;
        }

        if (isPrimaryButton) {
            // requestAndroidPermissions will call back into this class to finalize the action if it
            // returns true.
            if (AndroidPermissionRequester.requestAndroidPermissions(
                    mWindow, mContentSettingsTypes.clone(), this)) {
                return;
            }
        } else if (mSecondaryButtonShouldOpenSettings) {
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

        SpannableStringBuilder descriptionMessage = new SpannableStringBuilder(mDescription);
        if (mLearnMoreLinkText != null && !mLearnMoreLinkText.isEmpty()) {
            SpannableString link = new SpannableString(mLearnMoreLinkText);
            link.setSpan(
                    new NoUnderlineClickableSpan(layout.getContext(), view -> onLinkClicked()),
                    0,
                    link.length(),
                    Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
            descriptionMessage.append(" ").append(link);
        }
        layout.getMessageLayout().addDescription(descriptionMessage);
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
        mLastClickOpenedSettings = true;
        Bundle fragmentArguments = new Bundle();
        fragmentArguments.putString(
                SingleCategorySettings.EXTRA_CATEGORY,
                SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.NOTIFICATIONS));
        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        settingsNavigation.startSettings(
                getContext(), SingleCategorySettings.class, fragmentArguments);
    }

    /**
     * Creates and begins the process for showing a PermissionInfoBar.
     * @param window                The window this infobar will be displayed upon.
     * @param contentSettingsTypes  The list of ContentSettingTypes being requested by this infobar.
     * @param iconId                ID corresponding to the icon that will be shown for the infobar.
     * @param compactMessage        Message to show in the compact state.
     * @param compactLinkText       Text of link displayed right to the message in compact state.
     * @param message               Primary message in the extended state.
     * @param description           Secondary message (description) in the expanded state.
     * @param learnMoreLinkText     String to display on the `Learn more` link.
     * @param primaryButtonText     String to display on the primary button.
     * @param secondaryButtonText   String to display on the secondary button.
     * @param secondaryButtonShouldOpenSettings  Whether the secondary button should open site
     *         settings.
     */
    @CalledByNative
    private static PermissionInfoBar create(
            WindowAndroid window,
            int[] contentSettingsTypes,
            int iconId,
            String compactMessage,
            String compactLinkText,
            String message,
            String description,
            String learnMoreLinkText,
            String primaryButtonText,
            String secondaryButtonText,
            boolean secondaryButtonShouldOpenSettings) {
        PermissionInfoBar infoBar =
                new PermissionInfoBar(
                        window,
                        contentSettingsTypes,
                        iconId,
                        compactMessage,
                        compactLinkText,
                        message,
                        description,
                        learnMoreLinkText,
                        primaryButtonText,
                        secondaryButtonText,
                        secondaryButtonShouldOpenSettings);

        return infoBar;
    }
}
