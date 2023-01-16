// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.UiModeManager;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.StrictMode;
import android.print.PrintAttributes;
import android.print.PrintDocumentAdapter;
import android.print.PrintManager;
import android.provider.Browser;
import android.util.SparseArray;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.PermissionRequest;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.TextView;
import android.widget.Toast;
import android.window.OnBackInvokedCallback;
import android.window.OnBackInvokedDispatcher;

import androidx.activity.result.ActivityResultCallback;
import androidx.activity.result.ActivityResultLauncher;
import androidx.annotation.RequiresApi;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.app.AppCompatDelegate;
import androidx.appcompat.widget.Toolbar;
import androidx.webkit.TracingConfig;
import androidx.webkit.TracingController;
import androidx.webkit.WebSettingsCompat;
import androidx.webkit.WebViewClientCompat;
import androidx.webkit.WebViewCompat;
import androidx.webkit.WebViewFeature;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.StrictModeContext;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.Executors;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * This activity is designed for starting a "mini-browser" for manual testing of WebView.
 * It takes an optional URL as an argument, and displays the page. There is a URL bar
 * on top of the webview for manually specifying URLs to load.
 */
public class WebViewBrowserActivity extends AppCompatActivity {
    private static final String TAG = "WebViewShell";

    // Our imaginary Android permission to associate with the WebKit geo permission.
    private static final String RESOURCE_GEO = "RESOURCE_GEO";
    // Our imaginary WebKit permission to request when loading a file:// URL.
    private static final String RESOURCE_FILE_URL = "RESOURCE_FILE_URL";
    // Our imaginary WebKit permissions to request when loading a file:// URL on T+.
    private static final String RESOURCE_IMAGES_URL = "RESOURCE_IMAGES_URL";
    private static final String RESOURCE_VIDEO_URL = "RESOURCE_VIDEO_URL";
    // WebKit permissions with no corresponding Android permission can always be granted.
    private static final String NO_ANDROID_PERMISSION = "NO_ANDROID_PERMISSION";

    // TODO(timav): Remove these variables after http://crbug.com/626202 is fixed.
    // The Bundle key for WebView serialized state
    private static final String SAVE_RESTORE_STATE_KEY = "WEBVIEW_CHROMIUM_STATE";
    // Maximal size of this state.
    private static final int MAX_STATE_LENGTH = 300 * 1024;

    // Map from WebKit permissions to Android permissions
    private static final HashMap<String, String> sPermissions;
    static {
        sPermissions = new HashMap<String, String>();
        sPermissions.put(RESOURCE_GEO, Manifest.permission.ACCESS_FINE_LOCATION);
        sPermissions.put(RESOURCE_FILE_URL, Manifest.permission.READ_EXTERNAL_STORAGE);
        if (BuildInfo.isAtLeastT()) {
            sPermissions.put(RESOURCE_IMAGES_URL, Manifest.permission.READ_MEDIA_IMAGES);
            sPermissions.put(RESOURCE_VIDEO_URL, Manifest.permission.READ_MEDIA_VIDEO);
        }
        sPermissions.put(PermissionRequest.RESOURCE_AUDIO_CAPTURE,
                Manifest.permission.RECORD_AUDIO);
        sPermissions.put(PermissionRequest.RESOURCE_MIDI_SYSEX, NO_ANDROID_PERMISSION);
        sPermissions.put(PermissionRequest.RESOURCE_PROTECTED_MEDIA_ID, NO_ANDROID_PERMISSION);
        sPermissions.put(PermissionRequest.RESOURCE_VIDEO_CAPTURE,
                Manifest.permission.CAMERA);
    }

    private EditText mUrlBar;
    private WebView mWebView;
    private View mFullscreenView;
    private String mWebViewVersion;
    private boolean mEnableTracing;
    private boolean mIsStoppingTracing;
    private final OnBackInvokedCallback mOnBackInvokedCallback =
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU ? ()
            -> mWebView.goBack()
            : null;

