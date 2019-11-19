// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.data;

import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.view.ViewGroup;

import androidx.annotation.LayoutRes;
import androidx.annotation.Nullable;
import androidx.annotation.Px;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryAction;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;

import java.util.ArrayList;
import java.util.List;

/**
 * Interfaces in this class are used to pass data into keyboard accessory component.
 */
public class KeyboardAccessoryData {
    /**
     * Describes a tab which should be displayed as a small icon at the start of the keyboard
     * accessory. Typically, a tab is responsible to change the accessory sheet below the accessory.
     */
    public final static class Tab {
        private final String mTitle;
        private final Drawable mIcon;
        private final @Nullable String mOpeningAnnouncement;
        private final String mContentDescription;
        private final int mTabLayout;
        private final @AccessoryTabType int mRecordingType;
        private final @Nullable Listener mListener;

        /**
         * A Tab's Listener get's notified when e.g. the Tab was assigned a view.
         */
        public interface Listener {
            /**
             * Triggered when the tab was successfully created.
             * @param view The newly created accessory sheet of the tab.
             */
            void onTabCreated(ViewGroup view);

            /**
             * Triggered when the tab becomes visible to the user.
             */
            void onTabShown();
        }

        public Tab(String title, Drawable icon, String contentDescription, @LayoutRes int tabLayout,
                @AccessoryTabType int recordingType, @Nullable Listener listener) {
            this(title, icon, contentDescription, null, tabLayout, recordingType, listener);
        }

        public Tab(String title, Drawable icon, String contentDescription,
                @Nullable String openingAnnouncement, @LayoutRes int tabLayout,
                @AccessoryTabType int recordingType, @Nullable Listener listener) {
            mTitle = title;
            mIcon = icon;
            mContentDescription = contentDescription;
            mOpeningAnnouncement = openingAnnouncement;
            mTabLayout = tabLayout;
            mListener = listener;
            mRecordingType = recordingType;
        }

        /**
         * Returns the title describing the source of the tab's content.
         * @return A {@link String}
         */
        public String getTitle() {
            return mTitle;
        }

        /**
         * Provides the icon that will be displayed in the KeyboardAccessoryCoordinator.
         * @return The small icon that identifies this tab uniquely.
         */
        public Drawable getIcon() {
            return mIcon;
        }

        /**
         * The description for this tab. It will become the content description of the icon.
         * @return A short string describing the name of this tab.
         */
        public String getContentDescription() {
            return mContentDescription;
        }

        /**
         * An optional announcement triggered when the Tab is opened.
         * @return A string describing the contents of this tab.
         */
        public String getOpeningAnnouncement() {
            return mOpeningAnnouncement;
        }

        /**
         * Recording type of this tab. Used to sort it into the correct UMA bucket.
         * @return A {@link AccessoryTabType}.
         */
        public @AccessoryTabType int getRecordingType() {
            return mRecordingType;
        }

        /**
         * Returns the tab layout which allows to create the tab's view on demand.
         * @return The layout resource that allows to create the view necessary for this tab.
         */
        public @LayoutRes int getTabLayout() {
            return mTabLayout;
        }

        /**
         * Returns the listener which might need to react on changes to this tab.
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
        private final String mCaption;
        private final Callback<Action> mActionCallback;
        private @AccessoryAction int mType;

        public Action(String caption, @AccessoryAction int type, Callback<Action> actionCallback) {
            mCaption = caption;
            mActionCallback = actionCallback;
            mType = type;
        }

        public String getCaption() {
            return mCaption;
        }

        public Callback<Action> getCallback() {
            return mActionCallback;
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
            }
            return "'" + mCaption + "' of type " + typeName;
        }
    }

    /**
     * Represents a Profile, or a Credit Card, or the credentials for a website
     * (username + password), to be shown on the manual fallback UI.
     */
    public final static class UserInfo {
        private final String mOrigin;
        private final List<UserInfoField> mFields = new ArrayList<>();
        private final @Nullable FaviconProvider mFaviconProvider;

        /**
         * Favicons used by UserInfo views are provided and mocked using this interface.
         */
        public interface FaviconProvider {
            /**
             * Data object containing the result of a {@link FaviconProvider#fetchFavicon} calls.
             */
            class FaviconResult {
                public final String mOrigin;
                public final Bitmap mFavicon;

                public FaviconResult(String origin, Bitmap favicon) {
                    mOrigin = origin;
                    mFavicon = favicon;
                }
            }

            /**
             * Starts a request for a favicon. The callback can be called either asynchronously or
             * synchronously (depending on whether the icon was cached).
             * @param origin The origin the icon should be requested for.
             * @param desiredSize The size the icon should have. Used for height and width.
             * @param favicon The callback that will be called once the icon was fetched.
             */
            void fetchFavicon(String origin, @Px int desiredSize, Callback<FaviconResult> favicon);
        }

        public UserInfo(String origin, @Nullable FaviconProvider faviconProvider) {
            mOrigin = origin;
            mFaviconProvider = faviconProvider;
        }

        /**
         * Adds a new field to the group.
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
         * Possibly holds a favicon provider.
         * @return A {@link FaviconProvider}. Optional.
         */
        public @Nullable FaviconProvider getFaviconProvider() {
            return mFaviconProvider;
        }
    }

    /**
     * Represents a command below the suggestions, such as "Manage password...".
     */
    public final static class FooterCommand {
        private final String mDisplayText;
        private final Callback<FooterCommand> mCallback;

        /**
         * Creates a new FooterCommand.
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

        /**
         * Returns the translated text to be shown on the UI for this footer command. This text is
         * used for accessibility.
         */
        public void execute() {
            mCallback.onResult(this);
        }
    }

    /**
     * Represents the contents of a accessory sheet tab below the keyboard accessory, which can
     * correspond to passwords, credit cards, or profiles data. Created natively.
     */
    public final static class AccessorySheetData {
        private final String mTitle;
        private final String mWarning;
        private final @AccessoryTabType int mSheetType;
        private final List<UserInfo> mUserInfoList = new ArrayList<>();
        private final List<FooterCommand> mFooterCommands = new ArrayList<>();

        /**
         * Creates the AccessorySheetData object.
         * @param title The title of accessory sheet tab.
         * @param warning An optional warning to be displayed the beginning of the sheet.
         */
        public AccessorySheetData(@AccessoryTabType int sheetType, String title, String warning) {
            mSheetType = sheetType;
            mTitle = title;
            mWarning = warning;
        }

        public @AccessoryTabType int getSheetType() {
            return mSheetType;
        }

        /**
         * Returns the title of the accessory sheet. This text is also used for accessibility.
         */
        public String getTitle() {
            return mTitle;
        }

        /**
         * Returns a warning to be displayed at the beginning of the sheet. Empty string otherwise.
         */
        public String getWarning() {
            return mWarning;
        }

        /**
         * Returns the list of {@link UserInfo} to be shown on the accessory sheet.
         */
        public List<UserInfo> getUserInfoList() {
            return mUserInfoList;
        }

        /**
         * Returns the list of {@link FooterCommand} to be shown on the accessory sheet.
         */
        public List<FooterCommand> getFooterCommands() {
            return mFooterCommands;
        }
    }

    private KeyboardAccessoryData() {}
}
