// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.ImageView;
import android.widget.LinearLayout;

import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

/**
 * View for app management menu permission buttons.
 */
public class AppManagementMenuPermissionsView extends LinearLayout {
    private ImageView mNotificationsIcon;
    private ImageView mMicIcon;
    private ImageView mCameraIcon;
    private ImageView mLocationIcon;

    /**
     * Interface for receiving click events on permission buttons.
     */
    public interface OnButtonClickListener {
        void onButtonClick(WritableIntPropertyKey propertyKey);
    }

    /**
     * Constructor for inflating from XML.
     */
    public AppManagementMenuPermissionsView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mNotificationsIcon = findViewById(R.id.notifications_button);
        mMicIcon = findViewById(R.id.mic_button);
        mCameraIcon = findViewById(R.id.camera_button);
        mLocationIcon = findViewById(R.id.location_button);
    }

    void setOnImageButtonClick(OnButtonClickListener listener) {
        mNotificationsIcon.setOnClickListener(
                v -> listener.onButtonClick(AppManagementMenuPermissionsProperties.NOTIFICATIONS));
        mMicIcon.setOnClickListener(
                v -> listener.onButtonClick(AppManagementMenuPermissionsProperties.MIC));
        mCameraIcon.setOnClickListener(
                v -> listener.onButtonClick(AppManagementMenuPermissionsProperties.CAMERA));
        mLocationIcon.setOnClickListener(
                v -> listener.onButtonClick(AppManagementMenuPermissionsProperties.LOCATION));
    }
}
