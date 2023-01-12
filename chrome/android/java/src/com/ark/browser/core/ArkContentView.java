package com.ark.browser.core;

import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.EventOffsetHandler;

/**
 * The containing view for {@link WebContents} that exists in the Android UI hierarchy and exposes
 * the various {@link View} functionality to it.
 *
 * While ContentView is a ViewGroup, the only place that should add children is ViewAndroidDelegate,
 * and only for cases that WebContentsAccessibility handles (such as anchoring popups). This is
 * because the accessibility support provided by WebContentsAccessibility ignores all child views.
 * In other words, any children added to this are *not* accessible.
 */
public class ArkContentView extends ContentView {


    /**
     * Creates an instance of a ContentView.
     *
     * @param context            The Context the view is running in, through which it can
     *                           access the current theme, resources, etc.
     * @param eventOffsetHandler
     */
    protected ArkContentView(Context context, EventOffsetHandler eventOffsetHandler) {
        super(context, eventOffsetHandler, null);
    }

    /**
     * Constructs a new ContentView for the appropriate Android version.
     * @param context The Context the view is running in, through which it can
     *                access the current theme, resources, etc.
     * @return an instance of a ContentView.
     */
    public static ArkContentView createContentView(Context context,
                                                   @Nullable EventOffsetHandler eventOffsetHandler) {
        return new ArkContentView(context, eventOffsetHandler);
    }

}

