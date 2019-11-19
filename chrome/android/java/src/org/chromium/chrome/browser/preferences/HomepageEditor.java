// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.partnercustomizations.HomepageManager;
import org.chromium.components.url_formatter.UrlFormatter;

/**
 * Provides the Java-UI for editing the homepage preference.
 */
public class HomepageEditor extends Fragment implements TextWatcher {
    private HomepageManager mHomepageManager;
    private EditText mHomepageUrlEdit;
    private Button mSaveButton;
    private Button mResetButton;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mHomepageManager = HomepageManager.getInstance();
        getActivity().setTitle(R.string.options_homepage_edit_title);
        View v = inflater.inflate(R.layout.homepage_editor, container, false);
        View scrollView = v.findViewById(R.id.scroll_view);
        scrollView.getViewTreeObserver().addOnScrollChangedListener(
                PreferenceUtils.getShowShadowOnScrollListener(v, v.findViewById(R.id.shadow)));
        mHomepageUrlEdit = (EditText) v.findViewById(R.id.homepage_url_edit);
        mHomepageUrlEdit.setText(HomepageManager.getHomepageUri());
        mHomepageUrlEdit.addTextChangedListener(this);
        mHomepageUrlEdit.requestFocus();

        initializeSaveCancelResetButtons(v);
        return v;
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {
    }

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {
        mSaveButton.setEnabled(true);
        mResetButton.setEnabled(true);
    }

    @Override
    public void afterTextChanged(Editable s) {
    }

    private void initializeSaveCancelResetButtons(View v) {
        mResetButton = (Button) v.findViewById(R.id.homepage_reset);
        mResetButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mHomepageManager.setPrefHomepageUseDefaultUri(true);
                getActivity().finish();
            }
        });
        if (mHomepageManager.getPrefHomepageUseDefaultUri()) {
            mResetButton.setEnabled(false);
        }

        mSaveButton = (Button) v.findViewById(R.id.homepage_save);
        mSaveButton.setEnabled(false);
        mSaveButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mHomepageManager.setPrefHomepageCustomUri(
                        UrlFormatter.fixupUrl(mHomepageUrlEdit.getText().toString()));
                mHomepageManager.setPrefHomepageUseDefaultUri(false);
                getActivity().finish();
            }
        });

        Button button = (Button) v.findViewById(R.id.homepage_cancel);
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                getActivity().finish();
            }
        });
    }
}
