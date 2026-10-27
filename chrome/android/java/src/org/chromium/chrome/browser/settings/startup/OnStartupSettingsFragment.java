// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.startup;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.text.TextUtils;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.IntentUtils;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.history.BrowsingHistoryBridge;
import org.chromium.chrome.browser.multiwindow.TabbedStartupCoordinator;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsHostUtil;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Settings fragment allowing users to configure Chrome's "On startup" behavior. */
@NullMarked
public class OnStartupSettingsFragment extends ChromeBaseSettingsFragment {
    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(
                    OnStartupSettingsFragment.class.getName(),
                    ChromeBaseSearchIndexProvider.INDEX_OPT_OUT);

    /* package */ static final String PREF_RADIO_BUTTON_GROUP = "on_startup_radio_button_group";

    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();
    private @Nullable CallbackController mCallbackController;
    private @Nullable PrefChangeRegistrar mPrefChangeRegistrar;
    private @Nullable FaviconHelper mFaviconHelper;
    private @Nullable OnStartupUrlDialogCoordinator mUrlDialogCoordinator;
    private @Nullable RadioButtonGroupOnStartupPreference mRadioGroupPref;

    // ChromeBaseSettingsFragment implementation.
    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        mPageTitle.set(getString(R.string.on_startup_title));
        SettingsUtils.addPreferencesFromResource(this, R.xml.on_startup_preferences);

        PrefService prefService = UserPrefs.get(getProfile());
        if (mCallbackController != null) {
            mCallbackController.destroy();
        }
        mCallbackController = new CallbackController();
        if (mFaviconHelper == null) {
            mFaviconHelper = new FaviconHelper();
        }
        mRadioGroupPref = findPreference(PREF_RADIO_BUTTON_GROUP);
        assertNonNull(mRadioGroupPref)
                .initialize(
                        TabbedStartupCoordinator.getSessionStartupType(),
                        TabbedStartupCoordinator.getSessionStartupUrls(prefService),
                        startupType ->
                                TabbedStartupCoordinator.setSessionStartupType(
                                        getProfile(), startupType),
                        new RadioButtonGroupOnStartupPreference.StartupPageItemActionListener() {
                            @Override
                            public void onAddNewStartupPage() {
                                showUrlDialog(
                                        prefService, /* editIndex= */ -1, /* initialUrl= */ null);
                            }

                            @Override
                            public void onEditStartupPage(int index, String url) {
                                showUrlDialog(prefService, index, url);
                            }

                            @Override
                            public void onRemoveStartupPage(int index, String url) {
                                List<String> urls =
                                        new ArrayList<>(
                                                TabbedStartupCoordinator.getSessionStartupUrls(
                                                        prefService));
                                if (index >= 0
                                        && index < urls.size()
                                        && TextUtils.equals(urls.get(index), url)) {
                                    urls.remove(index);
                                } else {
                                    int matchedIndex = urls.indexOf(url);
                                    if (matchedIndex < 0) {
                                        return;
                                    }
                                    urls.remove(matchedIndex);
                                }
                                TabbedStartupCoordinator.setSessionStartupUrls(prefService, urls);
                            }

                            @Override
                            public void onUseCurrentPages() {
                                useCurrentPages(prefService);
                            }
                        },
                        this::loadFaviconForUrl,
                        this::loadTitleForUrl);

