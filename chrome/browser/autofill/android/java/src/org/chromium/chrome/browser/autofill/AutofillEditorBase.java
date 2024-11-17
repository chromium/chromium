// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnTouchListener;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.Spinner;

import androidx.fragment.app.Fragment;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.widget.FadingEdgeScrollView;
import org.chromium.ui.text.EmptyTextWatcher;

/** Base class for Autofill editors (e.g. credit cards and profiles). */
public abstract class AutofillEditorBase extends Fragment
        implements EmbeddableSettingsPage,
                OnItemSelectedListener,
                OnTouchListener,
                EmptyTextWatcher {
    /** We know which profile to edit based on the GUID stuffed in extras. */
    public static final String AUTOFILL_GUID = "guid";

    /** Needs to be in sync with autofill::kSettingsOrigin[]. */
    public static final String SETTINGS_ORIGIN = "Chrome settings";

    /** GUID of the profile we are editing.  Empty if creating a new profile. */
    protected String mGUID;

    /** Whether or not the editor is creating a new entry. */
    protected boolean mIsNewEntry;

    /** Context for the app. */
    protected Context mContext;

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        super.onCreateView(inflater, container, savedInstanceState);
        setHasOptionsMenu(true);
        mContext = container.getContext();

        Bundle extras = getArguments();
        if (extras != null) {
            mGUID = extras.getString(AUTOFILL_GUID);
        }
        if (mGUID == null) {
            mGUID = "";
            mIsNewEntry = true;
        } else {
            mIsNewEntry = false;
        }
        mPageTitle.set(getString(getTitleResourceId(mIsNewEntry)));

        View baseView = inflater.inflate(R.layout.autofill_editor_base, container, false);

        // Hide the top shadow on the ScrollView because the toolbar draws one.
        FadingEdgeScrollView scrollView =
                (FadingEdgeScrollView) baseView.findViewById(R.id.scroll_view);
        scrollView.setEdgeVisibility(
                FadingEdgeScrollView.EdgeType.NONE, FadingEdgeScrollView.EdgeType.FADING);
        scrollView
                .getViewTreeObserver()
                .addOnScrollChangedListener(
                        SettingsUtils.getShowShadowOnScrollListener(
                                scrollView, baseView.findViewById(R.id.shadow)));
        // Inflate the editor and buttons into the "content" LinearLayout.
        LinearLayout contentLayout = (LinearLayout) scrollView.findViewById(R.id.content);
        inflater.inflate(getLayoutId(), contentLayout, true);
        inflater.inflate(R.layout.autofill_editor_base_buttons, contentLayout, true);

        return baseView;
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    // Process touch event on spinner views so we can clear the keyboard.
    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouch(View v, MotionEvent event) {
        if (v instanceof Spinner) {
            InputMethodManager imm =
                    (InputMethodManager)
                            v.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
            imm.hideSoftInputFromWindow(v.getWindowToken(), 0);
        }
        return false;
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        inflater.inflate(R.menu.prefeditor_editor_menu, menu);

        MenuItem deleteItem = menu.findItem(R.id.delete_menu_id);
        if (deleteItem != null) deleteItem.setVisible(!mIsNewEntry && getIsDeletable());
    }

    /** @return True if the item is deletable. Can be false for server credit cards, for example. */
    protected boolean getIsDeletable() {
        return true;
    }

    /** Initializes the buttons within the layout. */
    protected void initializeButtons(View layout) {
        Button button = (Button) layout.findViewById(R.id.button_secondary);
        button.setOnClickListener(
                new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        finishPage();
                    }
                });

        button = (Button) layout.findViewById(R.id.button_primary);
        button.setOnClickListener(
                new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        if (saveEntry()) {
                            finishPage();
                        }
                    }
                });
        button.setEnabled(false);
    }

    /** Returns the ID of the layout to inflate. */
    protected abstract int getLayoutId();

    /** @return True if entry could be saved oand activity can be finished. */
    protected abstract boolean saveEntry();

    /** Called when the entry being edited should be deleted. */
    protected void deleteEntry() {
        assert false;
    }

    /** @return ID of the String to use as the title in the ActionBar. */
    protected abstract int getTitleResourceId(boolean isNewEntry);

    @Override
    public void onNothingSelected(AdapterView<?> parent) {}

    /** Finishes the current page. */
    protected void finishPage() {
        SettingsNavigationFactory.createSettingsNavigation().finishCurrentSettings(this);
    }
}
