// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.chrome.browser.base.SplitCompatApplication.CHROME_SPLIT_NAME;

import android.app.ActivityManager.TaskDescription;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.StyleRes;
import androidx.appcompat.app.AppCompatActivity;

import com.google.android.material.color.DynamicColors;

import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.base.ServiceTracingProxyProvider;
import org.chromium.chrome.browser.base.SplitChromeApplication;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.language.GlobalAppLocaleController;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.night_mode.GlobalNightModeStateProviderHolder;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

import java.util.LinkedHashSet;

/**
 * A subclass of {@link AppCompatActivity} that maintains states and objects applied to all
 * activities in {@link ChromeApplication} (e.g. night mode).
 */
public class ChromeBaseAppCompatActivity extends AppCompatActivity
        implements NightModeStateProvider.Observer, ModalDialogManagerHolder {
    private final ObservableSupplierImpl<ModalDialogManager> mModalDialogManagerSupplier =
            new ObservableSupplierImpl<>();
    private NightModeStateProvider mNightModeStateProvider;
    private LinkedHashSet<Integer> mThemeResIds = new LinkedHashSet<>();
    private ServiceTracingProxyProvider mServiceTracingProxyProvider;

    @Override
    protected void attachBaseContext(Context newBase) {
        super.attachBaseContext(newBase);

        // Make sure the "chrome" split is loaded before checking if ClassLoaders are equal.
        SplitChromeApplication.finishPreload(CHROME_SPLIT_NAME);
        ClassLoader chromeModuleClassLoader = ChromeBaseAppCompatActivity.class.getClassLoader();
        Context appContext = ContextUtils.getApplicationContext();
        if (!chromeModuleClassLoader.equals(appContext.getClassLoader())) {
            // This should only happen on Android O. See crbug.com/1146745 for more info.
            throw new IllegalStateException("ClassLoader mismatch detected.\nA: "
                    + chromeModuleClassLoader + "\nB: " + appContext.getClassLoader()
                    + "\nC: " + chromeModuleClassLoader.getParent()
                    + "\nD: " + appContext.getClassLoader().getParent() + "\nE: " + appContext);
        }
        // If ClassLoader was corrected by SplitCompatAppComponentFactory, also need to correct
        // the reference in the associated Context.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            BundleUtils.checkContextClassLoader(newBase, this);
        }

        mServiceTracingProxyProvider = ServiceTracingProxyProvider.create(newBase);

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
        BundleUtils.restoreLoadedSplits(savedInstanceState);
        mModalDialogManagerSupplier.set(createModalDialogManager());

        initializeNightModeStateProvider();
        mNightModeStateProvider.addObserver(this);
        super.onCreate(savedInstanceState);
        applyThemeOverlays();

        // Activity level locale overrides must be done in onCreate.
        GlobalAppLocaleController.getInstance().maybeOverrideContextConfig(this);

        setDefaultTaskDescription();
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
    public ClassLoader getClassLoader() {
        // Replace the default ClassLoader with a custom SplitAware one so that
        // LayoutInflaters that use this ClassLoader can find view classes that
        // live inside splits. Very useful when FragmentManger tries to inflate
        // the UI automatically on restore.
        return BundleUtils.getSplitCompatClassLoader();
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        BundleUtils.saveLoadedSplits(outState);
    }

    @Override
    protected void onRestoreInstanceState(@Nullable Bundle state) {
        if (state != null) {
            // Ensure that classes from previously loaded splits can be read from the bundle.
            // https://crbug.com/1382227
            ClassLoader splitClassLoader = BundleUtils.getSplitCompatClassLoader();
            state.setClassLoader(splitClassLoader);
            // See: https://cs.android.com/search?q=Activity.java%20symbol:onRestoreInstanceState
            Bundle windowState = state.getBundle("android:viewHierarchyState");
            if (windowState != null) {
                windowState.setClassLoader(splitClassLoader);
            }
        }
        super.onRestoreInstanceState(state);
    }

    @Override
    public void setTheme(@StyleRes int resid) {
        super.setTheme(resid);
        mThemeResIds.add(resid);
    }

    @Override
    @RequiresApi(Build.VERSION_CODES.O)
    public void onMultiWindowModeChanged(boolean inMultiWindowMode, Configuration configuration) {
        super.onMultiWindowModeChanged(inMultiWindowMode, configuration);
        onMultiWindowModeChanged(inMultiWindowMode);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        NightModeUtils.updateConfigurationForNightMode(
                this, mNightModeStateProvider.isInNightMode(), newConfig, mThemeResIds);
        // newConfig will have the default system locale so reapply the app locale override if
        // needed: https://crbug.com/1248944
        GlobalAppLocaleController.getInstance().maybeOverrideContextConfig(this);
    }

    // Implementation of ModalDialogManagerHolder
    /**
     * @return The {@link ModalDialogManager} that manages the display of modal dialogs (e.g.
     *         JavaScript dialogs).
     */
    @Override
    public ModalDialogManager getModalDialogManager() {
        // TODO(jinsukkim): Remove this method in favor of getModalDialogManagerSupplier().
        return getModalDialogManagerSupplier().get();
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

    /**
     * Apply theme overlay to this activity class.
     */
    @CallSuper
    protected void applyThemeOverlays() {
        if (ChromeFeatureList.sBaselineGm3SurfaceColors.isEnabled()) {
            getTheme().applyStyle(R.style.SurfaceColorsThemeOverlay, /* force= */ true);
            mThemeResIds.add(R.style.SurfaceColorsThemeOverlay);
        }
        DynamicColors.applyToActivityIfAvailable(this);

        DeferredStartupHandler.getInstance().addDeferredTask(() -> {
            // #registerSyntheticFieldTrial requires native.
            boolean isDynamicColorAvailable = DynamicColors.isDynamicColorAvailable();
            RecordHistogram.recordBooleanHistogram(
                    "Android.DynamicColors.IsAvailable", isDynamicColorAvailable);
            UmaSessionStats.registerSyntheticFieldTrial(
                    "IsDynamicColorAvailable", isDynamicColorAvailable ? "Enabled" : "Disabled");
        });
    }

    /**
     * Sets the default task description that will appear in the recents UI.
     */
    protected void setDefaultTaskDescription() {
        final TaskDescription taskDescription =
                new TaskDescription(null, null, getColor(R.color.default_task_description_color));
        setTaskDescription(taskDescription);
    }

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

    // Note that we do not need to (and can't) override getSystemService(Class<T>) as internally
    // that just gets the name of the Service and calls getSystemService(String) for backwards
    // compatibility with overrides like this one.
    @Override
    public Object getSystemService(String name) {
        Object service = super.getSystemService(name);
        if (mServiceTracingProxyProvider != null) {
            mServiceTracingProxyProvider.traceSystemServices();
        }
        return service;
    }
}
