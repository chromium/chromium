// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bookmarks;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.ComponentName;
import android.content.Intent;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;

import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.back_press.BackPressHelper;
import org.chromium.chrome.browser.back_press.BackPressHelper.OnKeyDownHandler;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerCoordinator;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpenerImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkOpener;
import org.chromium.chrome.browser.bookmarks.BookmarkOpenerImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkPage;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.chrome.browser.url_constants.UrlConstantResolverFactory;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager.ScrimClient;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.edge_to_edge.EdgeToEdgeSystemBarColorHelper;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.util.ColorUtils;

/**
 * The activity that displays the bookmark UI on the phone. It keeps a {@link
 * BookmarkManagerCoordinator} inside of it and creates a snackbar manager. This activity should
 * only be shown on phones; on tablet the bookmark UI is shown inside of a tab (see {@link
 * BookmarkPage}).
 */
@NullMarked
public class BookmarkActivity extends SnackbarActivity {
    public static final int EDIT_BOOKMARK_REQUEST_CODE = 14;
    public static final String INTENT_VISIT_BOOKMARK_ID = "BookmarkEditActivity.VisitBookmarkId";

    private @Nullable BookmarkManagerCoordinator mBookmarkManagerCoordinator;
    private @Nullable BookmarkOpener mBookmarkOpener;
    private @Nullable OnKeyDownHandler mOnKeyDownHandler;
    private @Nullable ActivityWindowAndroid mWindowAndroid;

    @Override
    protected void onProfileAvailable(Profile profile) {
        super.onProfileAvailable(profile);
        setContentView(R.layout.bookmark_activity);

        @Nullable ComponentName parentComponent =
                IntentUtils.safeGetParcelableExtra(
                        getIntent(), IntentHandler.EXTRA_PARENT_COMPONENT);
        mBookmarkOpener =
                new BookmarkOpenerImpl(
                        () -> BookmarkModel.getForProfile(profile),
                        /* context= */ this,
                        /* componentName= */ parentComponent);
        mWindowAndroid =
                new ActivityWindowAndroid(
                        this,
                        /* listenToActivityState= */ true,
                        IntentRequestTracker.createFromActivity(this),
                        getInsetObserver(),
                        /* trackOcclusion= */ true);

        ScrimManager scrimManager =
                new ScrimManager(this, getContentView(), ScrimClient.BOOKMARK_ACTIVITY);
        scrimManager
                .getStatusBarColorSupplier()
                .addSyncObserverAndPostIfNonNull(this::applyScrimToStatusBar);

        ViewGroup sheetContainer = findViewById(R.id.sheet_container);
        BottomSheetController bottomSheetController =
                BottomSheetControllerFactory.createBottomSheetController(
                        () -> scrimManager,
                        (sheet) -> {},
                        getWindow(),
                        mWindowAndroid.getKeyboardDelegate(),
                        () -> sheetContainer,
                        () -> getEdgeToEdgeInset(),
                        /* desktopWindowStateManager= */ null);

        mBookmarkManagerCoordinator =
                new BookmarkManagerCoordinator(
                        mWindowAndroid,
                        this,
                        true,
                        getSnackbarManager(),
                        () -> bottomSheetController,
                        getActivityResultTracker(),
                        profile,
                        new BookmarkUiPrefs(ChromeSharedPreferences.getInstance()),
                        mBookmarkOpener,
                        new BookmarkManagerOpenerImpl(),
                        PriceDropNotificationManagerFactory.create(profile),
                        /* edgeToEdgePadAdjusterGenerator= */ view ->
                                EdgeToEdgeControllerFactory.createForViewAndObserveSupplier(
                                        view, getEdgeToEdgeSupplier()),
                        /* backPressManager= */ null);
        String url = getIntent().getDataString();
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(profile);
        if (TextUtils.isEmpty(url)) url = resolver.getBookmarksPageUrl();
        mBookmarkManagerCoordinator.updateForUrl(url);

        // The Bookmark view should be the lowest in the content view so the other overlays can be
        // shown on top (e.g. bottom sheet container).
        getContentView().addView(mBookmarkManagerCoordinator.getView(), 0);
        mOnKeyDownHandler =
                BackPressHelper.create(
                        this, getOnBackPressedDispatcher(), mBookmarkManagerCoordinator);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (mOnKeyDownHandler != null && mOnKeyDownHandler.onKeyDown(keyCode, event)) {
            return true;
        }

        return super.onKeyDown(keyCode, event);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        if (mBookmarkManagerCoordinator != null) {
            mBookmarkManagerCoordinator.onDestroyed();
        }

        if (mBookmarkOpener != null) {
            mBookmarkOpener = null;
        }

        if (mWindowAndroid != null) {
            mWindowAndroid.destroy();
            mWindowAndroid = null;
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (mWindowAndroid != null) {
            assumeNonNull(mWindowAndroid.getIntentRequestTracker())
                    .onActivityResult(requestCode, resultCode, data);
        }

        if (requestCode == EDIT_BOOKMARK_REQUEST_CODE && resultCode == RESULT_OK) {
            assumeNonNull(data);
            BookmarkId bookmarkId =
                    BookmarkId.getBookmarkIdFromString(
                            data.getStringExtra(INTENT_VISIT_BOOKMARK_ID));
            assumeNonNull(mBookmarkManagerCoordinator).openBookmark(bookmarkId);
        }
    }

    @Override
    protected ModalDialogManager createModalDialogManager() {
        return new ModalDialogManager(new AppModalPresenter(this), ModalDialogType.APP);
    }

    /**
     * @return The {@link BookmarkManagerCoordinator} for testing purposes.
     */
    public @Nullable BookmarkManagerCoordinator getManagerForTesting() {
        return mBookmarkManagerCoordinator;
    }

    private void applyScrimToStatusBar(@ColorInt int scrimColor) {
        @ColorInt int baseColor = SemanticColorUtils.getDefaultBgColor(this);
        @ColorInt int finalColor = ColorUtils.overlayColor(baseColor, scrimColor);
        EdgeToEdgeSystemBarColorHelper edgeToEdgeSystemBarColorHelper =
                (getEdgeToEdgeManager() != null)
                        ? getEdgeToEdgeManager().getEdgeToEdgeSystemBarColorHelper()
                        : null;
        StatusBarColorController.setStatusBarColor(
                edgeToEdgeSystemBarColorHelper, this, finalColor);
    }

    private int getEdgeToEdgeInset() {
        EdgeToEdgeController edgeToEdgeController = getEdgeToEdgeSupplier().get();
        return edgeToEdgeController == null ? 0 : edgeToEdgeController.getBottomInset();
    }
}
