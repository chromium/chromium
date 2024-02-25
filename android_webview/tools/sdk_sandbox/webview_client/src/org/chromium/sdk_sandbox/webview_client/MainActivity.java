// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sdk_sandbox.webview_client;

import static android.app.sdksandbox.SdkSandboxManager.EXTRA_DISPLAY_ID;
import static android.app.sdksandbox.SdkSandboxManager.EXTRA_HEIGHT_IN_PIXELS;
import static android.app.sdksandbox.SdkSandboxManager.EXTRA_HOST_TOKEN;
import static android.app.sdksandbox.SdkSandboxManager.EXTRA_SURFACE_PACKAGE;
import static android.app.sdksandbox.SdkSandboxManager.EXTRA_WIDTH_IN_PIXELS;

import android.app.Activity;
import android.app.sdksandbox.LoadSdkException;
import android.app.sdksandbox.RequestSurfacePackageException;
import android.app.sdksandbox.SandboxedSdk;
import android.app.sdksandbox.SdkSandboxManager;
import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.OutcomeReceiver;
import android.os.RemoteException;
import android.view.KeyEvent;
import android.view.SurfaceControlViewHost.SurfacePackage;
import android.view.SurfaceView;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;

import org.chromium.sdk_sandbox.webview_sdk.IWebViewSdkApi;

/**
 * This activity allows manual testing of WebView inside the sdk sandbox.
 * There are buttons to:
 * -> load an sdk that has methods to interact with the WebView
 * -> load and display the WebView
 * -> load a url in the WebView specified via a URL bar
 */
public class MainActivity extends Activity {
    private static final String SDK_NAME = "org.chromium.sdk_sandbox.webview_sdk";
    private static final String TAG = "WebViewSdkSandboxActivity";

    private static final Handler sHandler = new Handler(Looper.getMainLooper());

    private SdkSandboxManager mSdkSandboxManager;
    private SandboxedSdk mSandboxedSdk;
    private IWebViewSdkApi mWebViewProxy;
    private boolean mSdkLoaded;
    private boolean mSurfacePackageLoaded;

    private Button mLoadSdkButton;
    private Button mLoadSurfacePackageButton;
    private ImageButton mLoadUrlButton;
    private EditText mUrlBar;
    private SurfaceView mRenderedView;

    private OutcomeReceiver<SandboxedSdk, LoadSdkException> mLoadSdkReceiver =
            new OutcomeReceiver<>() {
                @Override
                public void onResult(SandboxedSdk sandboxedSdk) {
                    mSdkLoaded = true;
                    mSandboxedSdk = sandboxedSdk;
                    mWebViewProxy = IWebViewSdkApi.Stub.asInterface(mSandboxedSdk.getInterface());
                    makeToast("SDK Loaded successfully!");
                    mLoadSdkButton.setText(R.string.unload_sdk_provider);
                }

                @Override
                public void onError(LoadSdkException error) {
                    makeToast(
                            String.format(
                                    "Failed: %s. Error code %s!",
                                    error, error.getLoadSdkErrorCode()));
                    resetStateForLoadSdkButton();
                }
            };

    private OutcomeReceiver<Bundle, RequestSurfacePackageException> mLoadSurfacePackageReceiver =
            new OutcomeReceiver<Bundle, RequestSurfacePackageException>() {
                @Override
                public void onResult(Bundle result) {
                    sHandler.post(
                            () -> {
                                SurfacePackage surfacePackage =
                                        result.getParcelable(
                                                EXTRA_SURFACE_PACKAGE, SurfacePackage.class);
                                mRenderedView.setChildSurfacePackage(surfacePackage);
                                mRenderedView.setVisibility(View.VISIBLE);
                                mSurfacePackageLoaded = true;
                            });
                    makeToast("Rendered surface view");
                }

                @Override
                public void onError(@NonNull RequestSurfacePackageException error) {
                    makeToast(
                            String.format(
                                    "Failed: %s. Error code %s!",
                                    error, error.getRequestSurfacePackageErrorCode()));
                }
            };

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        mSdkSandboxManager = getApplicationContext().getSystemService(SdkSandboxManager.class);

        mLoadSdkButton = findViewById(R.id.btn_load_sdk);
        mLoadSurfacePackageButton = findViewById(R.id.btn_load_surface_package);

