// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.Manifest;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Browser;
import android.util.SparseArray;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.webkit.CookieManager;
import android.webkit.DownloadListener;
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

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.ActivityResultRegistry;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.fragment.app.Fragment;
import androidx.webkit.WebViewClientCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.task.AsyncTask;
import org.chromium.net.ChromiumNetworkAdapter;
import org.chromium.net.NetworkTrafficAnnotationTag;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URL;
import java.net.URLConnection;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class WebViewBrowserFragment extends Fragment {
    private static final String TAG = "WebViewShell";

    public static final String ARG_PROFILE =
            "org.chromium.webview_shell.WebViewBrowserFragment.Profile";

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
        sPermissions = new HashMap<>();
        sPermissions.put(RESOURCE_GEO, Manifest.permission.ACCESS_FINE_LOCATION);
        sPermissions.put(RESOURCE_FILE_URL, Manifest.permission.READ_EXTERNAL_STORAGE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            sPermissions.put(RESOURCE_IMAGES_URL, Manifest.permission.READ_MEDIA_IMAGES);
            sPermissions.put(RESOURCE_VIDEO_URL, Manifest.permission.READ_MEDIA_VIDEO);
        }
        sPermissions.put(
                PermissionRequest.RESOURCE_AUDIO_CAPTURE, Manifest.permission.RECORD_AUDIO);
        sPermissions.put(PermissionRequest.RESOURCE_MIDI_SYSEX, NO_ANDROID_PERMISSION);
        sPermissions.put(PermissionRequest.RESOURCE_PROTECTED_MEDIA_ID, NO_ANDROID_PERMISSION);
        sPermissions.put(PermissionRequest.RESOURCE_VIDEO_CAPTURE, Manifest.permission.CAMERA);
    }

    private EditText mUrlBar;
    private WebView mWebView;
    private View mFullscreenView;
    private ActivityResultRegistry mActivityResultRegistry;
    private final OnBackInvokedCallback mOnBackInvokedCallback =
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU ? () -> mWebView.goBack() : null;

    // Each time we make a request, store it here with an int key. onRequestPermissionsResult will
    // look up the request in order to grant the approprate permissions.
    private SparseArray<PermissionRequest> mPendingRequests = new SparseArray<PermissionRequest>();
    private int mNextRequestKey;

    // Permit any number of slashes, since chromium seems to canonicalize bad values.
    private static final Pattern FILE_ANDROID_ASSET_PATTERN =
            Pattern.compile("^file:///android_(asset|res)/.*");

    private ValueCallback<Uri[]> mFilePathCallback;
    private final MultiFileSelector mMultiFileSelector = new MultiFileSelector();
    private ActivityResultLauncher<Void> mFileContents;

    private @Nullable String mProfileName;

    public void setFilePathCallback(ValueCallback<Uri[]> inCallback) {
        mFilePathCallback = inCallback;
    }

    public void setActivityResultRegistry(ActivityResultRegistry activityResultRegistry) {
        mActivityResultRegistry = activityResultRegistry;
    }

    public WebViewBrowserFragment() {}

    // Work around our wonky API by wrapping a geo permission prompt inside a regular
    // PermissionRequest.
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
            return new String[] {RESOURCE_GEO};
        }

        @Override
        public void grant(String[] resources) {
            assert resources.length == 1;
            assert RESOURCE_GEO.equals(resources[0]);
            mCallback.invoke(mOrigin, true, false);
        }

        @Override
        public void deny() {
            mCallback.invoke(mOrigin, false, false);
        }
    }

    // For simplicity, also treat the read access needed for file:// URLs as a regular
    // PermissionRequest.
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
            return new String[] {RESOURCE_FILE_URL};
        }

        @Override
        public void grant(String[] resources) {
            assert resources.length == 1;
            assert RESOURCE_FILE_URL.equals(resources[0]);

            // Try again now that we have read access.
            mWebView.loadUrl(mOrigin);
        }

        @Override
        public void deny() {
            // womp womp
        }
    }

    /** Background Async Task to download file */
    static class DownloadFileFromURL extends AsyncTask<String> {
        private String mFileUrl;
        private String mNameOfFile;
        private static final String DEFAULT_FILE_NAME = "default-filename";
        private static final int BUFFER_SIZE = 8 * 1024; // 8 KB

        private String extractFilename(String url) {
            String[] arrOfStr = url.split("/");
            int len = arrOfStr.length;
            return len == 0 ? "" : arrOfStr[len - 1];
        }

        public DownloadFileFromURL(String fUrl) {
            mFileUrl = fUrl;
            mNameOfFile = extractFilename(fUrl);
            if ("".equals(mNameOfFile)) {
                mNameOfFile = DEFAULT_FILE_NAME;
            }
            Log.i(TAG, "filename: " + mNameOfFile);
        }

        @Override
        protected void onPostExecute(String result) {}

        /** Downloading file in background thread */
        @Override
        protected String doInBackground() {
            try {
                NetworkTrafficAnnotationTag annotation =
                        NetworkTrafficAnnotationTag.createComplete(
                                "android_webview_shell",
                                """
                    semantics {
                      sender: "WebViewBrowserFragment (Android)"
                      description:
                        "Downloads files as specified by the shell browser."
                      trigger: "User interations within the browser, causing a download"
                      data: "No additional data."
                      destination: LOCAL
                      internal {
                        contacts {
                          email: "avvall@chromium.org"
                        }
                      }
                      user_data {
                        type: NONE
                      }
                      last_reviewed: "2024-07-25"
                    }
                    policy {
                      cookies_allowed: NO
                      setting: "This feature can not be disabled."
                      policy_exception_justification: "Not implemented."
                    }""");
                URL url = new URL(mFileUrl);
                URLConnection connection = ChromiumNetworkAdapter.openConnection(url, annotation);
                connection.connect();

                // download the file
                InputStream input =
                        new BufferedInputStream(ChromiumNetworkAdapter.openStream(url, annotation));

                File path =
                        Environment.getExternalStoragePublicDirectory(
                                Environment.DIRECTORY_DOWNLOADS);
                File file = new File(path, mNameOfFile);
                // Make sure the Downloads directory exists.
                path.mkdirs();
                OutputStream output = new FileOutputStream(file);

                int count;
                byte[] data = new byte[BUFFER_SIZE];
                while ((count = input.read(data)) != -1) {
                    output.write(data, 0, count);
                }

                output.flush();
                output.close();
                input.close();
            } catch (Exception e) {
                Log.e(TAG, "Error: " + e.getMessage());
            }

            return null;
        }
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_webview_browser, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        WebView.setWebContentsDebuggingEnabled(true);
        mUrlBar = view.findViewById(R.id.url_field);
        mUrlBar.setOnKeyListener(
                (View view1, int keyCode, KeyEvent event) -> {
                    if (keyCode == KeyEvent.KEYCODE_ENTER
                            && event.getAction() == KeyEvent.ACTION_UP) {
                        loadUrlFromUrlBar(view1);
                        return true;
                    }
                    return false;
                });
        ApiCompatibilityUtils.clearHandwritingBoundsOffsetBottom(mUrlBar);
        view.findViewById(R.id.btn_load_url)
                .setOnClickListener((view1) -> loadUrlFromUrlBar(view1));

        createAndInitializeWebView();
        mFileContents =
                registerForActivityResult(
                        mMultiFileSelector,
                        mActivityResultRegistry,
                        result -> mFilePathCallback.onReceiveValue(result));

        String url = getUrlFromIntent(requireActivity().getIntent());
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
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        ViewGroup viewGroup = (ViewGroup) mWebView.getParent();
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
            String message =
                    String.format(
                            Locale.US,
                            "Can't save state: %dkb is too long",
                            webViewState.length / 1024);
            Toast.makeText(requireContext(), message, Toast.LENGTH_SHORT).show();
        }
    }

    ViewGroup getContainer() {
        return getView().findViewById(R.id.container);
    }

    private void createAndInitializeWebView() {
        final Bundle args = getArguments();
        if (args != null) {
            mProfileName = args.getString(ARG_PROFILE);
        }

        final Context context = requireContext();
        WebView webview =
                new WebView(context) {
                    @Override
                    public Object getTag(int key) {
                        if (mProfileName != null) {
                            if (key == R.id.multi_profile_name_tag_key) {
                                return mProfileName;
                            }
                        }
                        return super.getTag(key);
                    }
                };
        WebSettings settings = webview.getSettings();
        initializeSettings(settings);
        // Third party cookies are off by default on L+;
        // turn them on for consistency with normal browsers.
        CookieManager.getInstance().setAcceptThirdPartyCookies(webview, true);

        webview.setWebViewClient(
                new WebViewClientCompat() {
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
                        if (url.startsWith("about:")
                                || url.startsWith("chrome://")
                                || FILE_ANDROID_ASSET_PATTERN.matcher(url).matches()) {
                            return false;
                        }
                        return startBrowsingIntent(requireContext(), url);
                    }

                    @SuppressWarnings("deprecation") // because we support api level 19 and up.
                    @Override
                    public void onReceivedError(
                            WebView view, int errorCode, String description, String failingUrl) {
                        setUrlFail(true);
                    }

                    @Override
                    public void doUpdateVisitedHistory(WebView view, String url, boolean isReload) {
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                            if (view.canGoBack()) {
                                requireActivity()
                                        .getOnBackInvokedDispatcher()
                                        .registerOnBackInvokedCallback(
                                                OnBackInvokedDispatcher.PRIORITY_DEFAULT,
                                                mOnBackInvokedCallback);
                            } else if (!view.canGoBack()) {
                                requireActivity()
                                        .getOnBackInvokedDispatcher()
                                        .unregisterOnBackInvokedCallback(mOnBackInvokedCallback);
                            }
                        }
                    }
                });

        webview.setWebChromeClient(
                new WebChromeClient() {
                    @Override
                    public Bitmap getDefaultVideoPoster() {
                        return Bitmap.createBitmap(
                                new int[] {Color.TRANSPARENT}, 1, 1, Bitmap.Config.ARGB_8888);
                    }

                    @Override
                    public void onGeolocationPermissionsShowPrompt(
                            String origin, GeolocationPermissions.Callback callback) {
                        onPermissionRequest(new GeoPermissionRequest(origin, callback));
                    }

                    @Override
                    public void onPermissionRequest(PermissionRequest request) {
                        requestPermissionsForPage(request);
                    }

                    @Override
                    public void onShowCustomView(
                            View view, WebChromeClient.CustomViewCallback callback) {
                        if (mFullscreenView != null) {
                            ((ViewGroup) mFullscreenView.getParent()).removeView(mFullscreenView);
                        }
                        mFullscreenView = view;
                        requireActivity()
                                .getWindow()
                                .addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
                        requireActivity()
                                .getWindow()
                                .addContentView(
                                        mFullscreenView,
                                        new FrameLayout.LayoutParams(
                                                ViewGroup.LayoutParams.MATCH_PARENT,
                                                ViewGroup.LayoutParams.MATCH_PARENT,
                                                Gravity.CENTER));
                    }

                    @Override
                    public void onHideCustomView() {
                        if (mFullscreenView == null) {
                            return;
                        }
                        requireActivity()
                                .getWindow()
                                .clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
                        ((ViewGroup) mFullscreenView.getParent()).removeView(mFullscreenView);
                        mFullscreenView = null;
                    }

                    @Override
                    public boolean onShowFileChooser(
                            WebView webView,
                            ValueCallback<Uri[]> filePathCallback,
                            WebChromeClient.FileChooserParams fileChooserParams) {
                        setFilePathCallback(filePathCallback);
                        mMultiFileSelector.setFileChooserParams(fileChooserParams);
                        mFileContents.launch(null);
                        return true;
                    }
                });

        webview.setDownloadListener(
                new DownloadListener() {
                    @Override
                    public void onDownloadStart(
                            String url,
                            String userAgent,
                            String contentDisposition,
                            String mimeType,
                            long contentLength) {
                        Log.i(TAG, "url: " + url);
                        Log.i(TAG, "useragent: " + userAgent);
                        Log.i(TAG, "contentDisposition: " + contentDisposition);
                        Log.i(TAG, "mimeType: " + mimeType);
                        Log.i(TAG, "contentLength: " + contentLength);
                        new DownloadFileFromURL(url)
                                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
                    }
                });

        mWebView = webview;
        getContainer()
                .addView(
                        webview,
                        new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
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
        return PackageManager.PERMISSION_GRANTED
                == requireContext().checkSelfPermission(androidPermission);
    }

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
    public void onRequestPermissionsResult(
            int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
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
        if (Uri.parse(url).getScheme() == null) {
            url = "http://" + url;
        }
        setUrlBarText(url);
        setUrlFail(false);
        loadUrl(url);
        hideKeyboard(mUrlBar);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    // setGeolocationDatabasePath deprecated in api level 24,
    // but we still use it because we support api level 19 and up.
    @SuppressWarnings("deprecation")
    private void initializeSettings(WebSettings settings) {
        File geolocation = null;
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            geolocation = requireContext().getDir("geolocation", 0);
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

    private void loadUrl(String url) {
        // Request read access if necessary.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
                && "file".equals(Uri.parse(url).getScheme())) {
            if (PackageManager.PERMISSION_DENIED
                    == requireContext()
                            .checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE)) {
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
            mUrlBar.setTextAppearance(
                    requireContext(), fail ? R.style.UrlTextError : R.style.UrlText);
        }
    }

    /**
     * Hides the keyboard.
     *
     * @param view The {@link View} that is currently accepting input.
     * @return Whether the keyboard was visible before.
     */
    private static boolean hideKeyboard(View view) {
        InputMethodManager imm =
                (InputMethodManager)
                        view.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        return imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
    }

    static final Pattern BROWSER_URI_SCHEMA =
            Pattern.compile(
                    "(?i)" // switch on case insensitive matching
                            + "(" // begin group for schema
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

    /** Search for intent handlers that are specific to the scheme of the URL in the intent. */
    private static boolean isSpecializedHandlerAvailable(Intent intent) {
        List<ResolveInfo> handlers =
                PackageManagerUtils.queryIntentActivities(
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

    /**
     * A method to be used of the parent activity.
     *
     * @return instance of WebView that's being shown.
     */
    public WebView getWebView() {
        return mWebView;
    }

    public void resetWebView() {
        if (mWebView != null) {
            ViewGroup container = getContainer();
            container.removeView(mWebView);
            mWebView.destroy();
            mWebView = null;
        }
        createAndInitializeWebView();
    }

    public void hideKeyboard() {
        hideKeyboard(mUrlBar);
    }
}
