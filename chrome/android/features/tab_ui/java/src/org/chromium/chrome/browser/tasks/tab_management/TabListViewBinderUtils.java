// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.UiTabState;
import org.chromium.chrome.browser.actor.ui.TabIndicatorStatus;
import org.chromium.chrome.browser.tab.MediaState;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.util.TextResolver;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.components.browser_ui.util.motion.OnPeripheralClickListener;
import org.chromium.components.tabs.TabAlert;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Shared utility class containing common static methods for binding and attaching interaction
 * listeners (clicks, long clicks, action buttons) to list items (grid cards, vertical rows).
 */
@NullMarked
public class TabListViewBinderUtils {

    private TabListViewBinderUtils() {}

    /**
     * Binds the action button click listeners using the metadata provided in TabActionButtonData.
     *
     * @param model the model containing the tab properties.
     * @param actionButton the action button View.
     * @param data the action button metadata containing the click listener callback.
     */
    public static void bindActionButton(
            PropertyModel model, View actionButton, @Nullable TabActionButtonData data) {
        @Nullable TabActionListener tabActionListener =
                data == null ? null : data.tabActionListener;
        setNullableClickListener(tabActionListener, actionButton, model);
        setNullablePeripheralClickListener(tabActionListener, actionButton, model);
    }

    /**
     * Attaches a standard click listener to the specified view, wrapping a nullable {@link
     * TabActionListener}.
     *
     * @param listener the listener callback to run when clicked. If null, removes the listener.
     * @param view the View to receive standard clicks.
     * @param propertyModel contains the data model to identify the clicked tab.
     */
    public static void setNullableClickListener(
            @Nullable TabActionListener listener, View view, PropertyModel propertyModel) {
        if (listener == null) {
            view.setOnClickListener(null);
        } else {
            view.setOnClickListener(
                    v ->
                            runTabActionListener(
                                    listener, v, propertyModel, /* triggeringMotion= */ null));
        }
    }

    /**
     * Sets an {@link OnPeripheralClickListener} on the given view to intercept clicks from
     * peripherals.
     *
     * @param tabActionListener the {@link TabActionListener} to run when a click is detected.
     * @param view the View to receive clicks.
     * @param propertyModel contains data to determine how to run the {@link TabActionListener}.
     */
    static void setNullablePeripheralClickListener(
            @Nullable TabActionListener tabActionListener, View view, PropertyModel propertyModel) {
        if (tabActionListener == null) {
            view.setOnTouchListener(null);
            return;
        }

        view.setOnTouchListener(
                new OnPeripheralClickListener(
                        view,
                        triggeringMotion ->
                                runTabActionListener(
                                        tabActionListener, view, propertyModel, triggeringMotion)));
    }

    /**
     * Attaches a long-press click listener to the specified view, wrapping a nullable {@link
     * TabActionListener}.
     *
     * @param listener the listener callback to run when long-pressed. If null, removes the
     *     listener.
     * @param view the View to receive long clicks.
     * @param propertyModel contains the data model to identify the long-clicked tab.
     */
    public static void setNullableLongClickListener(
            @Nullable TabActionListener listener, View view, PropertyModel propertyModel) {
        if (listener == null) {
            view.setOnLongClickListener(null);
        } else {
            view.setOnLongClickListener(
                    v -> {
                        runTabActionListener(
                                listener, v, propertyModel, /* triggeringMotion= */ null);
                        return true;
                    });
        }
    }

    /**
     * Attaches a context click (right-click or mouse context) listener to the specified view.
     *
     * @param listener the listener callback to run when context-clicked. If null, removes context
     *     click.
     * @param view the View to receive context clicks.
     * @param propertyModel contains the data model to identify the context-clicked tab.
     */
    public static void setNullableContextClickListener(
            @Nullable TabActionListener listener, View view, PropertyModel propertyModel) {
        if (listener == null) {
            view.setContextClickable(false);
            view.setOnContextClickListener(null);
        } else {
            view.setContextClickable(true);
            view.setOnContextClickListener(
                    v -> {
                        runTabActionListener(
                                listener, v, propertyModel, /* triggeringMotion= */ null);
                        return true;
                    });
        }
    }

    /**
     * Updates the favicon drawable of the specified {@link ImageView} and manages its visibility.
     *
     * <p>This method delegates the asynchronous fetching and binding of the image to {@link
     * #updateFaviconImage(PropertyModel, ImageView)}. It then adjusts the visibility of the {@code
     * faviconView} to {@link View#GONE} if no favicon fetcher is available, or {@link View#VISIBLE}
     * otherwise.
     *
     * @param model The model containing tab properties, specifically {@link
     *     TabProperties#FAVICON_FETCHER}.
     * @param faviconView The {@link ImageView} whose favicon image and visibility will be updated.
     */
    public static void updateFaviconAndVisibility(PropertyModel model, ImageView faviconView) {
        updateFaviconImage(model, faviconView);
        faviconView.setVisibility(
                model.get(TabProperties.FAVICON_FETCHER) == null ? View.GONE : View.VISIBLE);
    }