    // Each time we make a request, store it here with an int key. onRequestPermissionsResult will
    // look up the request in order to grant the approprate permissions.
    private SparseArray<PermissionRequest> mPendingRequests = new SparseArray<PermissionRequest>();
    private int mNextRequestKey;

    // Permit any number of slashes, since chromium seems to canonicalize bad values.
    private static final Pattern FILE_ANDROID_ASSET_PATTERN =
            Pattern.compile("^file:///android_(asset|res)/.*");

    private ActivityResultLauncher<Void> mFileContents;
    private ValueCallback<Uri[]> mFilePathCallback;
    private MultiFileSelector mMultiFileSelector;

    public void setFilePathCallback(ValueCallback<Uri[]> inCallback) {
        mFilePathCallback = inCallback;
    };

    // Work around our wonky API by wrapping a geo permission prompt inside a regular
    // PermissionRequest.
    @RequiresApi(Build.VERSION_CODES.LOLLIPOP) // GeoPermissionRequest class requires API level 21.
    private static class GeoPermissionRequest extends PermissionRequest {
        private String mOrigin;
        private GeolocationPermissions.Callback mCallback;

        public GeoPermissionRequest(String origin, GeolocationPermissions.Callback callback) {
            mOrigin = origin;
            mCallback = callback;
        }

        @Override
        public Uri getOrigin() {
            return Uri.parse(mOrigin);
        }

        @Override
        public String[] getResources() {
            return new String[] { WebViewBrowserActivity.RESOURCE_GEO };
        }

        @Override
        public void grant(String[] resources) {
            assert resources.length == 1;
            assert WebViewBrowserActivity.RESOURCE_GEO.equals(resources[0]);
            mCallback.invoke(mOrigin, true, false);
        }

        @Override
        public void deny() {
            mCallback.invoke(mOrigin, false, false);
        }
    }

    // For simplicity, also treat the read access needed for file:// URLs as a regular
    // PermissionRequest.
    @RequiresApi(Build.VERSION_CODES.LOLLIPOP) // FilePermissionRequest class requires API level 21.
    private class FilePermissionRequest extends PermissionRequest {
        private String mOrigin;

        public FilePermissionRequest(String origin) {
            mOrigin = origin;
        }

        @Override
        public Uri getOrigin() {
            return Uri.parse(mOrigin);
        }

        @Override
        public String[] getResources() {
            if (BuildInfo.isAtLeastT()) {
                return new String[] {WebViewBrowserActivity.RESOURCE_IMAGES_URL,
                        WebViewBrowserActivity.RESOURCE_VIDEO_URL};
            } else {
                return new String[] {WebViewBrowserActivity.RESOURCE_FILE_URL};
            }
        }

        @Override
        public void grant(String[] resources) {
            if (BuildInfo.isAtLeastT()) {
                assert resources.length == 2;
                assert WebViewBrowserActivity.RESOURCE_IMAGES_URL.equals(resources[0])
                        && WebViewBrowserActivity.RESOURCE_VIDEO_URL.equals(resources[1]);
            } else {
                assert resources.length == 1;
                assert WebViewBrowserActivity.RESOURCE_FILE_URL.equals(resources[0]);
            }
            // Try again now that we have read access.
            WebViewBrowserActivity.this.mWebView.loadUrl(mOrigin);
        }

        @Override
        public void deny() {
            // womp womp
        }
    }

    private class TracingLogger extends FileOutputStream {
        private long mByteCount;
        private long mChunkCount;
        private final Activity mActivity;

        public TracingLogger(String fileName, Activity activity) throws FileNotFoundException {
            super(fileName);
            mActivity = activity;
        }

