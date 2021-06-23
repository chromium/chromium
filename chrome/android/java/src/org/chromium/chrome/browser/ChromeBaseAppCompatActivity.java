// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.os.Bundle;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;
import androidx.annotation.StyleRes;
import androidx.appcompat.app.AppCompatActivity;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.base.SplitCompatUtils;
import org.chromium.chrome.browser.language.GlobalAppLocaleController;
import org.chromium.chrome.browser.night_mode.GlobalNightModeStateProviderHolder;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * A subclass of {@link AppCompatActivity} that maintains states and objects applied to all
 * activities in {@link ChromeApplication} (e.g. night mode).
 */
public class ChromeBaseAppCompatActivity
        extends AppCompatActivity implements NightModeStateProvider.Observer {
    private final ObservableSupplierImpl<ModalDialogManager> mModalDialogManagerSupplier =
            new ObservableSupplierImpl<>();
    private NightModeStateProvider mNightModeStateProvider;
    private @StyleRes int mThemeResId;

    @Override
    protected void attachBaseContext(Context newBase) {
        super.attachBaseContext(newBase);
        mNightModeStateProvider = createNightModeStateProvider();

        Configuration config = new Configuration();
        // Pre-Android O, fontScale gets initialized to 1 in the constructor. Set it to 0 so
        // that applyOverrideConfiguration() does not interpret it as an overridden value.
        // https://crbug.com/834191
        config.fontScale = 0;
        // NightMode and other applyOverrides must be done before onCreate in attachBaseContext.
        // https://crbug.com/1139760
        if (applyOverrides(newBase, config)) applyOverrideConfiguration(config);
    }

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        getSupportFragmentManager().setFragmentFactory(SplitCompatUtils.createFragmentFactory());
        mModalDialogManagerSupplier.set(createModalDialogManager());

        initializeNightModeStateProvider();
        mNightModeStateProvider.addObserver(this);
        setTheme(R.style.ColorOverlay);
        super.onCreate(savedInstanceState);

        // Activity level locale overrides must be done in onCreate.
        GlobalAppLocaleController.getInstance().maybeOverrideContextConfig(this);
    }

    @Override
    protected void onDestroy() {
        mNightModeStateProvider.removeObserver(this);
        if (mModalDialogManagerSupplier.get() != null) {
            mModalDialogManagerSupplier.get().destroy();
            mModalDialogManagerSupplier.set(null);
        }
        super.onDestroy();
    }

    @Override
    public void setTheme(@StyleRes int resid) {
        super.setTheme(resid);
        mThemeResId = resid;
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        NightModeUtils.updateConfigurationForNightMode(
                this, mNightModeStateProvider.isInNightMode(), newConfig, mThemeResId);
    }

    /**
     * Returns the supplier of {@link ModalDialogManager} that manages the display of modal dialogs.
     */
    public ObservableSupplier<ModalDialogManager> getModalDialogManagerSupplier() {
        return mModalDialogManagerSupplier;
    }

    /**
     * Creates a {@link ModalDialogManager} for this class. Subclasses that need one should override
     * this method.
     */
    @Nullable
    protected ModalDialogManager createModalDialogManager() {
        return null;
    }

    /**
     * Called during {@link #attachBaseContext(Context)} to allow configuration overrides to be
     * applied. If this methods return true, the overrides will be applied using
     * {@link #applyOverrideConfiguration(Configuration)}.
     * @param baseContext The base {@link Context} attached to this class.
     * @param overrideConfig The {@link Configuration} that will be passed to
     *                       @link #applyOverrideConfiguration(Configuration)} if necessary.
     * @return True if any configuration overrides were applied, and false otherwise.
     */
    @CallSuper
    protected boolean applyOverrides(Context baseContext, Configuration overrideConfig) {
        return NightModeUtils.applyOverridesForNightMode(
                getNightModeStateProvider(), overrideConfig);
    }

    /**
     * @return The {@link NightModeStateProvider} that provides the state of night mode.
     */
    protected final NightModeStateProvider getNightModeStateProvider() {
        return mNightModeStateProvider;
    }

    /**
     * @return The {@link NightModeStateProvider} that provides the state of night mode in the scope
     *         of this class.
     */
    protected NightModeStateProvider createNightModeStateProvider() {
        return GlobalNightModeStateProviderHolder.getInstance();
    }

    /**
     * Initializes the initial night mode state. This will be called at the beginning of
     * {@link #onCreate(Bundle)} so that the correct theme can be applied for the initial night mode
     * state.
     */
    protected void initializeNightModeStateProvider() {}

    // NightModeStateProvider.Observer implementation.
    @Override
    public void onNightModeStateChanged() {
        if (!isFinishing()) recreate();
    }

    /**
     * Required to make preference fragments use InMemorySharedPreferences in tests.
     */
    @Override
    public SharedPreferences getSharedPreferences(String name, int mode) {
        return ContextUtils.getApplicationContext().getSharedPreferences(name, mode);
    }
}
