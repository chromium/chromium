// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.UiModeManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.os.Build;
import android.os.Bundle;
import android.os.StrictMode;
import android.print.PrintAttributes;
import android.print.PrintDocumentAdapter;
import android.print.PrintManager;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.webkit.CookieManager;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.widget.Toast;

import androidx.activity.EdgeToEdge;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.app.AppCompatDelegate;
import androidx.appcompat.widget.Toolbar;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.webkit.TracingConfig;
import androidx.webkit.TracingController;
import androidx.webkit.WebSettingsCompat;
import androidx.webkit.WebViewCompat;
import androidx.webkit.WebViewFeature;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.StrictModeContext;

import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Locale;
import java.util.concurrent.Executors;

/**
 * This activity is designed for starting a "mini-browser" for manual testing of WebView.
 * It takes an optional URL as an argument, and displays the page. There is a URL bar
 * on top of the webview for manually specifying URLs to load.
 */
public class WebViewBrowserActivity extends AppCompatActivity {
    private static final String TAG = "WebViewShell";

    private WebViewBrowserFragment mFragment;
    private String mWebViewVersion;
    private boolean mEnableTracing;
    private boolean mIsStoppingTracing;
    private WebView mWebView;

    // This set of models will always bypass strict mode.
    // Google pre-release hardware models do not belong here.
    private static final HashSet<String> STRICT_MODE_BYPASS_MODELS =
            new HashSet<>(
                    Arrays.asList(
                            "humuhumu titan" // See https://crbug.com/1090841#c76
                            ));

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        EdgeToEdge.enable(this);
        super.onCreate(savedInstanceState);
        ContextUtils.initApplicationContext(getApplicationContext());

        setupEdgeToEdge();
        setContentView(R.layout.activity_webview_browser);
        setSupportActionBar((Toolbar) findViewById(R.id.browser_toolbar));
        mWebViewVersion = WebViewCompat.getCurrentWebViewPackage(this).versionName;
        getSupportActionBar().setTitle(getResources().getString(R.string.title_activity_browser));
        getSupportActionBar().setSubtitle(mWebViewVersion);

