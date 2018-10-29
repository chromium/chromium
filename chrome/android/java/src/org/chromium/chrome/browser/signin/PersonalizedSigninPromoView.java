// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.ui.widget.ButtonCompat;

/**
 * Container view for personalized signin promos.
 */
public class PersonalizedSigninPromoView extends LinearLayout {
    private ImageView mImage;
    private ImageButton mDismissButton;
    private TextView mDescription;
    private ButtonCompat mSigninButton;
    private Button mChooseAccountButton;

    public PersonalizedSigninPromoView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mImage = findViewById(R.id.signin_promo_image);
        mDismissButton = findViewById(R.id.signin_promo_close_button);
        mDescription = findViewById(R.id.signin_promo_description);
        mSigninButton = findViewById(R.id.signin_promo_signin_button);
        mChooseAccountButton = findViewById(R.id.signin_promo_choose_account_button);
    }

    /**
     * @return A reference to the image of the promo.
     */
    public ImageView getImage() {
        return mImage;
    }

    /**
     * @return A reference to the dismiss button.
     */
    public ImageButton getDismissButton() {
        return mDismissButton;
    }

    /**
     * @return A reference to the description of the promo.
     */
    public TextView getDescription() {
        return mDescription;
    }

    /**
     * @return A reference to the signin button.
     */
    public ButtonCompat getSigninButton() {
        return mSigninButton;
    }

    /**
     * @return A reference to the choose account button.
     */
    public Button getChooseAccountButton() {
        return mChooseAccountButton;
    }
}
