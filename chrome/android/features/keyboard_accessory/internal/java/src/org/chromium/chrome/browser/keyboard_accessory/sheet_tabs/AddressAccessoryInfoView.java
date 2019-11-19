// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.LinearLayout;

import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.ui.widget.ChipView;

/**
 * This view represents a section of user data in the address tab of the keyboard accessory.
 */
class AddressAccessoryInfoView extends LinearLayout {
    private ChipView mNameFull;
    private ChipView mCompanyName;
    private ChipView mAddressHomeLine1;
    private ChipView mAddressHomeLine2;
    private ChipView mAddressHomeZip;
    private ChipView mAddressHomeCity;
    private ChipView mAddressHomeState;
    private ChipView mAddressHomeCountry;
    private ChipView mPhoneHomeWholeNumber;
    private ChipView mEmailAddress;

    /**
     * Constructor for inflating from XML.
     */
    public AddressAccessoryInfoView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mNameFull = findViewById(R.id.name_full);
        mCompanyName = findViewById(R.id.company_name);
        mAddressHomeLine1 = findViewById(R.id.address_home_line_1);
        mAddressHomeLine2 = findViewById(R.id.address_home_line_2);
        mAddressHomeZip = findViewById(R.id.address_home_zip);
        mAddressHomeCity = findViewById(R.id.address_home_city);
        mAddressHomeState = findViewById(R.id.address_home_state);
        mAddressHomeCountry = findViewById(R.id.address_home_country);
        mPhoneHomeWholeNumber = findViewById(R.id.phone_home_whole_number);
        mEmailAddress = findViewById(R.id.email_address);
    }

    public ChipView getNameFull() {
        return mNameFull;
    }

    public ChipView getCompanyName() {
        return mCompanyName;
    }

    public ChipView getAddressHomeLine1() {
        return mAddressHomeLine1;
    }

    public ChipView getAddressHomeLine2() {
        return mAddressHomeLine2;
    }

    public ChipView getAddressHomeZip() {
        return mAddressHomeZip;
    }

    public ChipView getAddressHomeCity() {
        return mAddressHomeCity;
    }

    public ChipView getAddressHomeState() {
        return mAddressHomeState;
    }

    public ChipView getAddressHomeCountry() {
        return mAddressHomeCountry;
    }

    public ChipView getPhoneHomeWholeNumber() {
        return mPhoneHomeWholeNumber;
    }

    public ChipView getEmailAddress() {
        return mEmailAddress;
    }
}
