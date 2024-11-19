// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dependency_injection;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.ACTIVITY_CONTEXT;
import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.ACTIVITY_TYPE;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;

import androidx.appcompat.app.AppCompatActivity;

import dagger.Module;
import dagger.Provides;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tabmodel.TabModelInitializer;

import javax.inject.Named;

/** Module for common dependencies in {@link ChromeActivity}. */
@Module
public class ChromeActivityCommonsModule {
    private final ChromeActivity mActivity;
    private final ObservableSupplier<Integer> mAutofillUiBottomInsetSupplier;
    private final Supplier<ShareDelegate> mShareDelegateSupplier;
    private final TabModelInitializer mTabModelInitializer;
    private final @ActivityType int mActivityType;

    public ChromeActivityCommonsModule(
            ChromeActivity activity,
            ObservableSupplier<Integer> autofillUiBottomInsetSupplier,
            Supplier<ShareDelegate> shareDelegateSupplier,
            TabModelInitializer tabModelInitializer,
            @ActivityType int activityType) {
        mActivity = activity;
        mAutofillUiBottomInsetSupplier = autofillUiBottomInsetSupplier;
        mShareDelegateSupplier = shareDelegateSupplier;
        mTabModelInitializer = tabModelInitializer;
        mActivityType = activityType;
    }

    @Provides
    @Named(ACTIVITY_CONTEXT)
    public Context provideContext() {
        return mActivity;
    }

    @Provides
    public Activity provideActivity() {
        return mActivity;
    }

    @Provides
    public ChromeActivity provideChromeActivity() {
        return mActivity;
    }

    @Provides
    public AppCompatActivity provideAppCompatActivity() {
        return mActivity;
    }

    @Provides
    public Resources provideResources() {
        return mActivity.getResources();
    }

    @Provides
    public ObservableSupplier<Integer> provideAutofillUiBottomInsetSupplier() {
        return mAutofillUiBottomInsetSupplier;
    }

    @Provides
    public Supplier<ShareDelegate> provideShareDelegateSupplier() {
        return mShareDelegateSupplier;
    }

    @Provides
    public TabModelInitializer provideTabModelInitializer() {
        return mTabModelInitializer;
    }

    @Provides
    @Named(ACTIVITY_TYPE)
    public @ActivityType int provideActivityType() {
        return mActivityType;
    }
}