    /**
     * Asynchronously fetches and applies the favicon drawable to the specified {@link ImageView}.
     *
     * <p>This method handles the lifecycle of resolving the favicon image, choosing between the
     * selected and default drawables, mutating the drawable for color isolation, and safeguarding
     * against recycler view recycling race conditions by validating the view's tag. It does not
     * alter the visibility of the view.
     *
     * @param model The model containing tab properties, specifically {@link
     *     TabProperties#FAVICON_FETCHER} and {@link TabProperties#IS_SELECTED}.
     * @param faviconView The {@link ImageView} to update with the fetched favicon drawable.
     */
    public static void updateFaviconImage(PropertyModel model, ImageView faviconView) {
        @Nullable TabFaviconFetcher fetcher = model.get(TabProperties.FAVICON_FETCHER);
        faviconView.setTag(fetcher);

        if (fetcher == null) {
            faviconView.setImageDrawable(null);
            return;
        }

        fetcher.fetch(
                tabFavicon -> {
                    if (faviconView.getTag() != fetcher) return;
                    if (tabFavicon == null) {
                        faviconView.setImageDrawable(null);
                        return;
                    }
                    boolean isSelected = model.get(TabProperties.IS_SELECTED);
                    Drawable drawable =
                            isSelected
                                    ? tabFavicon.getSelectedDrawable()
                                    : tabFavicon.getDefaultDrawable();
                    if (drawable == null) {
                        drawable = tabFavicon.getDefaultDrawable();
                    }
                    if (drawable != null) {
                        drawable = drawable.mutate();
                    }
                    faviconView.setImageDrawable(drawable);
                });
    }

