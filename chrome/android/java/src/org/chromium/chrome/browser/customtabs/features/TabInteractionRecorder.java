// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features;

import android.os.SystemClock;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;

import java.util.Locale;

/**
 * Class used to monitor interactions for the current custom tab. This class is created in native
 * and owned by C++ object. This class has the ability to record whether the current web content has
 * seen interaction when the tab is closing, as well as the timestamp when this happens.
 *
 * Note that this object's lifecycle is bounded to a {@link WebContents} but not a {@link Tab}. To
 * observe the first frame of tab load, this recorder has to attach to the web content before the
 * first navigation for the visible frame finishes, or a pre-rendered frame become active.
 * */
@JNINamespace("customtabs")
public class TabInteractionRecorder {
    private static final String TAG = "CctInteraction";
    private static TabInteractionRecorder sInstanceForTesting;
    private final long mNativeTabInteractionRecorder;

    // Do not instantiate in Java.
    private TabInteractionRecorder(long nativePtr) {
        mNativeTabInteractionRecorder = nativePtr;
    }

    @VisibleForTesting
    TabInteractionRecorder() {
        this(1L);
    }

    @CalledByNative
    private static @Nullable TabInteractionRecorder create(long nativePtr) {
        if (nativePtr == 0) return null;
        return new TabInteractionRecorder(nativePtr);
    }

    /**
     * Get the TabInteractionRecorder that lives in the main web contents of the given tab.
     * Note that the object might be come stale if the web contents of the given tab is swapped
     * after this function is called.
     * */
    public static @Nullable TabInteractionRecorder getFromTab(Tab tab) {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return TabInteractionRecorderJni.get().getFromTab(tab);
    }

    /**
     * Create a TabInteractionRecorder and start observing the web contents in the given tab. If an
     * observer already exists for the tab, do nothing.
     */
    public static void createForTab(Tab tab) {
        TabInteractionRecorderJni.get().createForTab(tab);
    }

    /**
     * Notify this recorder tab is being closed. Record whether this instance has seen any
     * interaction, and the timestamp when the tab is closed, into SharedPreferences.
     *
     * This class works correctly assuming there will be only one tab opened throughout the lifetime
     * of a given CCT session. If CCT ever changed into serving multiple tabs, this recorder will
     * only works for the last tab being closed.
     */
    public void onTabClosing() {
        long timestamp = SystemClock.uptimeMillis();
        boolean hadInteraction = hadInteraction();
        boolean hadFormInteractionInSession = hadFormInteractionInSession();
        boolean hadFormInteractionInActivePage = hadFormInteractionInActivePage();
        boolean hadNavigationInteraction = hadNavigationInteraction();

        Log.d(
                TAG,
                String.format(
                        Locale.US,
                        "timestamp=%d, TabInteractionRecorder.recordInteractions=%b",
                        timestamp,
                        hadInteraction));

        SharedPreferencesManager pref = ChromeSharedPreferences.getInstance();
        pref.writeLong(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TIMESTAMP, timestamp);

        pref.writeBoolean(
                ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION, hadInteraction);
        RecordHistogram.recordBooleanHistogram(
                "CustomTabs.HadInteractionOnClose.Form", hadFormInteractionInSession);
        RecordHistogram.recordBooleanHistogram(
                "CustomTabs.HadInteractionOnClose.FormStillActive", hadFormInteractionInActivePage);
        RecordHistogram.recordBooleanHistogram(
                "CustomTabs.HadInteractionOnClose.Navigation", hadNavigationInteraction);
    }

    /**
     * Whether this instance has seen interactions in associated tab. Different than
     * {@link #didGetUserInteraction()}, this function returns whether user had interactions with
     * form entries, or had navigation entries by the time the method is called.
     *
     * More details see chrome/browser/android/customtabs/tab_interaction_recorder_android.h
     */
    public boolean hadInteraction() {
        return hadFormInteractionInSession() || hadNavigationInteraction();
    }

    private boolean hadFormInteractionInActivePage() {
        return TabInteractionRecorderJni.get()
                .hadFormInteractionInActivePage(mNativeTabInteractionRecorder);
    }

    private boolean hadFormInteractionInSession() {
        return TabInteractionRecorderJni.get()
                .hadFormInteractionInSession(mNativeTabInteractionRecorder);
    }

    private boolean hadNavigationInteraction() {
        return TabInteractionRecorderJni.get()
                .hadNavigationInteraction(mNativeTabInteractionRecorder);
    }

    /** Reset the interaction recorded. */
    public void reset() {
        TabInteractionRecorderJni.get().reset(mNativeTabInteractionRecorder);
    }

    /**
     * Whether there has been direct user interaction with the WebContents in the tab. For more
     * detail see content/public/browser/web_contents_observer.h
     *
     * @return Whether there has been direct user interaction.
     */
    public boolean didGetUserInteraction() {
        // TODO(crbug.com/40237418): Expose WebContentsObserver#didGetUserInteraction
        return TabInteractionRecorderJni.get().didGetUserInteraction(mNativeTabInteractionRecorder);
    }

    /** Remove all the shared preferences related to tab interactions. */
    public static void resetTabInteractionRecords() {
        SharedPreferencesManager pref = ChromeSharedPreferences.getInstance();
        pref.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TIMESTAMP);
        pref.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION);
    }

    public static void setInstanceForTesting(TabInteractionRecorder instance) {
        sInstanceForTesting = instance;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    @NativeMethods
    interface Natives {
        TabInteractionRecorder getFromTab(Tab tab);

        TabInteractionRecorder createForTab(Tab tab);

        boolean didGetUserInteraction(long nativeTabInteractionRecorderAndroid);

        boolean hadFormInteractionInActivePage(long nativeTabInteractionRecorderAndroid);

        boolean hadFormInteractionInSession(long nativeTabInteractionRecorderAndroid);

        boolean hadNavigationInteraction(long nativeTabInteractionRecorderAndroid);

        void reset(long nativeTabInteractionRecorderAndroid);
    }
}
