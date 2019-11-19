// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.annotation.TargetApi;
import android.os.Build;
import android.view.View;
import android.view.WindowInsets;

import org.chromium.base.BuildInfo;
import org.chromium.base.Supplier;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * Factory class for {@link HistoryNavigationDelegate}.
 */
public class HistoryNavigationDelegateFactory {
    /**
     * Default {@link HistoryNavigationDelegate} that does not support navigation. Mainly
     * used for SnackbarActivity-based UI on the phone to disable the feature.
     */
    public static final HistoryNavigationDelegate DEFAULT = new HistoryNavigationDelegate() {
        @Override
        public NavigationHandler.ActionDelegate createActionDelegate() {
            return null;
        }

        @Override
        public NavigationSheet.Delegate createSheetDelegate() {
            return null;
        }

        @Override
        public Supplier<BottomSheetController> getBottomSheetController() {
            assert false : "Should never be called";
            return null;
        }

        @Override
        public boolean isNavigationEnabled(View view) {
            return false;
        }

        @Override
        public void setWindowInsetsChangeObserver(View view, Runnable runnable) {}
    };

    private HistoryNavigationDelegateFactory() {}

    /**
     * Creates {@link HistoryNavigationDelegate} for native/rendered pages on Tab.
     * TODO(jinsukkim): Remove the early returns when q is available for upstream.
     */
    public static HistoryNavigationDelegate create(Tab tab) {
        if (!isFeatureFlagEnabled() || tab.getActivity() == null) return DEFAULT;

        return new HistoryNavigationDelegate() {
            // TODO(jinsukkim): Avoid getting activity from tab.
            private final Supplier<BottomSheetController> mController = () -> {
                ChromeActivity activity = tab.getActivity();
                return activity == null || activity.isActivityFinishingOrDestroyed()
                        ? null
                        : activity.getBottomSheetController();
            };
            private Runnable mInsetsChangeRunnable;

            @Override
            public NavigationHandler.ActionDelegate createActionDelegate() {
                return new TabbedActionDelegate(tab);
            }

            @Override
            public NavigationSheet.Delegate createSheetDelegate() {
                return new TabbedSheetDelegate(tab);
            }

            @Override
            public Supplier<BottomSheetController> getBottomSheetController() {
                return mController;
            }

            @TargetApi(Build.VERSION_CODES.O)
            @Override
            public boolean isNavigationEnabled(View view) {
                if (!BuildInfo.isAtLeastQ() || Build.VERSION.SDK_INT <= Build.VERSION_CODES.P) {
                    return true;
                }
                Object insets = getSystemGestureInsets(view.getRootWindowInsets());
                return insetsField(insets, "left") == 0 && insetsField(insets, "right") == 0;
            }

            /**
             * @return {@link android.graphics.Insets} object.
             */
            @TargetApi(Build.VERSION_CODES.O)
            private Object getSystemGestureInsets(Object windowInsets) {
                if (!BuildInfo.isAtLeastQ() || Build.VERSION.SDK_INT <= Build.VERSION_CODES.P) {
                    return null;
                }
                try {
                    Method method = WindowInsets.class.getMethod("getSystemGestureInsets");
                    return method.invoke(windowInsets);
                } catch (IllegalAccessException | InvocationTargetException
                        | NoSuchMethodException e) {
                    // silently fails.
                }
                return null;
            }

            private int insetsField(Object insets, String fieldName) {
                if (insets == null) return 0;
                try {
                    Class<?> Insets = Class.forName("android.graphics.Insets");
                    Field field = Insets.getDeclaredField(fieldName);
                    Object value = field.get(Insets.cast(insets));
                    return (int) value;
                } catch (ClassNotFoundException | NoSuchFieldException | IllegalAccessException e) {
                    // silently fails.
                }
                return 0;
            }

            @TargetApi(Build.VERSION_CODES.O)
            @Override
            public void setWindowInsetsChangeObserver(View view, Runnable runnable) {
                if (!BuildInfo.isAtLeastQ() || Build.VERSION.SDK_INT <= Build.VERSION_CODES.P) {
                    return;
                }
                mInsetsChangeRunnable = runnable;
                view.setOnApplyWindowInsetsListener(
                        runnable != null ? this::onApplyWindowInsets : null);
            }

            private WindowInsets onApplyWindowInsets(View view, WindowInsets insets) {
                assert BuildInfo.isAtLeastQ();
                mInsetsChangeRunnable.run();
                return insets;
            }
        };
    }

    private static boolean isFeatureFlagEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.OVERSCROLL_HISTORY_NAVIGATION);
    }
}