        @Override
        public void write(byte[] chunk) throws IOException {
            mByteCount += chunk.length;
            mChunkCount++;
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

            mActivity.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    AlertDialog dialog = new AlertDialog.Builder(mActivity)
                                                 .setTitle("Tracing API")
                                                 .setMessage(info)
                                                 .setNeutralButton(" OK ", null)
                                                 .create();
                    dialog.show();
                }
            });
        }
    }

    /**
     * Enables StrictMode to catch as much as reasonable. This selectively disables some StrictMode
     * policies for some devices, as some manufacturers modify the Android framework in such a
     * way as to unavoidably violate StrictMode (ex. the platform code which opens the 3-dots menu
     * is not controlled by WebView or by WebView shell browser).
     */
    private void enableStrictMode() {
        String manufacturer = Build.MANUFACTURER.toLowerCase(Locale.US);

        StrictMode.ThreadPolicy.Builder threadPolicyBuilder =
                new StrictMode.ThreadPolicy.Builder().detectAll().penaltyLog().penaltyDeath();

        if (manufacturer.equals("samsung")) {
            // See crbug.com/1056368, Samsung device has an internal method
            // "android.util.GeneralUtil#isSupportedGloveModeInternal", which reads file and
            // violates strict mode policy. This method is called when showing the dropdown menu
            // after user clicks the 3-dots menu. However this showing code is part of Android
            // framework and not controlled by this app, so we need to permit disk read for the UI
            // thread.
            threadPolicyBuilder = threadPolicyBuilder.permitDiskReads();
            // See crbug.com/1082701 and https://crbug.com/1090841#c38, Samsung device uses OEM
            // specific clipboard API, which will need to read the disk on UI thread. This app can't
            // control it because it is in the framework. We need to permit disk write for the UI
            // thread.
            //
            // Also: https://crbug.com/1090841#c31
            threadPolicyBuilder = threadPolicyBuilder.permitDiskWrites();
        } else if (manufacturer.equals("htc")) {
            // https://crbug.com/1090841#c30
            threadPolicyBuilder = threadPolicyBuilder.permitDiskReads();
        } else if (manufacturer.equals("huawei")) {
            // https://crbug.com/1090841#c32
            threadPolicyBuilder = threadPolicyBuilder.permitDiskReads();
        } else if (manufacturer.equals("lge")) {
            // https://crbug.com/1090841#c33
            threadPolicyBuilder = threadPolicyBuilder.permitDiskReads();
            // https://crbug.com/1198139
            threadPolicyBuilder = threadPolicyBuilder.permitDiskWrites();
        } else if (manufacturer.equals("oneplus")) {
            // https://crbug.com/1090841#c37
            threadPolicyBuilder = threadPolicyBuilder.permitDiskReads();
        } else if (manufacturer.equals("oppo")) {
            // https://crbug.com/1177779
            threadPolicyBuilder = threadPolicyBuilder.permitDiskReads();
        } else if (manufacturer.equals("nokia")) {
            // https://crbug.com/1385924
            threadPolicyBuilder = threadPolicyBuilder.permitDiskReads();
        } else if (manufacturer.equals("xiaomi")) {
            // https://crbug.com/1401331
            threadPolicyBuilder = threadPolicyBuilder.permitDiskReads();
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
        StrictMode.setVmPolicy(builder.detectLeakedRegistrationObjects()
                                       .detectLeakedSqlLiteObjects()
                                       .penaltyLog()
                                       .penaltyDeath()
                                       .build());
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        ContextUtils.initApplicationContext(getApplicationContext());
        WebView.setWebContentsDebuggingEnabled(true);
        setContentView(R.layout.activity_webview_browser);
        setSupportActionBar((Toolbar) findViewById(R.id.browser_toolbar));
        mUrlBar = (EditText) findViewById(R.id.url_field);
        mUrlBar.setOnKeyListener((View view, int keyCode, KeyEvent event) -> {
            if (keyCode == KeyEvent.KEYCODE_ENTER && event.getAction() == KeyEvent.ACTION_UP) {
                loadUrlFromUrlBar(view);
                return true;
            }
            return false;
        });
        findViewById(R.id.btn_load_url).setOnClickListener((view) -> loadUrlFromUrlBar(view));

        enableStrictMode();

        createAndInitializeWebView();

        String url = getUrlFromIntent(getIntent());
        if (url == null) {
            mWebView.restoreState(savedInstanceState);
            url = mWebView.getUrl();
            if (url != null) {
                // If we have restored state, and that state includes
                // a loaded URL, we reload. This allows us to keep the
                // scroll offset, and also doesn't add an additional
                // navigation history entry.
                setUrlBarText(url);
                // The immediately previous loadUrlFromurlbar must
                // have got as far as calling loadUrl, so there is no
                // URI parsing error at this point.
                setUrlFail(false);
                hideKeyboard(mUrlBar);
                mWebView.reload();
                mWebView.requestFocus();
                return;
            }
            // Make sure to load a blank page to make it immediately inspectable with
            // chrome://inspect.
            url = "about:blank";
        }
        setUrlBarText(url);
        setUrlFail(false);
        loadUrlFromUrlBar(mUrlBar);

        mMultiFileSelector = new MultiFileSelector();
        mFileContents =
                registerForActivityResult(mMultiFileSelector, new ActivityResultCallback<Uri[]>() {
                    @Override
                    public void onActivityResult(Uri[] result) {
                        mFilePathCallback.onReceiveValue(result);
                    }
                });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        ViewGroup viewGroup = (ViewGroup) (mWebView.getParent());
        viewGroup.removeView(mWebView);
        mWebView.destroy();
        mWebView = null;
    }

    @Override
    public void onSaveInstanceState(Bundle savedInstanceState) {
        super.onSaveInstanceState(savedInstanceState);
        // Deliberately don't catch TransactionTooLargeException here.
        mWebView.saveState(savedInstanceState);

        // TODO(timav): Remove this hack after http://crbug.com/626202 is fixed.
        // Drop the saved state of it is too long since Android N and above
        // can't handle large states without a crash.
        byte[] webViewState = savedInstanceState.getByteArray(SAVE_RESTORE_STATE_KEY);
        if (webViewState != null && webViewState.length > MAX_STATE_LENGTH) {
            savedInstanceState.remove(SAVE_RESTORE_STATE_KEY);
            String message = String.format(
                    Locale.US, "Can't save state: %dkb is too long", webViewState.length / 1024);
            Toast.makeText(this, message, Toast.LENGTH_SHORT).show();
        }
    }

    @Override
    public void onBackPressed() {
        if (mWebView.canGoBack()) {
            mWebView.goBack();
        } else {
            super.onBackPressed();
        }
    }

    ViewGroup getContainer() {
        return (ViewGroup) findViewById(R.id.container);
    }

    private void createAndInitializeWebView() {
        WebView webview = new WebView(this);
        WebSettings settings = webview.getSettings();
        initializeSettings(settings);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            // Third party cookies are off by default on L+;
            // turn them on for consistency with normal browsers.
            CookieManager.getInstance().setAcceptThirdPartyCookies(webview, true);
        }
        mWebViewVersion = WebViewCompat.getCurrentWebViewPackage(this).versionName;
        getSupportActionBar().setTitle(getResources().getString(R.string.title_activity_browser));
        getSupportActionBar().setSubtitle(mWebViewVersion);

        webview.setWebViewClient(new WebViewClientCompat() {
            @Override
            public void onPageStarted(WebView view, String url, Bitmap favicon) {
                setUrlFail(false);
                setUrlBarText(url);
            }

            @Override
            public void onPageFinished(WebView view, String url) {
                setUrlBarText(url);
            }

            @SuppressWarnings("deprecation") // because we support api level 19 and up.
            @Override
            public boolean shouldOverrideUrlLoading(WebView webView, String url) {
                // Treat some URLs as internal, always open them in the WebView:
                // * about: scheme URIs
                // * chrome:// scheme URIs
                // * file:///android_asset/ or file:///android_res/ URIs
                if (url.startsWith("about:") || url.startsWith("chrome://")
                        || FILE_ANDROID_ASSET_PATTERN.matcher(url).matches()) {
                    return false;
                }
                return startBrowsingIntent(WebViewBrowserActivity.this, url);
            }

            @SuppressWarnings("deprecation") // because we support api level 19 and up.
            @Override
            public void onReceivedError(WebView view, int errorCode, String description,
                    String failingUrl) {
                setUrlFail(true);
            }

            @Override
            public void doUpdateVisitedHistory(WebView view, String url, boolean isReload) {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                    if (view.canGoBack()) {
                        WebViewBrowserActivity.this.getOnBackInvokedDispatcher()
                                .registerOnBackInvokedCallback(
                                        OnBackInvokedDispatcher.PRIORITY_DEFAULT,
                                        mOnBackInvokedCallback);
                    } else if (!view.canGoBack()) {
                        WebViewBrowserActivity.this.getOnBackInvokedDispatcher()
                                .unregisterOnBackInvokedCallback(mOnBackInvokedCallback);
                    }
                }
            }
        });

        webview.setWebChromeClient(new WebChromeClient() {
            @Override
            public Bitmap getDefaultVideoPoster() {
                return Bitmap.createBitmap(
                        new int[] {Color.TRANSPARENT}, 1, 1, Bitmap.Config.ARGB_8888);
            }

            @Override
            public void onGeolocationPermissionsShowPrompt(String origin,
                    GeolocationPermissions.Callback callback) {
                if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
                    // Pre Lollipop versions (< api level 21) do not have PermissionRequest,
                    // hence grant here immediately.
                    callback.invoke(origin, true, false);
                    return;
                }

                onPermissionRequest(new GeoPermissionRequest(origin, callback));
            }

            @Override
            public void onPermissionRequest(PermissionRequest request) {
                WebViewBrowserActivity.this.requestPermissionsForPage(request);
            }

            @Override
            public void onShowCustomView(View view, WebChromeClient.CustomViewCallback callback) {
                if (mFullscreenView != null) {
                    ((ViewGroup) mFullscreenView.getParent()).removeView(mFullscreenView);
                }
                mFullscreenView = view;
                getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
                getWindow().addContentView(mFullscreenView,
                        new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                                ViewGroup.LayoutParams.MATCH_PARENT, Gravity.CENTER));
            }

            @Override
            public void onHideCustomView() {
                if (mFullscreenView == null) {
                    return;
                }
                getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
                ((ViewGroup) mFullscreenView.getParent()).removeView(mFullscreenView);
                mFullscreenView = null;
            }

            @Override
            public boolean onShowFileChooser(WebView webView, ValueCallback<Uri[]> filePathCallback,
                    WebChromeClient.FileChooserParams fileChooserParams) {
                setFilePathCallback(filePathCallback);
                mMultiFileSelector.setFileChooserParams(fileChooserParams);
                mFileContents.launch(null);
                return true;
            }
        });

        mWebView = webview;
        getContainer().addView(
                webview, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        setUrlBarText("");
    }

    // WebKit permissions which can be granted because either they have no associated Android
    // permission or the associated Android permission has been granted
    @RequiresApi(Build.VERSION_CODES.M)
    private boolean canGrant(String webkitPermission) {
        String androidPermission = sPermissions.get(webkitPermission);
        if (androidPermission.equals(NO_ANDROID_PERMISSION)) {
            return true;
        }
        return PackageManager.PERMISSION_GRANTED == checkSelfPermission(androidPermission);
    }

    @RequiresApi(Build.VERSION_CODES.LOLLIPOP) // PermissionRequest#deny requires API level 21.
    private void requestPermissionsForPage(PermissionRequest request) {
        // Deny any unrecognized permissions.
        for (String webkitPermission : request.getResources()) {
            if (!sPermissions.containsKey(webkitPermission)) {
                Log.w(TAG, "Unrecognized WebKit permission: " + webkitPermission);
                request.deny();
                return;
            }
        }

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            request.grant(request.getResources());
            return;
        }

        // Find what Android permissions we need before we can grant these WebKit permissions.
        ArrayList<String> androidPermissionsNeeded = new ArrayList<String>();
        for (String webkitPermission : request.getResources()) {
            if (!canGrant(webkitPermission)) {
                // We already checked for unrecognized permissions, and canGrant will skip over
                // NO_ANDROID_PERMISSION cases, so this is guaranteed to be a regular Android
                // permission.
                String androidPermission = sPermissions.get(webkitPermission);
                androidPermissionsNeeded.add(androidPermission);
            }
        }

        // If there are no such Android permissions, grant the WebKit permissions immediately.
        if (androidPermissionsNeeded.isEmpty()) {
            request.grant(request.getResources());
            return;
        }

        // Otherwise, file a new request
        if (mNextRequestKey == Integer.MAX_VALUE) {
            Log.e(TAG, "Too many permission requests");
            return;
        }
        int requestCode = mNextRequestKey;
        mNextRequestKey++;
        mPendingRequests.append(requestCode, request);
        requestPermissions(androidPermissionsNeeded.toArray(new String[0]), requestCode);
    }

    @Override
    @RequiresApi(Build.VERSION_CODES.LOLLIPOP) // PermissionRequest#deny requires API level 21.
    public void onRequestPermissionsResult(
            int requestCode, String permissions[], int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        // Verify that we can now grant all the requested permissions. Note that although grant()
        // takes a list of permissions, grant() is actually all-or-nothing. If there are any
        // requested permissions not included in the granted permissions, all will be denied.
        PermissionRequest request = mPendingRequests.get(requestCode);
        mPendingRequests.delete(requestCode);
        for (String webkitPermission : request.getResources()) {
            if (!canGrant(webkitPermission)) {
                request.deny();
                return;
            }
        }
        request.grant(request.getResources());
    }

    public void loadUrlFromUrlBar(View view) {
        String url = mUrlBar.getText().toString();
        // Parse with android.net.Uri instead of java.net.URI because Uri does no validation. Rather
        // than failing in the browser, let WebView handle weird URLs. WebView will escape illegal
        // characters and display error pages for bad URLs like "blah://example.com".
        if (Uri.parse(url).getScheme() == null) url = "http://" + url;
        setUrlBarText(url);
        setUrlFail(false);
        loadUrl(url);
        hideKeyboard(mUrlBar);
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
                        (UiModeManager) this.getApplicationContext().getSystemService(
                                UI_MODE_SERVICE);
                checked = UiModeManager.MODE_NIGHT_YES == uiModeManager.getNightMode();
            }
            menu.findItem(R.id.menu_night_mode_on).setChecked(checked);
        }
        if (BuildInfo.targetsAtLeastT()
                && WebViewFeature.isFeatureSupported(WebViewFeature.ALGORITHMIC_DARKENING)) {
            menu.findItem(R.id.menu_algorithmic_darkening_on)
                    .setChecked(WebSettingsCompat.isAlgorithmicDarkeningAllowed(
                            mWebView.getSettings()));
        }
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int itemId = item.getItemId();
        if (itemId == R.id.menu_reload_webview) {
            if (mWebView != null) mWebView.reload();
        } else if (itemId == R.id.menu_reset_webview) {
            if (mWebView != null) {
                ViewGroup container = getContainer();
                container.removeView(mWebView);
                mWebView.destroy();
                mWebView = null;
            }
            createAndInitializeWebView();
            return true;
        } else if (itemId == R.id.menu_clear_cache) {
            if (mWebView != null) {
                mWebView.clearCache(true);
            }
            return true;
        } else if (itemId == R.id.menu_enable_tracing) {
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
                        tracingController.stop(new TracingLogger(outFileName, this),
                                Executors.newSingleThreadExecutor());
                        mIsStoppingTracing = true;
                    } catch (FileNotFoundException e) {
                        throw new RuntimeException(e);
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
            AppCompatDelegate.setDefaultNightMode(item.isChecked()
                            ? AppCompatDelegate.MODE_NIGHT_NO
                            : AppCompatDelegate.MODE_NIGHT_YES);
            return true;
        } else if (itemId == R.id.menu_algorithmic_darkening_on) {
            if (WebViewFeature.isFeatureSupported(WebViewFeature.ALGORITHMIC_DARKENING)) {
                WebSettingsCompat.setAlgorithmicDarkeningAllowed(mWebView.getSettings(),
                        !WebSettingsCompat.isAlgorithmicDarkeningAllowed(mWebView.getSettings()));
            }
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
            hideKeyboard(mUrlBar);
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

    // setGeolocationDatabasePath deprecated in api level 24,
    // but we still use it because we support api level 19 and up.
    @SuppressWarnings("deprecation")
    private void initializeSettings(WebSettings settings) {
        File geolocation = null;
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            geolocation = getDir("geolocation", 0);
        }

        settings.setJavaScriptEnabled(true);

        // configure local storage apis and their database paths.
        settings.setGeolocationDatabasePath(geolocation.getPath());

        settings.setGeolocationEnabled(true);
        settings.setDatabaseEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setAllowFileAccess(true);
        settings.setAllowContentAccess(true);

        // Default layout behavior for chrome on android.
        settings.setBuiltInZoomControls(true);
        settings.setDisplayZoomControls(false);
        settings.setUseWideViewPort(true);
        settings.setLoadWithOverviewMode(true);
        settings.setLayoutAlgorithm(WebSettings.LayoutAlgorithm.TEXT_AUTOSIZING);
    }

    private void about() {
        WebSettings settings = mWebView.getSettings();
        StringBuilder summary = new StringBuilder();
        summary.append("WebView version : " + mWebViewVersion + "\n");

        for (Method method : settings.getClass().getMethods()) {
            if (!methodIsSimpleInspector(method)) continue;
            try {
                summary.append(method.getName() + " : " + method.invoke(settings) + "\n");
            } catch (IllegalAccessException e) {
            } catch (InvocationTargetException e) { }
        }

        AlertDialog dialog = new AlertDialog.Builder(this)
                .setTitle(getResources().getString(R.string.menu_about))
                .setMessage(summary)
                .setPositiveButton("OK", null)
                .create();
        dialog.show();
        dialog.getWindow().setLayout(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
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
            Log.e(TAG,
                    "Couldn't launch developer UI from current WebView package: "
                            + currentWebViewPackage);
            Toast.makeText(this, "No DevTools in " + currentWebViewPackageName, Toast.LENGTH_LONG)
                    .show();
        }
    }

    // Returns true is a method has no arguments and returns either a boolean or a String.
    private boolean methodIsSimpleInspector(Method method) {
        Class<?> returnType = method.getReturnType();
        return ((returnType.equals(boolean.class) || returnType.equals(String.class))
                && method.getParameterTypes().length == 0);
    }

    private void loadUrl(String url) {
        // Request read access if necessary.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
                && "file".equals(Uri.parse(url).getScheme())) {
            if (BuildInfo.isAtLeastT()) {
                if (PackageManager.PERMISSION_DENIED
                                == checkSelfPermission(Manifest.permission.READ_MEDIA_IMAGES)
                        && PackageManager.PERMISSION_DENIED
                                == checkSelfPermission(Manifest.permission.READ_MEDIA_VIDEO)) {
                    requestPermissionsForPage(new FilePermissionRequest(url));
                }
            } else if (PackageManager.PERMISSION_DENIED
                    == checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE)) {
                requestPermissionsForPage(new FilePermissionRequest(url));
            }
        }

        // If it is file:// and we don't have permission, they'll get the "Webpage not available"
        // "net::ERR_ACCESS_DENIED" page. When we get permission, FilePermissionRequest.grant()
        // will reload.
        mWebView.loadUrl(url);
        mWebView.requestFocus();
    }

    private void setUrlBarText(String url) {
        mUrlBar.setText(url, TextView.BufferType.EDITABLE);
    }

    private void setUrlFail(boolean fail) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            mUrlBar.setTextAppearance(fail ? R.style.UrlTextError : R.style.UrlText);
        } else {
            mUrlBar.setTextAppearance(this, fail ? R.style.UrlTextError : R.style.UrlText);
        }
    }

    /**
     * Hides the keyboard.
     * @param view The {@link View} that is currently accepting input.
     * @return Whether the keyboard was visible before.
     */
    private static boolean hideKeyboard(View view) {
        InputMethodManager imm = (InputMethodManager) view.getContext().getSystemService(
                Context.INPUT_METHOD_SERVICE);
        return imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
    }

    static final Pattern BROWSER_URI_SCHEMA = Pattern.compile(
            "(?i)"   // switch on case insensitive matching
            + "("    // begin group for schema
            + "(?:http|https|file):\\/\\/"
            + "|(?:inline|data|about|chrome|javascript):"
            + ")"
            + "(.*)");

    private static boolean startBrowsingIntent(Context context, String url) {
        Intent intent;
        // Perform generic parsing of the URI to turn it into an Intent.
        try {
            intent = Intent.parseUri(url, Intent.URI_INTENT_SCHEME);
        } catch (Exception ex) {
            Log.w(TAG, "Bad URI %s", url, ex);
            return false;
        }
        // Check for regular URIs that WebView supports by itself, but also
        // check if there is a specialized app that had registered itself
        // for this kind of an intent.
        Matcher m = BROWSER_URI_SCHEMA.matcher(url);
        if (m.matches() && !isSpecializedHandlerAvailable(intent)) {
            return false;
        }
        // Sanitize the Intent, ensuring web pages can not bypass browser
        // security (only access to BROWSABLE activities).
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        intent.setComponent(null);
        Intent selector = intent.getSelector();
        if (selector != null) {
            selector.addCategory(Intent.CATEGORY_BROWSABLE);
            selector.setComponent(null);
        }

        // Pass the package name as application ID so that the intent from the
        // same application can be opened in the same tab.
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        try {
            context.startActivity(intent);
            return true;
        } catch (ActivityNotFoundException ex) {
            Log.w(TAG, "No application can handle %s", url);
        } catch (SecurityException ex) {
            // This can happen if the Activity is exported="true", guarded by a permission, and sets
            // up an intent filter matching this intent. This is a valid configuration for an
            // Activity, so instead of crashing, we catch the exception and do nothing. See
            // https://crbug.com/808494 and https://crbug.com/889300.
            Log.w(TAG, "SecurityException when starting intent for %s", url);
        }
        return false;
    }

    /**
     * Search for intent handlers that are specific to the scheme of the URL in the intent.
     */
    private static boolean isSpecializedHandlerAvailable(Intent intent) {
        List<ResolveInfo> handlers = PackageManagerUtils.queryIntentActivities(
                intent, PackageManager.GET_RESOLVED_FILTER);
        if (handlers == null || handlers.size() == 0) {
            return false;
        }
        for (ResolveInfo resolveInfo : handlers) {
            if (!isNullOrGenericHandler(resolveInfo.filter)) {
                return true;
            }
        }
        return false;
    }

    private static boolean isNullOrGenericHandler(IntentFilter filter) {
        return filter == null
                || (filter.countDataAuthorities() == 0 && filter.countDataPaths() == 0);
    }
}