        if (mPrefChangeRegistrar != null) {
            mPrefChangeRegistrar.destroy();
        }
        mPrefChangeRegistrar = new PrefChangeRegistrar(prefService);
        mPrefChangeRegistrar.addObserver(
                Pref.RESTORE_ON_STARTUP,
                () -> {
                    if (mRadioGroupPref != null) {
                        mRadioGroupPref.setStartupType(
                                TabbedStartupCoordinator.getSessionStartupType());
                    }
                });
        mPrefChangeRegistrar.addObserver(
                Pref.URLS_TO_RESTORE_ON_STARTUP,
                () -> {
                    if (mUrlDialogCoordinator != null) {
                        mUrlDialogCoordinator.dismissIfEditing();
                    }
                    if (mRadioGroupPref != null) {
                        mRadioGroupPref.setStartupUrls(
                                TabbedStartupCoordinator.getSessionStartupUrls(prefService));
                    }
                });
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
        if (mUrlDialogCoordinator != null) {
            mUrlDialogCoordinator.destroy();
            mUrlDialogCoordinator = null;
        }
        if (mPrefChangeRegistrar != null) {
            mPrefChangeRegistrar.destroy();
            mPrefChangeRegistrar = null;
        }
        if (mFaviconHelper != null) {
            mFaviconHelper.destroy();
            mFaviconHelper = null;
        }
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }

    @Override
    public @Nullable String getMainMenuKey() {
        return "on_startup";
    }

    private void showUrlDialog(
            PrefService prefService, int editIndex, @Nullable String initialUrl) {
        if (mUrlDialogCoordinator == null) {
            ModalDialogManager modalDialogManager =
                    assertNonNull(
                            ((ModalDialogManagerHolder) requireActivity()).getModalDialogManager());
            // TODO(crbug.com/578155261): Use a framework-level modal dialog property to switch
            // between TAB and APP modal types once supported.
            @ModalDialogType
            int dialogType =
                    SettingsHostUtil.isShownInTab(this) ? ModalDialogType.TAB : ModalDialogType.APP;
            mUrlDialogCoordinator =
                    new OnStartupUrlDialogCoordinator(
                            requireContext(), modalDialogManager, dialogType);
        }
        mUrlDialogCoordinator.show(
                initialUrl,
                validatedUrl ->
                        saveStartupPageUrl(prefService, editIndex, initialUrl, validatedUrl));
    }

    private static void saveStartupPageUrl(
            PrefService prefService,
            int editIndex,
            @Nullable String initialUrl,
            String validatedUrl) {
        List<String> urls =
                new ArrayList<>(TabbedStartupCoordinator.getSessionStartupUrls(prefService));
        if (initialUrl == null) {
            urls.add(validatedUrl);
        } else if (editIndex >= 0
                && editIndex < urls.size()
                && TextUtils.equals(urls.get(editIndex), initialUrl)) {
            urls.set(editIndex, validatedUrl);
        } else {
            return;
        }
        TabbedStartupCoordinator.setSessionStartupUrls(prefService, urls);
    }

    private void useCurrentPages(PrefService prefService) {
        Activity activity = getActivity();
        if (activity == null) {
            return;
        }
        TabWindowManager tabWindowManager = TabWindowManagerSingleton.getInstance();
        @WindowId
        int windowId =
                IntentUtils.safeGetIntExtra(
                        activity.getIntent(),
                        IntentHandler.EXTRA_WINDOW_ID,
                        TabWindowManager.INVALID_WINDOW_ID);
        if (windowId == TabWindowManager.INVALID_WINDOW_ID) {
            windowId = tabWindowManager.getIdForWindow(activity);
        }
        if (windowId == TabWindowManager.INVALID_WINDOW_ID) {
            return;
        }
        TabModelSelector selector = tabWindowManager.getTabModelSelectorById(windowId);
        if (selector == null) {
            return;
        }
        TabModel normalModel = selector.getModel(/* incognito= */ false);
        List<String> currentUrls = new ArrayList<>();
        for (int i = 0; i < normalModel.getCount(); i++) {
            Tab tab = normalModel.getTabAt(i);
            if (tab == null) continue;
            GURL gurl = tab.getUrl();
            if (UrlUtilities.isHttpOrHttps(gurl)) {
                currentUrls.add(gurl.getSpec());
            }
        }
        TabbedStartupCoordinator.setSessionStartupUrls(prefService, currentUrls);
    }

    private void loadTitleForUrl(String url, Callback<String> callback) {
        GURL gurl = new GURL(url);
        if (mCallbackController == null || !isAdded() || !gurl.isValid()) {
            return;
        }
        // Check open normal tabs across browser windows before querying HistoryService.
        for (TabModelSelector selector :
                TabWindowManagerSingleton.getInstance().getAllTabModelSelectors()) {
            if (selector == null) {
                continue;
            }
            TabModel normalModel = selector.getModel(/* incognito= */ false);
            for (int i = 0; i < normalModel.getCount(); i++) {
                Tab tab = normalModel.getTabAt(i);
                if (tab != null
                        && gurl.equals(tab.getUrl())
                        && !TextUtils.isEmpty(tab.getTitle())) {
                    callback.onResult(tab.getTitle());
                    return;
                }
            }
        }
        BrowsingHistoryBridge.getPageTitleForUrl(
                getProfile(),
                gurl,
                mCallbackController.makeCancelable(
                        title -> {
                            if (isAdded()) {
                                callback.onResult(title);
                            }
                        }));
    }

    private void loadFaviconForUrl(String url, Callback<@Nullable Drawable> callback) {
        GURL gurl = new GURL(url);
        if (mFaviconHelper == null
                || mCallbackController == null
                || !isAdded()
                || !gurl.isValid()) {
            return;
        }
        int faviconSizePx =
                getResources()
                        .getDimensionPixelSize(R.dimen.radio_button_with_description_icon_size);
        Callback<@Nullable Drawable> cancelableCallback =
                mCallbackController.makeCancelable(callback);
        Callback<@Nullable Bitmap> onForeignFavicon =
                mCallbackController.makeCancelable(
                        foreignBitmap -> {
                            if (!isAdded()) {
                                return;
                            }
                            cancelableCallback.onResult(
                                    foreignBitmap != null
                                            ? new BitmapDrawable(getResources(), foreignBitmap)
                                            : null);
                        });
        Callback<@Nullable Bitmap> onLocalFavicon =
                mCallbackController.makeCancelable(
                        bitmap -> {
                            if (bitmap != null && isAdded()) {
                                cancelableCallback.onResult(
                                        new BitmapDrawable(getResources(), bitmap));
                                return;
                            }
                            if (mFaviconHelper == null || !isAdded()) {
                                return;
                            }
                            if (!mFaviconHelper.getForeignFaviconImageForURL(
                                    getProfile(),
                                    gurl,
                                    faviconSizePx,
                                    /* fallbackToHost= */ true,
                                    (foreignBitmap, foreignIconUrl) ->
                                            onForeignFavicon.onResult(foreignBitmap))) {
                                onForeignFavicon.onResult(null);
                            }
                        });
        if (!mFaviconHelper.getLocalFaviconImageForURL(
                getProfile(),
                gurl,
                faviconSizePx,
                /* fallbackToHost= */ true,
                (bitmap, iconUrl) -> onLocalFavicon.onResult(bitmap))) {
            onLocalFavicon.onResult(null);
        }
    }
}
