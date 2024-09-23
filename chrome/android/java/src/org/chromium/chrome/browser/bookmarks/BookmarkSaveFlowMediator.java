// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;
import android.view.View;
import android.widget.CompoundButton;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkSaveFlowProperties.FolderText;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkMetrics.PriceTrackingState;
import org.chromium.chrome.browser.commerce.PriceTrackingUtils;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.commerce.core.CommerceSubscription;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.SubscriptionsObserver;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Controls the bookmarks save-flow, which has 2 variants: standard, improved. The two variants have
 * different properties, so each of the methods is branched to reflect that.
 * BookmarkSaveFlowProperties shouldn't be used for the improved variant (it'll crash), and the same
 * is true for ImprovedBookmarkSaveFlow properties with the standard variant. standard: The default
 * save experience prior to android-improved-bookmarks. improved: The new experience for saving when
 * android-improved-bookmarks is enabled.
 */
public class BookmarkSaveFlowMediator extends BookmarkModelObserver
        implements SubscriptionsObserver {
    private static final String FOLDER_TEXT_TOKEN = "%1$s";
    private final Context mContext;
    private final Runnable mCloseRunnable;
    private final BookmarkImageFetcher mBookmarkImageFetcher;
    private final CallbackController mCallbackController = new CallbackController();
    private final PropertyModel mPropertyModel;
    private final BookmarkModel mBookmarkModel;
    private final ShoppingService mShoppingService;
    private final Profile mProfile;
    private final IdentityManager mIdentityManager;

    private BookmarkId mBookmarkId;
    private PowerBookmarkMeta mPowerBookmarkMeta;
    private boolean mWasBookmarkMoved;
    private boolean mIsNewBookmark;
    private CommerceSubscription mSubscription;
    private Callback<Boolean> mSubscriptionsManagerCallback;
    private String mFolderName;

    /**
     * @param bookmarkModel The {@link BookmarkModel} which supplies the data.
     * @param propertyModel The {@link PropertyModel} which allows the mediator to push data to the
     *     model.
     * @param context The {@link Context} associated with this mediator.
     * @param closeRunnable A {@link Runnable} which closes the bookmark save flow.
     * @param shoppingService Used to manage the price-tracking subscriptions.
     * @param bookmarkImageFetcher Used to fetch images/favicons for bookmarks.
     * @param profile The current chrome profile.
     * @param identityManager The {@link IdentityManager} which supplies the account data.
     */
    public BookmarkSaveFlowMediator(
            @NonNull BookmarkModel bookmarkModel,
            @NonNull PropertyModel propertyModel,
            @NonNull Context context,
            @NonNull Runnable closeRunnable,
            @NonNull ShoppingService shoppingService,
            @NonNull BookmarkImageFetcher bookmarkImageFetcher,
            @NonNull Profile profile,
            @NonNull IdentityManager identityManager) {
        mBookmarkModel = bookmarkModel;
        mBookmarkModel.addObserver(this);

        mPropertyModel = propertyModel;
        mContext = context;
        mCloseRunnable = closeRunnable;

        mShoppingService = shoppingService;
        if (mShoppingService != null) {
            mShoppingService.addSubscriptionsObserver(this);
        }

        mBookmarkImageFetcher = bookmarkImageFetcher;
        mProfile = profile;
        mIdentityManager = identityManager;
    }

    /**
     * Shows bottom sheet save-flow for the given {@link BookmarkId}.
     *
     * @param bookmarkId The {@link BookmarkId} to show.
     * @param meta The power bookmark metadata for the given BookmarkId.
     * @param fromExplicitTrackUi Whether the bookmark was added via a dedicated tracking entry
     *     point. This will change the UI of the bookmark save flow, either adding type-specific
     *     text (e.g. price tracking text) or adding UI bits to allow users to upgrade a regular
     *     bookmark.
     * @param wasBookmarkMoved Whether the save flow is shown as a result of a moved bookmark.
     * @param isNewBookmark Whether the bookmark is newly created.
     */
    public void show(
            BookmarkId bookmarkId,
            @Nullable PowerBookmarkMeta meta,
            boolean fromExplicitTrackUi,
            boolean wasBookmarkMoved,
            boolean isNewBookmark) {
        RecordUserAction.record("MobileBookmark.SaveFlow.Show");

        mBookmarkId = bookmarkId;
        mPowerBookmarkMeta = meta;
        mWasBookmarkMoved = wasBookmarkMoved;
        mIsNewBookmark = isNewBookmark;

        // Any flow from the explicit price tracking UI is attempting to track the bookmark. If the
        // product is already being tracked and we got to this point, the call is a no-op.
        if (fromExplicitTrackUi) {
            PriceTrackingUtils.setPriceTrackingStateForBookmark(
                    mProfile,
                    bookmarkId.getId(),
                    true,
                    (success) -> {
                        // TODO(b:326488326): Show error message if not successful.
                    });
        }

        mPropertyModel.set(
                ImprovedBookmarkSaveFlowProperties.BOOKMARK_ROW_CLICK_LISTENER,
                this::onEditClicked);

        if (meta != null) {
            mSubscription = PowerBookmarkUtils.createCommerceSubscriptionForPowerBookmarkMeta(meta);
        }

        BookmarkItem item = mBookmarkModel.getBookmarkById(bookmarkId);
        bindBookmarkProperties(item, mPowerBookmarkMeta, mWasBookmarkMoved);
        bindPowerBookmarkProperties(mPowerBookmarkMeta, fromExplicitTrackUi);
        bindImage(item, meta);
    }

    private void bindBookmarkProperties(
            BookmarkItem item, PowerBookmarkMeta meta, boolean wasBookmarkMoved) {
        mFolderName = mBookmarkModel.getBookmarkTitle(item.getParentId());

        mPropertyModel.set(ImprovedBookmarkSaveFlowProperties.TITLE, createTitleCharSequence());
        mPropertyModel.set(
                ImprovedBookmarkSaveFlowProperties.SUBTITLE,
                createSubTitleCharSequnce(wasBookmarkMoved));
    }

    private CharSequence createTitleCharSequence() {
        if (mBookmarkModel.areAccountBookmarkFoldersActive()) {
            return createHighlightedCharSequence(
                    mContext,
                    new FolderText(
                            mContext.getString(
                                    R.string.account_bookmark_save_flow_title, mFolderName),
                            mContext.getString(R.string.account_bookmark_save_flow_title)
                                    .indexOf(FOLDER_TEXT_TOKEN),
                            mFolderName.length()));
        } else {
            return mContext.getString(R.string.bookmark_save_flow_title);
        }
    }

    private CharSequence createSubTitleCharSequnce(boolean wasBookmarkMoved) {
        if (mBookmarkModel.areAccountBookmarkFoldersActive()) {
            BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(mBookmarkId);
            return bookmarkItem.isAccountBookmark()
                    ? mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN).getEmail()
                    : mContext.getString(R.string.account_bookmark_save_flow_subtitle_local);
        } else {
            String folderDisplayTextRaw = getFolderDisplayTextRaw(wasBookmarkMoved);
            String folderDisplayText = getFolderDisplayText(wasBookmarkMoved);
            return createHighlightedCharSequence(
                    mContext,
                    new FolderText(
                            folderDisplayText,
                            folderDisplayTextRaw.indexOf(FOLDER_TEXT_TOKEN),
                            mFolderName.length()));
        }
    }

    @VisibleForTesting
    static CharSequence createHighlightedCharSequence(Context context, FolderText folderText) {
        SpannableString ss = new SpannableString(folderText.getDisplayText());
        ForegroundColorSpan fcs =
                new ForegroundColorSpan(SemanticColorUtils.getDefaultTextColorAccent1(context));
        ss.setSpan(
                fcs,
                folderText.getFolderTitleStartIndex(),
                folderText.getFolderTitleEndIndex(),
                Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        return ss;
    }

    private void bindPowerBookmarkProperties(
            @Nullable PowerBookmarkMeta meta, boolean fromExplicitTrackUi) {
        if (meta == null) return;

        if (meta.hasShoppingSpecifics()) {
            setPriceTrackingNotificationUiEnabled(true);
            mPropertyModel.set(
                    ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_CHECKED, false);
            mPropertyModel.set(ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_VISIBLE, true);
            mPropertyModel.set(
                    ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_LISTENER,
                    this::handlePriceTrackingSwitchToggle);

            PriceTrackingUtils.isBookmarkPriceTracked(
                    mProfile,
                    mBookmarkId.getId(),
                    (subscribed) -> {
                        mPropertyModel.set(
                                ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_CHECKED,
                                subscribed);
                        PowerBookmarkMetrics.reportBookmarkSaveFlowPriceTrackingState(
                                PriceTrackingState.PRICE_TRACKING_SHOWN);
                    });
        }
    }

    void bindImage(BookmarkItem item, @Nullable PowerBookmarkMeta meta) {
        Callback<Drawable> callback =
                drawable -> {
                    mPropertyModel.set(
                            ImprovedBookmarkSaveFlowProperties.BOOKMARK_ROW_ICON, drawable);
                };

        mBookmarkImageFetcher.fetchImageForBookmarkWithFaviconFallback(item, callback);
    }

    void handlePriceTrackingSwitchToggle(CompoundButton view, boolean toggled) {
        // Make sure we're getting feedback from the UI for the model, otherwise a failure won't be
        // able to update the UI correctly.
        setPriceTrackingToggleVisualsOnly(toggled);
        if (mSubscriptionsManagerCallback == null) {
            mSubscriptionsManagerCallback =
                    mCallbackController.makeCancelable(
                            (Boolean success) -> {
                                setPriceTrackingToggleVisualsOnly(success && view.isChecked());
                                setPriceTrackingNotificationUiEnabled(success);
                            });
        }

        PriceTrackingUtils.setPriceTrackingStateForBookmark(
                mProfile,
                mBookmarkId.getId(),
                toggled,
                mSubscriptionsManagerCallback,
                mIsNewBookmark);
        PowerBookmarkMetrics.reportBookmarkSaveFlowPriceTrackingState(
                toggled
                        ? PriceTrackingState.PRICE_TRACKING_ENABLED
                        : PriceTrackingState.PRICE_TRACKING_DISABLED);
    }

    void setPriceTrackingNotificationUiEnabled(boolean enabled) {
        mPropertyModel.set(ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_ENABLED, enabled);
    }

    void destroy() {
        mBookmarkModel.removeObserver(this);
        if (mShoppingService != null) {
            mShoppingService.removeSubscriptionsObserver(this);
        }

        mBookmarkId = null;

        if (mCallbackController != null) {
            mCallbackController.destroy();
        }
    }

    @VisibleForTesting
    void setPriceTrackingToggleVisualsOnly(boolean enabled) {
        mPropertyModel.set(ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_LISTENER, null);
        mPropertyModel.set(
                ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_CHECKED, enabled);
        mPropertyModel.set(
                ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_LISTENER,
                this::handlePriceTrackingSwitchToggle);
    }

    void setSubscriptionForTesting(CommerceSubscription subscription) {
        mSubscription = subscription;
    }

    // BookmarkModelObserver implementation

    @Override
    public void bookmarkModelChanged() {
        // Possibility that the bookmark is deleted while in accessibility mode.
        if (mBookmarkId == null || mBookmarkModel.getBookmarkById(mBookmarkId) == null) {
            mCloseRunnable.run();
            return;
        }

        BookmarkItem item = mBookmarkModel.getBookmarkById(mBookmarkId);
        bindBookmarkProperties(item, mPowerBookmarkMeta, mWasBookmarkMoved);
    }

    // SubscriptionsObserver implementation

    @Override
    public void onSubscribe(CommerceSubscription subscription, boolean succeeded) {
        if (!succeeded || !subscription.equals(mSubscription)) return;
        setPriceTrackingToggleVisualsOnly(true);

        // Make sure the notification channel is initialized when the user tracks the product.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            PriceDropNotificationManagerFactory.create(mProfile).createNotificationChannel();
        }
    }

    @Override
    public void onUnsubscribe(CommerceSubscription subscription, boolean succeeded) {
        if (!succeeded || !subscription.equals(mSubscription)) return;
        setPriceTrackingToggleVisualsOnly(false);
    }

    // Private functions

    private String getFolderDisplayTextRaw(boolean wasBookmarkMoved) {
        @StringRes int stringRes;
        if (wasBookmarkMoved) {
            stringRes = R.string.bookmark_page_moved_location;
        } else {
            stringRes = R.string.bookmark_page_saved_location;
        }

        return mContext.getString(stringRes);
    }

    private String getFolderDisplayText(boolean wasBookmarkMoved) {
        @StringRes int stringRes;
        if (wasBookmarkMoved) {
            stringRes = R.string.bookmark_page_moved_location;
        } else {
            stringRes = R.string.bookmark_page_saved_location;
        }

        return mContext.getString(stringRes, mFolderName);
    }

    private void onEditClicked(View v) {
        RecordUserAction.record("MobileBookmark.SaveFlow.EditBookmark");
        BookmarkUtils.startEditActivity(mContext, mBookmarkId);
        mCloseRunnable.run();
    }

    private void onFolderSelectClicked(View v) {
        RecordUserAction.record("MobileBookmark.SaveFlow.EditFolder");
        BookmarkUtils.startFolderPickerActivity(mContext, mBookmarkId);
        TrackerFactory.getTrackerForProfile(mProfile)
                .notifyEvent(EventConstants.SHOPPING_LIST_SAVE_FLOW_FOLDER_TAP);
        mCloseRunnable.run();
    }
}
