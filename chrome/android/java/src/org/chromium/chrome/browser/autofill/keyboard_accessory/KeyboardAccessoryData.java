// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.support.annotation.LayoutRes;
import android.support.annotation.Nullable;
import android.support.annotation.Px;
import android.view.ViewGroup;

import org.chromium.base.Callback;

import java.util.ArrayList;
import java.util.List;

/**
 * Interfaces in this class are used to pass data into keyboard accessory component.
 */
public class KeyboardAccessoryData {
    /**
     * A provider notifies all registered {@link Observer} if the list of actions
     * changes.
     * TODO(fhorschig): Replace with android.databinding.ObservableField if available.
     * @param <T> Either an {@link Action} or a {@link Tab} that this instance provides.
     */
    public interface Provider<T> {
        /**
         * Every observer added by this need to be notified whenever the list of items changes.
         * @param observer The observer to be notified.
         */
        void addObserver(Observer<T> observer);

        /**
         * Passes the given items to all subscribed {@link Observer}s.
         * @param items The array of items to be passed to the {@link Observer}s.
         */
        void notifyObservers(T[] items);
    }

    /**
     * An observer receives notifications from an {@link Provider} it is subscribed to.
     * @param <T> An {@link Action}, {@link Tab} or {@link Item} that this instance observes.
     */
    public interface Observer<T> {
        int DEFAULT_TYPE = Integer.MIN_VALUE;

        /**
         * A provider calls this function with a list of items that should be available in the
         * keyboard accessory.
         * @param typeId Specifies which type of item this update affects.
         * @param items The items to be displayed in the Accessory. It's a native array as the
         *                provider is typically a bridge called via JNI which prefers native types.
         */
        void onItemsAvailable(int typeId, T[] items);
    }

    /**
     * Describes a tab which should be displayed as a small icon at the start of the keyboard
     * accessory. Typically, a tab is responsible to change the bottom sheet below the accessory.
     */
    public final static class Tab {
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

        public Tab(Drawable icon, String contentDescription, @LayoutRes int tabLayout,
                @AccessoryTabType int recordingType, @Nullable Listener listener) {
            this(icon, contentDescription, null, tabLayout, recordingType, listener);
        }

        public Tab(Drawable icon, String contentDescription, @Nullable String openingAnnouncement,
                @LayoutRes int tabLayout, @AccessoryTabType int recordingType,
                @Nullable Listener listener) {
            mIcon = icon;
            mContentDescription = contentDescription;
            mOpeningAnnouncement = openingAnnouncement;
            mTabLayout = tabLayout;
            mListener = listener;
            mRecordingType = recordingType;
        }

        /**
         * Provides the icon that will be displayed in the {@link KeyboardAccessoryCoordinator}.
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
    }

    /**
     * This describes an item in a accessory sheet. They are usually list items that were created
     * natively.
     */
    public final static class Item {
        private final int mType;
        private final String mCaption;
        private final String mContentDescription;
        private final boolean mIsPassword;
        private final @Nullable Callback<Item> mItemSelectedCallback;
        private final @Nullable FaviconProvider mFaviconProvider;

        /**
         * Items will call a class that implements this interface to request a favicon.
         */
        interface FaviconProvider {
            /**
             * Starts a request for a favicon. The callback can be called either asynchronously or
             * synchronously (depending on whether the icon was cached).
             * @param favicon The icon to be used for this Item. If null, use the default icon.
             */
            void fetchFavicon(@Px int desiredSize, Callback<Bitmap> favicon);
        }

        /**
         * Creates a new Item of type {@link ItemType#LABEL}. It is not interactive.
         * @param caption The text of the displayed item.
         * @param contentDescription The description of this item (i.e. used for accessibility).
         */
        public static Item createLabel(String caption, String contentDescription) {
            return new Item(ItemType.LABEL, caption, contentDescription, false, null, null);
        }

        /**
         * Creates a new Item of type {@link ItemType#SUGGESTION} if has a callback, otherwise, it
         * will be {@link ItemType#NON_INTERACTIVE_SUGGESTION}. It usually is part of a list of
         * suggestions and can have a callback that is triggered on selection.
         * @param caption The text of the displayed item. Only plain text if |isPassword| is false.
         * @param contentDescription The description of this item (i.e. used for accessibility).
         * @param isPassword If true, the displayed caption is transformed into stars.
         * @param itemSelectedCallback A click on this item will invoke this callback. Optional.
         */
        public static Item createSuggestion(String caption, String contentDescription,
                boolean isPassword, @Nullable Callback<Item> itemSelectedCallback,
                @Nullable FaviconProvider faviconProvider) {
            if (itemSelectedCallback == null) {
                return new Item(ItemType.NON_INTERACTIVE_SUGGESTION, caption, contentDescription,
                        isPassword, null, faviconProvider);
            }
            return new Item(ItemType.SUGGESTION, caption, contentDescription, isPassword,
                    itemSelectedCallback, faviconProvider);
        }