    /**
     * Resolves the accessibility text resolver and sets the content description on the view.
     *
     * @param model the model containing the tab properties.
     * @param view the View to receive the accessibility content description.
     */
    public static void updateContentDescription(PropertyModel model, View view) {
        @Nullable TextResolver contentDescriptionTextResolver =
                model.get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER);
        @Nullable CharSequence contentDescriptionString =
                TabCardViewBinderUtils.resolveNullSafe(
                        contentDescriptionTextResolver, view.getContext());
        view.setContentDescription(contentDescriptionString);
    }

    /**
     * Updates the content description of the action button in the view using the resolver from
     * model.
     *
     * @param model the model containing the tab properties.
     * @param actionButton the action button View to update.
     */
    public static void updateActionButtonContentDescription(
            PropertyModel model, View actionButton) {
        @Nullable TextResolver resolver =
                model.get(TabProperties.ACTION_BUTTON_DESCRIPTION_TEXT_RESOLVER);
        actionButton.setContentDescription(
                TabCardViewBinderUtils.resolveNullSafe(resolver, actionButton.getContext()));
    }

    private static void runTabActionListener(
            TabActionListener tabActionListener,
            View view,
            PropertyModel propertyModel,
            @Nullable MotionEventInfo triggeringMotion) {
        if (propertyModel.containsKey(TabProperties.TAB_GROUP_SYNC_ID)) {
            tabActionListener.run(
                    view, propertyModel.get(TabProperties.TAB_GROUP_SYNC_ID), triggeringMotion);
        } else {
            tabActionListener.run(view, propertyModel.get(TabProperties.TAB_ID), triggeringMotion);
        }
    }

    /** Returns whether an actor indicator is active for the given UI state. */
    public static boolean isActorActive(@Nullable UiTabState state) {
        return state != null && state.tabIndicator != TabIndicatorStatus.NONE;
    }

    /**
     * Resolves the ACTOR_UI_STATE and updates the accessibility content description.
     *
     * @param model the model containing the tab properties.
     * @param view the View to receive the accessibility content description.
     * @return true if the actor indicator should be visible, false otherwise.
     */
    public static boolean setupActorIndicator(PropertyModel model, View view) {
        @Nullable UiTabState state = model.get(TabProperties.ACTOR_UI_STATE);
        boolean shouldBeVisible = isActorActive(state);
        updateIndicatorAccessibilityDescription(model, view);
        return shouldBeVisible;
    }

    private static void updateIndicatorAccessibilityDescription(PropertyModel model, View view) {
        @Nullable UiTabState actorState = model.get(TabProperties.ACTOR_UI_STATE);
        boolean isActorActive = isActorActive(actorState);

        if (isActorActive) {
            String title = model.get(TabProperties.TITLE);
            String accessibilityDesc =
                    view.getResources().getString(R.string.tab_ax_label_actor_accessing, title);
            view.setContentDescription(accessibilityDesc);
        } else {
            if (model.containsKey(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                    && model.get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER) != null) {
                updateContentDescription(model, view);
            } else if (model.containsKey(TabProperties.TITLE)) {
                view.setContentDescription(model.get(TabProperties.TITLE));
            }
        }
    }

    /**
     * Resolves the string template resource for the tab's accessibility description based on pinned
     * state and alert state.
     *
     * @param isPinned Whether the tab is pinned.
     * @param alertState The {@link TabAlert} state of the tab.
     * @return The string resource ID for the accessibility description.
     */
    // TODO(crbug.com/456216687): Refactor a11y labels.
    public static @StringRes int getTabContentDescriptionStringId(
            boolean isPinned, @TabAlert int alertState) {
        return switch (alertState) {
            case TabAlert.AUDIO_MUTING ->
                    isPinned
                            ? R.string.accessibility_tabstrip_tab_pinned_muted
                            : R.string.accessibility_tabstrip_tab_muted;
            case TabAlert.AUDIO_PLAYING ->
                    isPinned
                            ? R.string.accessibility_tabstrip_tab_pinned_audible
                            : R.string.accessibility_tabstrip_tab_audible;
            case TabAlert.AUDIO_RECORDING, TabAlert.MEDIA_RECORDING, TabAlert.VIDEO_RECORDING ->
                    isPinned
                            ? R.string.accessibility_tabstrip_tab_pinned_recording
                            : R.string.accessibility_tabstrip_tab_recording;
            case TabAlert.TAB_CAPTURING, TabAlert.DESKTOP_CAPTURING ->
                    isPinned
                            ? R.string.accessibility_tabstrip_tab_pinned_sharing
                            : R.string.accessibility_tabstrip_tab_sharing;
            case TabAlert.PIP_PLAYING ->
                    isPinned
                            ? R.string.accessibility_tabstrip_tab_pinned_picture_in_picture
                            : R.string.accessibility_tabstrip_tab_picture_in_picture;
            default ->
                    isPinned
                            ? R.string.accessibility_tabstrip_tab_pinned
                            : R.string.accessibility_tabstrip_tab;
        };
    }

    /**
     * Resolves the string template resource for the tab's accessibility description based on pinned
     * state and media indicator.
     *
     * @deprecated Use {@link #getTabContentDescriptionStringId(boolean, int)} instead.
     */
    @Deprecated
    public static @StringRes int getMediaIndicatorTabContentDescriptionStringId(
            boolean isPinned, @MediaState int mediaState) {
        switch (mediaState) {
            case MediaState.MUTED:
                // e.g. "Google, Pinned Muted Tab" or "Google, Muted Tab".
                return isPinned
                        ? R.string.accessibility_tabstrip_tab_pinned_muted
                        : R.string.accessibility_tabstrip_tab_muted;
            case MediaState.AUDIBLE:
                // e.g. "Google, Pinned Audible Tab" or "Google, Audible Tab".
                return isPinned
                        ? R.string.accessibility_tabstrip_tab_pinned_audible
                        : R.string.accessibility_tabstrip_tab_audible;
            case MediaState.RECORDING:
                // e.g. "Google, Pinned Recording Tab" or "Google, Recording Tab".
                return isPinned
                        ? R.string.accessibility_tabstrip_tab_pinned_recording
                        : R.string.accessibility_tabstrip_tab_recording;
            case MediaState.SHARING:
                // e.g. "Google, Pinned Sharing Tab" or "Google, Sharing Tab".
                return isPinned
                        ? R.string.accessibility_tabstrip_tab_pinned_sharing
                        : R.string.accessibility_tabstrip_tab_sharing;
            case MediaState.PICTURE_IN_PICTURE:
                // e.g. "Google, Pinned Picture-in-Picture Tab" or "Google, Picture-in-Picture Tab".
                return isPinned
                        ? R.string.accessibility_tabstrip_tab_pinned_picture_in_picture
                        : R.string.accessibility_tabstrip_tab_picture_in_picture;
            case MediaState.NONE:
            default:
                // e.g. "Google, Pinned Tab" or "Google, Tab".
                return isPinned
                        ? R.string.accessibility_tabstrip_tab_pinned
                        : R.string.accessibility_tabstrip_tab;
        }
    }
}
