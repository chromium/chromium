// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.WindowManager;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;

/** Activity to exercise transform animations on WebView. */
public class WebViewAnimationTestActivity extends Activity {
    private static final String HTML =
            "<html>"
                    + "  <head>"
                    + "    <style type =\"text/css\">"
                    + "      .container {"
                    + "            display: grid;"
                    + "            grid-template-columns: 100px 100px 100px 100px 100px;"
                    + "            grid-template-rows: 100px 100px 100px 100px 100px;"
                    + "      }"
                    + "     .alt1 {"
                    + "       background-color: #aaffaa;"
                    + "     }"
                    + "     .alt2 {"
                    + "       background-color: #ff4545;"
                    + "     }"
                    + "    </style>"
                    + "  </head>"
                    + "  <body>"
                    + "   <div class=\"container\">"
                    + "     <div class=\"alt1\">00</div>"
                    + "     <div class=\"alt2\">01</div>"
                    + "     <div class=\"alt1\">02</div>"
                    + "     <div class=\"alt2\">03</div>"
                    + "     <div class=\"alt1\">04</div>"
                    + "     <div class=\"alt2\">05</div>"
                    + "     <div class=\"alt1\">06</div>"
                    + "     <div class=\"alt2\">07</div>"
                    + "     <div class=\"alt1\">08</div>"
                    + "     <div class=\"alt2\">09</div>"
                    + "     <div class=\"alt1\">10</div>"
                    + "     <div class=\"alt2\">11</div>"
                    + "     <div class=\"alt1\">12</div>"
                    + "     <div class=\"alt2\">13</div>"
                    + "     <div class=\"alt1\">14</div>"
                    + "     <div class=\"alt2\">15</div>"
                    + "     <div class=\"alt1\">16</div>"
                    + "     <div class=\"alt2\">17</div>"
                    + "     <div class=\"alt1\">18</div>"
                    + "     <div class=\"alt2\">19</div>"
                    + "     <div class=\"alt1\">20</div>"
                    + "     <div class=\"alt2\">21</div>"
                    + "     <div class=\"alt1\">22</div>"
                    + "     <div class=\"alt2\">23</div>"
                    + "     <div class=\"alt1\">24</div>"
                    + "   </div>"
                    + "  </body>"
                    + "</html>";

    private WebViewWithClipPath mWebView;
    private boolean mIsWindowHardwareAccelerated;

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_webview_animation_test);
        mWebView = (WebViewWithClipPath) findViewById(R.id.webview);

        mIsWindowHardwareAccelerated =
                (getWindow().getAttributes().flags
                                | WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED)
                        != 0;
        mWebView.setBackgroundColor(0);
        mWebView.loadDataWithBaseURL("http://foo.bar", HTML, "text/html", null, "http://foo.bar");
        OnClickListener onClickListner =
                (View v) -> {
                    int viewId = v.getId();
                    if (viewId == R.id.translate) {
                        runTranslate();
                    } else if (viewId == R.id.scale) {
                        runScale();
                    } else if (viewId == R.id.rotate) {
                        runRotate();
                    }
                };
        findViewById(R.id.scale).setOnClickListener(onClickListner);
        findViewById(R.id.translate).setOnClickListener(onClickListner);
        findViewById(R.id.rotate).setOnClickListener(onClickListner);
        ((SeekBar) findViewById(R.id.view_alpha))
                .setOnSeekBarChangeListener(
                        new OnSeekBarChangeListener() {
                            @Override
                            public void onProgressChanged(
                                    SeekBar view, int progress, boolean fromUser) {
                                if (view.getId() == R.id.view_alpha) {
                                    mWebView.setAlpha(progress / 100f);
                                }
                            }

                            @Override
                            public void onStartTrackingTouch(SeekBar seekBar) {}

                            @Override
                            public void onStopTrackingTouch(SeekBar seekBar) {}
                        });
        CheckBox layerCheckBox = findViewById(R.id.use_layer);
        layerCheckBox.setOnCheckedChangeListener(
                (CompoundButton arg0, boolean checked) -> {
                    setWebViewLayer(checked);
                });
        setWebViewLayer(layerCheckBox.isChecked());

        CheckBox stencilCheckBox = findViewById(R.id.use_stencil);
        stencilCheckBox.setOnCheckedChangeListener(
                (CompoundButton arg0, boolean checked) -> {
                    setUseExternalStencil(checked);
                });
        setUseExternalStencil(stencilCheckBox.isChecked());
    }

    private void runTranslate() {
        if (mWebView.getTranslationX() == 0f) {
            mWebView.animate().translationX(100f).translationY(100f);
        } else {
            mWebView.animate().translationX(0f).translationY(0f);
        }
    }

    private void runScale() {
        if (mWebView.getScaleX() == 1f) {
            mWebView.animate().scaleX(.5f).scaleY(.5f);
        } else {
            mWebView.animate().scaleX(1f).scaleY(1f);
        }
    }

    private void runRotate() {
        if (mWebView.getRotationX() == 0f) {
            mWebView.animate().rotationX(45f).rotationY(45f).rotation(90f);
        } else {
            mWebView.animate().rotationX(0f).rotationY(0f).rotation(0f);
        }
    }

    private void setWebViewLayer(boolean isOnLayer) {
        if (isOnLayer) {
            mWebView.setLayerType(
                    mIsWindowHardwareAccelerated
                            ? View.LAYER_TYPE_HARDWARE
                            : View.LAYER_TYPE_SOFTWARE,
                    null);
        } else {
            mWebView.setLayerType(View.LAYER_TYPE_NONE, null);
        }
    }

    private void setUseExternalStencil(boolean useStecil) {
        mWebView.setEnableClipPath(useStecil);
    }
}