        mFragment =
                (WebViewBrowserFragment)
                        getSupportFragmentManager().findFragmentById(R.id.container);
        assert mFragment != null;
        mFragment.setActivityResultRegistry(getActivityResultRegistry());
        enableStrictMode();
    }

    @Override
    protected void onResume() {
        super.onResume();
        mWebView = mFragment.getWebView();
    }

    @Override
    public void onBackPressed() {
        if (mWebView != null && mWebView.canGoBack()) {
            mWebView.goBack();
        } else {
            super.onBackPressed();
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.main_menu, menu);
        if (!WebViewFeature.isFeatureSupported(WebViewFeature.TRACING_CONTROLLER_BASIC_USAGE)) {
            menu.findItem(R.id.menu_enable_tracing).setEnabled(false);
        }
        if (!WebViewFeature.isFeatureSupported(WebViewFeature.FORCE_DARK)
                || BuildInfo.targetsAtLeastT()) {
            menu.findItem(R.id.menu_force_dark_off).setEnabled(false);
            menu.findItem(R.id.menu_force_dark_auto).setEnabled(false);
            menu.findItem(R.id.menu_force_dark_on).setEnabled(false);
        }
        if (!WebViewFeature.isFeatureSupported(WebViewFeature.MULTI_PROFILE)) {
            menu.findItem(R.id.menu_multi_profile).setEnabled(false);
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            menu.findItem(R.id.menu_night_mode_on).setEnabled(false);
        }
        if (!BuildInfo.targetsAtLeastT()
                || !WebViewFeature.isFeatureSupported(WebViewFeature.ALGORITHMIC_DARKENING)) {
            menu.findItem(R.id.menu_algorithmic_darkening_on).setEnabled(false);
        }
        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        if (WebViewFeature.isFeatureSupported(WebViewFeature.TRACING_CONTROLLER_BASIC_USAGE)
                && !mIsStoppingTracing) {
            menu.findItem(R.id.menu_enable_tracing).setEnabled(true);
            menu.findItem(R.id.menu_enable_tracing).setChecked(mEnableTracing);
        } else {
            menu.findItem(R.id.menu_enable_tracing).setEnabled(false);
        }
        if (WebViewFeature.isFeatureSupported(WebViewFeature.FORCE_DARK)
                && !BuildInfo.targetsAtLeastT()) {
            int forceDarkState = WebSettingsCompat.getForceDark(mWebView.getSettings());
            switch (forceDarkState) {
                case WebSettingsCompat.FORCE_DARK_OFF:
                    menu.findItem(R.id.menu_force_dark_off).setChecked(true);
                    break;
                case WebSettingsCompat.FORCE_DARK_AUTO:
                    menu.findItem(R.id.menu_force_dark_auto).setChecked(true);
                    break;
                case WebSettingsCompat.FORCE_DARK_ON:
                    menu.findItem(R.id.menu_force_dark_on).setChecked(true);
                    break;
            }
        }

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            boolean checked =
                    AppCompatDelegate.MODE_NIGHT_YES == AppCompatDelegate.getDefaultNightMode();
            int defaultNightMode = AppCompatDelegate.getDefaultNightMode();
            if (defaultNightMode == AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM
                    || defaultNightMode == AppCompatDelegate.MODE_NIGHT_UNSPECIFIED) {
                UiModeManager uiModeManager =
                        (UiModeManager)
                                this.getApplicationContext().getSystemService(UI_MODE_SERVICE);
                checked = UiModeManager.MODE_NIGHT_YES == uiModeManager.getNightMode();
            }
            menu.findItem(R.id.menu_night_mode_on).setChecked(checked);
        }
        if (BuildInfo.targetsAtLeastT()
                && WebViewFeature.isFeatureSupported(WebViewFeature.ALGORITHMIC_DARKENING)) {
            menu.findItem(R.id.menu_algorithmic_darkening_on)
                    .setChecked(
                            WebSettingsCompat.isAlgorithmicDarkeningAllowed(
                                    mWebView.getSettings()));
        }

        menu.findItem(R.id.menu_enable_third_party_cookies).setEnabled(mWebView != null);
        if (mWebView != null) {
            menu.findItem(R.id.menu_enable_third_party_cookies)
                    .setChecked(CookieManager.getInstance().acceptThirdPartyCookies(mWebView));
        }
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int itemId = item.getItemId();
        if (itemId == R.id.menu_reload_webview) {
            if (mWebView != null) mWebView.reload();
        } else if (itemId == R.id.menu_reset_webview) {
            mFragment.resetWebView();
            mWebView = mFragment.getWebView();
            return true;
        } else if (itemId == R.id.menu_clear_cache) {
            if (mWebView != null) {
                mWebView.clearCache(true);
            }
            return true;
        } else if (itemId == R.id.menu_get_cookie) {
            String url = mWebView.getUrl();
            if (url != null) {
                String cookie = CookieManager.getInstance().getCookie(url);
                Log.w(TAG, "GetCookie: " + cookie);
            } else {
                Toast.makeText(this, "Error: Url is not set", Toast.LENGTH_SHORT).show();
            }
            return true;
        } else if (itemId == R.id.menu_enable_tracing) {
            // This menu item is disabled when mIsStoppingTracing is true, but this
            // is only updated if the menu is closed and reopened. This check is for when
            // this menu item is triggered multiple times while the menu is open which
            // can cause tracing to start when it is already started and throw an error.
            if (!mIsStoppingTracing) {
                mEnableTracing = !mEnableTracing;
                item.setChecked(mEnableTracing);

                TracingController tracingController = TracingController.getInstance();
                if (mEnableTracing) {
                    tracingController.start(
                            new TracingConfig.Builder()
                                    .addCategories(TracingConfig.CATEGORIES_WEB_DEVELOPER)
                                    .setTracingMode(TracingConfig.RECORD_CONTINUOUSLY)
                                    .build());
                } else {
                    try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
                        String outFileName = getFilesDir() + "/webview_tracing.json";
                        try {
                            tracingController.stop(
                                    new TracingLogger(outFileName, this),
                                    Executors.newSingleThreadExecutor());
                            mIsStoppingTracing = true;
                        } catch (FileNotFoundException e) {
                            throw new RuntimeException(e);
                        }
                    }
                }
            }
            return true;
        } else if (itemId == R.id.menu_force_dark_off) {
            WebSettingsCompat.setForceDark(
                    mWebView.getSettings(), WebSettingsCompat.FORCE_DARK_OFF);
            item.setChecked(true);
            return true;
        } else if (itemId == R.id.menu_force_dark_auto) {
            WebSettingsCompat.setForceDark(
                    mWebView.getSettings(), WebSettingsCompat.FORCE_DARK_AUTO);
            item.setChecked(true);
            return true;
        } else if (itemId == R.id.menu_force_dark_on) {
            WebSettingsCompat.setForceDark(mWebView.getSettings(), WebSettingsCompat.FORCE_DARK_ON);
            item.setChecked(true);
            return true;
        } else if (itemId == R.id.menu_night_mode_on) {
            AppCompatDelegate.setDefaultNightMode(
                    item.isChecked()
                            ? AppCompatDelegate.MODE_NIGHT_NO
                            : AppCompatDelegate.MODE_NIGHT_YES);
            return true;
        } else if (itemId == R.id.menu_algorithmic_darkening_on) {
            if (WebViewFeature.isFeatureSupported(WebViewFeature.ALGORITHMIC_DARKENING)) {
                WebSettingsCompat.setAlgorithmicDarkeningAllowed(
                        mWebView.getSettings(),
                        !WebSettingsCompat.isAlgorithmicDarkeningAllowed(mWebView.getSettings()));
            }
            return true;
        } else if (itemId == R.id.menu_enable_third_party_cookies) {
            if (mWebView != null) {
                boolean enable = !item.isChecked();
                CookieManager.getInstance().setAcceptThirdPartyCookies(mWebView, enable);
                item.setChecked(enable);
                mWebView.reload(); // Reload to apply the settings.
            }
        } else if (itemId == R.id.menu_multi_profile) {
            startActivity(new Intent(this, WebViewMultiProfileBrowserActivity.class));
            return true;
        } else if (itemId == R.id.start_animation_activity) {
            startActivity(new Intent(this, WebViewAnimationTestActivity.class));
            return true;
        } else if (itemId == R.id.menu_print) {
            PrintManager printManager = (PrintManager) getSystemService(Context.PRINT_SERVICE);
            String jobName = "WebViewShell document";
            PrintDocumentAdapter printAdapter = mWebView.createPrintDocumentAdapter(jobName);
            printManager.print(jobName, printAdapter, new PrintAttributes.Builder().build());
            return true;
        } else if (itemId == R.id.menu_about) {
            about();
            mFragment.hideKeyboard();
            return true;
        } else if (itemId == R.id.menu_devui) {
            launchWebViewDevUI();
            return true;
        } else if (itemId == R.id.menu_hide) {
            if (mWebView.getVisibility() == View.VISIBLE) {
                mWebView.setVisibility(View.INVISIBLE);
            } else {
                mWebView.setVisibility(View.VISIBLE);
            }
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void about() {
        WebSettings settings = mWebView.getSettings();
        StringBuilder summary = new StringBuilder();
        summary.append("WebView version : " + mWebViewVersion + "\n");

        for (Method method : settings.getClass().getMethods()) {
            if (!methodIsSimpleInspector(method)) {
                continue;
            }
            try {
                summary.append(method.getName() + " : " + method.invoke(settings) + "\n");
            } catch (IllegalAccessException e) {
            } catch (InvocationTargetException e) {
            }
        }

        AlertDialog dialog =
                new AlertDialog.Builder(this)
                        .setTitle(getResources().getString(R.string.menu_about))
                        .setMessage(summary)
                        .setPositiveButton("OK", null)
                        .create();
        dialog.show();
        dialog.getWindow().setLayout(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
    }

    // Returns true is a method has no arguments and returns either a boolean or a String.
    private boolean methodIsSimpleInspector(Method method) {
        Class<?> returnType = method.getReturnType();
        return ((returnType.equals(boolean.class) || returnType.equals(String.class))
                && method.getParameterTypes().length == 0);
    }

    private void launchWebViewDevUI() {
        PackageInfo currentWebViewPackage = WebViewCompat.getCurrentWebViewPackage(this);
        if (currentWebViewPackage == null) {
            Log.e(TAG, "Couldn't find current WebView package");
            Toast.makeText(this, "WebView package isn't found", Toast.LENGTH_LONG).show();
            return;
        }
        String currentWebViewPackageName = currentWebViewPackage.packageName;
        Intent intent = new Intent("com.android.webview.SHOW_DEV_UI");
        intent.setPackage(currentWebViewPackageName);

        // Check if the intent is resolved, i.e current WebView package has a developer UI that
        // responds to "com.android.webview.SHOW_DEV_UI" action.
        if (PackageManagerUtils.canResolveActivity(intent)) {
            startActivity(intent);
        } else {
            Log.e(
                    TAG,
                    "Couldn't launch developer UI from current WebView package: "
                            + currentWebViewPackage);
            Toast.makeText(this, "No DevTools in " + currentWebViewPackageName, Toast.LENGTH_LONG)
                    .show();
        }
    }

    /**
     * Enables StrictMode to catch as much as reasonable. This selectively disables some StrictMode
     * policies for some devices, as some manufacturers modify the Android framework in such a way
     * as to unavoidably violate StrictMode (ex. the platform code which opens the 3-dots menu is
     * not controlled by WebView or by WebView shell browser).
     */
    private static void enableStrictMode() {
        String manufacturer = Build.MANUFACTURER.toLowerCase(Locale.US);
        String model = Build.MODEL.toLowerCase(Locale.US);

        StrictMode.ThreadPolicy.Builder threadPolicyBuilder =
                new StrictMode.ThreadPolicy.Builder().detectAll().penaltyLog().penaltyDeath();

        if (!manufacturer.equalsIgnoreCase("google") || STRICT_MODE_BYPASS_MODELS.contains(model)) {
            threadPolicyBuilder.permitDiskReads();
            threadPolicyBuilder.permitDiskWrites();
        }

        StrictMode.setThreadPolicy(threadPolicyBuilder.build());

        // Omissions:
        // * detectCleartextNetwork() to permit testing http:// URLs
        // * detectFileUriExposure() to permit testing file:// URLs
        // * detectLeakedClosableObjects() because of drag and drop (https://crbug.com/1090841#c40)
        StrictMode.VmPolicy.Builder builder = new StrictMode.VmPolicy.Builder();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            // WebViewBrowserActivity will have two instances when switching night mode back and
            // forth for the 3rd times. Don't know the reason, this probably needs the investigation
            // to rule out WebView holding the instance. (crbug.com/1348615)
            builder = builder.detectActivityLeaks();
        }
        StrictMode.setVmPolicy(
                builder.detectLeakedRegistrationObjects()
                        .detectLeakedSqlLiteObjects()
                        .penaltyLog()
                        .penaltyDeath()
                        .build());
    }

    private class TracingLogger extends FileOutputStream {
        private long mByteCount;
        private final Activity mActivity;

        public TracingLogger(String fileName, Activity activity) throws FileNotFoundException {
            super(fileName);
            mActivity = activity;
        }

        @Override
        public void write(byte[] chunk) throws IOException {
            mByteCount += chunk.length;
            super.write(chunk);
        }

        @Override
        public void close() throws IOException {
            super.close();
            showDialog(mByteCount);
            mIsStoppingTracing = false;
        }

        private void showDialog(long nbBytes) {
            StringBuilder info = new StringBuilder();
            info.append("Tracing data written to file\n");
            info.append("number of bytes: " + nbBytes);

            mActivity.runOnUiThread(
                    new Runnable() {
                        @Override
                        public void run() {
                            AlertDialog dialog =
                                    new AlertDialog.Builder(mActivity)
                                            .setTitle("Tracing API")
                                            .setMessage(info)
                                            .setNeutralButton(" OK ", null)
                                            .create();
                            dialog.show();
                        }
                    });
        }
    }

    private void setupEdgeToEdge() {
        ViewCompat.setOnApplyWindowInsetsListener(
                findViewById(android.R.id.content),
                (v, windowInsets) -> {
                    Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());
                    // Apply the insets paddings to the view.
                    v.setPadding(insets.left, insets.top, insets.right, insets.bottom);

                    // Return CONSUMED to indicate we have handled the insets for this view
                    // and don't want them to be passed down to descendant views.
                    return WindowInsetsCompat.CONSUMED;
                });
    }
}
