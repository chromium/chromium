// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarPrefs;
import org.chromium.chrome.browser.ui.bottombar.BottomBarConfigUtils;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;

/**
 * Java side of the JNI bridge between GlicKeyedServiceImpl in Java and C++. All method calls are
 * delegated to the native C++ class.
 */
@JNINamespace("glic")
@NullMarked
public class GlicKeyedServiceImpl implements GlicKeyedService {
    private long mNativePtr;
    private final ObserverList<GlobalShowHideObserver> mObservers = new ObserverList<>();
    private final ObserverList<UserEnabledActuationOnWebObserver>
            mUserEnabledActuationOnWebObservers = new ObserverList<>();
    private final ObserverList<AllowedChangedObserver> mAllowedChangedObservers =
            new ObserverList<>();

    @CalledByNative
    private static GlicKeyedServiceImpl create(long nativePtr) {
        return new GlicKeyedServiceImpl(nativePtr);
    }

    private GlicKeyedServiceImpl(long nativePtr) {
        mNativePtr = nativePtr;
    }

    @Override
    public void toggleUI(
            long browserWindowPtr,
            boolean preventClose,
            Profile profile,
            @GlicInvocationSource int invocationSource) {
        if (mNativePtr == 0) return;

        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.notifyEvent(EventConstants.GLIC_ANDROID_USED);

        GlicKeyedServiceImplJni.get()
                .toggleUI(mNativePtr, browserWindowPtr, preventClose, profile, invocationSource);
    }

    @Override
    public boolean invokeWithAutoSubmit(
            Tab tab, String text, @GlicInvocationSource int invocationSource) {
        if (mNativePtr == 0) return false;

        return GlicKeyedServiceImplJni.get()
                .invokeWithAutoSubmit(mNativePtr, tab, text, invocationSource);
    }

    @Override
    public boolean isPanelShowingForBrowser(long browserWindowPtr) {
        if (mNativePtr == 0) return false;
        return GlicKeyedServiceImplJni.get().isPanelShowingForBrowser(mNativePtr, browserWindowPtr);
    }

    @Override
    public boolean getUserEnabledActuationOnWeb() {
        if (mNativePtr == 0) return false;
        return GlicKeyedServiceImplJni.get().getUserEnabledActuationOnWeb(mNativePtr);
    }

    @Override
    public void setUserEnabledActuationOnWeb(boolean enabled) {
        if (mNativePtr == 0) return;
        GlicKeyedServiceImplJni.get().setUserEnabledActuationOnWeb(mNativePtr, enabled);
    }

    @Override
    @CalledByNative
    public boolean isGlicShortcutActive(Profile profile) {
        if (BottomBarConfigUtils.isBottomBarEnabled(ContextUtils.getApplicationContext())) {
            return false;
        }
        int setting = AdaptiveToolbarPrefs.getCustomizationSetting();
        if (setting == AdaptiveToolbarButtonVariant.GLIC) {
            return true;
        }
        if (setting == AdaptiveToolbarButtonVariant.AUTO) {
            return AdaptiveToolbarFeatures.getDefaultButtonVariant(
                            ContextUtils.getApplicationContext(), profile)
                    == AdaptiveToolbarButtonVariant.GLIC;
        }
        return false;
    }

    @Override
    @CalledByNative
    public boolean isBottomBarEnabled() {
        return BottomBarConfigUtils.isBottomBarEnabled(ContextUtils.getApplicationContext());
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativePtr = 0;
    }

    @Override
    public void addGlobalShowHideObserver(GlobalShowHideObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeGlobalShowHideObserver(GlobalShowHideObserver observer) {
        mObservers.removeObserver(observer);
    }

    @CalledByNative
    private void onGlobalShowHide() {
        for (GlobalShowHideObserver observer : mObservers) {
            observer.onGlobalShowHide();
        }
    }

    @Override
    public void addUserEnabledActuationOnWebObserver(UserEnabledActuationOnWebObserver observer) {
        mUserEnabledActuationOnWebObservers.addObserver(observer);
    }

    @Override
    public void removeUserEnabledActuationOnWebObserver(
            UserEnabledActuationOnWebObserver observer) {
        mUserEnabledActuationOnWebObservers.removeObserver(observer);
    }

    @Override
    public void addAllowedChangedObserver(AllowedChangedObserver observer) {
        mAllowedChangedObservers.addObserver(observer);
    }

    @Override
    public void removeAllowedChangedObserver(AllowedChangedObserver observer) {
        mAllowedChangedObservers.removeObserver(observer);
    }

    @CalledByNative
    private void onUserEnabledActuationOnWebChanged(boolean enabled) {
        for (UserEnabledActuationOnWebObserver observer : mUserEnabledActuationOnWebObservers) {
            observer.onUserEnabledActuationOnWebChanged(enabled);
        }
    }

    @CalledByNative
    private void onAllowedStateChanged() {
        for (AllowedChangedObserver observer : mAllowedChangedObservers) {
            observer.onAllowedStateChanged();
        }
    }

    @NativeMethods
    interface Natives {
        void toggleUI(
                long nativeGlicKeyedServiceAndroid,
                long browserWindowPtr,
                boolean preventClose,
                @JniType("Profile*") Profile profile,
                @GlicInvocationSource int source);

        boolean invokeWithAutoSubmit(
                long nativeGlicKeyedServiceAndroid,
                @JniType("TabAndroid*") Tab tab,
                @JniType("std::string") String text,
                @GlicInvocationSource int source);

        boolean isPanelShowingForBrowser(long nativeGlicKeyedServiceAndroid, long browserWindowPtr);

        boolean getUserEnabledActuationOnWeb(long nativeGlicKeyedServiceAndroid);

        void setUserEnabledActuationOnWeb(long nativeGlicKeyedServiceAndroid, boolean enabled);
    }
}