        mLoadUrlButton = findViewById(R.id.btn_load_url);
        mUrlBar = findViewById(R.id.url_field);

        mRenderedView = findViewById(R.id.rendered_view);
        mRenderedView.setZOrderOnTop(true);
        mRenderedView.setVisibility(View.INVISIBLE);

        registerLoadSdkButton();
        registerLoadSurfacePackageButton();
        registerLoadUrlComponents();
    }

    private void registerLoadSdkButton() {
        mLoadSdkButton.setOnClickListener(
                v -> {
                    if (mSdkLoaded) {
                        resetStateForLoadSdkButton();
                        return;
                    }

                    // Register for sandbox death event.
                    mSdkSandboxManager.addSdkSandboxProcessDeathCallback(
                            Runnable::run, () -> makeToast("Sdk Sandbox process died"));

                    Bundle params = new Bundle();
                    runOnUiThread(
                            () ->
                                    mSdkSandboxManager.loadSdk(
                                            SDK_NAME, params, Runnable::run, mLoadSdkReceiver));
                });
    }

    private void resetStateForLoadSdkButton() {
        try {
            mWebViewProxy.destroy();
        } catch (RemoteException e) {
        }
        mSdkSandboxManager.unloadSdk(SDK_NAME);
        mLoadSdkButton.setText(R.string.load_sdk_provider);
        mRenderedView.setVisibility(View.INVISIBLE);
        mSdkLoaded = false;
        mSurfacePackageLoaded = false;
    }

    private void registerLoadSurfacePackageButton() {
        mLoadSurfacePackageButton.setOnClickListener(
                v -> {
                    if (mSdkLoaded) {
                        sHandler.post(
                                () -> {
                                    mSdkSandboxManager.requestSurfacePackage(
                                            SDK_NAME,
                                            getRequestSurfacePackageParams(),
                                            Runnable::run,
                                            mLoadSurfacePackageReceiver);
                                });
                    } else {
                        makeToast("Sdk is not loaded");
                    }
                });
    }

    private void registerLoadUrlComponents() {
        mUrlBar.setOnKeyListener(
                (View view, int keyCode, KeyEvent event) -> {
                    if (keyCode == KeyEvent.KEYCODE_ENTER
                            && event.getAction() == KeyEvent.ACTION_UP) {
                        loadUrl();
                        return true;
                    }
                    return false;
                });

        mLoadUrlButton.setOnClickListener(
                v -> {
                    loadUrl();
                });
    }

    private void loadUrl() {
        if (mSdkLoaded) {
            if (mSurfacePackageLoaded) {
                try {
                    String url = mUrlBar.getText().toString();
                    // Parse with android.net.Uri instead of java.net.URI because Uri does no
                    // validation. Rather than failing in the browser, let WebView handle weird
                    // URLs. WebView will escape illegal characters and display error pages for bad
                    // URLs like "blah://example.com".
                    if (Uri.parse(url).getScheme() == null) url = "http://" + url;
                    mUrlBar.setText(url, TextView.BufferType.EDITABLE);
                    mWebViewProxy.loadUrl(url);
                    // hide keyboard
                    InputMethodManager imm =
                            (InputMethodManager)
                                    mUrlBar.getContext()
                                            .getSystemService(Context.INPUT_METHOD_SERVICE);
                    imm.hideSoftInputFromWindow(mUrlBar.getWindowToken(), 0);
                } catch (RemoteException e) {
                }
            } else {
                makeToast("Surface Package (WebView) is not loaded");
            }
        } else {
            makeToast("Sdk is not loaded");
        }
    }

    private Bundle getRequestSurfacePackageParams() {
        Bundle params = new Bundle();
        params.putInt(EXTRA_WIDTH_IN_PIXELS, mRenderedView.getWidth());
        params.putInt(EXTRA_HEIGHT_IN_PIXELS, mRenderedView.getHeight());
        params.putInt(EXTRA_DISPLAY_ID, getDisplay().getDisplayId());
        params.putBinder(EXTRA_HOST_TOKEN, mRenderedView.getHostToken());
        return params;
    }

    private void makeToast(String message) {
        runOnUiThread(() -> Toast.makeText(MainActivity.this, message, Toast.LENGTH_LONG).show());
    }
}
