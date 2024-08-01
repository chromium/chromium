// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.Button;
import android.widget.TextView;

import androidx.fragment.app.Fragment;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.DefaultSearchEngineDialogHelper;
import org.chromium.chrome.browser.search_engines.SearchEnginePromoType;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.browser_ui.widget.RadioButtonLayout;

/** A {@link Fragment} that presents a set of search engines for the user to choose from. */
public class DefaultSearchEngineFirstRunFragment extends Fragment implements FirstRunFragment {
    @SearchEnginePromoType private int mSearchEnginePromoDialogType;
    private boolean mShownRecorded;

    /** Layout that displays the available search engines to the user. */
    private RadioButtonLayout mEngineLayout;

    /** The button that lets a user proceed to the next page after an engine is selected. */
    private Button mButton;

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View rootView =
                inflater.inflate(
                        R.layout.default_search_engine_first_run_fragment, container, false);
        mEngineLayout = rootView.findViewById(R.id.default_search_engine_dialog_options);
        mButton = rootView.findViewById(R.id.button_primary);
        mButton.setEnabled(false);

        ((TextView) rootView.findViewById(R.id.footer))
                .setText(R.string.search_engine_dialog_footer);
        mButton.setText(R.string.search_engine_dialog_confirm_button_title);

        assert getPageDelegate().getProfileProviderSupplier().get() != null;
        Profile profile = getPageDelegate().getProfileProviderSupplier().get().getOriginalProfile();

        assert TemplateUrlServiceFactory.getForProfile(profile).isLoaded();
        mSearchEnginePromoDialogType = LocaleManager.getInstance().getSearchEnginePromoShowType();
        if (mSearchEnginePromoDialogType != SearchEnginePromoType.DONT_SHOW) {
            new DefaultSearchEngineDialogHelper(
                    mSearchEnginePromoDialogType,
                    LocaleManager.getInstance(),
                    mEngineLayout,
                    mButton,
                    getPageDelegate()::advanceToNextPage);
        }

        return rootView;
    }

    @Override
    public void setInitialA11yFocus() {
        // Ignore calls before view is created.
        if (getView() == null) return;

        final View title = getView().findViewById(R.id.chooser_title);
        title.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    @Override
    public void onResume() {
        super.onResume();

        if (mSearchEnginePromoDialogType == SearchEnginePromoType.DONT_SHOW) {
            PostTask.postTask(TaskTraits.UI_DEFAULT, () -> getPageDelegate().advanceToNextPage());
        }

        recordShown();
    }

    private void recordShown() {
        if (mShownRecorded) return;

        if (mSearchEnginePromoDialogType == SearchEnginePromoType.SHOW_NEW) {
            RecordUserAction.record("SearchEnginePromo.NewDevice.Shown.FirstRun");
        } else if (mSearchEnginePromoDialogType == SearchEnginePromoType.SHOW_EXISTING) {
            RecordUserAction.record("SearchEnginePromo.ExistingDevice.Shown.FirstRun");
        }

        mShownRecorded = true;
    }
}
