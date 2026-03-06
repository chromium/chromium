// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.dialog;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;

import com.google.android.material.textfield.TextInputLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link SiteSearchDialogViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SiteSearchDialogViewBinderUnitTest {

    private Activity mActivity;
    private View mView;
    private PropertyModel mModel;

    private EditText mNameInput;
    private EditText mKeywordInput;
    private EditText mUrlInput;
    private TextInputLayout mNameLayout;
    private TextInputLayout mKeywordLayout;
    private TextInputLayout mUrlLayout;

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();
    @Mock private Callback<String> mNameChangedCallback;
    @Mock private Callback<String> mKeywordChangedCallback;
    @Mock private Callback<String> mUrlChangedCallback;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();

        View root = LayoutInflater.from(mActivity).inflate(R.layout.site_search_dialog, null);
        mNameInput = root.findViewById(R.id.name_input);
        mKeywordInput = root.findViewById(R.id.shortcut_input);
        mUrlInput = root.findViewById(R.id.url_input);
        mNameLayout = root.findViewById(R.id.name_input_layout);
        mKeywordLayout = root.findViewById(R.id.shortcut_input_layout);
        mUrlLayout = root.findViewById(R.id.url_input_layout);

        mModel =
                new PropertyModel.Builder(SiteSearchDialogProperties.ALL_KEYS)
                        .with(SiteSearchDialogProperties.NAME, "")
                        .with(SiteSearchDialogProperties.KEYWORD, "")
                        .with(SiteSearchDialogProperties.URL, "")
                        .with(SiteSearchDialogProperties.INVALID_NAME_ERROR_MESSAGE, null)
                        .with(SiteSearchDialogProperties.INVALID_KEYWORD_ERROR_MESSAGE, null)
                        .with(SiteSearchDialogProperties.INVALID_URL_ERROR_MESSAGE, null)
                        .with(SiteSearchDialogProperties.URL_ENABLED, true)
                        .with(SiteSearchDialogProperties.ON_NAME_CHANGED, mNameChangedCallback)
                        .with(
                                SiteSearchDialogProperties.ON_KEYWORD_CHANGED,
                                mKeywordChangedCallback)
                        .with(SiteSearchDialogProperties.ON_URL_CHANGED, mUrlChangedCallback)
                        .build();

        PropertyModelChangeProcessor.create(mModel, root, SiteSearchDialogViewBinder::bind);
    }

    @Test
    public void testSetName() {
        mModel.set(SiteSearchDialogProperties.NAME, "Google");
        assertEquals("Google", mNameInput.getText().toString());
    }

    @Test
    public void testSetKeyword() {
        mModel.set(SiteSearchDialogProperties.KEYWORD, "google.com");
        assertEquals("google.com", mKeywordInput.getText().toString());
    }

    @Test
    public void testSetUrl() {
        mModel.set(SiteSearchDialogProperties.URL, "https://google.com/search?q=%s");
        assertEquals("https://google.com/search?q=%s", mUrlInput.getText().toString());
    }

    @Test
    public void testSetNameError() {
        String errorMsg = "test error";
        mModel.set(SiteSearchDialogProperties.INVALID_NAME_ERROR_MESSAGE, errorMsg);
        assertEquals(errorMsg, mNameLayout.getError());
    }

    @Test
    public void testSetKeywordError() {
        String errorMsg = "test error";
        mModel.set(SiteSearchDialogProperties.INVALID_KEYWORD_ERROR_MESSAGE, errorMsg);
        assertEquals(errorMsg, mKeywordLayout.getError());
    }

    @Test
    public void testSetUrlError() {
        String errorMsg = "test error";
        mModel.set(SiteSearchDialogProperties.INVALID_URL_ERROR_MESSAGE, errorMsg);
        assertEquals(errorMsg, mUrlLayout.getError());
    }

    @Test
    public void testUrlEnabled() {
        mModel.set(SiteSearchDialogProperties.URL_ENABLED, false);
        assertFalse(mUrlInput.isEnabled());

        mModel.set(SiteSearchDialogProperties.URL_ENABLED, true);
        assertTrue(mUrlInput.isEnabled());
    }

    @Test
    public void testOnNameChanged() {
        mNameInput.setText("Google");
        verify(mNameChangedCallback).onResult(eq("Google"));
    }

    @Test
    public void testOnKeywordChanged() {
        mKeywordInput.setText("google.com");
        verify(mKeywordChangedCallback).onResult(eq("google.com"));
    }

    @Test
    public void testOnUrlChanged() {
        mUrlInput.setText("https://google.com/search?q=%s");
        verify(mUrlChangedCallback).onResult(eq("https://google.com/search?q=%s"));
    }
}
