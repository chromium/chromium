// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.ntp.FakeboxDelegate;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.toolbar.top.ToolbarActionModeCallback;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Container that holds the {@link UrlBar} and SSL state related with the current {@link Tab}.
 */
public interface LocationBar extends UrlBarDelegate, FakeboxDelegate, Destroyable {

    /** Handle all necessary tasks that can be delayed until initialization completes. */
    default void onDeferredStartup() {}

    /** Handles native dependent initialization for this class. */
    void onNativeLibraryReady();

    /** Triggered when the current tab has changed to a {@link NewTabPage}. */
    default void onTabLoadingNTP(NewTabPage ntp){};

    /**
     * Call to force the UI to update the state of various buttons based on whether or not the
     * current tab is incognito.
     */
    void updateVisualsForState();

    /**
     * Sets the displayed URL to be the URL of the page currently showing.
     *
     * <p>The URL is converted to the most user friendly format (removing HTTP:// for example).
     *
     * <p>If the current tab is null, the URL text will be cleared.
     */
    void setUrlToPageUrl();

    /** Sets the displayed title to the page title. */
    void setTitleToPageTitle();

    /**
     * Sets whether the location bar should have a layout showing a title.
     *
     * @param showTitle Whether the title should be shown.
     */
    void setShowTitle(boolean showTitle);

    /**
     * Update the visuals based on a loading state change.
     *
     * @param updateUrl Whether to update the URL as a result of the this call.
     */
    void updateLoadingState(boolean updateUrl);

    /** Sets the {@link ToolbarDataProvider} to be used for accessing {@link Toolbar} state. */
    void setToolbarDataProvider(ToolbarDataProvider model);

    /** Gets the {@link ToolbarDataProvider} to be used for accessing {@link Toolbar} state. */
    ToolbarDataProvider getToolbarDataProvider();

    /**
     * Initialize controls that will act as hooks to various functions.
     *
     * @param windowDelegate {@link WindowDelegate} that will provide {@link Window} related info.
     * @param windowAndroid {@link WindowAndroid} that is used by the owning {@link Activity}.
     * @param activityTabProvider An {@link ActivityTabProvider} to access the activity's current
     *         tab.
     * @param modalDialogManagerSupplier A supplier for {@link ModalDialogManager} object.
     * @param shareDelegateSupplier A supplier for {@link ShareDelegate} object.
     * @param incognitoStateProvider An {@link IncognitoStateProvider} to access the current
     *         incognito state.
     */
    void initializeControls(WindowDelegate windowDelegate, WindowAndroid windowAndroid,
            ActivityTabProvider activityTabProvider,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            Supplier<ShareDelegate> shareDelegateSupplier,
            IncognitoStateProvider incognitoStateProvider);

    /**
     * Triggers the cursor to be visible in the UrlBar without triggering any of the focus animation
     * logic.
     *
     * <p>Only applies to devices with a hardware keyboard attached.
     */
    void showUrlBarCursorWithoutFocusAnimations();

    /** Selects all of the editable text in the {@link UrlBar}. */
    void selectAll();

    /**
     * Reverts any pending edits of the location bar and reset to the page state. This does not
     * change the focus state of the location bar.
     */
    void revertChanges();

    /** Updates the security icon displayed in the LocationBar. */
    void updateStatusIcon();

    /** Returns {@link ViewGroup} that this container holds. */
    View getContainerView();

    /**
     * TODO(twellington): Try to remove this method. It's only used to return an in-product help
     *                    bubble anchor view... which should be moved out of tab and perhaps into
     *                    the status bar icon component.
     * @return The view containing the security icon.
     */
    View getSecurityIconView();

    /** Updates the state of the mic button if there is one. */
    void updateMicButtonState();

    /** Sets the callback to be used by default for text editing action bar. */
    void setDefaultTextEditActionModeCallback(ToolbarActionModeCallback callback);

    /**
     * Sets the (observable) supplier of the active profile. This supplier will notify observers of
     * changes to the active profile, e.g. when selecting an incognito tab model.
     *
     * @param profileSupplier The supplier of the active profile.
     */
    void setProfileSupplier(ObservableSupplier<Profile> profileSupplier);
}
