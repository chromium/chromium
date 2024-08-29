// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.data;

import android.graphics.drawable.Drawable;
import android.view.ViewGroup;

import androidx.annotation.LayoutRes;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryAction;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Interfaces in this class are used to pass data into keyboard accessory component. */
public class KeyboardAccessoryData {
    /**
     * Describes a tab which should be displayed as a small icon at the start of the keyboard
     * accessory. Typically, a tab is responsible to change the accessory sheet below the accessory.
     */
    public static final class Tab {
        private final String mTitle;
        private Drawable mIcon;
        private final String mContentDescription;
        private final int mTabLayout;
        private final @AccessoryTabType int mRecordingType;
        private final @Nullable Listener mListener;
        private final PropertyProvider<Drawable> mIconProvider = new PropertyProvider<>();

        /** A Tab's Listener get's notified when e.g. the Tab was assigned a view. */
        public interface Listener {
            /**
             * Triggered when the tab was successfully created.
             *
             * @param view The newly created accessory sheet of the tab.
             */
            void onTabCreated(ViewGroup view);

            /** Triggered when the tab becomes visible to the user. */
            void onTabShown();
        }

        public Tab(
                String title,
                Drawable icon,
                String contentDescription,
                @LayoutRes int tabLayout,
                @AccessoryTabType int recordingType,
                @Nullable Listener listener) {
            mTitle = title;
            mIcon = icon;
            mContentDescription = contentDescription;
            mTabLayout = tabLayout;
            mListener = listener;
            mRecordingType = recordingType;
        }

        public void setIcon(Drawable icon) {
            mIcon = icon;
            mIconProvider.notifyObservers(mIcon);
        }

        /**
         * Adds an observer to be notified of icon changes.
         *
         * @param observer The observer that will be notified of the icon change.
         */
        public void addIconObserver(Provider.Observer<Drawable> observer) {
            mIconProvider.addObserver(observer);
        }

        /**
         * Returns the title describing the source of the tab's content.
         *
         * @return A {@link String}
         */
        public String getTitle() {
            return mTitle;
        }

        /**
         * Provides the icon that will be displayed in the KeyboardAccessoryCoordinator.
         *
         * @return The small icon that identifies this tab uniquely.
         */
        public Drawable getIcon() {
            return mIcon;
        }

        /**
         * The description for this tab. It will become the content description of the icon.
         *
         * @return A short string describing the name of this tab.
         */
        public String getContentDescription() {
            return mContentDescription;
        }

        /**
         * Recording type of this tab. Used to sort it into the correct UMA bucket.
         *
         * @return A {@link AccessoryTabType}.
         */
        public @AccessoryTabType int getRecordingType() {
            return mRecordingType;
        }

        /**
         * Returns the tab layout which allows to create the tab's view on demand.
         *
         * @return The layout resource that allows to create the view necessary for this tab.
         */
        public @LayoutRes int getTabLayout() {
            return mTabLayout;
        }

        /**
         * Returns the listener which might need to react on changes to this tab.
         *
         * @return A {@link Listener} to be called, e.g. when the tab is created.
         */
        public @Nullable Listener getListener() {
            return mListener;
        }
    }

    /**
     * This describes an action that can be invoked from the keyboard accessory.
     * The most prominent example hereof is the "Generate Password" action.
     */
    public static final class Action {
        private final Callback<Action> mActionCallback;
        private final Callback<Action> mLongPressCallback;
        private @AccessoryAction int mType;

        public Action(@AccessoryAction int type, Callback<Action> actionCallback) {
            this(type, actionCallback, null);
        }

        public Action(
                @AccessoryAction int type,
                Callback<Action> actionCallback,
                @Nullable Callback<Action> longPressCallback) {
            mActionCallback = actionCallback;
            mLongPressCallback = longPressCallback;
            mType = type;
        }

        public Callback<Action> getCallback() {
            return mActionCallback;
        }

        public Callback<Action> getLongPressCallback() {
            return mLongPressCallback;
        }

        public @AccessoryAction int getActionType() {
            return mType;
        }

        @Override
        public String toString() {
            String typeName = "AccessoryAction(" + mType + ")"; // Fallback. We shouldn't crash.
            switch (mType) {
                case AccessoryAction.AUTOFILL_SUGGESTION:
                    typeName = "AUTOFILL_SUGGESTION";
                    break;
                case AccessoryAction.GENERATE_PASSWORD_AUTOMATIC:
                    typeName = "GENERATE_PASSWORD_AUTOMATIC";
                    break;
                case AccessoryAction.MANAGE_PASSWORDS:
                    typeName = "MANAGE_PASSWORDS";
                    break;
                case AccessoryAction.CREDMAN_CONDITIONAL_UI_REENTRY:
                    typeName = "CREDMAN_CONDITIONAL_UI_REENTRY";
                    break;
            }
            return typeName;
        }
    }

