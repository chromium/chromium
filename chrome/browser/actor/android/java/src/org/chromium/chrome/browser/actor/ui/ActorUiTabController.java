// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.base.UserData;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;

/** Java-side representation of the C++ ActorUiTabControllerAndroid. */
@JNINamespace("actor::ui")
@NullMarked
public class ActorUiTabController implements UserData {
    private static final Class<ActorUiTabController> USER_DATA_KEY = ActorUiTabController.class;

    /** Represents visual state of the tab's actor components. */
    public static class UiTabState {
        public final ActorOverlayState actorOverlay;
        public final HandoffButtonState handoffButton;
        public final @TabIndicatorStatus int tabIndicator;
        public final boolean borderGlowVisible;

        /**
         * Constructor for UiTabState.
         *
         * @param actorOverlay The overlay configuration.
         * @param handoffButton The handoff button configuration.
         * @param tabIndicator The current {@link TabIndicatorStatus}.
         * @param borderGlowVisible Whether the glow effect is active.
         */
        UiTabState(
                ActorOverlayState actorOverlay,
                HandoffButtonState handoffButton,
                @TabIndicatorStatus int tabIndicator,
                boolean borderGlowVisible) {
            this.actorOverlay = actorOverlay;
            this.handoffButton = handoffButton;
            this.tabIndicator = tabIndicator;
            this.borderGlowVisible = borderGlowVisible;
        }
    }

    /** Represents a tab's actor overlay state. */
    public static class ActorOverlayState {
        public final boolean isActive;
        public final boolean borderGlowVisible;
        public final boolean mouseDown;

        /**
         * Constructor for ActorOverlayState.
         *
         * @param isActive True if the overlay should be shown.
         * @param borderGlowVisible True if the boundary glow is active.
         * @param mouseDown True if a click is currently being simulated.
         */
        ActorOverlayState(boolean isActive, boolean borderGlowVisible, boolean mouseDown) {
            this.isActive = isActive;
            this.borderGlowVisible = borderGlowVisible;
            this.mouseDown = mouseDown;
        }
    }

    /** Represents a Tab-scoped state for control ownership. */
    public static class HandoffButtonState {
        /**
         * Whether or not the component is active. This member is intended to be used alongside the
         * relevant tab's visibility status to determine whether or not the handoff button should be
         * shown.
         */
        public final boolean isActive;

        /** The current controller of the tab. Values are defined in {@link ControlOwnership}. */
        public final @ControlOwnership int controller;

        /**
         * Constructor for HandoffButtonState.
         *
         * @param isActive True if the handoff button should be visible.
         * @param controller The current {@link ControlOwnership} value.
         */
        HandoffButtonState(boolean isActive, @ControlOwnership int controller) {
            this.isActive = isActive;
            this.controller = controller;
        }
    }

    /** Observer interface for tracking changes to the {@link UiTabState} of a tab. */
    @FunctionalInterface
    public interface Observer {
        void onUiTabStateChanged(UiTabState state);
    }

    private final Tab mTab;
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private @Nullable UiTabState mCurrentState;

    /** Returns the controller for a given tab, creating it if necessary. */
    public static ActorUiTabController from(Tab tab) {
        ActorUiTabController controller = tab.getUserDataHost().getUserData(USER_DATA_KEY);
        if (controller == null) {
            controller = new ActorUiTabController(tab);
        }
        return controller;
    }

    private ActorUiTabController(Tab tab) {
        mTab = tab;
        mTab.getUserDataHost().setUserData(USER_DATA_KEY, this);
    }

    /** Pauses the ActorTask associated with this tab. */
    public void setActorTaskPaused() {
        ActorUiTabControllerJni.get().setActorTaskPaused(mTab);
    }

    /** Resumes the ActorTask associated with this tab. */
    public void setActorTaskResume() {
        ActorUiTabControllerJni.get().setActorTaskResume(mTab);
    }

    /** Allows Java UI components to listen for tab state changes. */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /** Removes an observer. */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /** Returns the most recent UI state snapshot for this tab. */
    public @Nullable UiTabState getTabUiState() {
        return mCurrentState;
    }

    @CalledByNative
    private static boolean onUiTabStateChange(
            Tab tab,
            boolean overlayActive,
            boolean overlayGlow,
            boolean overlayClick,
            boolean handoffActive,
            @ControlOwnership int handoffOwner,
            @TabIndicatorStatus int indicator,
            boolean borderGlow) {
        if (tab == null || tab.isDestroyed()) {
            return false;
        }

        from(tab)
                .onUiTabStateChange(
                        new UiTabState(
                                new ActorOverlayState(overlayActive, overlayGlow, overlayClick),
                                new HandoffButtonState(handoffActive, handoffOwner),
                                indicator,
                                borderGlow));
        return true;
    }

    /** Instance method to update state and notify observers. */
    private void onUiTabStateChange(UiTabState state) {
        mCurrentState = state;
        for (Observer observer : mObservers) {
            observer.onUiTabStateChanged(state);
        }
    }

    @Override
    public void destroy() {
        mTab.getUserDataHost().removeUserData(USER_DATA_KEY);
        mObservers.clear();
    }

    @NativeMethods
    interface Natives {
        void setActorTaskPaused(Tab tab);

        void setActorTaskResume(Tab tab);
    }
}