        /**
         * Creates an Item of type {@link ItemType#DIVIDER}. Basically, it's a horizontal line.
         */
        public static Item createDivider() {
            return new Item(ItemType.DIVIDER, null, null, false, null, null);
        }

        /**
         * Creates a new Item of type {@link ItemType#OPTION}. They are normally independent items
         * that trigger a unique action (e.g. generate a password or navigate to an overview).
         * @param caption The text of the displayed option.
         * @param contentDescription The description of this option (i.e. used for accessibility).
         * @param callback A click on this item will invoke this callback.
         */
        public static Item createOption(
                String caption, String contentDescription, Callback<Item> callback) {
            return new Item(ItemType.OPTION, caption, contentDescription, false, callback, null);
        }

        /**
         * Creates an Item of type {@link ItemType#TOP_DIVIDER}. A horizontal line meant to be
         * displayed at the top of the accessory sheet.
         */
        public static Item createTopDivider() {
            return new Item(ItemType.TOP_DIVIDER, null, null, false, null, null);
        }

        /**
         * Creates a new item.
         * @param type Type of the item (e.g. non-clickable LABEL or clickable SUGGESTION).
         * @param caption The text of the displayed item. Only plain text if |isPassword| is false.
         * @param contentDescription The description of this item (i.e. used for accessibility).
         * @param isPassword If true, the displayed caption is transformed into stars.
         * @param itemSelectedCallback If the Item is interactive, a click on it will trigger this.
         * @param faviconProvider
         */
        private Item(@ItemType int type, String caption, String contentDescription,
                boolean isPassword, @Nullable Callback<Item> itemSelectedCallback,
                @Nullable FaviconProvider faviconProvider) {
            mType = type;
            mCaption = caption;
            mContentDescription = contentDescription;
            mIsPassword = isPassword;
            mItemSelectedCallback = itemSelectedCallback;
            mFaviconProvider = faviconProvider;
        }

        /**
         * Returns the type of the item.
         * @return Returns a {@link ItemType}.
         */
        public @ItemType int getType() {
            return mType;
        }

        /**
         * Returns a human-readable, translated string that will appear as text of the item.
         * @return A short descriptive string of the item.
         */
        public String getCaption() {
            return mCaption;
        }

        /**
         * Returns a translated description that can be used for accessibility.
         * @return A short description of the displayed item.
         */
        public String getContentDescription() {
            return mContentDescription;
        }

        /**
         * Returns whether the item (i.e. its caption) contains a password. Can be used to determine
         * when to apply text transformations to hide passwords.
         * @return Returns true if the caption is a password. False otherwise.
         */
        public boolean isPassword() {
            return mIsPassword;
        }

        /**
         * The delegate is called when the Item is selected by a user.
         */
        public Callback<Item> getItemSelectedCallback() {
            return mItemSelectedCallback;
        }

        public void fetchFavicon(@Px int desiredSize, Callback<Bitmap> faviconCallback) {
            if (mFaviconProvider == null) {
                faviconCallback.onResult(null); // Use default icon without provider.
                return;
            }
            mFaviconProvider.fetchFavicon(desiredSize, faviconCallback);
        }
    }

    /**
     * A simple class that holds a list of {@link Observer}s which can be notified about new data by
     * directly passing that data into {@link PropertyProvider#notifyObservers(T[])}.
     * @param <T> Either {@link Action}s or {@link Tab}s provided by this class.
     */
    public static class PropertyProvider<T> implements Provider<T> {
        private final List<Observer<T>> mObservers = new ArrayList<>();
        protected int mType;

        public PropertyProvider() {
            this(Observer.DEFAULT_TYPE);
        }

        public PropertyProvider(int type) {
            mType = type;
        }

        @Override
        public void addObserver(Observer<T> observer) {
            mObservers.add(observer);
        }

        @Override
        public void notifyObservers(T[] items) {
            for (Observer<T> observer : mObservers) {
                observer.onItemsAvailable(mType, items);
            }
        }
    }

    private KeyboardAccessoryData() {}
}