    /**
     * Represents a toggle displayed above suggestions in the accessory sheet, through which the
     * user can set an option. Displayed for example when password saving is disabled for the
     * current site, to allow the user to easily re-enable saving if desired.
     */
    public static final class OptionToggle {
        private final String mDisplayText;
        private final boolean mEnabled;
        private final Callback<Boolean> mCallback;
        private final @AccessoryAction int mType;

        public OptionToggle(
                String displayText,
                boolean enabled,
                @AccessoryAction int type,
                Callback<Boolean> callback) {
            mDisplayText = displayText;
            mEnabled = enabled;
            mCallback = callback;
            mType = type;
        }

        public String getDisplayText() {
            return mDisplayText;
        }

        public boolean isEnabled() {
            return mEnabled;
        }

        public Callback<Boolean> getCallback() {
            return mCallback;
        }

        public @AccessoryAction int getActionType() {
            return mType;
        }
    }

    public static final class PlusAddressInfo {
        private final String mOrigin;
        private final UserInfoField mPlusAddressInfo;

        public PlusAddressInfo(String origin, UserInfoField plusAddressInfo) {
            mOrigin = origin;
            mPlusAddressInfo = plusAddressInfo;
        }

        public String getOrigin() {
            return mOrigin;
        }

        public UserInfoField getPlusAddress() {
            return mPlusAddressInfo;
        }
    }

    public static final class PlusAddressSection {
        private final String mTitle;
        private final List<PlusAddressInfo> mPlusAddressInfoList = new ArrayList<>();

        public PlusAddressSection(String title) {
            mTitle = title;
        }

        public String getTitle() {
            return mTitle;
        }

        public List<PlusAddressInfo> getPlusAddressInfoList() {
            return mPlusAddressInfoList;
        }
    }

    /** Represents a Passkey (name and ID), to be shown on the manual fallback UI. */
    public static final class PasskeySection {
        private final String mDisplayName;
        private final Runnable mSelectPasskeyCallback;

        /**
         * Creates a new PasskeySection.
         *
         * @param displayName The text to be displayed on the footer.
         * @param selectPasskeyCallback Called when the user taps the suggestions.
         */
        public PasskeySection(String displayName, Runnable selectPasskeyCallback) {
            mDisplayName = displayName;
            mSelectPasskeyCallback = selectPasskeyCallback;
        }

        /**
         * This text is used for accessibility.
         *
         * @return The formatted username.
         */
        public String getDisplayName() {
            return mDisplayName;
        }

        /** Invokes the stored callback. To be called when the user taps on the chip. */
        public void triggerSelection() {
            mSelectPasskeyCallback.run();
        }
    }

    /**
     * Represents a Profile, or a Credit Card, or the credentials for a website
     * (username + password), to be shown on the manual fallback UI.
     */
    public static final class UserInfo {
        private final String mOrigin;
        private final GURL mIconUrl;
        private final List<UserInfoField> mFields = new ArrayList<>();
        private final boolean mIsExactMatch;

        public UserInfo(String origin, boolean isExactMatch) {
            this(origin, isExactMatch, null);
        }

        public UserInfo(String origin, boolean isExactMatch, GURL iconUrl) {
            mOrigin = origin;
            mIsExactMatch = isExactMatch;
            mIconUrl = iconUrl;
        }

        /**
         * Adds a new field to the group.
         *
         * @param field The field to be added.
         */
        public void addField(UserInfoField field) {
            mFields.add(field);
        }

        /**
         * @return A list of {@link UserInfoField}s in this group.
         */
        public List<UserInfoField> getFields() {
            return mFields;
        }

        /**
         * @return A string indicating the origin. May be empty but not null.
         */
        public String getOrigin() {
            return mOrigin;
        }

        /**
         * @return True iff the user info originates from a first-party item and not from a PSL or
         *         affiliated match.
         */
        public boolean isExactMatch() {
            return mIsExactMatch;
        }

        /**
         * The url for the icon to be downloaded and displayed in the manual filling view. For
         * credit cards, the `mOrigin` is used as an identifier for the icon. However, if the
         * `mIconUrl` is set, it'll be used to download the icon and then display it.
         */
        public GURL getIconUrl() {
            return mIconUrl;
        }
    }

    /**
     * Represents a list of Profiles, or a Credit Cards, or the credentials for a website (username
     * + password), to be shown on the manual fallback UI. Contains a possibly empty title for the
     * user info section.
     */
    public static final class UserInfoSection {
        private final String mTitle;
        private final List<UserInfo> mUserInfoList = new ArrayList();

        public UserInfoSection(String title) {
            mTitle = title;
        }

        public String getTitle() {
            return mTitle;
        }

        public List<UserInfo> getUserInfoList() {
            return mUserInfoList;
        }
    }

    /** Represents a Promo Code Offer to be shown on the manual fallback UI. */
    public static final class PromoCodeInfo {
        private UserInfoField mPromoCode;
        private String mDetailsText;

        public PromoCodeInfo() {}

        public void setPromoCode(UserInfoField promoCode) {
            mPromoCode = promoCode;
        }

