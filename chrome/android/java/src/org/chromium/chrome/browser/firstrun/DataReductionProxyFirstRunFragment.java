// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.content.res.Resources;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v7.widget.SwitchCompat;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.datareduction.DataReductionPromoUtils;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/**
 * The First Run Experience fragment that allows the user to opt in to Data Saver.
 */
public class DataReductionProxyFirstRunFragment extends Fragment implements FirstRunFragment {
    /** FRE page that instantiates this fragment. */
    public static class Page implements FirstRunPage<DataReductionProxyFirstRunFragment> {
        @Override
        public DataReductionProxyFirstRunFragment instantiateFragment() {
            return new DataReductionProxyFirstRunFragment();
        }
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fre_data_reduction_proxy_lite_mode, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        final TextView promoSummaryTextView =
                (TextView) view.findViewById(R.id.data_reduction_promo_summary_text);
        final SwitchCompat enableDataSaverSwitch = (SwitchCompat) view
                .findViewById(R.id.enable_data_saver_switch);
        Button nextButton = (Button) view.findViewById(R.id.next_button);

        enableDataSaverSwitch.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                DataReductionProxySettings.getInstance().setDataReductionProxyEnabled(
                        v.getContext(), enableDataSaverSwitch.isChecked());
                if (enableDataSaverSwitch.isChecked()) {
                    enableDataSaverSwitch.setText(R.string.data_reduction_enabled_switch_lite_mode);
                } else {
                    enableDataSaverSwitch.setText(
                            R.string.data_reduction_disabled_switch_lite_mode);
                }
            }
        });

        // Setup Promo Text Learn More Link
        Resources resources = getResources();
        NoUnderlineClickableSpan clickablePromoLearnMoreSpan =
                new NoUnderlineClickableSpan(resources, (view1) -> {
                    if (!isAdded()) return;
                    getPageDelegate().showInfoPage(R.string.data_reduction_promo_learn_more_url);
                });

        promoSummaryTextView.setMovementMethod(LinkMovementMethod.getInstance());
        promoSummaryTextView.setText(
                SpanApplier.applySpans(getString(R.string.data_reduction_promo_summary_lite_mode),
                        new SpanInfo("<link>", "</link>", clickablePromoLearnMoreSpan)));

        nextButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                getPageDelegate().advanceToNextPage();
            }
        });

        enableDataSaverSwitch.setChecked(true);
        DataReductionProxySettings.getInstance().setDataReductionProxyEnabled(
                view.getContext(), enableDataSaverSwitch.isChecked());
    }

    @Override
    public void onStart() {
        super.onStart();
        DataReductionPromoUtils.saveFreOrSecondRunPromoDisplayed();
    }
}
