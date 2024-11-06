// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dependency_injection;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.APP_CONTEXT;

import android.content.Context;

import dagger.Module;
import dagger.Provides;

import org.chromium.base.ContextUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.night_mode.SystemNightModeMonitor;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;

import javax.inject.Named;

/** Module for {@link ChromeAppComponent}. */
@Module
public class ChromeAppModule {
    /** See {@link ModuleFactoryOverrides} */
    public interface Factory {
        ChromeAppModule create();
    }

    @Provides
    public SharedPreferencesManager providesChromeSharedPreferences() {
        return ChromeSharedPreferences.getInstance();
    }

    @Provides
    @Named(APP_CONTEXT)
    public Context provideContext() {
        return ContextUtils.getApplicationContext();
    }

    @Provides
    public ChromeBrowserInitializer provideChromeBrowserInitializer() {
        return ChromeBrowserInitializer.getInstance();
    }

    @Provides
    public WarmupManager provideWarmupManager() {
        return WarmupManager.getInstance();
    }

    @Provides
    public SystemNightModeMonitor provideSystemNightModeMonitor() {
        return SystemNightModeMonitor.getInstance();
    }

    @Provides
    public AsyncTabParamsManager provideAsyncTabParamsManager() {
        return AsyncTabParamsManagerSingleton.getInstance();
    }
}
