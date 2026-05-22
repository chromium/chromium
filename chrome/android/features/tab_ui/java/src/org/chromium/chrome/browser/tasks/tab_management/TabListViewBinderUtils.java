// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.components.browser_ui.util.TextResolver;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.components.browser_ui.util.motion.OnPeripheralClickListener;
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
     * Asynchronously fetches and binds the favicon drawable to the specified ImageView, using a
     * tag-validation system to protect against recycler view recycling race conditions and
     * performing drawable mutation for color isolation safety.
     *
     * @param model the model containing the tab properties.
     * @param faviconView the ImageView to receive the favicon drawable.
     */
    public static void updateFavicon(PropertyModel model, ImageView faviconView) {
        @Nullable TabFaviconFetcher fetcher = model.get(TabProperties.FAVICON_FETCHER);
        faviconView.setTag(fetcher);

        if (fetcher == null) {
            faviconView.setVisibility(View.GONE);
            faviconView.setImageDrawable(null);
            return;
        }

        faviconView.setVisibility(View.VISIBLE);
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
}