        public void setDetailsText(String detailsText) {
            mDetailsText = detailsText;
        }

        public UserInfoField getPromoCode() {
            return mPromoCode;
        }

        public String getDetailsText() {
            return mDetailsText;
        }
    }

    /** Represents an IBAN to be shown on the manual fallback UI. */
    public static final class IbanInfo {
        private UserInfoField mIbanInfo;

        public IbanInfo() {}

        public void setValue(UserInfoField ibanInfo) {
            mIbanInfo = ibanInfo;
        }

        public UserInfoField getValue() {
            return mIbanInfo;
        }
    }

    /** Represents a command below the suggestions, such as "Manage password...". */
    public static final class FooterCommand {
        private final String mDisplayText;
        private final Callback<FooterCommand> mCallback;

        /**
         * Creates a new FooterCommand.
         *
         * @param displayText The text to be displayed on the footer.
         * @param callback Called when the user taps the suggestions.
         */
        public FooterCommand(String displayText, Callback<FooterCommand> callback) {
            mDisplayText = displayText;
            mCallback = callback;
        }

        /**
         * Returns the translated text to be shown on the UI for this footer command. This text is
         * used for accessibility.
         */
        public String getDisplayText() {
            return mDisplayText;
        }

        /** Invokes the stored callback. To be called when the user taps on the footer command. */
        public void execute() {
            mCallback.onResult(this);
        }
    }

    /**
     * Represents the contents of a accessory sheet tab below the keyboard accessory, which can
     * correspond to passwords, credit cards, IBANs, or profiles data. Created natively.
     */
    public static final class AccessorySheetData {
        private final String mWarning;
        private final @AccessoryTabType int mSheetType;
        private OptionToggle mToggle;
        private final PlusAddressSection mPlusAddressSection;
        private final UserInfoSection mUserInfoSection;
        private final List<PasskeySection> mPasskeySectionList = new ArrayList<>();
        private final List<PromoCodeInfo> mPromoCodeInfoList = new ArrayList<>();
        private final List<IbanInfo> mIbanInfoList = new ArrayList<>();
        private final List<FooterCommand> mFooterCommands = new ArrayList<>();

        /**
         * Creates the AccessorySheetData object.
         *
         * @param sheetType The type of the accessory manual filling sheet (addresses, credit cards,
         *     passwords).
         * @param userInfoTitle The user info title of accessory sheet tab.
         * @param plusAddressTitle The plus address section title.
         * @param warning An optional warning to be displayed the beginning of the sheet.
         */
        public AccessorySheetData(
                @AccessoryTabType int sheetType,
                String userInfoTitle,
                String plusAddressTitle,
                String warning) {
            mWarning = warning;
            mSheetType = sheetType;
            mToggle = null;
            mUserInfoSection = new UserInfoSection(userInfoTitle);
            mPlusAddressSection = new PlusAddressSection(plusAddressTitle);
        }

        public @AccessoryTabType int getSheetType() {
            return mSheetType;
        }

        public void setOptionToggle(OptionToggle toggle) {
            mToggle = toggle;
        }

        public @Nullable OptionToggle getOptionToggle() {
            return mToggle;
        }

        /**
         * Returns a warning to be displayed at the beginning of the sheet. Empty string otherwise.
         */
        public String getWarning() {
            return mWarning;
        }

        /**
         * Returns the possible empty title for the user info section to be shown on the accessory
         * sheet
         */
        public String getUserInfoTitle() {
            return mUserInfoSection.getTitle();
        }

        /** Returns the list of {@link UserInfo} to be shown on the accessory sheet. */
        public List<UserInfo> getUserInfoList() {
            return mUserInfoSection.getUserInfoList();
        }

        /**
         * @return a possibly empty title for the plus address section to be shown on the accessory
         *     sheet
         */
        public String getPlusAddressSectionTitle() {
            return mPlusAddressSection.getTitle();
        }

        /**
         * @return a list if {@link PlusAddressInfo} to be shown on the accessory sheet.
         */
        public List<PlusAddressInfo> getPlusAddressInfoList() {
            return mPlusAddressSection.getPlusAddressInfoList();
        }

        /** Returns the list of {@link PasskeySection} to be shown on the accessory sheet. */
        public List<PasskeySection> getPasskeySectionList() {
            return mPasskeySectionList;
        }

        /** Returns the list of {@link PromoCodeInfo} to be shown on the accessory sheet. */
        public List<PromoCodeInfo> getPromoCodeInfoList() {
            return mPromoCodeInfoList;
        }

        /** Returns the list of {@link IbanInfo} to be shown on the accessory sheet. */
        public List<IbanInfo> getIbanInfoList() {
            return mIbanInfoList;
        }

        /** Returns the list of {@link FooterCommand} to be shown on the accessory sheet. */
        public List<FooterCommand> getFooterCommands() {
            return mFooterCommands;
        }
    }

    private KeyboardAccessoryData() {}
}
