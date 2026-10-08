// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils.isActivityStateBookmarkBarCompatible;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkModelObserver;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher.ActivityState;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.WindowFocusChangedObserver;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.bookmarks.BookmarkBarVisibilityState;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;

import java.util.List;

/** Controller for the Bookmark Bar In-Product Help. */
@NullMarked
public class BookmarkBarIphController extends BookmarkModelObserver
        implements Destroyable,
                BottomSheetObserver,
                ModalDialogManagerObserver,
                PauseResumeWithNativeObserver,
                WindowFocusChangedObserver {
    private static final String VARIANT_HIGH = "high";
    private static final String VARIANT_HIGH_TRACKING_ONLY = "high_tracking_only";

    private final Profile mProfile;
    private final BookmarkModel mBookmarkModel;
    private final AppMenuHandler mAppMenuHandler;
    private final View mToolbarMenuButton;
    private final UserEducationHelper mUserEducationHelper;
    private final NonNullObservableSupplier<Boolean> mXrSpaceModeObservableSupplier;
    private final @Nullable BottomSheetController mBottomSheetController;
    private final @Nullable ModalDialogManager mModalDialogManager;
    private final @Nullable ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    private boolean mHasWindowFocus = true;
    private boolean mShowIphPending;

    /**
     * @param activity The current activity.
     * @param profile The current user profile.
     * @param appMenuHandler The appMenuHandler, used to highlight the settings item.
     * @param toolbarMenuButton The toolbar menu button (3-dot) to which the IPH will be anchored.
     * @param bookmarkModel The bookmarkModel, which this class will observe for events.
     * @param xrSpaceModeObservableSupplier The supplier for the XR space mode state.
     * @param bottomSheetController The controller for bottom sheets, used to defer IPH until bottom
     *     sheets are dismissed.
     * @param modalDialogManager The manager for modal dialogs, used to defer IPH until dialogs are
     *     dismissed.
     * @param activityLifecycleDispatcher The lifecycle dispatcher for the activity, used to defer
     *     IPH until dialog activities are dismissed.
     */
    public BookmarkBarIphController(
            Activity activity,
            Profile profile,
            AppMenuHandler appMenuHandler,
            View toolbarMenuButton,
            BookmarkModel bookmarkModel,
            NonNullObservableSupplier<Boolean> xrSpaceModeObservableSupplier,
            @Nullable BottomSheetController bottomSheetController,
            @Nullable ModalDialogManager modalDialogManager,
            @Nullable ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        this(
                profile,
                appMenuHandler,
                toolbarMenuButton,
                bookmarkModel,
                new UserEducationHelper(activity, profile, new Handler(Looper.getMainLooper())),
                xrSpaceModeObservableSupplier,
                bottomSheetController,
                modalDialogManager,
                activityLifecycleDispatcher);
    }

    @VisibleForTesting
    protected BookmarkBarIphController(
            Profile profile,
            AppMenuHandler appMenuHandler,
            View toolbarMenuButton,
            BookmarkModel bookmarkModel,
            UserEducationHelper userEducationHelper,
            NonNullObservableSupplier<Boolean> xrSpaceModeObservableSupplier) {
        this(
                profile,
                appMenuHandler,
                toolbarMenuButton,
                bookmarkModel,
                userEducationHelper,
                xrSpaceModeObservableSupplier,
                /* bottomSheetController= */ null,
                /* modalDialogManager= */ null,
                /* activityLifecycleDispatcher= */ null);
    }

    @VisibleForTesting
    protected BookmarkBarIphController(
            Profile profile,
            AppMenuHandler appMenuHandler,
            View toolbarMenuButton,
            BookmarkModel bookmarkModel,
            UserEducationHelper userEducationHelper,
            NonNullObservableSupplier<Boolean> xrSpaceModeObservableSupplier,
            @Nullable BottomSheetController bottomSheetController,
            @Nullable ModalDialogManager modalDialogManager,
            @Nullable ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        mProfile = profile;
        mAppMenuHandler = appMenuHandler;
        mToolbarMenuButton = toolbarMenuButton;
        mBookmarkModel = bookmarkModel;
        mXrSpaceModeObservableSupplier = xrSpaceModeObservableSupplier;
        mBottomSheetController = bottomSheetController;
        mModalDialogManager = modalDialogManager;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        // The BookmarkBarIphController object subscribes to mBookmarkModel.
        mBookmarkModel.addObserver(this);
        if (mBottomSheetController != null) {
            mBottomSheetController.addObserver(this);
        }
        if (mModalDialogManager != null) {
            mModalDialogManager.addObserver(this);
        }
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.register(this);
        }
        mUserEducationHelper = userEducationHelper;
        boolean mightTriggerIph =
                TrackerFactory.getTrackerForProfile(profile)
                        .wouldTriggerHelpUi(FeatureConstants.BOOKMARK_BAR_VISIBILITY_FEATURE);
        if (mightTriggerIph && passesPreChecks()) {
            // We have this so that the bookmark model is loaded when the app opens even when the
            // bookmark bar is invisible/disabled.
            mBookmarkModel.finishLoadingBookmarkModel(() -> {});
        }
    }

    @Override
    public void destroy() {
        mBookmarkModel.removeObserver(this);
        if (mBottomSheetController != null) {
            mBottomSheetController.removeObserver(this);
        }
        if (mModalDialogManager != null) {
            mModalDialogManager.removeObserver(this);
        }
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(this);
        }
    }

    /** This callback ensures we only access the bookmark model after it has been fully loaded. */
    @Override
    public void bookmarkModelLoaded() {
        // Trigger condition 1 (medium variant only): Check for existing bookmarks only in the
        // bookmarks bar (not mobile bookmarks or reading list) now that the bookmark model is
        // loaded.
        if (!isHighVariant() && hasAtLeastOneBookmarkInBookmarksBar()) {
            showIph();
        }
    }

    @Override
    public void bookmarkNodeAdded(BookmarkItem parent, int index, boolean addedByUser) {
        // Only trigger the IPH if the bookmark was added by the user on this device.
        // This ignores bookmarks that are added via sync.
        if (!addedByUser) return;

        BookmarkId addedId = mBookmarkModel.getChildIds(parent.getId()).get(index);
        BookmarkItem addedItem = mBookmarkModel.getBookmarkById(addedId);

        // The IPH is triggered only when an actual bookmark, and not an empty folder, is added.
        if (addedItem == null || addedItem.isFolder()) return;

        // Trigger condition 2: A new bookmark was added to the bookmarks bar, mobile bookmarks, or
        // reading list. In the high variant, this requires the user to also have bookmarks in their
        // Bookmarks Bar folder and a profile synced pref with the bookmark bar visible.
        if (!isHighVariant() || isEligibleForHighVariantBookmarkManagerOrBookmarkAddedTrigger()) {
            showIph();
        }
    }

    @Override
    public void bookmarkManagerOpened() {
        if (isHighVariant() && isEligibleForHighVariantBookmarkManagerOrBookmarkAddedTrigger()) {
            showIph();
        }
    }

    @Override
    public void bookmarkFolderViewed(@Nullable BookmarkId folderId) {
        if (isHighVariant() && isBookmarksBarFolder(folderId)) {
            showIphWhenBottomSheetsAndDialogsDismissed();
        }
    }

    private boolean isBookmarksBarFolder(@Nullable BookmarkId folderId) {
        return folderId != null
                && (folderId.equals(mBookmarkModel.getDesktopFolderId())
                        || folderId.equals(mBookmarkModel.getAccountDesktopFolderId()));
    }

    /**
     * Returns whether the user's profile-synced preferences ({@link UserPrefs}) have the bookmark
     * bar visible (either always or only on the NTP).
     *
     * <p>Bookmark bar visibility is controlled by four preferences across platforms/form factors
     * (see {@link BookmarkBarUtils}, {@link BookmarkBarConstants}, {@code
     * components/bookmarks/common/bookmark_pref_names.h}, and {@code
     * components/bookmarks/browser/bookmark_utils.cc}):
     *
     * <ul>
     *   <li><b>{@link Pref#SHOW_BOOKMARK_BAR}</b> ({@code bookmarks::prefs::kShowBookmarkBar} =
     *       {@code "bookmark_bar.show_on_all_tabs"}, default {@code false}): <b>Synced profile
     *       pref</b>. Used on Desktop (W/M/L/CrOS) when {@code
     *       ntp_features::kNtpSimplificationBookmarkBar} is disabled (and mirrored to {@code true}
     *       when {@code ALWAYS_SHOW} is active in {@code
     *       chrome/browser/ui/bookmarks/bookmark_bar_controller.cc}), and on Desktop Android
     *       ({@code DeviceInfo.isDesktop() == true}) when {@link
     *       ChromeFeatureList#BOOKMARKS_BAR_NTP} is disabled. Not set on Android tablets/foldables
     *       or phones.
     *   <li><b>{@link Pref#BOOKMARK_BAR_VISIBILITY_STATE}</b> ({@code
     *       bookmarks::prefs::kBookmarkBarVisibilityState} = {@code
     *       "bookmark_bar.visibility_state"}, default {@link
     *       BookmarkBarVisibilityState#ONLY_SHOW_ON_NTP}): <b>Synced profile pref</b>. Used on
     *       Desktop (W/M/L/CrOS) when {@code kNtpSimplificationBookmarkBar} is enabled (enabled by
     *       default in {@code components/search/ntp_features.cc}), and on Desktop Android ({@code
     *       DeviceInfo.isDesktop() == true}) when {@link ChromeFeatureList#BOOKMARKS_BAR_NTP} is
     *       enabled. Not set on Android tablets/foldables or phones.
     *   <li><b>{@link BookmarkBarConstants#BOOKMARK_BAR_SHOW_BOOKMARK_BAR}</b> (default {@code
     *       false}) and <b>{@link
     *       BookmarkBarConstants#BOOKMARK_BAR_BOOKMARK_BAR_VISIBILITY_STATE}</b> (default {@link
     *       BookmarkBarVisibilityState#ALWAYS_HIDE}): <b>Local device-only prefs</b> in Android
     *       {@code SharedPreferences} (<b>not synced</b>). Used only on Android tablets and
     *       unfolded foldables ({@code DeviceInfo.isDesktop() == false}; phones do not support the
     *       bookmark bar), selected when {@link ChromeFeatureList#BOOKMARKS_BAR_NTP} is disabled vs
     *       enabled respectively. These local device prefs are checked in {@link
     *       #passesPreChecks()}, not here.
     * </ul>
     *
     * <p><b>Why {@link ChromeFeatureList#BOOKMARKS_BAR_NTP} is not checked here:</b> {@code
     * BOOKMARKS_BAR_NTP} ({@code kBookmarksBarNTP} in {@code
     * chrome/browser/flags/android/chrome_feature_list.cc}) is the Android counterpart to Desktop's
     * {@code kNtpSimplificationBookmarkBar}, switching Android UI ({@code
     * BookmarkBarSettingsFragment}) from the boolean prefs to the tri-state prefs. Regardless of
     * whether {@code BOOKMARKS_BAR_NTP} is enabled on this Android device, a user porting from
     * Desktop (W/M/L/CrOS, where {@code kNtpSimplificationBookmarkBar} is enabled by default, or
     * another Desktop Android device) may have their synced choice stored in {@link
     * Pref#BOOKMARK_BAR_VISIBILITY_STATE} (note that {@code ONLY_SHOW_ON_NTP} sets {@link
     * Pref#SHOW_BOOKMARK_BAR} to {@code false} on Desktop) or in {@link Pref#SHOW_BOOKMARK_BAR} if
     * set on a Desktop client without the tri-state flag. Checking both prefs covers all Desktop
     * sources.
     */
    private boolean isUserPrefsBookmarkBarVisible() {
        // 1. Legacy boolean synced profile pref (default = false in bookmark_utils.cc). Because the
        // default is false, this is true only when explicitly enabled on Desktop or via policy.
        if (BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(mProfile)) {
            return true;
        }

        // 2. Tri-state synced profile pref. In components/bookmarks/browser/bookmark_utils.cc,
        // kBookmarkBarVisibilityState is registered with a default of ONLY_SHOW_ON_NTP (matching
        // legacy Desktop NTP behavior). On Desktop, user settings or NTP bookmark bar interactions
        // (UpdateBookmarkBarVisibilityPrefOnUserAction in
        // chrome/browser/ui/bookmarks/bookmark_utils.cc) explicitly write to the user pref store,
        // making isDefaultValuePreference() false and syncing the value. On Android-only profiles
        // where Desktop never wrote/synced this pref, isDefaultValuePreference() is true and
        // getInteger() returns the default ONLY_SHOW_ON_NTP, so we must ignore default values.
        if (UserPrefs.get(mProfile.getOriginalProfile())
                .isDefaultValuePreference(Pref.BOOKMARK_BAR_VISIBILITY_STATE)) {
            return false;
        }

        @BookmarkBarVisibilityState
        int state = BookmarkBarUtils.getUserPrefsBookmarkBarVisibilityState(mProfile);
        return state == BookmarkBarVisibilityState.ALWAYS_SHOW
                || state == BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP;
    }

    private boolean isEligibleForHighVariantBookmarkManagerOrBookmarkAddedTrigger() {
        return hasAtLeastOneBookmarkInBookmarksBar() && isUserPrefsBookmarkBarVisible();
    }

    private boolean isHighVariant() {
        String variant =
                ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.IPH_BOOKMARK_BAR_VISIBILITY,
                        ChromeFeatureList.IPH_BOOKMARK_BAR_VISIBILITY_VARIANT);
        return VARIANT_HIGH.equals(variant) || VARIANT_HIGH_TRACKING_ONLY.equals(variant);
    }

    public boolean hasAtLeastOneBookmark(@Nullable BookmarkId folderId) {
        if (folderId == null) return false;

        // Get all of the children in the bookmarks bar, which may include both bookmarks and
        // folders.
        List<BookmarkId> children = mBookmarkModel.getChildIds(folderId);
        for (BookmarkId childId : children) {
            BookmarkItem item = mBookmarkModel.getBookmarkById(childId);
            if (item != null && !item.isFolder()) {
                return true;
            }
        }
        return false;
    }

    public boolean hasAtLeastOneBookmarkInBookmarksBar() {
        // Check the local/device bookmarks bar.
        if (hasAtLeastOneBookmark(mBookmarkModel.getDesktopFolderId())) {
            return true;
        }

        // Check the account/synced bookmarks bar.
        if (hasAtLeastOneBookmark(mBookmarkModel.getAccountDesktopFolderId())) {
            return true;
        }

        // No bookmarks were found.
        return false;
    }

    private void showIphWhenBottomSheetsAndDialogsDismissed() {
        if (!passesPreChecks()) return;
        if (areBottomSheetsAndDialogsDismissed()) {
            showIph();
        } else {
            mShowIphPending = true;
        }
    }

    private void maybeShowPendingIph() {
        if (mShowIphPending && areBottomSheetsAndDialogsDismissed()) {
            mShowIphPending = false;
            showIph();
        }
    }

    private boolean areBottomSheetsAndDialogsDismissed() {
        if (mBottomSheetController == null
                || mBottomSheetController.isSheetOpen()
                || mBottomSheetController.getCurrentSheetContent() != null) {
            return false;
        }
        if (mModalDialogManager == null || mModalDialogManager.isShowing()) {
            return false;
        }
        if (mActivityLifecycleDispatcher == null
                || mActivityLifecycleDispatcher.getCurrentActivityState()
                        != ActivityState.RESUMED_WITH_NATIVE) {
            return false;
        }
        return mHasWindowFocus;
    }

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        maybeShowPendingIph();
    }

    @Override
    public void onSheetContentChanged(@Nullable BottomSheetContent newContent) {
        maybeShowPendingIph();
    }

    @Override
    public void onLastDialogDismissed() {
        maybeShowPendingIph();
    }

    @Override
    public void onResumeWithNative() {
        maybeShowPendingIph();
    }

    @Override
    public void onPauseWithNative() {}

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        mHasWindowFocus = hasFocus;
        if (hasFocus) {
            maybeShowPendingIph();
        }
    }

    /** Shows the In-Product Help text bubble. */
    @VisibleForTesting
    void showIph() {
        if (!passesPreChecks()) return;

        mToolbarMenuButton.post(
                () -> {
                    mUserEducationHelper.requestShowIph(
                            new IphCommandBuilder(
                                            mToolbarMenuButton.getContext().getResources(),
                                            FeatureConstants.BOOKMARK_BAR_VISIBILITY_FEATURE,
                                            R.string.bookmark_bar_iph_message,
                                            R.string.bookmark_bar_iph_message)
                                    .setAnchorView(mToolbarMenuButton)
                                    .setOnShowCallback(
                                            () ->
                                                    // Highlight the app menu settings item.
                                                    mAppMenuHandler.setMenuHighlight(
                                                            R.id.preferences_id))
                                    .setOnDismissCallback(mAppMenuHandler::clearMenuHighlight)
                                    .build());
                });
    }

    @Override
    public void bookmarkModelChanged() {}

    /**
     * Runs all of the prerequisite checks for showing the IPH. These checks do not require the
     * bookmark model to be loaded.
     */
    private boolean passesPreChecks() {
        boolean isXrFullSpaceMode = mXrSpaceModeObservableSupplier.get();
        // Do not show the IPH in XR full space mode, on devices that use profile-synced user prefs
        // (e.g. Desktop Android), or if the activity state (e.g. window width) is incompatible with
        // the bookmark bar. Note: BookmarkBarUtils visibility methods also check these, but they
        // return false / ALWAYS_HIDE when in XR mode or incompatible, which would otherwise pass
        // the "bookmark bar is hidden" check below.
        if (isXrFullSpaceMode
                || BookmarkBarUtils.shouldUseProfileUserPrefs()
                || !isActivityStateBookmarkBarCompatible(mToolbarMenuButton.getContext())) {
            return false;
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.BOOKMARKS_BAR_NTP)) {
            // If the bookmark bar is already visible (ALWAYS_SHOW or ONLY_SHOW_ON_NTP), or if the
            // user has explicitly set the device preference, do not show the IPH.
            if (BookmarkBarUtils.getBookmarkBarVisibilityState(
                            mToolbarMenuButton.getContext(), mProfile, isXrFullSpaceMode)
                    != BookmarkBarVisibilityState.ALWAYS_HIDE) {
                return false;
            }
            if (BookmarkBarUtils.hasUserSetDevicePrefBookmarkBarVisibilityState()) {
                return false;
            }
        } else {
            // If the bookmark bar is already visible, or if the user has explicitly set the device
            // preference, do not show the IPH.
            if (BookmarkBarUtils.isBookmarkBarVisible(
                    mToolbarMenuButton.getContext(), mProfile, isXrFullSpaceMode)) {
                return false;
            }
            if (BookmarkBarUtils.hasUserSetDevicePrefShowBookmarksBar()) {
                return false;
            }
        }

        // All checks passed.
        return true;
    }
}
