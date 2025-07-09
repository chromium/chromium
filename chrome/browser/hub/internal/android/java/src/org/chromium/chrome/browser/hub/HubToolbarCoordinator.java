// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubColorMixer.COLOR_MIXER;

import android.app.Activity;
import android.view.View;
import android.widget.ImageButton;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButton;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Sets up the component that handles the toolbar of the Hub. */
@NullMarked
public class HubToolbarCoordinator {
    private final Callback<Boolean> mIsAnimatingObserver = this::tryToTriggerAddToGroupIph;
    private final HubToolbarMediator mMediator;
    private final HubToolbarView mHubToolbarView;
    private final MenuButtonCoordinator mMenuButtonCoordinator;
    private final MenuButton mMenuButton;
    private final UserEducationHelper mUserEducationHelper;
    private final ObservableSupplier<Boolean> mIsAnimatingSupplier;

    /**
     * Eagerly creates the component, but will not be rooted in the view tree yet.
     *
     * @param hubToolbarView The root view of this component. Inserted into hierarchy for us.
     * @param paneManager Interact with the current and all {@link Pane}s.
     * @param menuButtonCoordinator Root component for the app menu.
     * @param tracker Used to record user engagement events.
     * @param searchActivityClient A client for the search activity, used to launch search.
     * @param hubColorMixer Mixes the Hub Overview Color.
     * @param userEducationHelper Used to show IPHs.
     * @param isHubAnimatingSupplier Supplies whether a hub layout animation is running.
     */
    public HubToolbarCoordinator(
            Activity activity,
            HubToolbarView hubToolbarView,
            PaneManager paneManager,
            MenuButtonCoordinator menuButtonCoordinator,
            Tracker tracker,
            SearchActivityClient searchActivityClient,
            HubColorMixer hubColorMixer,
            UserEducationHelper userEducationHelper,
            ObservableSupplier<Boolean> isHubAnimatingSupplier) {
        mUserEducationHelper = userEducationHelper;
        mMenuButtonCoordinator = menuButtonCoordinator;
        mIsAnimatingSupplier = isHubAnimatingSupplier;
        PropertyModel model =
                new PropertyModel.Builder(HubToolbarProperties.ALL_KEYS)
                        .with(COLOR_MIXER, hubColorMixer)
                        .build();
        PropertyModelChangeProcessor.create(model, hubToolbarView, HubToolbarViewBinder::bind);
        mMediator =
                new HubToolbarMediator(activity, model, paneManager, tracker, searchActivityClient);
        mHubToolbarView = hubToolbarView;

        mMenuButton = hubToolbarView.findViewById(R.id.menu_button_wrapper);
        ImageButton imageButton = mMenuButton.getImageButton();
        imageButton.setContentDescription(
                activity.getString(R.string.accessibility_tab_switcher_toolbar_btn_menu));
        menuButtonCoordinator.setMenuButton(mMenuButton);

        if (ChromeFeatureList.sTabGroupEntryPointsAndroid.isEnabled()) {
            mIsAnimatingSupplier.addSyncObserver(mIsAnimatingObserver);
        }
    }

    private void tryToTriggerAddToGroupIph(boolean isAnimating) {
        if (isAnimating) {
            return;
        }
        mUserEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                mMenuButton.getResources(),
                                FeatureConstants.TAB_SWITCHER_ADD_TO_GROUP,
                                R.string.tab_switcher_add_to_group_iph,
                                R.string.tab_switcher_add_to_group_iph)
                        .setAnchorView(mMenuButton)
                        .setHighlightParams(new HighlightParams(HighlightShape.CIRCLE))
                        .setOnShowCallback(
                                () ->
                                        mMenuButtonCoordinator.highlightMenuItemOnShow(
                                                R.id.new_tab_group_menu_id))
                        .build());
        mIsAnimatingSupplier.removeObserver(mIsAnimatingObserver);
    }

    /** Returns the button view for a given pane if present. */
    public @Nullable View getPaneButton(@PaneId int paneId) {
        return mMediator.getButton(paneId);
    }

    /** Cleans up observers and resources. */
    public void destroy() {
        mMediator.destroy();
        mIsAnimatingSupplier.removeObserver(mIsAnimatingObserver);
    }

    public boolean isSearchBoxVisible() {
        return mHubToolbarView.findViewById(R.id.search_box).getVisibility() == View.VISIBLE;
    }
}
